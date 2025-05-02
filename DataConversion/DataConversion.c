// DataConversion.c
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Para vsnprintf
#include "DataConversion.h"

// Función para convertir un entero en una cadena de texto
char *intToStr(int num) {
    int length = snprintf(NULL, 0, "%d", num);
    char *str = (char *)malloc(length + 1);
    if (str == NULL) {
        exit(1);
    }
    snprintf(str, length + 1, "%d", num);
    return str;
}

// Función para imprimir texto sin usar printf (dinámico)
void customPrintf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Obtener el tamaño necesario
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (size < 0) {
        return; // Error en el formato
    }

    // Reservar memoria para el mensaje
    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        return; // Error de asignación
    }

    // Escribir el formato en el buffer
    va_start(args, format);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    // Escribir en stdout
    write(STDOUT_FILENO, buffer, size);

    // Liberar memoria
    free(buffer);
}
