#ifndef FLECK_H
#define FLECK_H

#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h> // Per a isspace
#include <dirent.h> // Per treballar amb directoris

#define printF(x) write(1, x, strlen(x)) // Macro per escriure missatges

// Estructura de configuració de Fleck
typedef struct {
    char *user;
    char *directory;
    char *ipGotham;
    int portGotham;
} FleckConfig;

// Funcions per la gestió de fitxers i configuració
int endsWith(char *str, char *suffix);
void listMedia(char *directory);
void listText(char *directory);
char *trim(char *str);
char *readUntil(int fd, char cEnd);
void removeChar(char *string, char charToRemove);
void readConfigFile(const char *configFile, FleckConfig *fleckConfig);
char *separarParaules(char *string, const char *delimiter, char **context);
void processCommand(char *command /*char *directory*/);
void alliberarMemoria(FleckConfig *fleckConfig);

#endif // FLECK_H
