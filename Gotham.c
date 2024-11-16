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
    char type[10] = {0}, ip[16] = {0};
    int port = 0;

    // Divideix el payload en parts utilitzant '&'
    char *payloadCopy = strdup(payload);
    if (!payloadCopy) {
        logError("Error duplicant el payload.");
        send_frame(client_fd, "CON_KO", strlen("CON_KO"));
        return;
    }

    char *token = strtok(payloadCopy, "&");
    if (token) strncpy(type, token, sizeof(type) - 1);

    token = strtok(NULL, "&");
    if (token) strncpy(ip, token, sizeof(ip) - 1);

    token = strtok(NULL, "&");
    if (token) port = atoi(token); // Convertim el port

    free(payloadCopy);

    pthread_mutex_lock(&manager->mutex);

    // Comprova si cal ampliar la capacitat
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
    strncpy(worker->type, type, sizeof(worker->type));
    strncpy(worker->ip, ip, sizeof(worker->ip));
    worker->port = port;

    pthread_mutex_unlock(&manager->mutex);

    // Responem al client i tanquem la connexió
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

    char frame[FRAME_SIZE];
    while (read(client_fd, frame, FRAME_SIZE) > 0) {
        processCommandInGotham(frame, client_fd, manager); // Processa les comandes rebudes.
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
            *client_fd = accept(server_fd_fleck, NULL, NULL);
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
            *client_fd = accept(server_fd_enigma, NULL, NULL);
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
            *client_fd = accept(server_fd_harley, NULL, NULL);
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

/***********************************************
* @Finalitat: Processa una comanda rebuda a Gotham.
* Identifica el tipus de comanda i executa les accions corresponents.
* @Paràmetres:
*   in: command = la comanda rebuda.
*   in: client_fd = descriptor del client que ha enviat la comanda.
*   in: manager = punter al WorkerManager.
* @Retorn: ----
************************************************/
void processCommandInGotham(const char *command, int client_fd, WorkerManager *manager) {
    // Depuración del comando recibido
    printF("\033[34m[DEBUG]: Comando completo recibido: ");
    printF(command);
    printF("\033[0m\n");

    // Limpiar prefijo numérico (si existe)
    const char *cleanCommand = command;
    if (isdigit(command[0])) {
        cleanCommand = strchr(command, '|'); // Buscar el separador '|'
        if (cleanCommand) {
            cleanCommand++; // Saltar el '|'
        } else {
            cleanCommand = command; // Si no hay '|', usar el comando completo
        }
    }

    // Depuración del comando limpio
    printF("\033[34m[DEBUG]: Comando limpio: ");
    printF(cleanCommand);
    printF("\033[0m\n");

    // Crear una copia del comando porque strtok modificará la cadena
    char *commandCopy = strdup(cleanCommand);
    if (!commandCopy) {
        char *error_msg = strdup("\033[31m[ERROR]: Error duplicant la comanda\033[0m\n");
        if (error_msg) {
            printF(error_msg);
            free(error_msg);
        }
        send_frame(client_fd, "ERROR COMMAND", strlen("ERROR COMMAND"));
        return;
    }

    // Separar la comanda en campos utilizando '&' o '|'
    char *token = strtok(commandCopy, "&|");
    if (!token) {
        char *warning_msg = strdup("\033[33m[WARNING]: Comanda rebuda sense contingut vàlid\033[0m\n");
        if (warning_msg) {
            printF(warning_msg);
            free(warning_msg);
        }
        free(commandCopy);
        send_frame(client_fd, "ERROR COMMAND", strlen("ERROR COMMAND"));
        return;
    }

    printF("\033[34m[DEBUG]: Token inicial: ");
    printF(token);
    printF("\033[0m\n");

    // Analizar e identificar el tipo de comando
    if (strcasecmp(token, "REGISTER WORKER") == 0) {
        // Procesar el registro del worker
        char *workerType = strtok(NULL, "&");
        char *workerIP = strtok(NULL, "&");
        char *workerPortStr = strtok(NULL, "&");

        if (!workerType || !workerIP || !workerPortStr) {
            logError("Dades incompletes per registrar el worker.");
            send_frame(client_fd, "CON_KO", strlen("CON_KO"));
            free(commandCopy);
            return;
        }

        int workerPort = atoi(workerPortStr);

        // Añadir el worker a la lista
        pthread_mutex_lock(&manager->mutex);
        if (manager->workerCount == manager->capacity) {
            manager->capacity *= 2;
            WorkerInfo *newWorkers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
            if (!newWorkers) {
                logError("Error ampliant la capacitat de la llista de workers.");
                pthread_mutex_unlock(&manager->mutex);
                send_frame(client_fd, "CON_KO", strlen("CON_KO"));
                free(commandCopy);
                return;
            }
            manager->workers = newWorkers;
        }

        WorkerInfo *worker = &manager->workers[manager->workerCount++];
        strncpy(worker->type, workerType, sizeof(worker->type) - 1);
        strncpy(worker->ip, workerIP, sizeof(worker->ip) - 1);
        worker->port = workerPort;
        worker->socket_fd = client_fd;
        pthread_mutex_unlock(&manager->mutex);

        logSuccess("Worker registrat correctament.");
        send_frame(client_fd, "CON_OK", strlen("CON_OK"));

    } else if (strcasecmp(token, "CONNECT") == 0) {
        send_frame(client_fd, "ACK", strlen("ACK"));
        char *connect_msg = strdup("\033[32m[SUCCESS]: Client connectat correctament.\033[0m\n");
        if (connect_msg) {
            printF(connect_msg);
            free(connect_msg);
        }

    } else if (strcasecmp(token, "DISTORT") == 0) {
        // Comanda DISTORT
        char *fileName = strtok(NULL, "&");
        char *factorStr = strtok(NULL, "&");

        if (!fileName || !factorStr) {
            char *error_msg = strdup("\033[31m[ERROR]: Falten camps per DISTORT.\033[0m\n");
            if (error_msg) {
                printF(error_msg);
                free(error_msg);
            }
            send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
            free(commandCopy);
            return;
        }

        int factor = atoi(factorStr);

        char workerIP[16] = {0};
        int workerPort = 0;

        pthread_mutex_lock(&manager->mutex);
        int result = buscarWorker("MEDIA", manager, workerIP, &workerPort);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            char response[FRAME_SIZE];
            snprintf(response, FRAME_SIZE, "DISTORT_OK&%s&%d&%d", workerIP, workerPort, factor);
            send_frame(client_fd, response, strlen(response));
            char *distort_msg = strdup("\033[36m[INFO]: DISTORT redirigit correctament.\033[0m\n");
            if (distort_msg) {
                printF(distort_msg);
                free(distort_msg);
            }
        } else {
            send_frame(client_fd, "DISTORT_KO", strlen("DISTORT_KO"));
            char *warning_msg = strdup("\033[33m[WARNING]: No hi ha workers disponibles per DISTORT.\033[0m\n");
            if (warning_msg) {
                printF(warning_msg);
                free(warning_msg);
            }
        }

    } else if (strcasecmp(token, "CHECK STATUS") == 0) {
        send_frame(client_fd, "STATUS OK ACK", strlen("STATUS OK ACK"));
        char *status_msg = strdup("\033[32m[SUCCESS]: Sol·licitud de CHECK STATUS processada.\033[0m\n");
        if (status_msg) {
            printF(status_msg);
            free(status_msg);
        }

    } else if (strcasecmp(token, "CLEAR ALL") == 0) {
        send_frame(client_fd, "ACK CLEAR ALL", strlen("ACK CLEAR ALL"));
        char *clear_msg = strdup("\033[35m[INFO]: Sol·licitud de CLEAR ALL rebuda i processada.\033[0m\n");
        if (clear_msg) {
            printF(clear_msg);
            free(clear_msg);
        }

    } else if (strcasecmp(token, "LOGOUT") == 0) {
        pthread_mutex_lock(&manager->mutex);
        int result = logoutWorkerBySocket(client_fd, manager);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            send_frame(client_fd, "ACK LOGOUT", strlen("ACK LOGOUT"));
            char *logout_msg = strdup("\033[32m[SUCCESS]: Worker eliminat del registre.\033[0m\n");
            if (logout_msg) {
                printF(logout_msg);
                free(logout_msg);
            }
        } else {
            send_frame(client_fd, "ERROR LOGOUT", strlen("ERROR LOGOUT"));
            char *error_msg = strdup("\033[31m[ERROR]: Error eliminant el worker del registre.\033[0m\n");
            if (error_msg) {
                printF(error_msg);
                free(error_msg);
            }
        }

    } else {
        send_frame(client_fd, "ERROR COMMAND", strlen("ERROR COMMAND"));
        char *unknown_msg = strdup("\033[31m[ERROR]: Comanda desconeguda rebuda.\033[0m\n");
        if (unknown_msg) {
            printF(unknown_msg);
            free(unknown_msg);
        }
    }

    free(commandCopy); // Liberar memoria de la copia
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
    char ip[16] = {0}, type[10] = {0};
    int port = 0;

    // Parsejar la informació del payload utilitzant strtok
    char *payloadCopy = strdup(payload); // Feu una còpia per no modificar l'original
    if (!payloadCopy) {
        perror("Error duplicant el payload");
        return;
    }

    char *token = strtok(payloadCopy, " ");
    if (token) {
        strncpy(ip, token, sizeof(ip) - 1);
        token = strtok(NULL, " ");
        if (token) {
            port = atoi(token); // Convertir el port en un número
            token = strtok(NULL, " ");
            if (token) {
                strncpy(type, token, sizeof(type) - 1);
            }
        }
    }
    free(payloadCopy); // Allibera la memòria de la còpia

    pthread_mutex_lock(&manager->mutex);

    // Comprovar si cal ampliar la capacitat
    if (manager->workerCount == manager->capacity) {
        manager->capacity *= 2; // Duplicar la capacitat
        WorkerInfo *newWorkers = realloc(manager->workers, manager->capacity * sizeof(WorkerInfo));
        if (!newWorkers) {
            perror("Error ampliant la llista de workers");
            pthread_mutex_unlock(&manager->mutex);
            return;
        }
        manager->workers = newWorkers;
    }

    // Afegir el nou worker
    WorkerInfo *worker = &manager->workers[manager->workerCount++];
    strncpy(worker->ip, ip, sizeof(worker->ip));
    worker->port = port;
    strncpy(worker->type, type, sizeof(worker->type));
    worker->socket_fd = client_fd;

    pthread_mutex_unlock(&manager->mutex);
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
        if (strcmp(manager->workers[i].type, type) == 0) {
            strncpy(ip, manager->workers[i].ip, 16);
            *port = manager->workers[i].port;
            return 0;
        }
    }
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
