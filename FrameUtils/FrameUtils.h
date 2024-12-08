#ifndef FRAME_UTILS_H
#define FRAME_UTILS_H

#include <stdint.h>
#include <stddef.h>

#define FRAME_SIZE 256
#define DATA_MAX_SIZE (FRAME_SIZE - 9) // 256 - 1(TYPE) - 2(DATA_LENGTH) - 2(CHECKSUM) - 4(TIMESTAMP)
#define TIMESTAMP_SIZE 64
#define CHECKSUM_MODULO 65536

#define printF(x) write(1, x, strlen(x))

// Estructura de un frame
typedef struct {
    uint8_t type;                      
    uint16_t data_length;              
    char data[240];          
    uint16_t checksum;                 
    uint32_t timestamp;                
} Frame;

// Funciones para manejar frames
void serialize_frame(const Frame *frame, char *buffer);
int deserialize_frame(const char *buffer, Frame *frame);
int send_frame(int socket_fd, const Frame *frame);
int receive_frame(int socket_fd, Frame *frame);
uint16_t calculate_checksum(const char *data, size_t length, int include_null);
void get_timestamp(char *timestamp);

#endif // FRAME_UTILS_H
