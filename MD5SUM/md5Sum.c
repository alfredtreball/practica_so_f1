#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include "md5Sum.h"

void calculate_md5(const char *filePath, char *md5Sum) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        write(STDERR_FILENO, "Error creando pipe para MD5\n", 29);
        strcpy(md5Sum, "ERROR");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Proceso hijo: ejecuta md5sum y redirige la salida al pipe
        close(pipe_fd[0]); // Cerrar extremo de lectura
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]); // Cerrar el descriptor duplicado
        execlp("md5sum", "md5sum", filePath, NULL);
        write(STDERR_FILENO, "Error ejecutando md5sum\n", 24);
        _exit(1); // Salida con error si execlp falla
    } else if (pid > 0) {
        // Proceso padre: lee la salida de md5sum desde el pipe
        close(pipe_fd[1]); // Cerrar extremo de escritura
        char buffer[64] = {0};
        ssize_t bytesRead = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        close(pipe_fd[0]); // Cerrar el extremo de lectura

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // Asegurar nulo-terminado
            if (sscanf(buffer, "%32s", md5Sum) != 1) {
                write(STDERR_FILENO, "Error procesando salida de md5sum\n", 34);
                strcpy(md5Sum, "ERROR");
            }
        } else {
            write(STDERR_FILENO, "Error leyendo salida de md5sum\n", 31);
            strcpy(md5Sum, "ERROR");
        }

        wait(NULL); // Esperar a que el proceso hijo termine
    } else {
        // Error al crear el proceso hijo
        write(STDERR_FILENO, "Error creando proceso hijo para MD5\n", 36);
        strcpy(md5Sum, "ERROR");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    }
}

