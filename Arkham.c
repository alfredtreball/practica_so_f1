/***********************************************
* @Fitxer: Arkham.c
* @Descripció: Logger basat exclusivament en file descriptors.
* Llegeix per stdin (redirigit per pipe) i escriu en fitxer de log.
************************************************/

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#define LOG_FILE "logs.txt"
#define BUFFER_SIZE 512

// Escriu missatge amb timestamp a file descriptor
void write_log_entry(int fd_log, const char *message) {
    char log_entry[BUFFER_SIZE + 128];
    char timestamp[64];
    time_t now = time(NULL);
    struct tm tm_info;

    localtime_r(&now, &tm_info);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &tm_info);

    // Compose log message
    int len = snprintf(log_entry, sizeof(log_entry), "%s %s\n", timestamp, message);
    if (len > 0) {
        write(fd_log, log_entry, len);
    }
}

int main() {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char *newline;

    // Obre fitxer de log en mode append, amb permisos rw-r--r--
    int fd_log = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_log < 0) {
        _exit(EXIT_FAILURE);
    }

    // Llegeix de stdin (pipe del pare)
    while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';

        // Elimina possible salt de línia
        newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
        }

        write_log_entry(fd_log, buffer);
    }

    close(fd_log);
    return 0;
}