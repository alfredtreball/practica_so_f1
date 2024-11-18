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
#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>  // Per a la comunicació de xarxa
#include <sys/socket.h> // Funcions per a sockets
#include <netinet/in.h> // Estructura sockaddr_in
#include <pthread.h>    // Per treballar amb fils
#include <ctype.h>
#include <signal.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"

// Representa un worker que es connecta a Gotham
typedef struct {
    char ip[16];    // Direcció IP del worker
    int port;       // Port del worker
    char type[10];  // Tipus de worker: "TEXT" o "MEDIA"
    int socket_fd; // Descriptor del socket associat al worker
} WorkerInfo;

// Administra una llista dinàmica de workers registrats
typedef struct {
    WorkerInfo *workers;        // Lista dinámica de workers registrados
    int workerCount;            // Número de workers registrados
    int capacity;               // Capacidad actual de la lista
    WorkerInfo *mainTextWorker; // Worker principal para TEXT
    WorkerInfo *mainMediaWorker; // Worker principal para MEDIA
    pthread_mutex_t mutex;      // Mutex para sincronizar el acceso
} WorkerManager;

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

// Funció per registrar els logs al terminal
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
// Declaració de funcions
void *gestionarConexion(void *arg);
void *esperarConexiones(void *arg);
void processCommandInGotham(const char *command, int client_fd, WorkerManager *manager);
void registrarWorker(const char *payload, WorkerManager *manager, int client_fd);

int logoutWorkerBySocket(int socket_fd, WorkerManager *manager);
void alliberarMemoria(GothamConfig *gothamConfig);
WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager);

void processRegisterWorker(const char *payload, WorkerManager *manager, int client_fd) {
    char type[10], ip[16];
    int port;
    sscanf(payload, "%[^&]&%[^&]&%d", type, ip, &port);

    pthread_mutex_lock(&manager->mutex);

    if (strcasecmp(type, "TEXT") == 0) {
        if (manager->mainTextWorker == NULL) {
            manager->mainTextWorker = malloc(sizeof(WorkerInfo));
            strncpy(manager->mainTextWorker->type, type, sizeof(manager->mainTextWorker->type) - 1);
            strncpy(manager->mainTextWorker->ip, ip, sizeof(manager->mainTextWorker->ip) - 1);
            manager->mainTextWorker->port = port;
            manager->mainTextWorker->socket_fd = client_fd;
            logSuccess("Worker principal asignado para TEXT.");
        }
    } else if (strcasecmp(type, "MEDIA") == 0) {
        if (manager->mainMediaWorker == NULL) {
            manager->mainMediaWorker = malloc(sizeof(WorkerInfo));
            strncpy(manager->mainMediaWorker->type, type, sizeof(manager->mainMediaWorker->type) - 1);
            strncpy(manager->mainMediaWorker->ip, ip, sizeof(manager->mainMediaWorker->ip) - 1);
            manager->mainMediaWorker->port = port;
            manager->mainMediaWorker->socket_fd = client_fd;
            logSuccess("Worker principal asignado para MEDIA.");
        }
    } else {
        logError("Tipo de worker desconocido. No se puede asignar.");
    }

    pthread_mutex_unlock(&manager->mutex);
}


WorkerManager *createWorkerManager() {
    WorkerManager *manager = malloc(sizeof(WorkerManager)); // Reserva memòria per al WorkerManager.
    if (!manager) {
        perror("Error inicialitzant WorkerManager");
        exit(EXIT_FAILURE); // Termina el programa si hi ha error de memòria.
    }

    manager->capacity = 10; // Capacitat inicial de 10 workers.
    manager->workerCount = 0; // Cap worker inicialment.
    manager->workers = malloc(manager->capacity * sizeof(WorkerInfo)); // Reserva memòria per als workers.
    if (!manager->workers) {
        perror("Error inicialitzant la llista de workers");
        free(manager); // Allibera la memòria si hi ha error amb la llista.
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&manager->mutex, NULL); // Inicialitza el mutex per gestionar accessos simultanis.
    return manager;
}

void freeWorkerManager(WorkerManager *manager) {
    if (manager) {
        free(manager->workers); // Allibera la memòria per a la llista de workers.
        pthread_mutex_destroy(&manager->mutex); // Destrueix el mutex.
        free(manager); // Allibera el `WorkerManager`.
    }
}

/***********************************************
* @Finalitat: Gestiona la connexió amb un client (Fleck, Harley o Enigma).
* Cada connexió es tracta en un fil separat. Rep trames i les processa.
* @Paràmetres:
*   in: arg = punter a la informació de connexió i al WorkerManager.
* @Retorn: NULL
************************************************/
void *gestionarConexion(void *arg) {
    // Obtenim el descriptor del socket i el punter a WorkerManager
    int client_fd = *(int *)arg;
    WorkerManager *manager = *((WorkerManager **)((int *)arg + 1));

    free(arg); // Alliberem només l'espai assignat per `arg`, no `manager`.

    char frame_buffer[FRAME_SIZE];
    while (read(client_fd, frame_buffer, FRAME_SIZE) > 0) {
        processCommandInGotham(frame_buffer, client_fd, manager);
    }

    // Si el client es desconnecta o hi ha un error, elimina el worker.
    pthread_mutex_lock(&manager->mutex);
    logoutWorkerBySocket(client_fd, manager);
    pthread_mutex_unlock(&manager->mutex);

    close(client_fd); // Tanca la connexió amb el client.
    return NULL;
}

/***********************************************
* @Finalitat: Accepta connexions entrants i crea un fil per a cada connexió.
* Utilitza `select` per gestionar múltiples sockets de forma concurrent.
* @Paràmetres:
*   in: arg = descriptors dels sockets del servidor.
* @Retorn: NULL
************************************************/
void *esperarConexiones(void *arg) { //Utilitzem select() per gestionar tres sockets diferents, connexions amb fleck, harley i enigma

    int *server_fds = (int *)arg; // Descriptors dels sockets (Fleck, Enigma, Harley)
    int server_fd_fleck = server_fds[0];
    int server_fd_enigma = server_fds[1];
    int server_fd_harley = server_fds[2];

    // Crear un conjunt de descriptors per `select`
    fd_set read_fds;
    int max_fd = server_fd_fleck > server_fd_enigma ? server_fd_fleck : server_fd_enigma;
    if (server_fd_harley > max_fd) max_fd = server_fd_harley;

    // Inicialitzar el WorkerManager
    WorkerManager *manager = createWorkerManager();

    while (1) {
        FD_ZERO(&read_fds); // Reinicialitza el conjunt de descriptors
        FD_SET(server_fd_fleck, &read_fds);
        FD_SET(server_fd_enigma, &read_fds);
        FD_SET(server_fd_harley, &read_fds);

        // Espera connexions o dades noves
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error en select");
            continue;
        }

       // Gestiona connexions de Fleck
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

        // Gestiona connexions de Enigma
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

        // Gestiona connexions de Harley
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

    // Alliberar recursos del WorkerManager
    freeWorkerManager(manager);
    return NULL;
}

// Ajusta el uso de checksum en Gotham.c
void serialize_frame(const Frame *frame, char *buffer) {
    memset(buffer, 0, FRAME_SIZE);
    snprintf(buffer, FRAME_SIZE, "%02x|%04x|%u|%04x|%s",
             frame->type, frame->data_length, frame->timestamp,
             frame->checksum, frame->data);
}

int deserialize_frame(const char *buffer, Frame *frame) {
    if (!buffer || !frame) return -1;

    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0';

    char *token = strtok(localBuffer, "|");
    if (token) frame->type = (uint8_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (token) frame->data_length = (uint16_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (token) frame->timestamp = (uint32_t)strtoul(token, NULL, 10);

    token = strtok(NULL, "|");
    if (token) frame->checksum = (uint16_t)strtol(token, NULL, 16);

    token = strtok(NULL, "|");
    if (token) {
        strncpy(frame->data, token, frame->data_length);
        frame->data[frame->data_length] = '\0'; // Terminación nula
    }

    return 0;
}


/***********************************************
* @Finalitat: Processa una comanda rebuda a Gotham.
* Identifica el tipus de comanda i executa les accions corresponents.
* @Paràmetres:
*   in: command = la comanda rebuda.
*   in: client_fd = descriptor del client que ha enviat la comanda.
*   in: manager = punter al WorkerManager.
* @Retorn: ----
************************************************/
void processCommandInGotham(const char *frame_buffer, int client_fd, WorkerManager *manager) {
    Frame frame;
    if (deserialize_frame(frame_buffer, &frame) != 0) {
        logError("Error deserialitzant el frame rebut.");
        return;
    }

    // Validar checksum solo sobre los datos del frame
    uint16_t calculated_checksum = calculate_checksum(frame.data, frame.data_length);

    
    if (calculated_checksum != frame.checksum) {
        logError("Trama amb checksum invàlid.");
        return;
    }

    switch (frame.type) {
        case 0x01: // CONNECT
            logSuccess("Client connectat correctament.");
            Frame ack_frame = {.type = 0x01, .timestamp = time(NULL)};
            strncpy(ack_frame.data, "CONN_OK", sizeof(ack_frame.data) - 1);
            ack_frame.data_length = strlen(ack_frame.data);
            ack_frame.checksum = calculate_checksum(ack_frame.data, ack_frame.data_length);

            char ack_buffer[FRAME_SIZE];
            serialize_frame(&ack_frame, ack_buffer);
            write(client_fd, ack_buffer, FRAME_SIZE);
            break;

        case 0x02: // REGISTER
            logInfo("[DEBUG]: Processant registre de worker...");
            registrarWorker(frame.data, manager, client_fd);
            // Construir el frame de resposta
            Frame response = {.type = 0x02, .data_length = 0, .timestamp = time(NULL)};
            strncpy(response.data, "CON_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);

            // Calcular checksum
            response.checksum = calculate_checksum(response.data, response.data_length);

            // Serialitzar el frame
            char buffer[FRAME_SIZE];
            serialize_frame(&response, buffer);

            // Enviar la resposta a Harley
            write(client_fd, buffer, FRAME_SIZE);
            break;

        case 0x10: // DISTORT
            logInfo("Processing DISTORT request...");

            // Buscar el worker basado en el archivo
            WorkerInfo *targetWorker = buscarWorker(frame.data, manager);
            if (targetWorker == NULL) {
                logError("No hay workers disponibles para este tipo de archivo.");
                send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
                return;
            }

            // Conectar al worker y redirigir la solicitud
            int workerSocket = connect_to_server(targetWorker->ip, targetWorker->port);
            if (workerSocket < 0) {
                logError("No se pudo conectar al worker.");
                send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
                return;
            }

            write(workerSocket, frame_buffer, FRAME_SIZE);

            // Leer la respuesta del worker
            char workerResponse[FRAME_SIZE];
            if (read(workerSocket, workerResponse, FRAME_SIZE) > 0) {
                write(client_fd, workerResponse, FRAME_SIZE);
            } else {
                logError("No se recibió respuesta del worker.");
                send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
            }

            close(workerSocket);
            break;

        case 0x20: // CHECK STATUS
            logInfo("Processing CHECK STATUS...");
            // Aquí enviarás una respuesta adecuada.
            Frame status_frame = {.type = 0x20, .timestamp = time(NULL)};
            strncpy(status_frame.data, "STATUS_OK", sizeof(status_frame.data) - 1);
            status_frame.data_length = strlen(status_frame.data);
            status_frame.checksum = calculate_checksum(status_frame.data, status_frame.data_length);

            char status_buffer[FRAME_SIZE];
            serialize_frame(&status_frame, status_buffer);
            write(client_fd, status_buffer, FRAME_SIZE);
            break;

        case 0x30: //DISTORT
            break;

        default: 
            logError("Comanda desconeguda rebuda.");
            Frame error_frame = {.type = 0xFF, .data_length = 0, .timestamp = time(NULL)};
            strncpy(error_frame.data, "CMD_KO", sizeof(error_frame.data) - 1);
            error_frame.data_length = strlen(error_frame.data);

            // Calcular checksum
            error_frame.checksum = calculate_checksum(error_frame.data, error_frame.data_length);

            // Serialitzar el frame
            char error_buffer[FRAME_SIZE];
            serialize_frame(&error_frame, error_buffer);

            // Enviar la resposta d'error
            write(client_fd, error_buffer, FRAME_SIZE);
            break;
    }
}

/***********************************************
* @Finalitat: Registra un nou worker a Gotham.
* @Paràmetres:
*   in: payload = informació del worker (IP, port, tipus).
*   in: manager = punter al WorkerManager.
*   in: client_fd = descriptor del client associat.
* @Retorn: ----
************************************************/
void registrarWorker(const char *payload, WorkerManager *manager, int client_fd) {
    char type[10] = {0}, ip[16] = {0};
    int port = 0;

    // Parsear el payload
    sscanf(payload, "%[^&]&%[^&]&%d", type, ip, &port);

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

    pthread_mutex_unlock(&manager->mutex);

    logSuccess("Worker registrado correctamente en Gotham.");
}

/***********************************************
* @Finalitat: Busca un worker disponible segons el tipus.
* @Paràmetres:
*   in: type = tipus de worker (TEXT o MEDIA).
*   in: manager = punter al WorkerManager.
*   out: ip = IP del worker trobat.
*   out: port = port del worker trobat.
* @Retorn: 0 si trobat, -1 si no trobat.
************************************************/
WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager) {
    if (!filename || !manager) {
        logError("Nombre de archivo o manager inválido.");
        return NULL;
    }

    // Extraer la extensión del archivo
    char *extension = strrchr(filename, '.');
    if (!extension) {
        logError("No se pudo determinar la extensión del archivo.");
        return NULL;
    }

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *targetWorker = NULL;

    // Determinar el worker principal basado en la extensión
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

/***********************************************
* @Finalitat: Elimina un worker pel seu socket_fd.
************************************************/
int logoutWorkerBySocket(int socket_fd, WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->workers[i].socket_fd == socket_fd) {
            manager->workers[i] = manager->workers[--manager->workerCount];
            pthread_mutex_unlock(&manager->mutex);
            return 0; // Worker eliminat correctament
        }
    }
    pthread_mutex_unlock(&manager->mutex);
    return -1; // Worker no trobat
}

/***********************************************
* @Finalitat: Allibera la memòria dinàmica utilitzada per GothamConfig.
* @Paràmetres:
*   in: gothamConfig = estructura de configuració.
* @Retorn: ----
************************************************/
void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) free(gothamConfig->ipFleck);
    if (gothamConfig->ipHarEni) free(gothamConfig->ipHarEni);
    free(gothamConfig);
}

WorkerManager *workerManager = NULL;

void handleSigint(int sig) {
    (void)sig;
    // Log de cierre del sistema
    logInfo("S'ha rebut SIGINT. Tancant el sistema...");

    // Notificar a Fleck
    logInfo("Notificant a Fleck...");
    // Enviar un frame de tipo especial para indicar cierre
    Frame closeFrame;
    memset(&closeFrame, 0, sizeof(Frame));
    closeFrame.type = 0xFF; // Tipo de frame para notificar cierre
    closeFrame.data_length = 0; // No necesita datos adicionales
    closeFrame.timestamp = (uint32_t)time(NULL);
    closeFrame.checksum = calculate_checksum(closeFrame.data, closeFrame.data_length);

    char buffer[FRAME_SIZE];
    serialize_frame(&closeFrame, buffer);
    
    // Iterar sobre los workers registrados y enviarles la notificación de cierre
    pthread_mutex_lock(&workerManager->mutex);
    for (int i = 0; i < workerManager->workerCount; i++) {
        WorkerInfo *worker = &workerManager->workers[i];
        write(worker->socket_fd, buffer, FRAME_SIZE);
        logInfo("Notificat a un worker.");
    }
    pthread_mutex_unlock(&workerManager->mutex);

    // Cerrar conexiones con Fleck y los sockets abiertos
    logInfo("Tancant els sockets dels workers i el servidor principal.");
    for (int i = 0; i < workerManager->workerCount; i++) {
        close(workerManager->workers[i].socket_fd);
    }
    freeWorkerManager(workerManager);

    // Salir del programa
    logSuccess("Sistema tancat correctament.");
    exit(0);
}

/***********************************************
* @Finalitat: Funció principal del programa Gotham.
************************************************/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(RED "Ús: ./gotham <fitxer de configuració>\n" RESET);
        return 1;
    }

    mostrarCaratula();

    // Configurar el manejador de señales
    signal(SIGINT, handleSigint);

    // Carrega la configuració
    GothamConfig *config = malloc(sizeof(GothamConfig));
    if (!config) {
        logError("Error assignant memòria per a la configuració.");
        return 1;
    }

    readConfigFileGeneric(argv[1], config, CONFIG_GOTHAM);
    logInfo("Configuració carregada correctament.");

    // Inicia els sockets
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
