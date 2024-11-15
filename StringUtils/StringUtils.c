// StringUtils.c
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "StringUtils.h"

/***********************************************
* @Finalitat: Verifica si l'extensió del fitxer és compatible amb el tipus de worker.
* @Paràmetres:
*   in: filename = nom del fitxer.
*   in: workerType = tipus de worker ("media" o "text").
* @Retorn: Retorna 1 si el tipus de fitxer és compatible, en cas contrari retorna 0.
************************************************/
int esTipoValido(const char *filename, const char *workerType) {
    const char *extension = strrchr(filename, '.');
    if (extension == NULL) {
        return 0;
    }

    if (strcasecmp(workerType, "media") == 0) {
        return (strcasecmp(extension, ".wav") == 0 || 
                strcasecmp(extension, ".jpg") == 0 || 
                strcasecmp(extension, ".png") == 0);
    } else if (strcasecmp(workerType, "text") == 0) {
        return (strcasecmp(extension, ".txt") == 0);
    }

    return 0;
}

// Funció per "trimejar" espais en blanc i salts de línia
void trimCommand(char *command) {
    char *trimmed = trim(command); // Elimina espais al principi i al final
    if (trimmed != command) {
        memmove(command, trimmed, strlen(trimmed) + 1);
    }

    // Elimina caràcters no desitjats com '\n' explícitament
    removeChar(command, '\n');
    removeChar(command, '\r');
}

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
