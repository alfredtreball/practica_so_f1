#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "file_transfer.h"

void send_file(int socket, const char *filePath) {
    int file = open(filePath, O_RDONLY);
    if (file < 0) {
        write(STDERR_FILENO, "Error abriendo archivo para enviar\n", 35);
        return;
    }

    char buffer[256];
    ssize_t bytesRead;
    while ((bytesRead = read(file, buffer, sizeof(buffer))) > 0) {
        write(socket, buffer, bytesRead);
    }

    close(file);
}

void receive_file(int socket, const char *destinationPath) {
    int file = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file < 0) {
        write(STDERR_FILENO, "Error creando archivo para recibir\n", 35);
        return;
    }

    char buffer[256];
    ssize_t bytesRead;
    while ((bytesRead = read(socket, buffer, sizeof(buffer))) > 0) {
        write(file, buffer, bytesRead);
    }

    close(file);
}
