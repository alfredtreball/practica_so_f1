// DataConversion.c
#include <stdio.h>
#include <stdlib.h>
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
