// FileReader.c
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "FileReader.h"

// Funció per llegir descripcions del fitxer fins a trobar un caràcter específic o el final del fitxer
/***********************************************
* @Finalitat: Llegeix caràcters d'un fitxer descriptor fins que es troba un caràcter específic (cEnd) o el final del fitxer.
* @Paràmetres:
*   in: fd = descriptor del fitxer des del qual es vol llegir.
*   in: cEnd = caràcter fins al qual es vol llegir.
* @Retorn: Retorna un punter a una cadena de caràcters llegida o NULL en cas d'error o EOF.
************************************************/
char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;

    while (1) {
        chars_read = read(fd, &c, sizeof(char));
        if (chars_read == 0) {
            if (i == 0) {
                return NULL;
            }
            break;
        } else if (chars_read < 0) {
            free(buffer);
            return NULL;
        }

        if (c == cEnd) {
            break;
        }

        buffer = (char *)realloc(buffer, i + 2);
        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return buffer;
}
