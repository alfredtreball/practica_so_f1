#ifndef FRAME_UTILS_BINARY_H
#define FRAME_UTILS_BINARY_H

#include <stdint.h>
#include <stddef.h>

#define FRAME_BINARY_SIZE 256
#define DATA_BINARY_MAX_SIZE (FRAME_BINARY_SIZE - 9) // Tamaño de datos máximo (247 bytes)
#define CHECKSUM_BINARY_MODULO 65536

typedef struct {
    uint8_t type;
    uint16_t data_length;
    uint32_t timestamp;
    uint16_t checksum;
    char data[DATA_BINARY_MAX_SIZE];
} BinaryFrame;

// Funciones de serialización y deserialización
void serialize_frame_binary(const BinaryFrame *frame, char *buffer);
int deserialize_frame_binary(const char *buffer, BinaryFrame *frame);

// Funciones de envío y recepción binaria
int send_frame_binary(int socket_fd, const BinaryFrame *frame);
int receive_frame_binary(int socket_fd, BinaryFrame *frame);
int receive_any_frame(int socket_fd, void *frame, int *is_binary);

// Calcula el checksum para un frame binario
uint16_t calculate_checksum_binary(const char *data, size_t length, int include_null);

#endif
