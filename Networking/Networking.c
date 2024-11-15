#include "Networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "StringUtils.h"

int connect_to_server(const char *ip, int port) {
    if (ip == NULL) {
        printF("Error: IP NULL\n");
        return -1;
    }

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

    // Convertir la IP especificada en formato binario
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Error en inet_pton (IP no válida)");
        close(server_fd);
        return -1;
    }

    // Asocia el socket a la IP y puerto especificados
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el bind del servidor");
        close(server_fd);
        return -1;
    }

    // Poner el socket en modo escucha
    if (listen(server_fd, 5) < 0) {
        perror("Error en el listen del servidor");
        close(server_fd);
        return -1;
    }

    printf("Servidor escuchando en %s:%d\n", ip, port);
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
    snprintf(frame, FRAME_SIZE, "%d|%s|%s|%s", data_length, data, checksum, timestamp);

    // Enviar la trama
    if (write(socket_fd, frame, FRAME_SIZE) < 0) {
        perror("Error enviant la trama");
        return -1;
    }

    return 0;
}

// Rep una trama del socket 
int receive_frame(int socket_fd, char *data, int *data_length) {
    char frame[FRAME_SIZE];                // Emmagatzema la trama completa rebuda des del socket
    char received_checksum[CHECKSUM_SIZE]; // Emmagatzema el checksum rebut en la trama
    char timestamp[TIMESTAMP_SIZE];        // Emmagatzema el timestamp rebut en la trama
    char *token;                           // Punter per dividir la trama en tokens
    int received_length;                   // Variable temporal per a la longitud de les dades rebudes

    // Llegeix la trama completa des del socket
    if (read(socket_fd, frame, FRAME_SIZE) <= 0) {
        printF("Error en rebre la trama\n"); // Missatge d'error si la lectura falla
        return -1;                          
    }

    token = strtok(frame, "|"); //Camp DATA_LENGTH
    if (token == NULL) return -1;
    received_length = atoi(token); // Converteix DATA_LENGTH de cadena a enter
    *data_length = received_length; // Emmagatzema la longitud de les dades en el punter rebut

    token = strtok(NULL, "|"); //Camp DATA
    if (token == NULL) return -1;
    strncpy(data, token, received_length);    // Copia DATA en el buffer de dades
    data[received_length] = '\0';             

    token = strtok(NULL, "|"); // Tercer camp: CHECKSUM
    if (token == NULL) return -1;
    strncpy(received_checksum, token, CHECKSUM_SIZE); // Copia CHECKSUM en received_checksum
    received_checksum[CHECKSUM_SIZE - 1] = '\0';     

    token = strtok(NULL, "|"); // Quart camp: TIMESTAMP
    if (token == NULL) return -1;
    strncpy(timestamp, token, TIMESTAMP_SIZE); // Copia TIMESTAMP en la variable timestamp
    timestamp[TIMESTAMP_SIZE - 1] = '\0';     

    //Validació del checksum
    char calculated_checksum[CHECKSUM_SIZE];
    calculate_checksum(data, received_length, calculated_checksum); // Calcula el checksum del DATA
    if (strcmp(calculated_checksum, received_checksum) != 0) { // Compara el checksum calculat i rebut
        printF("Error: Checksum invàlid en la trama\n");      
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
