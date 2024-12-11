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
#include "Logging/Logging.h"

volatile sig_atomic_t stop_server = 0; // Bandera para indicar el cierre

typedef struct {
    int server_fd_fleck;
    int server_fd_enigma;
} ServerFds;

static ServerFds *global_server_fds = NULL;
static GothamConfig *global_config = NULL;

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

typedef struct {
    char username[64]; // Nombre de usuario del cliente
    char ip[16];       // Dirección IP del cliente
    int socket_fd;     // Descriptor del socket del cliente
} ClientInfo;

typedef struct {
    ClientInfo *clients;     // Lista dinámica de clientes
    int clientCount;         // Número de clientes conectados
    int capacity;            // Capacidad actual de la lista
    pthread_mutex_t mutex;   // Mutex para sincronización
} ClientManager;

typedef struct {
    int client_fd;
    WorkerManager *workerManager;
    ClientManager *clientManager;
} ConnectionArgs;

WorkerManager *workerManager = NULL;
ClientManager *clientManager = NULL;

// Declaració de funcions
void mostrarCaratula();
WorkerManager *createWorkerManager();
void freeWorkerManager(WorkerManager *manager);
void *gestionarConexion(void *arg);
void processCommandInGotham(const Frame *frame, int client_fd, WorkerManager *manager, ClientManager *clientManager);
void registrarWorker(const char *payload, WorkerManager *manager, int client_fd);
void asignarNuevoWorkerPrincipal(WorkerInfo *worker);
int logoutWorkerBySocket(int socket_fd, WorkerManager *manager);
WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager);
void handleDisconnectFrame(const Frame *frame, int client_fd, WorkerManager *manager, ClientManager *clientManager);
ClientManager *createClientManager();
void freeClientManager(ClientManager *manager);
void addClient(ClientManager *manager, const char *username, const char *ip, int socket_fd);
void removeClientBySocket(ClientManager *manager, int socket_fd);
void listClients(ClientManager *manager);
void handleSigint(int sig);
void alliberarMemoria(GothamConfig *gothamConfig);

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

// Crea un nou gestor de clients
ClientManager *createClientManager() {
    ClientManager *manager = malloc(sizeof(ClientManager));
    if (!manager) {
        perror("Error al inicializar ClientManager");
        exit(EXIT_FAILURE);
    }

    manager->capacity = 10;
    manager->clientCount = 0;
    manager->clients = calloc(manager->capacity, sizeof(ClientInfo));
    if (!manager->clients) {
        perror("Error al inicializar la lista de clientes");
        free(manager);
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        perror("Error al inicializar el mutex de ClientManager");
        free(manager->clients);
        free(manager);
        exit(EXIT_FAILURE);
    }

    logInfo("[DEBUG]: ClientManager creado con éxito.");
    return manager;
}

void freeClientManager(ClientManager *manager) {
    if (manager) {
        if (manager->clients) {
            free(manager->clients); // Liberar la lista dinámica de clientes
        }
        pthread_mutex_destroy(&manager->mutex); // Destruir el mutex
        free(manager); // Liberar la estructura principal
        logInfo("[DEBUG]: ClientManager liberado correctamente.");
    }
}

// Añade un cliente a la lista dinámica
void addClient(ClientManager *manager, const char *username, const char *ip, int socket_fd) {
    pthread_mutex_lock(&manager->mutex);

    // Verificar si el cliente ya está registrado
    for (int i = 0; i < manager->clientCount; i++) {
        if (strcmp(manager->clients[i].username, username) == 0) {
            strncpy(manager->clients[i].ip, ip, sizeof(manager->clients[i].ip) - 1);
            manager->clients[i].socket_fd = socket_fd;
            logInfo("[INFO]: Cliente existente actualizado.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
    }

    // Ampliar la capacidad si es necesario
    if (manager->clientCount == manager->capacity) {
        manager->capacity *= 2;
        ClientInfo *new_clients = realloc(manager->clients, manager->capacity * sizeof(ClientInfo));
        if (!new_clients) {
            logError("[ERROR]: Error al ampliar la capacidad de clientes.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->clients = new_clients;
    }

    // Añadir un nuevo cliente
    ClientInfo *client = &manager->clients[manager->clientCount++];
    strncpy(client->username, username, sizeof(client->username) - 1);
    strncpy(client->ip, ip, sizeof(client->ip) - 1);
    client->socket_fd = socket_fd;

    logInfo("[INFO]: Cliente añadido exitosamente.");
    pthread_mutex_unlock(&manager->mutex);
}


// Elimina un cliente de la lista por su socket
void removeClientBySocket(ClientManager *manager, int socket_fd) {
    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < manager->clientCount; i++) {
        if (manager->clients[i].socket_fd == socket_fd) {
            // Mueve el último cliente al lugar del eliminado
            if (i != manager->clientCount - 1) {
                manager->clients[i] = manager->clients[manager->clientCount - 1];
            }
            manager->clientCount--;
            logInfo("[INFO]: Cliente eliminado de la lista.");
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}

// Lista los clientes conectados
void listClients(ClientManager *manager) {
    pthread_mutex_lock(&manager->mutex);

    if (manager->clientCount == 0) {
        logInfo("No hay clientes conectados.");
    } else {
        logInfo("Lista de clientes conectados:");
        for (int i = 0; i < manager->clientCount; i++) {
            ClientInfo *client = &manager->clients[i];
            char log_message[256];
            snprintf(log_message, sizeof(log_message), "Cliente %d: Usuario: %s, IP: %s, Socket: %d",
                     i + 1, client->username, client->ip, client->socket_fd);
            logInfo(log_message);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}

// Crea un nou gestor de workers
WorkerManager *createWorkerManager() {
    WorkerManager *manager = malloc(sizeof(WorkerManager));
    if (!manager) {
        perror("Error al inicializar WorkerManager");
        exit(EXIT_FAILURE);
    }

    manager->capacity = 10;
    manager->workerCount = 0;
    manager->workers = calloc(manager->capacity, sizeof(WorkerInfo));
    if (!manager->workers) {
        perror("Error al inicializar la lista de workers");
        free(manager);
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        perror("Error al inicializar el mutex de WorkerManager");
        free(manager->workers);
        free(manager);
        exit(EXIT_FAILURE);
    }

    manager->mainTextWorker = NULL;
    manager->mainMediaWorker = NULL;

    logInfo("[DEBUG]: WorkerManager creado con éxito.");
    return manager;
}

void listarWorkers(WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex); // Asegura acceso seguro a la lista dinámica

    logInfo("[INFO]: Lista de workers registrados: ");
    for (int i = 0; i < manager->workerCount; i++) {
        WorkerInfo *worker = &manager->workers[i];
        char *log_message;
        asprintf(&log_message, "\nWorker %d: Tipo: %s, IP: %s, Puerto: %d, Socket FD: %d\n",
                    i + 1, worker->type, worker->ip, worker->port, worker->socket_fd);
        printF(log_message);
        free(log_message);
    }

    pthread_mutex_unlock(&manager->mutex);
}

void registrarWorker(const char *payload, WorkerManager *manager, int client_fd) {
    if (!manager || !payload) {
        logError("[ERROR]: Parámetros inválidos en registrarWorker.");
        return;
    }

    logInfo("[INFO]: Iniciando el registro del worker...\n");

    char type[10] = {0}, ip[16] = {0};
    int port = 0;

    if (sscanf(payload, "%9[^&]&%15[^&]&%d", type, ip, &port) != 3) {
        logError("[ERROR]: Payload inválido (esperado TYPE&IP&PORT).");
        Frame response = {0};
        response.type = 0x02;
        strncpy(response.data, "CON_KO", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.checksum = calculate_checksum(response.data, response.data_length, 1);
        send_frame(client_fd, &response);
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    // Ampliar capacidad si es necesario
    if (manager->workerCount == manager->capacity) {
        logInfo("[DEBUG]: Ampliando capacidad del WorkerManager...");
        manager->capacity *= 2;
        WorkerInfo *temp = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
        if (!temp) {
            logError("[ERROR]: Fallo al ampliar la capacidad de WorkerManager.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->workers = temp;
    }

    // Registrar nuevo worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    memset(worker, 0, sizeof(WorkerInfo));
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

    pthread_mutex_unlock(&manager->mutex);

    logSuccess("[SUCCESS]: Worker registrado correctamente.\n");

    // Enviar ACK
    Frame response = {0};
    response.type = 0x02;
    response.data_length = 0;
    response.checksum = calculate_checksum(response.data, response.data_length, 1);
    send_frame(client_fd, &response);
}

WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager) {
    if (!filename || !manager) {
        logError("[ERROR]: Nombre de archivo o manager inválido.\n");
        return NULL;
    }

    char *extension = strrchr(filename, '.');
    if (!extension) {
        logError("[ERROR]: No se pudo determinar la extensión del archivo.\n");
        return NULL;
    }

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *targetWorker = NULL;

    if (strcasecmp(extension, ".txt") == 0) {
        if (manager->mainTextWorker && manager->mainTextWorker->ip[0] != '\0') {
            targetWorker = manager->mainTextWorker;
            logInfo("[INFO]: Worker principal asignado para archivos de texto.\n");
        } else {
            logError("[ERROR]: No hay un Worker principal asignado para archivos de texto.\n");
        }
    } else if (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".jpg") == 0) {
        if (manager->mainMediaWorker && manager->mainMediaWorker->ip[0] != '\0') {
            targetWorker = manager->mainMediaWorker;
            logInfo("[INFO]: Worker principal asignado para archivos multimedia.\n");
        } else {
            logError("[ERROR]: No hay un Worker principal asignado para archivos multimedia.\n");
        }
    } else {
        logError("[ERROR]: Extensión no reconocida.\n");
    }

    if (!targetWorker) {
        logError("[ERROR]: No hay workers disponibles para el tipo de archivo.\n");
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }

    // Validar los campos del Worker antes de usarlo
    if (targetWorker->ip[0] == '\0' || targetWorker->port <= 0) {
        logError("[ERROR]: Worker tiene datos inválidos.\n");
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }

    if (targetWorker) {
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "[INFO]: Worker encontrado -> IP: %s, Puerto: %d\n",
                 targetWorker->ip, targetWorker->port);
        printF(log_message);
    } else {
        logError("[ERROR]: No hay workers disponibles para el tipo de archivo.\n");
    }

    pthread_mutex_unlock(&manager->mutex);
    return targetWorker;
}

void asignarNuevoWorkerPrincipal(WorkerInfo *worker) {
    if (!worker) {
        logError("[ERROR]: Worker no válido para asignar como principal.");
        return;
    }

    Frame frame = {0};
    frame.type = 0x08;  // Tipo de trama para asignar principal
    frame.data_length = 0;  // Sin datos adicionales
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Asignando nuevo Worker principal...");
    if (send_frame(worker->socket_fd, &frame) == 0) {
        logSuccess("[SUCCESS]: Trama 0x08 enviada correctamente al Worker principal.");
    } else {
        logError("[ERROR]: Error enviando la trama 0x08 al Worker principal.");
    }
}

int logoutWorkerBySocket(int socket_fd, WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex);

    int found = 0;
    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->workers[i].socket_fd == socket_fd) {
            found = 1;

            // Si es principal, reasignar
            if (manager->mainTextWorker == &manager->workers[i]) {
                manager->mainTextWorker = NULL;
            }
            if (manager->mainMediaWorker == &manager->workers[i]) {
                manager->mainMediaWorker = NULL;
            }

            // Reemplaza el worker eliminado por el último de la lista
            if (i != manager->workerCount - 1) {
                manager->workers[i] = manager->workers[manager->workerCount - 1];
            }
            manager->workerCount--;
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!found) {
        logWarning("[WARNING]: Worker no encontrado con el socket proporcionado.");
        return -1;
    }

    logInfo("[INFO]: Worker eliminado correctamente.");
    return 0;
}

void reasignarWorkersPrincipales(WorkerManager *manager) {
    if (!manager || !manager->workers) {
        logError("[ERROR]: WorkerManager o la lista de Workers es NULL.");
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->mainTextWorker == NULL && strcasecmp(manager->workers[i].type, "TEXT") == 0) {
            manager->mainTextWorker = &manager->workers[i];
            logInfo("[INFO]: Nuevo Worker principal de texto asignado.");
            asignarNuevoWorkerPrincipal(manager->mainTextWorker);
        }
        if (manager->mainMediaWorker == NULL && strcasecmp(manager->workers[i].type, "MEDIA") == 0) {
            manager->mainMediaWorker = &manager->workers[i];
            logInfo("[INFO]: Nuevo Worker principal de multimedia asignado.");
            asignarNuevoWorkerPrincipal(manager->mainMediaWorker);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}


void handleWorkerFailure(const char *mediaType, WorkerManager *manager, int client_fd) {
    if (!mediaType || !manager) {
        logError("[ERROR]: Parámetros inválidos en handleWorkerFailure.\n");
        return;
    }

    logError("[ERROR]: Worker falló durante la operación DISTORT.\n");

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *newWorker = NULL;
    for (int i = 0; i < manager->workerCount; i++) {
        WorkerInfo *worker = &manager->workers[i];
        if (strcasecmp(worker->type, mediaType) == 0) {
            newWorker = worker;
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    Frame response = {0};
    response.type = 0x11; // Tipo de trama para la reasignación
    if (newWorker) {
        snprintf(response.data, sizeof(response.data), "%s&%d", newWorker->ip, newWorker->port);
        response.data_length = strlen(response.data);
    } else {
        logError("[ERROR]: No hay Workers disponibles para reasignar.");
        strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
    }
    response.checksum = calculate_checksum(response.data, response.data_length, 0);
    send_frame(client_fd, &response);
}

// Allibera la memòria utilitzada pel WorkerManager
void freeWorkerManager(WorkerManager *manager) {
    if (manager) {
        if (manager->workers) {
            free(manager->workers); // Liberar la lista dinámica de workers
        }
        pthread_mutex_destroy(&manager->mutex); // Destruir el mutex
        free(manager); // Liberar la estructura principal
        logInfo("[DEBUG]: WorkerManager liberado correctamente.");
    }
}

// Gestiona la connexió amb un client
// Gestiona la connexió amb un client
void *gestionarConexion(void *arg) {
    ConnectionArgs *args = (ConnectionArgs *)arg;
    int client_fd = args->client_fd;
    WorkerManager *workerManager = args->workerManager;
    ClientManager *clientManager = args->clientManager;

    // Liberamos args inmediatamente después de guardar los datos relevantes
    free(args);
    args = NULL; // Asegurarnos de que no sea usado accidentalmente

    if (!workerManager || !clientManager) {
        logError("[ERROR]: WorkerManager o ClientManager NULL en gestionarConexion.");
        close(client_fd);
        return NULL;
    }

    Frame frame;
    int disconnectHandled = 0;

    // Manejo de frames inicial
    if (receive_frame(client_fd, &frame) != 0) {
        logError("[ERROR]: Error al recibir el primer frame del cliente.");
        close(client_fd);
        return NULL;
    }

    processCommandInGotham(&frame, client_fd, workerManager, clientManager);

    // Bucle principal para manejar los frames del cliente
    while (!stop_server) { // Verifica la bandera global para el cierre del servidor
        if (receive_frame(client_fd, &frame) != 0) {
            logInfo("[INFO]: El cliente cerró la conexión o hubo un error.");
            break;
        }

        if (frame.type == 0x07) { // Trama de desconexión explícita del cliente
            logInfo("[DEBUG]: Trama de desconexión recibida.");
            handleDisconnectFrame(&frame, client_fd, workerManager, clientManager);
            disconnectHandled = 1;
            break;
        }

        // Procesar el frame recibido
        processCommandInGotham(&frame, client_fd, workerManager, clientManager);
    }

    // Si el servidor está en proceso de cierre, gestionar recursos del cliente
    if (stop_server && !disconnectHandled) {
        logInfo("[INFO]: Finalizando conexión por cierre del servidor.");
        removeClientBySocket(clientManager, client_fd);
    } else if (!disconnectHandled) {
        logInfo("[INFO]: El cliente cerró la conexión sin trama de desconexión.");
        removeClientBySocket(clientManager, client_fd);
    }

    close(client_fd);
    logInfo("[INFO]: Conexión cerrada y recursos liberados.");
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
void processCommandInGotham(const Frame *frame, int client_fd, WorkerManager *manager, ClientManager *clientManager) {
    if (!frame) {
        logError("El frame rebut és NULL.");
        return;
    }

    // Validar el checksum del frame rebut
    uint16_t calculated_checksum;

    if (frame->type == 0x07) {
        calculated_checksum = calculate_checksum(frame->data, frame->data_length, 1);
    } else {
        calculated_checksum = calculate_checksum(frame->data, frame->data_length, 0);
    }
   
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
            logInfo("[INFO]: Procesando conexión de Fleck...\n");

            // Parsear los datos de la conexión (nombre de usuario, IP y puerto)
            char username[64] = {0}, ip[16] = {0};
            int port = 0;

            if (sscanf(frame->data, "%63[^&]&%15[^&]&%d", username, ip, &port) != 3) {
                logError("[ERROR]: Formato de datos de conexión inválido.");
                response.type = 0x01;
                strncpy(response.data, "CONN_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                send_frame(client_fd, &response);
                break;
            }

            // Registrar al cliente en el ClientManager
            addClient(clientManager, username, ip, client_fd);

            char log_message[256];
            snprintf(log_message, sizeof(log_message),
                    "[DEBUG]: Cliente conectado: Usuario: %s, IP: %s, Puerto: %d, Socket: %d\n",
                    username, ip, port, client_fd);
            logInfo(log_message);

            // Enviar respuesta de éxito
            response.type = 0x01;
            strncpy(response.data, "CONN_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            send_frame(client_fd, &response);
            break;

        case 0x02: // REGISTER
            logInfo("[DEBUG]: Processant registre de worker...\n");
            registrarWorker(frame->data, manager, client_fd);
            listarWorkers(manager);
            break;

        case 0x07:
            logInfo("[INFO]: Trama de desconexión recibida.");

            // Llamamos a la función centralizada para manejar desconexión
            handleDisconnectFrame(frame, client_fd, manager, clientManager);

            // Confirmar desconexión al cliente/worker
            response.type = 0x07;
            strncpy(response.data, "DISCONNECT_OK", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            send_frame(client_fd, &response);

            close(client_fd);
            break;

        case 0x10: // DISTORT
            logInfo("[INFO]: Procesando comando DISTORT...\n");

            // Parsear el payload recibido: mediaType y fileName
            char mediaType[10], fileName[256];
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType, fileName) != 2) {
                logError("[ERROR]: Formato de datos incorrecto en comando DISTORT.");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                send_frame(client_fd, &response);
                break;
            }

            // Verificar si el fileName tiene la extensión correcta según el mediaType
            char *fileExtension = strrchr(fileName, '.');
            if (!fileExtension || 
                (strcasecmp(mediaType, "TEXT") == 0 && strcasecmp(fileExtension, ".txt") != 0) ||
                (strcasecmp(mediaType, "MEDIA") == 0 && strcasecmp(fileExtension, ".wav") != 0 &&
                strcasecmp(fileExtension, ".png") != 0 && strcasecmp(fileExtension, ".jpg") != 0)) {
                logError("[ERROR]: Extensión de archivo no válida o no coincide con mediaType.\n");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                send_frame(client_fd, &response);
                break;
            }

            // Buscar worker adecuado
            WorkerInfo *targetWorker = buscarWorker(fileName, manager);
            if (targetWorker) {
                logInfo("[DEBUG]: Worker encontrado:");
                char debug_message[256];
                snprintf(debug_message, sizeof(debug_message), "IP: %s, Port: %d, FD: %d", targetWorker->ip, targetWorker->port, targetWorker->socket_fd);
                logInfo(debug_message);
            }
            if (!targetWorker) {
                logError("[ERROR]: No se encontró un worker para el archivo especificado.\n");
                handleWorkerFailure(mediaType, manager, client_fd);
                break;
            }


            // Validar worker antes de usarlo
            if (targetWorker == NULL || targetWorker->ip[0] == '\0') {
                logError("[ERROR]: Worker inválido después de buscarWorker.\n");
                // Intentar reasignación si es posible
                handleWorkerFailure(mediaType, manager, client_fd);
                break; // Terminar después de manejar la reasignación
            }

            // Preparar respuesta con la información del Worker
            snprintf(response.data, sizeof(response.data), "%s&%d", targetWorker->ip, targetWorker->port);
            response.type = 0x10;
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            send_frame(client_fd, &response);
            break;

        case 0x11: //REASINGAR WORKER
            logInfo("[INFO]: Procesando solicitud de reasignación de Worker...\n");

            // Parsear datos: <mediaType>&<FileName>
            char mediaType0x11[10] = {0};
            char fileName0x11[256] = {0};
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType0x11, fileName0x11) != 2) {
                logError("[ERROR]: Formato inválido en la solicitud de reasignación.\n");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
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
                logError("[ERROR]: Extensión de archivo no válida o no coincide con el mediaType.\n");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                send_frame(client_fd, &response);
                break;
            }

            // Buscar un Worker disponible
            WorkerInfo *targetWorkerCaiguda = buscarWorker(fileName0x11, manager);
            if (!targetWorkerCaiguda) {
                logError("[ERROR]: No hay Workers disponibles para el tipo especificado.\n");
                response.type = 0x11;
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                send_frame(client_fd, &response);
                break;
            }

            // Responder con la información del Worker
            char workerInfo0x11[DATA_MAX_SIZE] = {0};
            snprintf(workerInfo0x11, sizeof(workerInfo0x11), "%s&%d", targetWorkerCaiguda->ip, targetWorkerCaiguda->port);
            response.type = 0x11;
            strncpy(response.data, workerInfo0x11, sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);

            logInfo("[INFO]: Enviando información del Worker reasignado...\n");
            send_frame(client_fd, &response);
            break;

        default: // Comanda desconeguda
            logError("Comanda desconeguda rebuda.\n");
            response.type = 0xFF; // ERROR del comandament desconegut
            strncpy(response.data, "CMD_KO", sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            send_frame(client_fd, &response);
            break;
    }
}

void handleDisconnectFrame(const Frame *frame, int client_fd, WorkerManager *manager, ClientManager *clientManager) {
    if (!frame || !manager || !clientManager) {
        logError("[ERROR]: Parámetros inválidos en handleDisconnectFrame.");
        return;
    }

    int isWorker = 0;
    int isClient = 0;

    // Verificar si es un Worker
    pthread_mutex_lock(&manager->mutex);
    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->workers[i].socket_fd == client_fd) {
            isWorker = 1;
            break;
        }
    }
    pthread_mutex_unlock(&manager->mutex);

    // Verificar si es un Cliente
    pthread_mutex_lock(&clientManager->mutex);
    if (!isWorker) {
        for (int i = 0; i < clientManager->clientCount; i++) {
            if (clientManager->clients[i].socket_fd == client_fd) {
                isClient = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clientManager->mutex);

    if (isWorker) {
        logInfo("[INFO]: Identificado como Worker. Procesando desconexión...");
        if (logoutWorkerBySocket(client_fd, manager) == 0) {
            logInfo("[INFO]: Worker desconectado correctamente.");
            reasignarWorkersPrincipales(manager);
        }
    } else if (isClient) {
        logInfo("[INFO]: Identificado como Cliente. Procesando desconexión...");
        removeClientBySocket(clientManager, client_fd);
    } else {
        logWarning("[WARNING]: Socket no corresponde a un Worker ni a un Cliente.");
    }

    // Mostrar listas actualizadas
    logInfo("[INFO]: Lista actualizada de Workers:");
    listarWorkers(manager);

    logInfo("[INFO]: Lista actualizada de Clientes:");
    listClients(clientManager);
}


void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) free(gothamConfig->ipFleck);
    if (gothamConfig->ipHarEni) free(gothamConfig->ipHarEni);
    free(gothamConfig);
}

void handleSigint(int sig) {
    (void)sig;
    logInfo("S'ha rebut SIGINT. Tancant el sistema...\n");
    stop_server = 1;

    // Liberar los recursos de WorkerManager
    if (workerManager) {
        pthread_mutex_lock(&workerManager->mutex);
        for (int i = 0; i < workerManager->workerCount; i++) {
            WorkerInfo *worker = &workerManager->workers[i];
            Frame closeFrame = {.type = 0xFF, .timestamp = (uint32_t)time(NULL)};
            closeFrame.checksum = calculate_checksum(closeFrame.data, closeFrame.data_length, 1);
            send_frame(worker->socket_fd, &closeFrame); // Informar al worker
            close(worker->socket_fd); // Cerrar el socket
        }
        pthread_mutex_unlock(&workerManager->mutex);
        freeWorkerManager(workerManager); // Liberar el WorkerManager
        workerManager = NULL;
    }

    // Liberar los recursos de ClientManager
    if (clientManager) {
        pthread_mutex_lock(&clientManager->mutex);
        for (int i = 0; i < clientManager->clientCount; i++) {
            close(clientManager->clients[i].socket_fd); // Cerrar el socket del cliente
        }
        pthread_mutex_unlock(&clientManager->mutex);
        freeClientManager(clientManager); // Liberar el ClientManager
        clientManager = NULL;
    }

    // Cerrar los descriptores de archivo globales
    if (global_server_fds) {
        close(global_server_fds->server_fd_fleck);
        close(global_server_fds->server_fd_enigma);
    }

    // Liberar la configuración global
    if (global_config) {
        free(global_config);
        global_config = NULL;
    }

    logSuccess("Sistema tancat correctament.\n");
    exit(0);
}

// Funció principal del servidor Gotham
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: ./gotham <archivo de configuración>\n");
        return EXIT_FAILURE;
    }

    mostrarCaratula();

    GothamConfig *config = malloc(sizeof(GothamConfig));
    global_config = config; // Asignar el puntero global

    WorkerManager *manager = NULL; // Declarar manager aquí
    ClientManager *clientManager = NULL; // Declarar clientManager aquí

    if (!config) {
        logError("Error asignando memoria para la configuración.\n");
        return EXIT_FAILURE;
    }

    readConfigFileGeneric(argv[1], config, CONFIG_GOTHAM);
    logInfo("Configuración cargada correctamente.\n");

    ServerFds server_fds;
    global_server_fds = &server_fds; // Asignar el puntero global

    signal(SIGINT, handleSigint);

    server_fds.server_fd_fleck = startServer(config->ipFleck, config->portFleck);
    server_fds.server_fd_enigma = startServer(config->ipHarEni, config->portHarEni);

    if (server_fds.server_fd_fleck < 0 || server_fds.server_fd_enigma < 0) {
        logError("Error al iniciar los servidores.\n");
        if (server_fds.server_fd_fleck >= 0) close(server_fds.server_fd_fleck);
        if (server_fds.server_fd_enigma >= 0) close(server_fds.server_fd_enigma);

        if (clientManager) {
            freeClientManager(clientManager);
            clientManager = NULL;
        }

        if (manager) {
            freeWorkerManager(manager);
            manager = NULL;
        }

        free(config);
        global_config = NULL;
        return EXIT_FAILURE;
    }

    logSuccess("Servidores iniciados correctamente.\n");

    // Crear el WorkerManager compartido
    manager = createWorkerManager();
    clientManager = createClientManager();

    // Bucle principal de aceptación de conexiones
    while (!stop_server) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fds.server_fd_fleck, &read_fds);
        FD_SET(server_fds.server_fd_enigma, &read_fds);

        int max_fd = (server_fds.server_fd_fleck > server_fds.server_fd_enigma) ?
                        server_fds.server_fd_fleck : server_fds.server_fd_enigma;

        logInfo("Esperando conexiones...");
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (stop_server) break;
            perror("[ERROR]: Error en select");
            break;
        }

        // Manejo de conexiones de Fleck (clientes)
        if (FD_ISSET(server_fds.server_fd_fleck, &read_fds)) {
            int client_fd = accept_connection(server_fds.server_fd_fleck);
            if (client_fd >= 0) {
                logInfo("[INFO]: Conexión aceptada desde Fleck.");
                pthread_t thread;
                ConnectionArgs *args = malloc(sizeof(ConnectionArgs));
                if (!args) {
                    logError("[ERROR]: Error asignando memoria para los argumentos del hilo.");
                    close(client_fd);
                    continue;
                }
                args->client_fd = client_fd;
                args->workerManager = manager;
                args->clientManager = clientManager;

                if (pthread_create(&thread, NULL, gestionarConexion, args) != 0) {
                    perror("[ERROR]: Fallo al crear hilo para Fleck");
                    free(args);
                    close(client_fd);
                } else {
                    pthread_detach(thread);
                }
            }
        }

        // Manejo de conexiones de Harley/Enigma (workers)
        if (FD_ISSET(server_fds.server_fd_enigma, &read_fds)) {
            int client_fd = accept_connection(server_fds.server_fd_enigma);
            if (client_fd >= 0) {
                logInfo("[INFO]: Conexión aceptada desde Harley/Enigma.");
                pthread_t thread;
                ConnectionArgs *args = malloc(sizeof(ConnectionArgs));
                if (!args) {
                    logError("[ERROR]: Error asignando memoria para los argumentos del hilo.");
                    close(client_fd);
                    continue;
                }
                args->client_fd = client_fd;
                args->workerManager = manager;
                args->clientManager = clientManager;

                if (pthread_create(&thread, NULL, gestionarConexion, args) != 0) {
                    perror("[ERROR]: Fallo al crear hilo para Harley/Enigma");
                    free(args);
                    close(client_fd);
                } else {
                    pthread_detach(thread);
                }
            }
        }
    }

    // Antes de salir, libera todo explícitamente
    if (manager) {
        freeWorkerManager(manager);
        manager = NULL;
    }
    if (clientManager) {
        freeClientManager(clientManager);
        clientManager = NULL;
    }
    if (config) {
        free(config);
        config = NULL;
    }

    close(server_fds.server_fd_fleck);
    close(server_fds.server_fd_enigma);

    logInfo("Servidor Gotham cerrado correctamente.\n");
    return EXIT_SUCCESS;
}