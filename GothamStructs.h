#ifndef GOTHAMSTRUCTS_H
#define GOTHAMSTRUCTS_H

#include <pthread.h>
#include <signal.h>

#include "../FileReader/FileReader.h"  // Incluye la definición de GothamConfig

typedef struct {
    char ip[16];
    int port;
    char type[10];
    int socket_fd;
} WorkerInfo;

typedef struct {
    WorkerInfo *workers;
    int workerCount;
    int capacity;
    WorkerInfo *mainTextWorker;
    WorkerInfo *mainMediaWorker;
    pthread_mutex_t mutex;
} WorkerManager;

typedef struct {
    char username[64];
    char ip[16];
    int socket_fd;
} ClientInfo;

typedef struct {
    ClientInfo *clients;
    int clientCount;
    int capacity;
    pthread_mutex_t mutex;
} ClientManager;

typedef struct {
    int server_fd_fleck;
    int server_fd_enigma;
} ServerFds;

extern ServerFds *global_server_fds;
extern GothamConfig *global_config;
extern volatile sig_atomic_t stop_server;

#endif // GOTHAMSTRUCTS_H
