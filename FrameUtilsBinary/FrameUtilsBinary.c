#include "FrameUtilsBinary.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Serializa un frame binario
void serialize_frame_binary(const BinaryFrame *frame, char *buffer) {
    if (!frame || !buffer) return;

    memset(buffer, 0, FRAME_BINARY_SIZE);

    // Serializar campos
    memcpy(buffer, &frame->type, sizeof(frame->type));
    memcpy(buffer + 1, &frame->data_length, sizeof(frame->data_length));
    memcpy(buffer + 3, frame->data, frame->data_length);

    if (frame->data_length < DATA_BINARY_MAX_SIZE) {
        memset(buffer + 3 + frame->data_length, 0, DATA_BINARY_MAX_SIZE - frame->data_length);
    }

    memcpy(buffer + 3 + DATA_BINARY_MAX_SIZE, &frame->checksum, sizeof(frame->checksum));
    memcpy(buffer + 3 + DATA_BINARY_MAX_SIZE + 2, &frame->timestamp, sizeof(frame->timestamp));
}

// Deserializa un frame binario
int deserialize_frame_binary(const char *buffer, BinaryFrame *frame) {
    if (!buffer || !frame) return -1;

    memset(frame, 0, sizeof(BinaryFrame));
    memcpy(&frame->type, buffer, sizeof(frame->type));
    memcpy(&frame->data_length, buffer + 1, sizeof(frame->data_length));

    if (frame->data_length > DATA_BINARY_MAX_SIZE) {
        fprintf(stderr, "[ERROR][Deserialize] Longitud de datos excede el máximo permitido\n");
        return -1;
    }

    memcpy(frame->data, buffer + 3, frame->data_length);
    memcpy(&frame->checksum, buffer + 3 + DATA_BINARY_MAX_SIZE, sizeof(frame->checksum));
    memcpy(&frame->timestamp, buffer + 3 + DATA_BINARY_MAX_SIZE + 2, sizeof(frame->timestamp));
    return 0;
}


// Envía un frame binario
int send_frame_binary(int socket_fd, const BinaryFrame *frame) {
    if (!frame) return -1;

    char buffer[FRAME_BINARY_SIZE];
    serialize_frame_binary(frame, buffer);

    if (write(socket_fd, buffer, FRAME_BINARY_SIZE) < 0) {
        perror("Error enviando el frame");
        return -1;
    }

    return 0;
}

// Recibe un frame binario
int receive_frame_binary(int socket_fd, BinaryFrame *frame) {
    if (!frame) return -1;

    char buffer[FRAME_BINARY_SIZE];
    size_t totalBytesRead = 0;

    while (totalBytesRead < FRAME_BINARY_SIZE) {
        ssize_t bytesRead = read(socket_fd, buffer + totalBytesRead, FRAME_BINARY_SIZE - totalBytesRead);
        if (bytesRead <= 0) {
            perror("[ERROR][ReceiveFrame] Error recibiendo el frame");
            return -1;
        }
        totalBytesRead += bytesRead;
    }

    return deserialize_frame_binary(buffer, frame);
}

// Calcula el checksum binario
uint16_t calculate_checksum_binary(const char *data, size_t length, int include_null) {
    uint32_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }

    if (include_null && length < DATA_BINARY_MAX_SIZE) {
        sum += '\0';
    }

    uint16_t checksum = (uint16_t)(sum % CHECKSUM_BINARY_MODULO);
    return checksum;
}
