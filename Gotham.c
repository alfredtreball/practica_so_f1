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
    // Feu una còpia de la comanda perquè strtok la modificarà
    char *commandCopy = strdup(command);
    if (!commandCopy) {
        perror("Error duplicant la comanda");
        send_frame(client_fd, "ERROR COMMAND", 13);
        return;
    }

    // Separar la comanda per camps utilitzant '|'
    char *token = strtok(commandCopy, "|"); // Primer camp (id)
    token = strtok(NULL, "|"); // Segon camp (comanda)
    
    // Verificar que s'ha obtingut la comanda
    if (token == NULL) {
        free(commandCopy);
        send_frame(client_fd, "ERROR COMMAND", 13);
        return;
    }

    if (strcasecmp(token, "CONNECT") == 0) {
        send_frame(client_fd, "ACK", 3); // Confirma la connexió
    } else if (strncasecmp(token, "DISTORT", 7) == 0) {
        char fileName[100];
        int factor;

        // Parsejar manualment la comanda per obtenir els paràmetres
        char *rest = strtok(NULL, "|"); // Obtenim el paràmetre de fitxer
        if (rest) {
            strncpy(fileName, rest, sizeof(fileName));
            rest = strtok(NULL, "|"); // Obtenim el factor
            if (rest) {
                factor = atoi(rest);
            } else {
                send_frame(client_fd, "ERROR DISTORT PARAMS", 20);
                free(commandCopy);
                return;
            }
        }

        char ip[16];
        int port;
        pthread_mutex_lock(&manager->mutex); // Bloqueja l'accés al WorkerManager
        int result = buscarWorker("MEDIA", manager, ip, &port);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            char response[256];
            snprintf(response, sizeof(response), "DISTORT_OK %s:%d Factor:%d", ip, port, factor);
            send_frame(client_fd, response, strlen(response));
        } else {
            send_frame(client_fd, "DISTORT_KO", 10);
        }
    } else if (strcasecmp(token, "CHECK STATUS") == 0) {
        send_frame(client_fd, "STATUS OK ACK", 13);
    } else if (strcasecmp(token, "CLEAR ALL") == 0) {
        send_frame(client_fd, "ACK CLEAR ALL", 13);
    } else if (strcasecmp(token, "LOGOUT") == 0) {
        send_frame(client_fd, "ACK LOGOUT", 10);
        pthread_mutex_lock(&manager->mutex);
        int result = logoutWorkerBySocket(client_fd, manager);
        pthread_mutex_unlock(&manager->mutex);

        if (result == 0) {
            send_frame(client_fd, "ACK LOGOUT", 10);
        } else {
            send_frame(client_fd, "ERROR LOGOUT", 12);
        }
        close(client_fd); // Tanca el socket del client
        printf("Fleck desconnectat: socket_fd=%d\n", client_fd);
    } else {
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
        printf("Ús: ./gotham <fitxer de configuració>\n");
        return 1;
    }

    GothamConfig *config = malloc(sizeof(GothamConfig));
    if (!config) {
        printf("Error assignant memòria.\n");
        return 1;
    }

    readConfigFileGeneric(argv[1], config, CONFIG_GOTHAM);

    int server_fd_fleck = startServer(config->ipFleck, config->portFleck);
    int server_fd_enigma = startServer(config->ipHarEni, config->portHarEni);

    int server_fds[3] = {server_fd_fleck, server_fd_enigma, -1};
    pthread_t thread;
    pthread_create(&thread, NULL, esperarConexiones, server_fds);
    pthread_join(thread, NULL);

    close(server_fd_fleck);
    close(server_fd_enigma);
    alliberarMemoria(config);
    return 0;
}
