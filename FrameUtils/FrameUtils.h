#ifndef FRAMEUTILS_H
#define FRAMEUTILS_H

#include <stdint.h>
#include <stddef.h>

#define FRAME_SIZE 256
#define CHECKSUM_SIZE 32
#define TIMESTAMP_SIZE 64
#define CHECKSUM_MODULO 65536

#define printF(x) write(1, x, strlen(x))

// Estructura del Frame
typedef struct {
    uint8_t type;
    uint16_t data_length;
    char data[240]; // Ajustado al espacio restante
    uint16_t checksum;
    uint32_t timestamp;
} Frame;

// Funciones para serializar y deserializar frames
void serialize_frame(const Frame *frame, char *buffer);
int deserialize_frame(const char *buffer, Frame *frame);

// Funciones para enviar y recibir frames a través de sockets
int send_frame(int socket_fd, const Frame *frame);
int receive_frame(int socket_fd, Frame *frame);

// Funciones auxiliares
uint16_t calculate_checksum(const char *data, size_t length);
void get_timestamp(char *timestamp);

#endif // FRAMEUTILS_H
