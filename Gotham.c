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

// Declaració de funcions
void *gestionarConexion(void *arg);
void *esperarConexiones(void *arg);
void processCommandInGotham(const char *command, int client_fd, WorkerManager *manager);
void registrarWorker(const char *payload, WorkerManager *manager, int client_fd);
int buscarWorker(const char *type, WorkerManager *manager, char *ip, int *port);
int logoutWorkerBySocket(int socket_fd, WorkerManager *manager);
void alliberarMemoria(GothamConfig *gothamConfig);


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
    // Crear una còpia de la comanda en memòria dinàmica perquè strtok la modificarà
    char *commandCopy = strdup(command);
    if (!commandCopy) {
        char *error_msg = strdup("\033[31m[ERROR]: Error duplicant la comanda\033[0m\n"); // Vermell
        if (error_msg) {
            printF(error_msg);
            free(error_msg);
        }
        send_frame(client_fd, "ERROR COMMAND", 13);
        return;
    }

    // Separar la comanda per camps utilitzant '|'
    char *token = strtok(commandCopy, "|"); // Primer camp (id)
    token = strtok(NULL, "|"); // Segon camp (comanda)

    // Verificar que s'ha obtingut la comanda
    if (token == NULL) {
        char *warning_msg = strdup("\033[33m[WARNING]: Comanda rebuda sense contingut vàlid\033[0m\n"); // Groc
        if (warning_msg) {
            printF(warning_msg);
            free(warning_msg);
        }
        free(commandCopy);
        send_frame(client_fd, "ERROR COMMAND", 13);
        return;
    }

    if (strcasecmp(token, "CONNECT") == 0) {
        char *connect_msg = strdup("\033[32m[SUCCESS]: Client connectat correctament\033[0m\n"); // Verd
        if (connect_msg) {
            printF(connect_msg);
            free(connect_msg);
        }
        send_frame(client_fd, "ACK", 3); // Confirma la connexió
    } else if (strncasecmp(token, "DISTORT", 7) == 0) {
        char *fileName = malloc(100);
        if (!fileName) {
            char *error_msg = strdup("\033[31m[ERROR]: Error assignant memòria per al fitxer DISTORT\033[0m\n");
            if (error_msg) {
                printF(error_msg);
                free(error_msg);
            }
            free(commandCopy);
            send_frame(client_fd, "ERROR DISTORT MEMORY", 20);
            return;
        }
        int factor = 0;

        // Parsejar manualment la comanda per obtenir els paràmetres
        char *rest = strtok(NULL, "|"); // Obtenim el paràmetre de fitxer
        if (rest) {
            strncpy(fileName, rest, 100);
            rest = strtok(NULL, "|"); // Obtenim el factor
            if (rest) {
                factor = atoi(rest);
            } else {
                char *error_params_msg = strdup("\033[31m[ERROR]: Paràmetres de DISTORT incorrectes\033[0m\n"); // Vermell
                if (error_params_msg) {
                    printF(error_params_msg);
                    free(error_params_msg);
                }
                free(fileName);
                free(commandCopy);
                send_frame(client_fd, "ERROR DISTORT PARAMS", 20);
                return;
            }
        }

        char *ip = malloc(16);
        int port = 0;
        if (!ip) {
            char *error_msg = strdup("\033[31m[ERROR]: Error assignant memòria per a l'IP del worker\033[0m\n");
            if (error_msg) {
                printF(error_msg);
                free(error_msg);
            }
            free(fileName);
            free(commandCopy);
            send_frame(client_fd, "ERROR DISTORT MEMORY", 20);
            return;
        }

        pthread_mutex_lock(&manager->mutex); // Bloqueja l'accés al WorkerManager
        int result = buscarWorker("MEDIA", manager, ip, &port);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            char *success_distort_msg = malloc(256);
            if (success_distort_msg) {
                snprintf(success_distort_msg, 256, "\033[36m[INFO]: DISTORT redirigit al worker amb IP %s i port %d\033[0m\n", ip, port); // Blau clar
                printF(success_distort_msg);
                free(success_distort_msg);
            }

            char *response = malloc(256);
            if (response) {
                snprintf(response, 256, "DISTORT_OK %s:%d Factor:%d", ip, port, factor);
                send_frame(client_fd, response, strlen(response));
                free(response);
            }
        } else {
            char *warning_no_worker_msg = strdup("\033[33m[WARNING]: Cap worker disponible per DISTORT\033[0m\n"); // Groc
            if (warning_no_worker_msg) {
                printF(warning_no_worker_msg);
                free(warning_no_worker_msg);
            }
            send_frame(client_fd, "DISTORT_KO", 10);
        }
        free(ip);
        free(fileName);
    } else if (strcasecmp(token, "CHECK STATUS") == 0) {
        char *success_status_msg = strdup("\033[32m[SUCCESS]: Sol·licitud de CHECK STATUS processada\033[0m\n"); // Verd
        if (success_status_msg) {
            printF(success_status_msg);
            free(success_status_msg);
        }
        send_frame(client_fd, "STATUS OK ACK", 13);
    } else if (strcasecmp(token, "CLEAR ALL") == 0) {
        char *info_clear_all_msg = strdup("\033[35m[INFO]: Sol·licitud de CLEAR ALL rebuda i processada\033[0m\n"); // Magenta
        if (info_clear_all_msg) {
            printF(info_clear_all_msg);
            free(info_clear_all_msg);
        }
        send_frame(client_fd, "ACK CLEAR ALL", 13);
    } else if (strcasecmp(token, "LOGOUT") == 0) {
        char *warning_logout_msg = strdup("\033[33m[WARNING]: Client sol·licitant LOGOUT\033[0m\n"); // Groc
        if (warning_logout_msg) {
            printF(warning_logout_msg);
            free(warning_logout_msg);
        }
        send_frame(client_fd, "ACK LOGOUT", 10);

        pthread_mutex_lock(&manager->mutex);
        int result = logoutWorkerBySocket(client_fd, manager);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            char *success_logout_msg = strdup("\033[32m[SUCCESS]: Worker eliminat del registre\033[0m\n"); // Verd
            if (success_logout_msg) {
                printF(success_logout_msg);
                free(success_logout_msg);
            }
            send_frame(client_fd, "ACK LOGOUT", 10);
        } else {
            char *error_logout_msg = strdup("\033[31m[ERROR]: Error eliminant el worker del registre\033[0m\n"); // Vermell
            if (error_logout_msg) {
                printF(error_logout_msg);
                free(error_logout_msg);
            }
            send_frame(client_fd, "ERROR LOGOUT", 12);
        }

        close(client_fd); // Tanca el socket del client
        char *info_disconnect_msg = malloc(128);
        if (info_disconnect_msg) {
            snprintf(info_disconnect_msg, 128, "\033[36m[INFO]: Connexió del client desconnectada: socket_fd=%d\033[0m\n", client_fd); // Blau clar
            printF(info_disconnect_msg);
            free(info_disconnect_msg);
        }
    } else {
        char *error_unknown_command_msg = strdup("\033[31m[ERROR]: Comanda desconeguda rebuda\033[0m\n"); // Vermell
        if (error_unknown_command_msg) {
            printF(error_unknown_command_msg);
            free(error_unknown_command_msg);
        }
        send_frame(client_fd, "ERROR COMMAND", 13);
    }

    free(commandCopy); // Alliberar la memòria de la còpia
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
