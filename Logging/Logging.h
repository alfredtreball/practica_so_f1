#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <unistd.h>  // Para write()
#include <string.h>  // Para strlen()

// Colores de terminal para los logs
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"

// Macro para imprimir directamente
#define printF(x) write(1, x, strlen(x))

// Funciones para loggear mensajes
void logInfo(const char *msg);
void logWarning(const char *msg);
void logError(const char *msg);
void logSuccess(const char *msg);

#endif // LOGGING_H
