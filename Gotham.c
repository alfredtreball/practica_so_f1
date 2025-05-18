/***********************************************
* @Fitxer: Gotham.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández
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
#include <time.h> // Para time()
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>

#include "GestorTramas/GestorTramas.h"
#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"
#include "Logging/Logging.h"
#include "Semafors/semaphore_v2.h"

volatile sig_atomic_t stop_server = 0; // Bandera para indicar el cierre

typedef struct {
    int server_fd_fleck;
    int server_fd_worker;
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

int arkham_pipe[2];
int arkham_pid = -1;
semaphore arkham_sem;

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
void reasignarWorkersPrincipales(WorkerManager *manager);
void freeClientManager(ClientManager *manager);
void addClient(ClientManager *manager, const char *username, const char *ip, int socket_fd);
void removeClientBySocket(ClientManager *manager, int socket_fd);
void listClients(ClientManager *manager);
void handleSigint(int sig);
void alliberarMemoria(GothamConfig *gothamConfig);
void logEvent(const char *msg);

void *enviarHeartbeat(void *arg) {
    ConnectionArgs *args = (ConnectionArgs *)arg;
    WorkerManager *workerManager = args->workerManager;
    ClientManager *clientManager = args->clientManager;

    while (!stop_server) {
        Frame heartbeat = {0};
        heartbeat.type = 0x12; // Tipo HEARTBEAT
        heartbeat.data_length = 0;
        heartbeat.timestamp = (uint32_t)time(NULL);

        // Depuración para asegurar acceso correcto
        if (!clientManager) {
            customPrintf("[ERROR]: ClientManager es NULL en el hilo de HEARTBEAT.");
        } else {
            pthread_mutex_lock(&clientManager->mutex);
            for (int i = 0; i < clientManager->clientCount; i++) {
                int clientSocket = clientManager->clients[i].socket_fd;
                if (escribirTrama(clientSocket, &heartbeat) < 0) {
                    customPrintf("[ERROR]: Error enviando HEARTBEAT a cliente.");
                }
            }
            pthread_mutex_unlock(&clientManager->mutex);
        }

        if (!workerManager) {
            customPrintf("[ERROR]: WorkerManager es NULL en el hilo de HEARTBEAT.");
        } else {
            pthread_mutex_lock(&workerManager->mutex);
            for (int i = 0; i < workerManager->workerCount; i++) {
                WorkerInfo *worker = &workerManager->workers[i];
                if (escribirTrama(worker->socket_fd, &heartbeat) < 0) {
                    customPrintf("[ERROR]: Error enviando HEARTBEAT a worker.");
                    // Cerrar socket y eliminar worker
                    shutdown(worker->socket_fd, SHUT_RDWR);
                    close(worker->socket_fd);
                    logoutWorkerBySocket(worker->socket_fd, workerManager);
                    i--;  // Ajustar índice tras eliminar worker
                }
            }
            pthread_mutex_unlock(&workerManager->mutex);
        }

        sleep(1); // Esperar antes del siguiente HEARTBEAT
    }

    free(args);
    return NULL;
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

    return manager;
}

void freeClientManager(ClientManager *manager) {
    if (manager) {
        if (manager->clients) {
            free(manager->clients); // Liberar la lista dinámica de clientes
        }
        pthread_mutex_destroy(&manager->mutex); // Destruir el mutex
        free(manager); // Liberar la estructura principal
        customPrintf("[DEBUG]: ClientManager liberado correctamente.");
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
            customPrintf("\n[INFO]: Cliente existente actualizado.\n");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
    }

    // Ampliar la capacidad si es necesario
    if (manager->clientCount == manager->capacity) {
        manager->capacity *= 2;
        ClientInfo *new_clients = realloc(manager->clients, manager->capacity * sizeof(ClientInfo));
        if (!new_clients) {
            customPrintf("[ERROR]: Error al ampliar la capacidad de clientes.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->clients = new_clients;
    }

    //Añadir un nuevo cliente
    ClientInfo *client = &manager->clients[manager->clientCount++];
    strncpy(client->username, username, sizeof(client->username) - 1);
    strncpy(client->ip, ip, sizeof(client->ip) - 1);
    client->socket_fd = socket_fd;

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
            customPrintf("[INFO]: Cliente eliminado de la lista.");
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}

// Lista los clientes conectados
void listClients(ClientManager *manager) {
    pthread_mutex_lock(&manager->mutex);

    if (manager->clientCount == 0) {
        customPrintf("No hay clientes conectados.");
    } else {
        customPrintf("\nLista de clientes conectados: \n");
        for (int i = 0; i < manager->clientCount; i++) {
            ClientInfo *client = &manager->clients[i];
            customPrintf("\nCliente %d: Usuario: %s, IP: %s, Socket: %d\n",
                     i + 1, client->username, client->ip, client->socket_fd);
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

    return manager;
}

void listarWorkers(WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex); // Asegura acceso seguro a la lista dinámica

    for (int i = 0; i < manager->workerCount; i++) {
        WorkerInfo *worker = &manager->workers[i];
        char *log_message;
        asprintf(&log_message, "\nWorker %d: Tipus: %s, IP: %s,Port: %d\n",
                    i + 1, worker->type, worker->ip, worker->port);
        printF(log_message);
        free(log_message);
    }

    pthread_mutex_unlock(&manager->mutex);
}

void registrarWorker(const char *payload, WorkerManager *manager, int client_fd) {
    if (!manager || !payload) {
        customPrintf("[ERROR]: Parámetros inválidos en registrarWorker.");
        return;
    }

    char type[10] = {0}, ip[16] = {0};
    int port = 0;

    if (sscanf(payload, "%9[^&]&%15[^&]&%d", type, ip, &port) != 3) {
        customPrintf("[ERROR]: Payload inválido (esperado TYPE&IP&PORT).");
        Frame response = {0};
        response.type = 0x02;
        strncpy(response.data, "CON_KO", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.checksum = calculate_checksum(response.data, response.data_length, 1);
        escribirTrama(client_fd, &response);
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    //Verificar si el worker ya está registrado
    for (int i = 0; i < manager->workerCount; i++) {
        if (strcmp(manager->workers[i].ip, ip) == 0 && manager->workers[i].port == port) {
            logWarning("[WARNING]: Worker ya registrado. Actualizando socket...");
            manager->workers[i].socket_fd = client_fd;  // 🔄 **Actualizar socket en vez de duplicar**
            pthread_mutex_unlock(&manager->mutex);
            
            // Responder con éxito sin volver a añadirlo
            Frame response = {0};
            response.type = 0x02;
            response.data_length = 0;
            response.checksum = calculate_checksum(response.data, response.data_length, 1);
            escribirTrama(client_fd, &response);
            return;
        }
    }

    // Ampliar capacidad si es necesario
    if (manager->workerCount == manager->capacity) {
        customPrintf("[DEBUG]: Ampliando capacidad del WorkerManager...");
        int new_capacity = manager->capacity *= 2;
        WorkerInfo *temp = realloc(manager->workers, new_capacity * sizeof(WorkerInfo));
        if (!temp) {
            customPrintf("[ERROR]: Fallo al ampliar la capacidad de WorkerManager.");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->workers = temp;
        manager->capacity = new_capacity;
    }

    // Registrar nuevo worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    char *logLine = NULL;
    asprintf(&logLine, "Worker connected: %s - IP: %s, Port: %d", type, ip, port);
    if (logLine) {
        logEvent(logLine);
        free(logLine);
    }
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

    //Mensajeún el tipo
    if (strcasecmp(type, "TEXT") == 0) {
        customPrintf("\nNew Enigma  personalizado segworker connected - ready to distort!\n");
    } else if (strcasecmp(type, "MEDIA") == 0) {
        customPrintf("\nNew Harley worker connected - ready to distort!\n");
    }

    pthread_mutex_unlock(&manager->mutex);

    // Enviar ACK
    Frame response = {0};
    response.type = 0x02;
    response.data_length = 0;
    response.checksum = calculate_checksum(response.data, response.data_length, 1);
    escribirTrama(client_fd, &response);
}

WorkerInfo *buscarWorker(const char *filename, WorkerManager *manager) {
    if (!filename || !manager) {
        customPrintf("[ERROR]: Nombre de archivo o manager inválido.\n");
        return NULL;
    }

    char *extension = strrchr(filename, '.');
    if (!extension) {
        customPrintf("[ERROR]: No se pudo determinar la extensión del archivo.\n");
        return NULL;
    }

    pthread_mutex_lock(&manager->mutex);

    WorkerInfo *targetWorker = NULL;

    if (strcasecmp(extension, ".txt") == 0) {
        if (manager->mainTextWorker && manager->mainTextWorker->ip[0] != '\0') {
            targetWorker = manager->mainTextWorker;
            customPrintf("\n[INFO]: Worker principal asignado para archivos de texto.\n");
        } else {
            customPrintf("[ERROR]: No hay un Worker principal asignado para archivos de texto.\n");
        }
    } else if (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".jpg") == 0) {
        if (manager->mainMediaWorker && manager->mainMediaWorker->ip[0] != '\0') {
            targetWorker = manager->mainMediaWorker;
            customPrintf("\n[INFO]: Worker principal asignado para archivos multimedia.\n");
        } else {
            customPrintf("[ERROR]: No hay un Worker principal asignado para archivos multimedia.\n");
        }
    } else {
        customPrintf("[ERROR]: Extensión no reconocida.\n");
    }

    if (!targetWorker) {
        customPrintf("[ERROR]: No hay workers disponibles para el tipo de archivo.\n");
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }

    // Validar los campos del Worker antes de usarlo
    if (targetWorker->ip[0] == '\0' || targetWorker->port <= 0) {
        customPrintf("[ERROR]: Worker tiene datos inválidos.\n");
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }

    if (targetWorker) {
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "[INFO]: Worker encontrado -> IP: %s, Puerto: %d\n",
                 targetWorker->ip, targetWorker->port);
        printF(log_message);
    } else {
        customPrintf("[ERROR]: No hay workers disponibles para el tipo de archivo.\n");
    }

    pthread_mutex_unlock(&manager->mutex);
    return targetWorker;
}

void asignarNuevoWorkerPrincipal(WorkerInfo *worker) {
    if (!worker) {
        customPrintf("[ERROR]: Worker no válido para asignar como principal.");
        return;
    }

    Frame frame = {0};
    frame.type = 0x08;  // Tipo de trama para asignar principal
    frame.data_length = 0;  // Sin datos adicionales
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    char *logLine = NULL;
    asprintf(&logLine, "New main worker assigned: %s - IP: %s, Port: %d", worker->type, worker->ip, worker->port);
    if (logLine) {
        logEvent(logLine);
        free(logLine);
    }
    if (escribirTrama(worker->socket_fd, &frame) == 0) {
        logSuccess("[SUCCESS]: Trama 0x08 enviada correctamente al Worker principal.\n");
    } else {
        customPrintf("[ERROR]: Error enviando la trama 0x08 al Worker principal.");
    }
}

int logoutWorkerBySocket(int socket_fd, WorkerManager *manager) {
    pthread_mutex_lock(&manager->mutex);

    int found = 0;
    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->workers[i].socket_fd == socket_fd) {
            found = 1;

            close(manager->workers[i].socket_fd);
            manager->workers[i].socket_fd = -1;

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
    if (found) {
        reasignarWorkersPrincipales(manager);
    }

    return found ? 0 : -1;
}

void reasignarWorkersPrincipales(WorkerManager *manager) {
    if (!manager || !manager->workers) {
        customPrintf("[ERROR]: WorkerManager o la lista de Workers es NULL.");
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < manager->workerCount; i++) {
        if (manager->mainTextWorker == NULL && strcasecmp(manager->workers[i].type, "TEXT") == 0) {
            manager->mainTextWorker = &manager->workers[i];
            asignarNuevoWorkerPrincipal(manager->mainTextWorker);
        }
        if (manager->mainMediaWorker == NULL && strcasecmp(manager->workers[i].type, "MEDIA") == 0) {
            manager->mainMediaWorker = &manager->workers[i];
            asignarNuevoWorkerPrincipal(manager->mainMediaWorker);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}

// Allibera la memòria utilitzada pel WorkerManager
void freeWorkerManager(WorkerManager *manager) {
    if (manager) {
        if (manager->workers) {
            free(manager->workers); // Liberar la lista dinámica de workers
        }
        pthread_mutex_destroy(&manager->mutex); // Destruir el mutex
        free(manager); // Liberar la estructura principal
        customPrintf("[DEBUG]: WorkerManager liberado correctamente.");
    }
}

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
        customPrintf("[ERROR]: WorkerManager o ClientManager NULL en gestionarConexion.");
        close(client_fd);
        return NULL;
    }

    Frame frame;
    int disconnectHandled = 0;

    // Manejo de frames inicial
    if (leerTrama(client_fd, &frame) != 0) {
        customPrintf("[ERROR]: Error al recibir el primer frame del cliente.");
        close(client_fd);
        return NULL;
    }

    processCommandInGotham(&frame, client_fd, workerManager, clientManager);

    // Bucle principal para manejar los frames del cliente
    while (!stop_server) { // Verifica la bandera global para el cierre del servidor
        if (leerTrama(client_fd, &frame) != 0) {
            customPrintf("\n[INFO]: Cliente o Worker desconectado.\n");
            
            // Determinar si es Worker o Cliente y eliminarlo
            pthread_mutex_lock(&workerManager->mutex);
            int esWorker = 0;
            for (int i = 0; i < workerManager->workerCount; i++) {
                if (workerManager->workers[i].socket_fd == client_fd) {
                    esWorker = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&workerManager->mutex);

            if (esWorker) {
                logoutWorkerBySocket(client_fd, workerManager);
            } else {
                removeClientBySocket(clientManager, client_fd);
            }

            close(client_fd);
            return NULL;
        }
        if (frame.type == 0x07) { // Trama de desconexión explícita del cliente
            customPrintf("\n[DEBUG]: Trama de desconexión recibida.\n");
            handleDisconnectFrame(&frame, client_fd, workerManager, clientManager);
            disconnectHandled = 1;
            break;
        }

        // Procesar el frame recibido
        processCommandInGotham(&frame, client_fd, workerManager, clientManager);
    }

    // Si el servidor está en proceso de cierre, gestionar recursos del cliente
    if (stop_server && !disconnectHandled) {
        customPrintf("\n[INFO]: Finalizando conexión por cierre del servidor.\n");
        removeClientBySocket(clientManager, client_fd);
    } else if (!disconnectHandled) {
        removeClientBySocket(clientManager, client_fd);
    }

    close(client_fd);
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
        customPrintf("El frame rebut és NULL.");
        return;
    }

    // Estructura per preparar la resposta
    Frame response = {0};
    response.timestamp = (uint32_t)time(NULL);

    switch (frame->type) {
        case 0x01: // CONNECT
            // Parsear los datos de la conexión (nombre de usuario, IP y puerto)
            char username[64] = {0}, ip[16] = {0};
            int port = 0;

            if (sscanf(frame->data, "%63[^&]&%15[^&]&%d", username, ip, &port) != 3) {
                customPrintf("[ERROR]: Formato de datos de conexión inválido.");
                response.type = 0x01;
                strncpy(response.data, "CON_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
                break;
            }

            // Registrar al cliente en el ClientManager
            addClient(clientManager, username, ip, client_fd);

            char *formattedName = strdup(username);
            if (formattedName) {
                formattedName[0] = toupper(formattedName[0]);

                char *logLine = NULL;
                asprintf(&logLine, "Client connected: %s", formattedName);
                if (logLine) {
                    logEvent(logLine);
                    free(logLine);
                }

                free(formattedName);
            }
            
            char *log_message = NULL;
            asprintf(&log_message, "\nNew user connected: %s.\n\n", username);
            customPrintf(log_message);
            free(log_message); // Liberar memoria asignada por asprintf

            // Enviar respuesta de éxito
            response.type = 0x01;
            response.data[0] = '\0'; // Configurar datos como vacío
            response.data_length = 0; // Longitud de datos = 0
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            escribirTrama(client_fd, &response);
            break;

        case 0x02: // REGISTER
            registrarWorker(frame->data, manager, client_fd);
            listarWorkers(manager);
            break;

        case 0x07:
            // Llamamos a la función centralizada para manejar desconexión
            handleDisconnectFrame(frame, client_fd, manager, clientManager);

            close(client_fd);
            break;

        case 0x10: // DISTORT
            customPrintf("\nProcesando comando DISTORT de Fleck\n");

            // Parsear el payload recibido: mediaType y fileName
            char mediaType[10], fileName[256];
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType, fileName) != 2) {
                customPrintf("[ERROR]: Formato de datos incorrecto en comando DISTORT.");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
                break;
            }

            // Verificar si el fileName tiene la extensión correcta según el mediaType
            char *fileExtension = strrchr(fileName, '.');
            if (!fileExtension || 
                (strcasecmp(mediaType, "TEXT") == 0 && strcasecmp(fileExtension, ".txt") != 0) ||
                (strcasecmp(mediaType, "MEDIA") == 0 && strcasecmp(fileExtension, ".wav") != 0 &&
                strcasecmp(fileExtension, ".png") != 0 && strcasecmp(fileExtension, ".jpg") != 0)) {
                customPrintf("[ERROR]: Extensión de archivo no válida o no coincide con mediaType.\n");
                response.type = 0x10;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
                break;
            }

            // Buscar worker adecuado
            WorkerInfo *targetWorker = buscarWorker(fileName, manager);           
            if (!targetWorker) {
                customPrintf("[ERROR]: No se encontró un worker para el archivo especificado.\n");
                //handleWorkerFailure(mediaType, manager, client_fd);
                break;
            }

            //Obtener el nombre del cliente según su socket
            char clientName[64] = "Unknown";
            pthread_mutex_lock(&clientManager->mutex);
            for (int i = 0; i < clientManager->clientCount; i++) {
                if (clientManager->clients[i].socket_fd == client_fd) {
                    strncpy(clientName, clientManager->clients[i].username, sizeof(clientName) - 1);
                    break;
                }
            }
            pthread_mutex_unlock(&clientManager->mutex);

            char *logLine = NULL;
            asprintf(&logLine, "Client %s requested distortion of file: %s", clientName, fileName);
            if (logLine) {
                logEvent(logLine);
                free(logLine);
            }

            // Mensaje informativo personalizado
            if (strcasecmp(mediaType, "TEXT") == 0) {
                customPrintf("%s has sent a text distortion petition – redirecting %s to Enigma worker.\n\n",
                            clientName, clientName);
            } else if (strcasecmp(mediaType, "MEDIA") == 0) {
                customPrintf("%s has sent a media distortion petition – redirecting %s to Harley worker.\n\n",
                            clientName, clientName);
            }

            // Preparar respuesta con la información del Worker
            snprintf(response.data, sizeof(response.data), "%s&%d", targetWorker->ip, targetWorker->port);
            response.type = 0x10;
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);
            escribirTrama(client_fd, &response);
            break;

        case 0x11: //REASINGAR WORKER
            customPrintf("\n[INFO]: Procesando solicitud de reasignación de Worker...\n");

            // Parsear datos: <mediaType>&<FileName>
            char mediaType0x11[10] = {0};
            char fileName0x11[256] = {0};
            if (sscanf(frame->data, "%9[^&]&%255s", mediaType0x11, fileName0x11) != 2) {
                customPrintf("[ERROR]: Formato inválido en la solicitud de reasignación.\n");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
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
                customPrintf("[ERROR]: Extensión de archivo no válida o no coincide con el mediaType.\n");
                response.type = 0x11;
                strncpy(response.data, "MEDIA_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
                break;
            }

            // Buscar un Worker disponible
            WorkerInfo *targetWorkerCaiguda = buscarWorker(fileName0x11, manager);
            if (!targetWorkerCaiguda) {
                customPrintf("[ERROR]: No hay Workers disponibles para el tipo especificado.\n");
                response.type = 0x11;
                strncpy(response.data, "DISTORT_KO", sizeof(response.data) - 1);
                response.data_length = strlen(response.data);
                response.checksum = calculate_checksum(response.data, response.data_length, 0);
                escribirTrama(client_fd, &response);
                break;
            }

            // Responder con la información del Worker
            char workerInfo0x11[DATA_MAX_SIZE] = {0};
            snprintf(workerInfo0x11, sizeof(workerInfo0x11), "%s&%d", targetWorkerCaiguda->ip, targetWorkerCaiguda->port);
            response.type = 0x11;
            strncpy(response.data, workerInfo0x11, sizeof(response.data) - 1);
            response.data_length = strlen(response.data);
            response.checksum = calculate_checksum(response.data, response.data_length, 0);

            customPrintf("\n[INFO]: Enviando información del Worker reasignado...\n");
            escribirTrama(client_fd, &response);
            break;

        default: // Comanda desconeguda
            customPrintf("Comanda desconeguda rebuda.\n");
            enviarTramaError(client_fd);
            break;
    }
}

void handleDisconnectFrame(const Frame *frame, int client_fd, WorkerManager *manager, ClientManager *clientManager) {
    if (!frame || !manager || !clientManager) {
        customPrintf("[ERROR]: Parámetros inválidos en handleDisconnectFrame.");
        return;
    }

    int isWorker = 0;
    int isClient = 0;
    WorkerInfo *disconnectedWorker = NULL; 

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
        pthread_mutex_lock(&manager->mutex);
        for (int i = 0; i < manager->workerCount; i++) {
            if (manager->workers[i].socket_fd == client_fd) {
                disconnectedWorker = &manager->workers[i];
                break;
            }
        }
        pthread_mutex_unlock(&manager->mutex);
    } 

    if (disconnectedWorker) {
            char *logLine = NULL;
            asprintf(&logLine, "Worker disconnected: %s - IP: %s, Port: %d",
                    disconnectedWorker->type, disconnectedWorker->ip, disconnectedWorker->port);
            if (logLine) {
                logEvent(logLine);
                free(logLine);
            }
            close(client_fd);  // Cerrar el socket antes de eliminar el Worker
            int wasMain = (manager->mainTextWorker == disconnectedWorker || manager->mainMediaWorker == disconnectedWorker);
            if (logoutWorkerBySocket(client_fd, manager) == 0) {
                customPrintf("\n[INFO]: Worker desconectado correctamente.\n");
                if (wasMain) {
                    customPrintf("\n[INFO]: Reasignando nuevo Worker principal...\n");
                    reasignarWorkersPrincipales(manager);
                }
            }
        } else if (isClient) {
            customPrintf("\nIdentificado como Cliente. Procesando desconexión...\n");
            char *clientName = NULL;

            pthread_mutex_lock(&clientManager->mutex);
            for (int i = 0; i < clientManager->clientCount; i++) {
                if (clientManager->clients[i].socket_fd == client_fd) {
                    asprintf(&clientName, "Client disconnected: %s", clientManager->clients[i].username);
                    break;
                }
            }
            pthread_mutex_unlock(&clientManager->mutex);

            if (clientName) {
                logEvent(clientName);
                free(clientName);
            }

            close(client_fd);
            removeClientBySocket(clientManager, client_fd);
        } else {
            logWarning("[WARNING]: Socket no corresponde a un Worker ni a un Cliente.\n");
        }
}

void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) free(gothamConfig->ipFleck);
    if (gothamConfig->ipHarEni) free(gothamConfig->ipHarEni);
    free(gothamConfig);
}

void handleSigint(int sig) {
    (void)sig; // Ignorar el valor de la señal
    customPrintf("\nS'ha rebut SIGINT. Tancant el sistema...\n");
    stop_server = 1;

    SEM_destructor(&arkham_sem);
    close(arkham_pipe[1]); // asegúrate de cerrar el lado de escritura

    // Liberar los recursos de WorkerManager
    if (workerManager) {
        pthread_mutex_lock(&workerManager->mutex);
        for (int i = 0; i < workerManager->workerCount; i++) {
            WorkerInfo *worker = &workerManager->workers[i];
            close(worker->socket_fd); // Cerrar el socket del worker
        }
        pthread_mutex_unlock(&workerManager->mutex);
        freeWorkerManager(workerManager); // Liberar el WorkerManager
        workerManager = NULL;
    }

    // Liberar los recursos de ClientManager
    if (clientManager) {
        pthread_mutex_lock(&clientManager->mutex);
        for (int i = 0; i < clientManager->clientCount; i++) {
            shutdown(clientManager->clients[i].socket_fd, SHUT_RDWR);
            close(clientManager->clients[i].socket_fd); // Cerrar el socket del cliente
        }
        pthread_mutex_unlock(&clientManager->mutex);
        freeClientManager(clientManager); // Liberar el ClientManager
        clientManager = NULL;
    }

    // Cerrar los descriptores de archivo globales
    if (global_server_fds) {
        if (global_server_fds->server_fd_fleck >= 0) {
            close(global_server_fds->server_fd_fleck);
            global_server_fds->server_fd_fleck = -1;
        }
        if (global_server_fds->server_fd_worker >= 0) {
            close(global_server_fds->server_fd_worker);
            global_server_fds->server_fd_worker = -1;
        }
    }

    // Liberar la configuración global
    if (global_config) {
        if (global_config->ipFleck) free(global_config->ipFleck);
        if (global_config->ipHarEni) free(global_config->ipHarEni);
        free(global_config);
        global_config = NULL;
    }

    logSuccess("Sistema tancat correctament.\n");
    exit(EXIT_SUCCESS);
}


//Loggear eventos de Arkham
void logEvent(const char *msg) {
    SEM_wait(&arkham_sem);
    write(arkham_pipe[1], msg, strlen(msg));
    write(arkham_pipe[1], "\n", 1);  // Asegura fin de línea
    SEM_signal(&arkham_sem);
}

// Funció principal del servidor Gotham
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: ./gotham <archivo de configuración>\n");
        return EXIT_FAILURE;
    }

    GothamConfig *config = malloc(sizeof(GothamConfig));
    global_config = config; // Asignar el puntero global

    WorkerManager *manager = NULL; // Declarar manager aquí
    ClientManager *clientManager = NULL; // Declarar clientManager aquí

    if (!config) {
        customPrintf("Error asignando memoria para la configuración.\n");
        return EXIT_FAILURE;
    }

    readConfigFileGeneric(argv[1], config, CONFIG_GOTHAM);

    ServerFds server_fds;
    global_server_fds = &server_fds; // Asignar el puntero global

    signal(SIGINT, handleSigint);
    signal(SIGPIPE, SIG_IGN);

    server_fds.server_fd_fleck = startServer(config->ipFleck, config->portFleck);
    server_fds.server_fd_worker = startServer(config->ipHarEni, config->portHarEni);

    if (server_fds.server_fd_fleck < 0 || server_fds.server_fd_worker < 0) {
        customPrintf("Error al iniciar los servidores.\n");
        //FER TOTS ELS FREES
        if (server_fds.server_fd_fleck >= 0) close(server_fds.server_fd_fleck);
        if (server_fds.server_fd_worker >= 0) close(server_fds.server_fd_worker);

        if (clientManager) {
            freeClientManager(clientManager);
            clientManager = NULL;
        }

        if (manager) {
            freeWorkerManager(manager);
            manager = NULL;
        }

        free(config);
        free(global_config->ipFleck);
        free(global_config->ipHarEni);
        global_config = NULL;
        return EXIT_FAILURE;
    }

    // Crear el WorkerManager compartido
    manager = createWorkerManager();
    clientManager = createClientManager();
    if (!manager || !clientManager) {
        customPrintf("[ERROR]: No se pudo inicializar WorkerManager o ClientManager.");
        exit(EXIT_FAILURE);
    }

    // Crear el hilo de HEARTBEAT
    ConnectionArgs *heartbeatArgs = malloc(sizeof(ConnectionArgs));
    heartbeatArgs->workerManager = manager;
    heartbeatArgs->clientManager = clientManager;
    heartbeatArgs->client_fd = -1; // No lo usaremos en este caso

    pthread_t heartbeatThread;
    if (pthread_create(&heartbeatThread, NULL, enviarHeartbeat, heartbeatArgs) != 0) {
        customPrintf("[ERROR]: No se pudo crear el hilo de HEARTBEAT.");
        free(heartbeatArgs); // Liberar en caso de error
        exit(EXIT_FAILURE);
    } else {
        pthread_detach(heartbeatThread);         
    }

    if (pipe(arkham_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    arkham_pid = fork();
    if (arkham_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (arkham_pid == 0) {
        // Código de Arkham
        close(arkham_pipe[1]); // Cierra escritura
        dup2(arkham_pipe[0], STDIN_FILENO); // Redirige stdin
        execl("./arkham", "arkham", NULL);
        perror("execl Arkham");
        exit(EXIT_FAILURE);
    }

    close(arkham_pipe[0]); // Cierra lectura en Gotham

    // Inicialización del semáforo (solo en Gotham)
    key_t key = ftok("Gotham.c", 'A');
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    if (SEM_constructor_with_name(&arkham_sem, key) < 0) {
        write(STDERR_FILENO, "Error creando semáforo\n", 24);
        exit(EXIT_FAILURE);
    }

    if (SEM_init(&arkham_sem, 1) < 0) {
        write(STDERR_FILENO, "Error inicializando semáforo\n", 30);
        exit(EXIT_FAILURE);
    }


    // Bucle principal de aceptación de conexiones
    while (!stop_server) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fds.server_fd_fleck, &read_fds);
        FD_SET(server_fds.server_fd_worker, &read_fds);

        int max_fd = (server_fds.server_fd_fleck > server_fds.server_fd_worker) ?
                        server_fds.server_fd_fleck : server_fds.server_fd_worker;

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
                pthread_t thread;
                ConnectionArgs *args = malloc(sizeof(ConnectionArgs));
                if (!args) {
                    customPrintf("[ERROR]: Error asignando memoria para los argumentos del hilo.");
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
                    pthread_detach(thread); // Liberar recursos del thread automáticamente
                }
            }
        }

        // Manejo de conexiones de Harley/Enigma (workers)
        if (FD_ISSET(server_fds.server_fd_worker, &read_fds)) {
            int client_fd = accept_connection(server_fds.server_fd_worker);
            if (client_fd >= 0) {
                pthread_t thread;
                ConnectionArgs *args = malloc(sizeof(ConnectionArgs));
                if (!args) {
                    customPrintf("[ERROR]: Error asignando memoria para los argumentos del hilo.");
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
    close(server_fds.server_fd_worker);
    close(arkham_pipe[1]); // asegúrate de cerrar el lado de escritura

    // Esperar a que el hilo HEARTBEAT termine
    SEM_destructor(&arkham_sem);
    close(arkham_pipe[1]);
    for (int fd = 3; fd < 1024; ++fd) close(fd);

    customPrintf("\nServidor Gotham cerrado correctamente.\n");
    
    return EXIT_SUCCESS;
}