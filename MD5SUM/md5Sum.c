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
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        execlp("md5sum", "md5sum", filePath, NULL);
        _exit(1); // Si execlp falla
    } else if (pid > 0) {
        // Proceso padre: lee la salida de md5sum desde el pipe
        close(pipe_fd[1]);
        char buffer[64] = {0};
        read(pipe_fd[0], buffer, sizeof(buffer));
        close(pipe_fd[0]);

        // Extraer el MD5SUM de la salida de md5sum
        sscanf(buffer, "%32s", md5Sum);
        wait(NULL); // Esperar a que el proceso hijo termine
    } else {
        write(STDERR_FILENO, "Error creando proceso hijo para MD5\n", 36);
        strcpy(md5Sum, "ERROR");
    }
}
