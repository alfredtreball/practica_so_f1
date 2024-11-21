#include "FrameUtils.h"
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

// Serializa un frame en un buffer
void serialize_frame(const Frame *frame, char *buffer) {
    if (!frame || !buffer) return;

    memset(buffer, 0, FRAME_SIZE);
    snprintf(buffer, FRAME_SIZE, "%02x|%04x|%u|%04x|%s",
             frame->type, frame->data_length, frame->timestamp,
             frame->checksum, frame->data);
}

// Deserializa un buffer en un frame
int deserialize_frame(const char *buffer, Frame *frame) {
    if (!buffer || !frame) return -1;

    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0'; // Asegurar terminación nula

    char *token = strtok(localBuffer, "|");
    if (!token) return -1;
    frame->type = (uint8_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->data_length = (uint16_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->timestamp = (uint32_t)strtoul(token, NULL, 10);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->checksum = (uint16_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (!token) return -1;
    strncpy(frame->data, token, frame->data_length);
    frame->data[frame->data_length] = '\0'; // Terminación nula

    return 0; // Éxito
}

// Envía un frame a través de un socket
int send_frame(int socket_fd, const Frame *frame) {
    if (!frame) return -1;

    char buffer[FRAME_SIZE];
    serialize_frame(frame, buffer);

    if (write(socket_fd, buffer, FRAME_SIZE) < 0) {
        perror("Error enviando el frame");
        return -1;
    }

    return 0;
}

// Recibe un frame desde un socket
int receive_frame(int socket_fd, Frame *frame) {
    if (!frame) return -1;

    char buffer[FRAME_SIZE];
    if (read(socket_fd, buffer, FRAME_SIZE) <= 0) {
        perror("Error recibiendo el frame");
        return -1;
    }

    if (deserialize_frame(buffer, frame) != 0) {
        printF("Error deserializando el frame recibido\n");
        return -1;
    }

    return 0;
}

// Calcula el checksum de un conjunto de datos
uint16_t calculate_checksum(const char *data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }
    return (uint16_t)(sum % CHECKSUM_MODULO);
}

// Obtiene el timestamp actual
void get_timestamp(char *timestamp) {
    if (!timestamp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(timestamp, TIMESTAMP_SIZE, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}
