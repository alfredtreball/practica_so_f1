#include "Networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "StringUtils.h"

int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creant el socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Afegeix aquesta línia per mostrar l'adreça IP abans de `inet_pton`
    printF("Adreça IP per connectar: '");
    printF(ip);
    printF("'\n");
    
    char lengthStr[20];
    snprintf(lengthStr, 20, "%ld", strlen(ip));
    printF("Longitud de l'adreça IP: ");
    printF(lengthStr);
    printF("\n"); // Verificar la longitud de la cadena

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Adreça IP no vàlida o error en inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connectant al servidor");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Crea un socket servidor per escoltar connexions
int start_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error creant el socket del servidor");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el bind del servidor");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Error en el listen del servidor");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

// Accepta una connexió entrant
int accept_connection(int server_fd) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Error acceptant la connexió");
        return -1;
    }
    return client_fd;
}

// Envia una trama segons el protocol especificat
int send_frame(int socket_fd, const char *data, int data_length) {
    char frame[FRAME_SIZE];
    memset(frame, 0, FRAME_SIZE);
    char checksum[CHECKSUM_SIZE];
    char timestamp[TIMESTAMP_SIZE];

    // Calcular el checksum i el timestamp
    calculate_checksum(data, data_length, checksum);
    get_timestamp(timestamp);

    // Preparar la trama
    snprintf(frame, FRAME_SIZE, "%s|%d|%s|%s", timestamp, data_length, checksum, data);

    // Enviar la trama
    if (write(socket_fd, frame, FRAME_SIZE) < 0) {
        perror("Error enviant la trama");
        return -1;
    }

    return 0;
}

// Rep una trama del socket
int receive_frame(int socket_fd, char *buffer) {
    if (read(socket_fd, buffer, FRAME_SIZE) < 0) {
        perror("Error rebent la trama");
        return -1;
    }
    return 0;
}

// Calcula el checksum d'una cadena de caràcters
int calculate_checksum(const char *data, int data_length, char *checksum) {
    unsigned int sum = 0;
    for (int i = 0; i < data_length; i++) {
        sum += data[i];
    }
    snprintf(checksum, CHECKSUM_SIZE, "%08x", sum);
    return 0;
}

// Obté el timestamp actual
void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", t);
}
