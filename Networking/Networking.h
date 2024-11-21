#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define printF(x) write(1, x, strlen(x))

// Funciones relacionadas con el manejo de servidores y conexiones
int connect_to_server(const char *ip, int port);
int startServer(const char *ip, int port);
int accept_connection(int server_fd);

#endif // NETWORKING_H
