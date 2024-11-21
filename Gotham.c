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

void registrarWorker(const char *payload, WorkerManager *manager, int client_fd) {
    char type[10] = {0}, ip[16] = {0};
    int port = 0;

    // Usamos sscanf para procesar el payload
    if (sscanf(payload, "%9[^&]&%15[^&]&%d", type, ip, &port) != 3) {
        logError("[ERROR]: El payload recibido no tiene el formato esperado (TYPE&IP&PORT).");
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    // Ampliar la capacidad si es necesario
    if (manager->workerCount == manager->capacity) {
        manager->capacity *= 2;
        manager->workers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
    }

    // Registrar el nuevo worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    strncpy(worker->type, type, sizeof(worker->type) - 1);
    strncpy(worker->ip, ip, sizeof(worker->ip) - 1);
    worker->port = port;
    worker->socket_fd = client_fd;

    // Asignar como principal si es el primer worker de su tipo
    if (strcasecmp(type, "TEXT") == 0 && manager->mainTextWorker == NULL) {
        manager->mainTextWorker = worker;
    } else if (strcasecmp(type, "MEDIA") == 0 && manager->mainMediaWorker == NULL) {
        manager->mainMediaWorker = worker;
    }

    char *log_message;
    asprintf(&log_message, "[SUCCESS]: Worker registrado correctamente en Gotham.\n"
                           "Tipo: %s, IP: %s, Puerto: %d, Socket FD: %d\n",
             worker->type, worker->ip, worker->port, worker->socket_fd);
    logSuccess(log_message);
    free(log_message);

    pthread_mutex_unlock(&manager->mutex);
}


WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager) {
    if (!filename || !manager) {
        logError("Nombre de archivo o manager inválido.");
        return NULL;
    }

    char *extension = strrchr(filename, '.');
    if (!extension) {
        logError("No se pudo determinar la extensión del archivo.");
        return NULL;
    }

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *targetWorker = NULL;

    if (strcasecmp(extension, ".txt") == 0) {
        targetWorker = manager->mainTextWorker;
        logInfo("Asignando worker principal para archivos de texto.");
    } else if (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".png") == 0) {
        targetWorker = manager->mainMediaWorker;
        logInfo("Asignando worker principal para archivos multimedia.");
    } else {
        logError("Extensión no reconocida.");
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!targetWorker) {
        logError("No hay workers disponibles para el tipo de archivo.");
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

    // Estructura per preparar la resposta
    Frame response = {0};
    response.timestamp = (uint32_t)time(NULL);

    switch (frame->type) {
        case 0x01: // CONNECT
            logSuccess("Client connectat correctament.");
            response.type = 0x01; // ACK del CONNECT
            strncpy(response.data, "CONN_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);
            break;

        case 0x02: // REGISTER
            logInfo("[DEBUG]: Processant registre de worker...");
            registrarWorker(frame->data, manager, client_fd);
            response.type = 0x02; // ACK del REGISTER
            strncpy(response.data, "CON_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
            send_frame(client_fd, &response);
            break;

        case 0x10: // DISTORT
            logInfo("[DEBUG]: Processant DISTORT...");
            // Buscar el worker adequat segons el tipus de fitxer
            WorkerInfo *targetWorker = buscarWorker(frame->data, manager);
            if (!targetWorker) {
                logError("No s'ha trobat cap worker adequat per a aquest fitxer.");
                response.type = 0x10; // Error del DISTORT
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Connectar-se al worker
            int workerSocket = connect_to_server(targetWorker->ip, targetWorker->port);
            if (workerSocket < 0) {
                logError("Error connectant amb el worker.");
                response.type = 0x10; // Error del DISTORT
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
                break;
            }

            // Reenviar el frame al worker
            send_frame(workerSocket, frame);

            // Llegir la resposta del worker
            Frame workerResponse;
            if (receive_frame(workerSocket, &workerResponse) == 0) {
                send_frame(client_fd, &workerResponse);
            } else {
                logError("No s'ha rebut resposta del worker.");
                response.type = 0x10; // Error del DISTORT
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length);
                send_frame(client_fd, &response);
            }

            close(workerSocket); // Tancar la connexió amb el worker
            break;

        case 0x20: // CHECK STATUS
            logInfo("Processing CHECK STATUS...");
            response.type = 0x20; // ACK del CHECK STATUS
            strncpy(response.data, "STATUS_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);
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
