/***********************************************
* @Fitxer: Gotham.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Implementació de Gotham per gestionar connexions i processar comandes.
************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"
#include "FrameUtils/FrameUtils.h"

// Representa un worker que es connecta a Gotham
typedef struct {
    char ip[16];    // Direcció IP del worker
    int port;       // Port del worker
    char type[10];  // Tipus de worker: "TEXT" o "MEDIA"
    int socket_fd;  // Descriptor del socket associat al worker
} WorkerInfo;

// Administra una llista dinàmica de workers registrats
typedef struct {
    WorkerInfo *workers;        // Llista dinàmica de workers registrats
    int workerCount;            // Nombre de workers registrats
    int capacity;               // Capacitat actual de la llista
    WorkerInfo *mainTextWorker; // Worker principal per a TEXT
    WorkerInfo *mainMediaWorker;// Worker principal per a MEDIA
    pthread_mutex_t mutex;      // Mutex per sincronitzar l'accés
} WorkerManager;

// Declaració de funcions
void mostrarCaratula();
void logInfo(const char *msg);
void logWarning(const char *msg);
void logError(const char *msg);
void logSuccess(const char *msg);
WorkerManager *createWorkerManager();
void freeWorkerManager(WorkerManager *manager);
void *gestionarConexion(void *arg);
void *esperarConexiones(void *arg);
void processCommandInGotham(const Frame *frame, int client_fd, WorkerManager *manager);
void registrarWorker(const char *payload, WorkerManager *manager, int client_fd);
int logoutWorkerBySocket(int socket_fd, WorkerManager *manager);
WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager);
void handleDisconnectFrame(const Frame *frame, int client_fd, WorkerManager *manager);
void handleSigint(int sig);
void alliberarMemoria(GothamConfig *gothamConfig);

WorkerManager *workerManager = NULL;

// Mostra una caràtula informativa al terminal
void mostrarCaratula() {
    printF(GREEN BOLD);
    printF("###############################################\n");
    printF("#                                             #\n");
    printF("#       BENVINGUT AL SERVIDOR GOTHAM          #\n");
    printF("#                                             #\n");
    printF("#      Gestió de connexions amb Harley,       #\n");
    printF("#        Enigma i Fleck Workers              #\n");
    printF("#                                             #\n");
    printF("###############################################\n\n");
    printF(RESET);
}

// Funcions de registre de logs
void logInfo(const char *msg) {
    printF(CYAN "[INFO]: " RESET);
    printF(msg);
    printF("\n");
}

void logWarning(const char *msg) {
    printF(YELLOW "[WARNING]: " RESET);
    printF(msg);
    printF("\n");
}

void logError(const char *msg) {
    printF(RED "[ERROR]: " RESET);
    printF(msg);
    printF("\n");
}

void logSuccess(const char *msg) {
    printF(GREEN "[SUCCESS]: " RESET);
    printF(msg);
    printF("\n");
}

// Crea un nou gestor de workers
WorkerManager *createWorkerManager() {
    WorkerManager *manager = malloc(sizeof(WorkerManager));
    if (!manager) {
        perror("Error inicialitzant WorkerManager");
        exit(EXIT_FAILURE);
    }
    manager->capacity = 10;
    manager->workerCount = 0;
    manager->workers = malloc(manager->capacity * sizeof(WorkerInfo));
    if (!manager->workers) {
        perror("Error inicialitzant la llista de workers");
        free(manager);
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&manager->mutex, NULL);
    return manager;
}

void listarWorkers(WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex); // Asegura acceso seguro a la lista dinámica

    logInfo("[INFO]: Lista de workers registrados: ");
    for (int i = 0; i < manager->workerCount; i++) {
        WorkerInfo *worker = &manager->workers[i];
        char *log_message;
        asprintf(&log_message, "\nWorker %d: Tipo: %s, IP: %s, Puerto: %d, Socket FD: %d",
                    i + 1, worker->type, worker->ip, worker->port, worker->socket_fd);
        printF(log_message);
        free(log_message);
    }

    pthread_mutex_unlock(&manager->mutex);
}

void registrarWorker(const char *payload, WorkerManager *manager, int client_fd) {
    logInfo("[INFO]: Iniciando el registro del worker...");
    char type[10] = {0}, ip[16] = {0};
    int port = 0;
    Frame response = {0};

    // Procesar el payload del worker
    if (sscanf(payload, "%9[^&]&%15[^&]&%d", type, ip, &port) != 3) {
        logError("[ERROR]: El payload no tiene el formato esperado (TYPE&IP&PORT).");

        // Enviar NACK
        response.type = 0x02;
        strncpy(response.data, "CON_KO", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.checksum = calculate_checksum(response.data, response.data_length);
        send_frame(client_fd, &response);
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    // Ampliar la capacidad si es necesario
    if (manager->workerCount == manager->capacity) {
        manager->capacity *= 2;
        manager->workers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
        if (!manager->workers) {
            logError("[ERROR]: Error ampliando la capacidad de workers.");

            // Enviar NACK
            response.type = 0x02;
            strncpy(response.data, "CON_KO", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);

            pthread_mutex_unlock(&manager->mutex);
            return;
        }
    }

    // Registrar el nuevo worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    strncpy(worker->type, type, sizeof(worker->type) - 1);
    strncpy(worker->ip, ip, sizeof(worker->ip) - 1);
    worker->port = port;
    worker->socket_fd = client_fd;

    // Asignar como principal si es el primero de su tipo
    if (strcasecmp(type, "TEXT") == 0 && manager->mainTextWorker == NULL) {
        manager->mainTextWorker = worker;
    } else if (strcasecmp(type, "MEDIA") == 0 && manager->mainMediaWorker == NULL) {
        manager->mainMediaWorker = worker;
    }

    // Confirmar registro
    logSuccess("[SUCCESS]: Worker registrado correctamente.");
    char *log_message;
    asprintf(&log_message, "Tipo: %s, IP: %s, Puerto: %d, Socket FD: %d\n",
             worker->type, worker->ip, worker->port, worker->socket_fd);
    printF(log_message);
    free(log_message);

    pthread_mutex_unlock(&manager->mutex);

    // Enviar ACK
    response.type = 0x02;
    response.data_length = 0;
    response.checksum = calculate_checksum(response.data, response.data_length);
    send_frame(client_fd, &response);
}


WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager) {
    if (!filename || !manager) {
        logError("[ERROR]: Nombre de archivo o manager inválido.");
        return NULL;
    }

    char *extension = strrchr(filename, '.');
    if (!extension) {
        logError("[ERROR]: No se pudo determinar la extensión del archivo.");
        return NULL;
    }

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *targetWorker = NULL;

    if (strcasecmp(extension, ".txt") == 0) {
        targetWorker = manager->mainTextWorker;
        logInfo("[INFO]: Worker principal asignado para archivos de texto.");
    } else if (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".png") == 0) {
        targetWorker = manager->mainMediaWorker;
        logInfo("[INFO]: Worker principal asignado para archivos multimedia.");
    } else {
        logError("[ERROR]: Extensión no reconocida.");
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!targetWorker) {
        logError("[ERROR]: No hay workers disponibles para el tipo de archivo.");
    } else {
        char *log_message;
        asprintf(&log_message, "[DEBUG]: Worker encontrado -> IP: %s, Puerto: %d\n",
                 targetWorker->ip, targetWorker->port);
        printF(log_message);
        free(log_message);
    }

    return targetWorker;
}

int logoutWorkerBySocket(int socket_fd, WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->workers[i].socket_fd == socket_fd) {
            manager->workers[i] = manager->workers[--manager->workerCount];
            pthread_mutex_unlock(&manager->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    return -1;
}


// Allibera la memòria utilitzada pel WorkerManager
void freeWorkerManager(WorkerManager *manager) {
    if (manager) {
        free(manager->workers);
        pthread_mutex_destroy(&manager->mutex);
        free(manager);
    }
}

// Gestiona la connexió amb un client
void *gestionarConexion(void *arg) {
    int client_fd = *(int *)arg;
    WorkerManager *manager = *((WorkerManager **)((int *)arg + 1));

    free(arg);

    Frame frame;
    while (receive_frame(client_fd, &frame) == 0) {
        processCommandInGotham(&frame, client_fd, manager);
    }

    pthread_mutex_lock(&manager->mutex);
    logoutWorkerBySocket(client_fd, manager);
    pthread_mutex_unlock(&manager->mutex);

    close(client_fd);
    return NULL;
}

// Espera connexions entrants i crea fils per gestionar-les
void *esperarConexiones(void *arg) {
    int *server_fds = (int *)arg;
    int server_fd_fleck = server_fds[0];
    int server_fd_enigma = server_fds[1];
    int server_fd_harley = server_fds[2];

    fd_set read_fds;
    int max_fd = server_fd_fleck > server_fd_enigma ? server_fd_fleck : server_fd_enigma;
    max_fd = server_fd_harley > max_fd ? server_fd_harley : max_fd;

    WorkerManager *manager = createWorkerManager();

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd_fleck, &read_fds);
        FD_SET(server_fd_enigma, &read_fds);
        FD_SET(server_fd_harley, &read_fds);

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error en select");
            continue;
        }

        if (FD_ISSET(server_fd_fleck, &read_fds)) {
            int *client_fd = malloc(sizeof(int) + sizeof(WorkerManager *));
            *client_fd = accept_connection(server_fd_fleck);
            if (*client_fd >= 0) {
                memcpy(client_fd + 1, &manager, sizeof(WorkerManager *));
                pthread_t thread;
                pthread_create(&thread, NULL, gestionarConexion, client_fd);
                pthread_detach(thread);
            } else {
                free(client_fd);
            }
        }

        if (FD_ISSET(server_fd_enigma, &read_fds)) {
            int *client_fd = malloc(sizeof(int) + sizeof(WorkerManager *));
            *client_fd = accept_connection(server_fd_enigma);
            if (*client_fd >= 0) {
                memcpy(client_fd + 1, &manager, sizeof(WorkerManager *));
                pthread_t thread;
                pthread_create(&thread, NULL, gestionarConexion, client_fd);
                pthread_detach(thread);
            } else {
                free(client_fd);
            }
        }

        if (FD_ISSET(server_fd_harley, &read_fds)) {
            int *client_fd = malloc(sizeof(int) + sizeof(WorkerManager *));
            *client_fd = accept_connection(server_fd_harley);
            if (*client_fd >= 0) {
                memcpy(client_fd + 1, &manager, sizeof(WorkerManager *));
                pthread_t thread;
                pthread_create(&thread, NULL, gestionarConexion, client_fd);
                pthread_detach(thread);
            } else {
                free(client_fd);
            }
        }
    }

    freeWorkerManager(manager);
    return NULL;
}

// Processa un frame rebut
/***********************************************
* @Finalitat: Processa una comanda rebuda a Gotham.
* Identifica el tipus de comanda i executa les accions corresponents.
* @Paràmetres:
*   in: frame = el frame rebut del client.
*   in: client_fd = descriptor del client que ha enviat la comanda.
*   in: manager = punter al WorkerManager.
* @Retorn: ----
************************************************/
void processCommandInGotham(const Frame *frame, int client_fd, WorkerManager *manager) {
    if (!frame) {
        logError("El frame rebut és NULL.");
        return;
    }

    // Validar el checksum del frame rebut
    uint16_t calculated_checksum = calculate_checksum(frame->data, frame->data_length);       
    if (calculated_checksum != frame->checksum) {
        logError("Trama amb checksum invàlid.");
        return;
    }

    logSuccess("Checksum correcte.");

    // Estructura per preparar la resposta
    Frame response = {0};
    response.timestamp = (uint32_t)time(NULL);

    switch (frame->type) {
        case 0x01: // CONNECT
            logInfo("[INFO]: Procesando conexión de Fleck...");
            
            // Validar datos recibidos (puedes añadir más validaciones según sea necesario)
            if (frame->data_length == 0) {
                logError("[ERROR]: Datos de conexión vacíos.");
                response.type = 0x01;
                strncpy(response.data, "CONN_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Enviar respuesta de éxito
            logSuccess("[SUCCESS]: Client connectat correctament.");
            response.type = 0x01;
            strncpy(response.data, "CONN_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);
            break;

        case 0x02: // REGISTER
            logInfo("[DEBUG]: Processant registre de worker...");
            registrarWorker(frame->data, manager, client_fd);
            listarWorkers(manager);
            break;

        case 0x07:
            handleDisconnectFrame(frame, client_fd, manager);
            break;

        case 0x10: // DISTORT
            logInfo("[INFO]: Procesando comando DISTORT...");

            // Parsear el payload recibido: mediaType y fileName
            char mediaType[10] = {0};
            char fileName[256] = {0};
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType, fileName) != 2) {
                logError("[ERROR]: Formato de datos incorrecto en comando DISTORT.");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Verificar si el fileName tiene la extensión correcta según el mediaType
            char *fileExtension = strrchr(fileName, '.');
            if (!fileExtension || 
                (strcasecmp(mediaType, "TEXT") == 0 && strcasecmp(fileExtension, ".txt") != 0) ||
                (strcasecmp(mediaType, "MEDIA") == 0 && strcasecmp(fileExtension, ".wav") != 0 &&
                strcasecmp(fileExtension, ".png") != 0 && strcasecmp(fileExtension, ".jpg") != 0)) {
                logError("[ERROR]: Extensión de archivo no válida o no coincide con mediaType.");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Buscar worker adecuado
            WorkerInfo *targetWorker = buscarWorker(fileName, manager);
            if (!targetWorker) {
                logError("[ERROR]: No se encontró un worker para el archivo especificado.");
                response.type = 0x10;
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Construir la respuesta para Fleck con la información del worker
            char workerInfo[DATA_MAX_SIZE] = {0};
            snprintf(workerInfo, sizeof(workerInfo), "%s&%d", targetWorker->ip, targetWorker->port);
            response.type = 0x10; // Tipo DISTORT
            strncpy(response.data, workerInfo, sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);

            // Enviar la información del worker a Fleck
            logInfo("[INFO]: Enviando información del worker a Fleck...");
            send_frame(client_fd, &response);
            break;

        case 0x20: // CHECK STATUS
            logInfo("Processing CHECK STATUS...");
            response.type = 0x20; // ACK del CHECK STATUS
            strncpy(response.data, "STATUS_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);
            break;

        case 0x11: //REASINGAR WORKER
            logInfo("[INFO]: Procesando solicitud de reasignación de Worker...");

            // Parsear datos: <mediaType>&<FileName>
            char mediaType0x11[10] = {0};
            char fileName0x11[256] = {0};
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType0x11, fileName0x11) != 2) {
                logError("[ERROR]: Formato inválido en la solicitud de reasignación.");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

             // Verificar la extensión del archivo coincide con el mediaType
            char *fileExtension0x11 = strrchr(fileName0x11, '.');
            if (!fileExtension0x11 ||
                (strcasecmp(mediaType0x11, "TEXT") == 0 && strcasecmp(fileExtension0x11, ".txt") != 0) ||
                (strcasecmp(mediaType0x11, "MEDIA") == 0 &&
                strcasecmp(fileExtension0x11, ".wav") != 0 &&
                strcasecmp(fileExtension0x11, ".png") != 0 &&
                strcasecmp(fileExtension0x11, ".jpg") != 0)) {
                logError("[ERROR]: Extensión de archivo no válida o no coincide con el mediaType.");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Buscar un Worker disponible
            WorkerInfo *targetWorkerCaiguda = buscarWorker(fileName0x11, manager);
            if (!targetWorkerCaiguda) {
                logError("[ERROR]: No hay Workers disponibles para el tipo especificado.");
                response.type = 0x11;
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Responder con la información del Worker
            char workerInfo0x11[DATA_MAX_SIZE] = {0};
            snprintf(workerInfo0x11, sizeof(workerInfo0x11), "%s&%d", targetWorkerCaiguda->ip, targetWorkerCaiguda->port);
            response.type = 0x11;
            strncpy(response.data, workerInfo0x11, sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);

            logInfo("[INFO]: Enviando información del Worker reasignado...");
            send_frame(client_fd, &response);
            break;

        default: // Comanda desconeguda
            logError("Comanda desconeguda rebuda.");
            response.type = 0xFF; // ERROR del comandament desconegut
            strncpy(response.data, "CMD_KO", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);
            break;
    }
}

void handleDisconnectFrame(const Frame *frame, int client_fd, WorkerManager *manager) {
    if (frame->data_length == 0) {
        logError("[ERROR]: Desconexión recibida sin datos válidos.");
        return;
    }

    logInfo("[INFO]: Procesando solicitud de desconexión...");
    if (frame->type == 0x07) {
        // Desconexión de un Worker
        if (strstr(frame->data, "TEXT") || strstr(frame->data, "MEDIA")) {
            if (logoutWorkerBySocket(client_fd, manager) == 0) {
                logInfo("[INFO]: Worker desconectado correctamente.");
                listarWorkers(manager);
            } else {
                logError("[ERROR]: No se encontró el Worker para desconectar.");
            }
        } else { // Desconexión de un Fleck
            logInfo("[INFO]: Fleck desconectado correctamente.");
        }
    }
}

void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) free(gothamConfig->ipFleck);
    if (gothamConfig->ipHarEni) free(gothamConfig->ipHarEni);
    free(gothamConfig);
}

void handleSigint(int sig) {
    (void)sig;
    logInfo("S'ha rebut SIGINT. Tancant el sistema...");

    if (workerManager) {
        pthread_mutex_lock(&workerManager->mutex);

        for (int i = 0; i < workerManager->workerCount; i++) {
            WorkerInfo *worker = &workerManager->workers[i];
            Frame closeFrame = {.type = 0xFF, .timestamp = (uint32_t)time(NULL)};
            closeFrame.checksum = calculate_checksum(closeFrame.data, closeFrame.data_length);
            send_frame(worker->socket_fd, &closeFrame);
            close(worker->socket_fd);
        }

        pthread_mutex_unlock(&workerManager->mutex);
        freeWorkerManager(workerManager);
    }

    logSuccess("Sistema tancat correctament.");
    exit(0);
}


// Funció principal del servidor Gotham
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(RED "Ús: ./gotham <fitxer de configuració>\n" RESET);
        return 1;
    }

    mostrarCaratula();
    signal(SIGINT, handleSigint);

    GothamConfig *config = malloc(sizeof(GothamConfig));
    if (!config) {
        logError("Error assignant memòria per a la configuració.");
        return 1;
    }

    readConfigFileGeneric(argv[1], config, CONFIG_GOTHAM);
    logInfo("Configuració carregada correctament.");

    int server_fd_fleck = startServer(config->ipFleck, config->portFleck);
    if (server_fd_fleck < 0) {
        logError("No s'ha pogut iniciar el servidor de Fleck.");
        free(config);
        return 1;
    }
    logSuccess("Servidor Fleck en funcionament.");

    int server_fd_enigma = startServer(config->ipHarEni, config->portHarEni);
    if (server_fd_enigma < 0) {
        logError("No s'ha pogut iniciar el servidor de Enigma/Harley.");
        close(server_fd_fleck);
        free(config);
        return 1;
    }
    logSuccess("Servidor Enigma/Harley en funcionament.");

    int server_fds[3] = {server_fd_fleck, server_fd_enigma, -1};
    pthread_t thread;
    pthread_create(&thread, NULL, esperarConexiones, server_fds);
    pthread_join(thread, NULL);

    close(server_fd_fleck);
    close(server_fd_enigma);
    free(config);

    logInfo("Servidor Gotham tancat correctament.");
    return 0;
}
