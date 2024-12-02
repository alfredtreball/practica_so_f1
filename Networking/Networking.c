#define _GNU_SOURCE // Necesario para funciones GNU como asprintf

#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "Networking.h"

// Conecta a un servidor
int connect_to_server(const char *ip, int port) {
    if (!ip) {
        printF("Error: IP NULL\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printF("Error creando el socket\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printF("IP no válida\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printF("Error conectando al servidor\n");
        close(sockfd);
        return -1;
    }

    char *msg = NULL;
    asprintf(&msg, "Conectado al servidor: %s:%d\n", ip, port);
    printF(msg);
    free(msg);

    return sockfd;
}

// Inicia un servidor
int startServer(const char *ip, int port) {
    (void)ip;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printF("Error creando el socket del servidor\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printF("IP no válida\n");
        close(server_fd);
        return -1;
    }*/

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printF("Error en el bind\n");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        printF("Error en el listen\n");
        close(server_fd);
        return -1;
    }

    char *msg = NULL;
    asprintf(&msg, "Servidor escuchando en todas las interfaces en el puerto %d\n", port);
    printF(msg);
    free(msg);

    return server_fd;
}

// Acepta una conexión
int accept_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        printF("Error al aceptar la conexión\n");
        return -1;
    }

    char *client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = client_addr.sin_port;

    char *msg = NULL;
    asprintf(&msg, "Nueva conexión aceptada desde IP: %s, Puerto: %d\n", client_ip, client_port);
    printF(msg);
    free(msg);

    return client_fd;
}
