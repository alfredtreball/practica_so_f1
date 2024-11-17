#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "Networking.h"
#include "StringUtils.h" // trim()

int connect_to_server(const char *ip, int port) {
    if (ip == NULL) {
        printF("Error: IP NULL\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creando el socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    printF("Conectando a la IP: ");
    printF(ip);
    printF("\n");

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("IP no válida");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error conectando al servidor");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int startServer(const char *ip, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error creando el socket del servidor");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("IP no válida");
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Error en el listen");
        close(server_fd);
        return -1;
    }

    printF("Servidor escuchando en ");
    printF(ip);
    printF("\n");

    return server_fd;
}

int accept_connection(int server_fd) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Error aceptando la conexión");
        return -1;
    }
    return client_fd;
}

int send_frame(int socket_fd, const char *data, int data_length) {
    char frame[FRAME_SIZE] = {0};
    char checksum[CHECKSUM_SIZE];
    char timestamp[TIMESTAMP_SIZE];

    uint16_t checksum_value = calculate_checksum(data, data_length);
    snprintf(checksum, CHECKSUM_SIZE, "%04x", checksum_value);
    get_timestamp(timestamp);

    snprintf(frame, FRAME_SIZE, "%d|%s|%s|%s", data_length, data, checksum, timestamp);

    if (write(socket_fd, frame, FRAME_SIZE) < 0) {
        perror("Error enviando el frame");
        return -1;
    }

    return 0;
}

uint16_t calculate_checksum(const char *data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }
    return (uint16_t)(sum % CHECKSUM_MODULO);
}

int receive_frame(int socket_fd, char *data, int *data_length) {
    char frame[FRAME_SIZE] = {0};
    char received_checksum[CHECKSUM_SIZE];
    char timestamp[TIMESTAMP_SIZE];

    if (read(socket_fd, frame, FRAME_SIZE) <= 0) {
        printF("Error recibiendo el frame\n");
        return -1;
    }

    sscanf(frame, "%d|%[^|]|%[^|]|%s", data_length, data, received_checksum, timestamp);

    uint16_t calculated_checksum = calculate_checksum(data, *data_length);
    char calculated_checksum_str[CHECKSUM_SIZE];
    snprintf(calculated_checksum_str, CHECKSUM_SIZE, "%04x", calculated_checksum);

    if (strcmp(calculated_checksum_str, received_checksum) != 0) {
        printF("Error: checksum inválido\n");
        return -1;
    }

    return 0;
}

void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", t);
}
