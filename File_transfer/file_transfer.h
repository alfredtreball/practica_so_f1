#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

void send_file(int socket, const char *filePath);
void receive_file(int socket, const char *destinationPath);

#endif // FILE_TRANSFER_H
