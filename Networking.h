#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>

// Constants per al protocol
#define FRAME_SIZE 256
#define CHECKSUM_SIZE 32
#define TIMESTAMP_SIZE 20

// Funcions per a la gestió de connexions
int connect_to_server(const char *ip, int port);
int startServer(const char *ip, int port);
int accept_connection(int server_fd);

// Funcions per a l'enviament i recepció de trames
int send_frame(int socket_fd, const char *data, int data_length);
int receive_frame(int socket_fd, char *buffer);

// Funcions auxiliars
int calculate_checksum(const char *data, int data_length, char *checksum);
void get_timestamp(char *timestamp);

#endif // NETWORKING_H
