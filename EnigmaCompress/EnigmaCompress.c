#define _GNU_SOURCE // Necesario para funciones GNU como asprintf

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "../DataConversion/DataConversion.h"

int is_delimiter(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

int compress_text_file(const char *filePath, int threshold) {
    if (!filePath || threshold <= 0) {
        customPrintf("[ERROR]: compress_text_file recibió parámetros inválidos.");
        return -1;
    }

    int fd_in = open(filePath, O_RDONLY);
    if (fd_in < 0) {
        customPrintf("[ERROR]: No se pudo abrir el archivo original.");
        return -1;
    }

    char *tempPath = NULL;
    if (asprintf(&tempPath, "%s.tmp", filePath) == -1) {
        customPrintf("[ERROR]: No se pudo generar el nombre del archivo temporal.");
        close(fd_in);
        return -1;
    }

    int fd_out = open(tempPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_out < 0) {
        customPrintf("[ERROR]: No se pudo crear el archivo temporal.");
        close(fd_in);
        free(tempPath);
        return -1;
    }

    char *buffer = malloc(1024);
    char *word = malloc(512);
    if (!buffer || !word) {
        customPrintf("[ERROR]: Fallo de memoria.");
        close(fd_in);
        close(fd_out);
        free(tempPath);
        free(buffer);
        free(word);
        return -1;
    }

    ssize_t bytesRead;
    ssize_t word_len = 0;

    while ((bytesRead = read(fd_in, buffer, 1024)) > 0) {
        for (ssize_t i = 0; i < bytesRead; ++i) {
            char c = buffer[i];
            if (!is_delimiter(c)) {
                if (word_len < 511) {
                    word[word_len++] = c;
                }
            } else {
                if (word_len >= threshold) {
                    write(fd_out, word, word_len);
                    write(fd_out, &c, 1);  // conservar separador
                } else if (c == '\n' || c == '\r') {
                    write(fd_out, &c, 1);  // conservar saltos de línea
                }
                word_len = 0;
            }
        }
    }

    // Si hay una palabra final sin separador
    if (word_len >= threshold) {
        write(fd_out, word, word_len);
    }

    close(fd_in);
    close(fd_out);
    free(buffer);
    free(word);

    if (unlink(filePath) != 0 || rename(tempPath, filePath) != 0) {
        customPrintf("[ERROR]: No se pudo reemplazar el archivo original.");
        unlink(tempPath);
        free(tempPath);
        return -1;
    }

    free(tempPath);
    customPrintf("[SUCCESS]: Compresión de texto completada correctamente.");
    return 0;
}
