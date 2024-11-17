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
    WorkerInfo *workers;        // Llista dinàmica de workers
    int workerCount;            // Nombre de workers registrats
    int capacity;               // Capacitat actual de la llista
    pthread_mutex_t mutex;      // Mutex per sincronitzar l'accés als workers
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
int buscarWorker(const char *type, WorkerManager *manager, char *ip, int *port);
int logoutWorkerBySocket(int socket_fd, WorkerManager *manager);
void alliberarMemoria(GothamConfig *gothamConfig);

void processRegisterWorker(const char *payload, WorkerManager *manager, int client_fd) {
    char type[10] = {0};  // Variable per emmagatzemar el tipus de worker (Media o Text)
    char ip[16] = {0};    // Variable per emmagatzemar la IP del worker
    int port = 0;         // Variable per emmagatzemar el port del worker

    // Copiem el payload per processar-lo sense modificar l'original
    char *payloadCopy = strdup(payload);
    if (!payloadCopy) {
        logError("Error duplicant el payload.");
        send_frame(client_fd, "CON_KO", strlen("CON_KO"));
        return;
    }

    // Dividim el payload en parts utilitzant '&'
    char *token = strtok(payloadCopy, "&");
    if (token) strncpy(type, token, sizeof(type) - 1);

    token = strtok(NULL, "&");
    if (token) strncpy(ip, token, sizeof(ip) - 1);

    token = strtok(NULL, "&");
    if (token) port = atoi(token); // Convertim el port a un número enter

    free(payloadCopy);

    // Debug: Afegim logs per comprovar les variables
    printf("[DEBUG]: Tipus de worker: %s, IP: %s, Port: %d\n", type, ip, port);

    pthread_mutex_lock(&manager->mutex);

    // Ampliem la capacitat si cal
    if (manager->workerCount == manager->capacity) {
        manager->capacity *= 2;
        WorkerInfo *newWorkers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
        if (!newWorkers) {
            logError("Error ampliant la capacitat dels workers.");
            pthread_mutex_unlock(&manager->mutex);
            send_frame(client_fd, "CON_KO", strlen("CON_KO"));
            return;
        }
        manager->workers = newWorkers;
    }

    // Afegim el nou worker a la llista
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    strncpy(worker->type, type, sizeof(worker->type) - 1);
    strncpy(worker->ip, ip, sizeof(worker->ip) - 1);
    worker->port = port;

    pthread_mutex_unlock(&manager->mutex);

    // Responem al client amb èxit
    send_frame(client_fd, "CON_OK", strlen("CON_OK"));
    logSuccess("Worker registrat correctament a Gotham.");
    close(client_fd);
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
    if (buffer == NULL || frame == NULL) {
        // Error: Parámetros de entrada inválidos
        return -1;
    }

    memset(frame, 0, sizeof(Frame));

    // Hacemos una copia del buffer para no modificar el original
    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0'; // Aseguramos la terminación nula

    char *token = NULL;

    // Campo TYPE
    token = strtok(localBuffer, "|");
    if (token != NULL) {
        frame->type = (uint8_t)strtoul(token, NULL, 16);
    } else {
        return -1;
    }

    // Campo DATA_LENGTH
    token = strtok(NULL, "|");
    if (token != NULL) {
        unsigned int data_length = atoi(token);
        if (data_length > sizeof(frame->data)) {
            // Error: DATA_LENGTH fuera de rango
            return -1;
        }
        frame->data_length = (uint16_t)data_length;
    } else {
        // Error: Campo DATA_LENGTH faltante
        return -1;
    }

    // Campo TIMESTAMP
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->timestamp = (uint32_t)strtoul(token, NULL, 10);
    } else {
        // Error: Campo TIMESTAMP faltante
        return -1;
    }

    // Campo CHECKSUM
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->checksum = (uint16_t)strtoul(token, NULL, 16);
    } else {
        // Error: Campo CHECKSUM faltante
        return -1;
    }

    // Campo DATA
    token = strtok(NULL, "|");
    if (token != NULL) {
        // Aseguramos que no copiamos más datos de los que podemos manejar
        size_t dataToCopy = frame->data_length < sizeof(frame->data) - 1 ? frame->data_length : sizeof(frame->data) - 1;
        strncpy(frame->data, token, dataToCopy);
        frame->data[dataToCopy] = '\0'; // Aseguramos la terminación nula
    } else {
        // Error: Campo DATA faltante
        return -1;
    }

    return 0; // Éxito
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
    deserialize_frame(frame_buffer, &frame);
    printf("[DEBUG]: Frame deserialitzat a Gotham - type: %02x, data: %s\n", frame.type, frame.data);

    // Validar checksum solo sobre los datos del frame
    uint16_t calculated_checksum = calculate_checksum(frame.data, frame.data_length);
    if (calculated_checksum != frame.checksum) {
        logError("Trama amb checksum invàlid.");
        Frame response = {.type = 0xFF, .data_length = 0, .timestamp = time(NULL)};
        response.checksum = calculate_checksum(response.data, response.data_length);

        char buffer[FRAME_SIZE];
        serialize_frame(&response, buffer);
        write(client_fd, buffer, FRAME_SIZE);
        return;
    }
    printf("[DEBUG]: Tipus de frame rebut: %02x\n", frame.type);
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
            Frame response = {.type = 0x02, .data_length = 0, .timestamp = time(NULL)};
            strncpy(response.data, "CON_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length);

            char buffer[FRAME_SIZE];
            serialize_frame(&response, buffer);
            write(client_fd, buffer, FRAME_SIZE);
            break;

        case 0x10: // DISTORT
            logInfo("[DEBUG]: Processant DISTORT...");

            char workerIP[16];
            int workerPort;
            pthread_mutex_lock(&manager->mutex);
            int workerFound = buscarWorker("MEDIA", manager, workerIP, &workerPort);
            pthread_mutex_unlock(&manager->mutex);

            if (workerFound == 0) {
                logInfo("[INFO]: Worker trobat. Redirigint comanda...");

                int workerSocket = connect_to_server(workerIP, workerPort);
                if (workerSocket >= 0) {
                    write(workerSocket, frame_buffer, FRAME_SIZE);

                    char workerResponse[FRAME_SIZE];
                    if (read(workerSocket, workerResponse, FRAME_SIZE) > 0) {
                        logInfo("[DEBUG]: Resposta del worker:");
                        logInfo(workerResponse);

                        write(client_fd, workerResponse, FRAME_SIZE); // Envia la resposta a Fleck
                        close(workerSocket);
                    } else {
                        logError("[ERROR]: Worker no ha respost.");
                        send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
                    }
                } else {
                    logError("[ERROR]: No s'ha pogut connectar al worker.");
                    send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
                }
            } else {
                logError("[ERROR]: Cap worker disponible.");
                send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
            }
            break;

        case 0x20: // CHECK STATUS
            logSuccess("Processant CHECK STATUS...");
            Frame status_frame = {.type = 0x20, .data_length = 0, .timestamp = time(NULL)};
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
            error_frame.checksum = calculate_checksum(error_frame.data, error_frame.data_length);

            char error_buffer[FRAME_SIZE];
            serialize_frame(&error_frame, error_buffer);
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

    // Hacemos una copia del payload para no modificar la original
    char *payloadCopy = strdup(payload);
    if (!payloadCopy) {
        logError("Error duplicando el payload.");
        return;
    }

    // Utilizamos strtok para dividir el payload por '&'
    char *token = strtok(payloadCopy, "&");
    if (token) {
        strncpy(type, token, sizeof(type) - 1);
    } else {
        logError("Error al parsear el tipo de worker.");
        free(payloadCopy);
        return;
    }

    token = strtok(NULL, "&");
    if (token) {
        strncpy(ip, token, sizeof(ip) - 1);
    } else {
        logError("Error al parsear la IP del worker.");
        free(payloadCopy);
        return;
    }

    token = strtok(NULL, "&");
    if (token) {
        port = atoi(token);
    } else {
        logError("Error al parsear el puerto del worker.");
        free(payloadCopy);
        return;
    }

    free(payloadCopy);

    pthread_mutex_lock(&manager->mutex);

    // Comprobar si es necesario ampliar la capacidad
    if (manager->workerCount == manager->capacity) {
        manager->capacity *= 2;
        WorkerInfo *newWorkers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
        if (!newWorkers) {
            logError("Error ampliando la lista de workers.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->workers = newWorkers;
    }

    // Añadir el nuevo worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    strncpy(worker->type, type, sizeof(worker->type) - 1);
    strncpy(worker->ip, ip, sizeof(worker->ip) - 1);
    worker->port = port;
    worker->socket_fd = client_fd;

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
int buscarWorker(const char *type, WorkerManager *manager, char *ip, int *port) {
    for (int i = 0; i < manager->workerCount; i++) {
        if (strcasecmp(manager->workers[i].type, type) == 0) {
            strncpy(ip, manager->workers[i].ip, 16);
            *port = manager->workers[i].port;
            printF("[INFO]: Worker trobat: ");
            printF(ip);
            printF(":");
            char portDebug[10];
            snprintf(portDebug, 10, "%d\n", *port);
            printF(portDebug);
            return 0;
        }
    }
    printF("[DEBUG]: No hi ha workers disponibles del tipus sol·licitat.\n");
    return -1;
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

/***********************************************
* @Finalitat: Funció principal del programa Gotham.
************************************************/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(RED "Ús: ./gotham <fitxer de configuració>\n" RESET);
        return 1;
    }

    mostrarCaratula();

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
