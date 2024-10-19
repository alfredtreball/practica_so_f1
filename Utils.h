#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>


#define printF(x) write(1, x, strlen(x))

// Funciones de utilidad general
char *readUntil(int fd, char cEnd);
int endsWith(char *str, char *suffix);
char *trim(char *str);
void removeChar(char *string, char charToRemove);

#endif // UTILS_H
