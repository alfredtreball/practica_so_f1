#include "FrameUtilsBinary.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Serializa un frame binario
void serialize_frame_binary(const BinaryFrame *frame, char *buffer) {
    if (!frame || !buffer) return;

    memset(buffer, 0, FRAME_BINARY_SIZE);

    // Serializar TYPE
    memcpy(buffer, &frame->type, sizeof(frame->type));
    // Serializar DATA_LENGTH
    memcpy(buffer + 1, &frame->data_length, sizeof(frame->data_length));
    // Serializar DATA (relleno si no llega a 247 Bytes)
    memcpy(buffer + 3, frame->data, frame->data_length);
    // Agregar bytes de relleno si la trama no alcanza los 247 Bytes
    if (frame->data_length < DATA_BINARY_MAX_SIZE) {
        memset(buffer + 3 + frame->data_length, 0, DATA_BINARY_MAX_SIZE - frame->data_length);
    }
    // Serializar CHECKSUM
    memcpy(buffer + 3 + DATA_BINARY_MAX_SIZE, &frame->checksum, sizeof(frame->checksum));
    // Serializar TIMESTAMP
    memcpy(buffer + 3 + DATA_BINARY_MAX_SIZE + 2, &frame->timestamp, sizeof(frame->timestamp));
}


// Deserializa un frame binario
int deserialize_frame_binary(const char *buffer, BinaryFrame *frame) {
    if (!buffer || !frame) return -1;

    memset(frame, 0, sizeof(BinaryFrame));

    // Deserializar TYPE
    memcpy(&frame->type, buffer, sizeof(frame->type));
    // Deserializar DATA_LENGTH
    memcpy(&frame->data_length, buffer + 1, sizeof(frame->data_length));

    // Validar longitud de datos
    if (frame->data_length > DATA_BINARY_MAX_SIZE) {
        fprintf(stderr, "[ERROR][Deserialize] Longitud de datos excede el máximo permitido\n");
        return -1;
    }

    // Deserializar DATA (solo los datos útiles, ignorando el relleno)
    memcpy(frame->data, buffer + 3, frame->data_length);
    // Deserializar CHECKSUM
    memcpy(&frame->checksum, buffer + 3 + DATA_BINARY_MAX_SIZE, sizeof(frame->checksum));
    // Deserializar TIMESTAMP
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
    ssize_t bytesRead = read(socket_fd, buffer, FRAME_BINARY_SIZE);

    if (bytesRead < 0) {
        perror("[ERROR][ReceiveFrame] Error recibiendo el frame");
        return -1;
    } else if (bytesRead != FRAME_BINARY_SIZE) {
        fprintf(stderr, "[ERROR][ReceiveFrame] Frame incompleto recibido. Bytes leídos: %zd\n", bytesRead);
        return -1;
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

    return (uint16_t)(sum % CHECKSUM_BINARY_MODULO);
}