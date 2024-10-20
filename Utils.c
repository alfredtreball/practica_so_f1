/***********************************************
* @Fitxer: Utils.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer conté funcions utilitàries generals per a la manipulació
* de cadenes de caràcters i per a la gestió de fitxers. Les funcions inclouen la lectura
* de dades d'un fitxer fins a un caràcter específic, la verificació de sufixos en cadenes, 
* l'eliminació d'espais en blanc a l'inici i al final de les cadenes, i l'eliminació 
* de caràcters específics dins de les cadenes.
************************************************/
#include "Utils.h"

// Funció per llegir descripcions del fitxer fins a trobar un caràcter específic o el final del fitxer
/***********************************************
* @Finalitat: Llegeix caràcters d'un fitxer descriptor fins que es troba un caràcter específic (cEnd) o el final del fitxer.
* @Paràmetres:
*   in: fd = descriptor del fitxer des del qual es vol llegir.
*   in: cEnd = caràcter fins al qual es vol llegir.
* @Retorn: Retorna un punter a una cadena de caràcters llegida o NULL en cas d'error o EOF.
************************************************/
char *readUntil(int fd, char cEnd) {
    int i = 0; // Índex per a la posició actual en el buffer
    ssize_t chars_read; // Nombre de caràcters llegits
    char c = 0; // Caràcter llegit
    char *buffer = NULL; // Punter al buffer de lectura

    // Bucle per llegir caràcter per caràcter
    while (1) {
        chars_read = read(fd, &c, sizeof(char)); // Llegeix un caràcter del fitxer
        if (chars_read == 0) { // Si s'ha arribat al final del fitxer
            if (i == 0) { // Si no s'han llegit caràcters, retorna NULL
                return NULL;
            }
            break; // Finalitza la lectura
        } else if (chars_read < 0) { // Si hi ha un error en la lectura
            free(buffer); // Allibera la memòria assignada
            return NULL; // Retorna NULL
        }

        if (c == cEnd) { // Si s'ha trobat el caràcter de finalització
            break; // Finalitza la lectura
        }

        buffer = (char *)realloc(buffer, i + 2); // Redimensiona el buffer per acomodar un nou caràcter
        buffer[i++] = c; // Afegeix el caràcter llegit al buffer
    }

    buffer[i] = '\0'; // Afegeix el caràcter de terminació de cadena
    return buffer; // Retorna el buffer amb el contingut llegit
}

// Funció per verificar si una cadena de text acaba amb un sufix determinat
/***********************************************
* @Finalitat: Comprova si la cadena 'str' acaba amb el sufix 'suffix'.
* @Paràmetres:
*   in: str = cadena de text original.
*   in: suffix = sufix a comprovar.
* @Retorn: Retorna 1 si la cadena acaba amb el sufix, en cas contrari retorna 0.
************************************************/
int endsWith(char *str, char *suffix) {
    if (!str || !suffix) return 0; // Retorna 0 si algun dels paràmetres és NULL
    size_t lenStr = strlen(str); // Longitud de la cadena
    size_t lenSuffix = strlen(suffix); // Longitud del sufix
    if (lenSuffix > lenStr) return 0; // Retorna 0 si el sufix és més llarg que la cadena
    // Compara el final de la cadena amb el sufix
    return strncmp(str + lenStr - lenSuffix, suffix, lenSuffix) == 0;
}

// Funció per eliminar espais en blanc del començament i del final d'una cadena
/***********************************************
* @Finalitat: Elimina els espais en blanc que es troben al començament i al final de la cadena.
* @Paràmetres:
*   in: str = cadena de text a modificar.
* @Retorn: Retorna un punter a la cadena modificada (sense espais en blanc).
************************************************/
char *trim(char *str) {
    char *end; // Punter al final de la cadena

    // Elimina espais en blanc del començament
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) // Si la cadena està buida després de treure els espais
        return str;

    // Elimina espais en blanc del final
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0'; // Col·loca el caràcter de terminació de cadena

    return str; // Retorna la cadena sense espais en blanc
}

// Funció per eliminar totes les aparicions d'un caràcter específic en una cadena
/***********************************************
* @Finalitat: Elimina totes les aparicions del caràcter 'charToRemove' de la cadena 'string'.
* @Paràmetres:
*   in/out: string = cadena de text en la qual es vol eliminar el caràcter.
*   in: charToRemove = caràcter a eliminar.
* @Retorn: ----
************************************************/
void removeChar(char *string, char charToRemove) {
    char *origen, *dst; // Punter d'origen i de destinació

    // Bucle per recórrer la cadena
    for (origen = dst = string; *origen != '\0'; origen++) {
        *dst = *origen; // Copia el caràcter actual
        if (*dst != charToRemove) dst++; // Si el caràcter no és el que es vol eliminar, avança el punter de destinació
    }
    *dst = '\0'; // Afegeix el caràcter de terminació de cadena
}


char *intToStr(int num) {
    // Calcula la longitud necessària per emmagatzemar l'enter com a cadena
    int length = snprintf(NULL, 0, "%d", num);
    char *str = (char *)malloc(length + 1); // Reserva memòria per a la cadena
    if (str == NULL) {
        printF("Error reservant memòria per a intToStr\n");
        exit(1);
    }
    snprintf(str, length + 1, "%d", num); // Converteix l'enter a cadena
    return str;
}