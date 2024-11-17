#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdint.h>
#include <netinet/in.h>

#define FRAME_SIZE 256
#define CHECKSUM_SIZE 32
#define TIMESTAMP_SIZE 20
#define CHECKSUM_MODULO 65536

typedef struct {
    uint8_t type;
    uint16_t data_length;
    char data[240];
    uint16_t checksum;
    uint32_t timestamp;
} Frame;

int deserialize_frame(const char *buffer, Frame *frame);
void serialize_frame(const Frame *frame, char *buffer);
uint16_t calculate_frame_checksum(const Frame *frame);

int connect_to_server(const char *ip, int port);
int startServer(const char *ip, int port);
int accept_connection(int server_fd);

int send_frame(int socket_fd, const char *data, int data_length);
int receive_frame(int socket_fd, char *data, int *data_length);

uint16_t calculate_checksum(const char *data, size_t length);
void get_timestamp(char *timestamp);

#endif // NETWORKING_H
