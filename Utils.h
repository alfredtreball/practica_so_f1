#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Funciones de utilidad general
char *readUntil(int fd, char cEnd);
char* separarParaules(char* string, const char* delimiter, char** context);
void printF(const char *str);
int endsWith(char *str, char *suffix);
char *trim(char *str);
void removeChar(char *string, char charToRemove);

#endif // UTILS_H
