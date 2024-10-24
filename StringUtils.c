// StringUtils.c
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "StringUtils.h"

/***********************************************
* @Finalitat: Elimina els espais en blanc que es troben al començament i al final de la cadena.
* @Paràmetres:
*   in: str = cadena de text a modificar.
* @Retorn: Retorna un punter a la cadena modificada (sense espais en blanc).
************************************************/
char *trim(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++;
    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    return str;
}

/***********************************************
* @Finalitat: Elimina totes les aparicions del caràcter 'charToRemove' de la cadena 'string'.
* @Paràmetres:
*   in/out: string = cadena de text en la qual es vol eliminar el caràcter.
*   in: charToRemove = caràcter a eliminar.
* @Retorn: ----
************************************************/
void removeChar(char *string, char charToRemove) {
    char *src, *dst;

    for (src = dst = string; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != charToRemove) dst++;
    }
    *dst = '\0';
}

/***********************************************
* @Finalitat: Comprova si la cadena 'str' acaba amb el sufix 'suffix'.
* @Paràmetres:
*   in: str = cadena de text original.
*   in: suffix = sufix a comprovar.
* @Retorn: Retorna 1 si la cadena acaba amb el sufix, en cas contrari retorna 0.
************************************************/
int endsWith(char *str, char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenStr = strlen(str);
    size_t lenSuffix = strlen(suffix);
    if (lenSuffix > lenStr) return 0;
    return strncmp(str + lenStr - lenSuffix, suffix, lenSuffix) == 0;
}
