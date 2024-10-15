#define _GNU_SOURCE //asprintf OK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h> // Para isspace
#include <dirent.h> // Per treballar amb directoris


#define printF(x) write(1, x, strlen(x)) // Macro para escribir mensajes

typedef struct {
    char *user;
    char *directory;
    char *ipGotham;
    int portGotham;
} FleckConfig;

int endsWith(char *str, char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenStr = strlen(str);
    size_t lenSuffix = strlen(suffix);
    if (lenSuffix > lenStr) return 0;
    return strncmp(str + lenStr - lenSuffix, suffix, lenSuffix) == 0;
}

void listMedia(char *directory) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if ((dir = opendir(directory)) == NULL) {
        printF("Error obrint el directori\n");
        return;
    }

    char *output = NULL;  // Inicialitzem sense mida
    size_t output_len = 0;

    printF("Media files available:\n");

    while ((entry = readdir(dir)) != NULL) {
        if (endsWith(entry->d_name, ".wav") || endsWith(entry->d_name, ".png") || endsWith(entry->d_name, ".jpg")) {
            char *entry_str;
            int entry_len = asprintf(&entry_str, "%d. %s\n", ++count, entry->d_name);  // Generem l'string del fitxer

            if (entry_len == -1) {
                printF("Error generant el missatge\n");
                free(output);
                closedir(dir);
                return;
            }

            // Reassignem memòria per al nou missatge
            output = (char *)realloc(output, output_len + entry_len + 1);
            if (output == NULL) {
                printF("Error d'assignació de memòria\n");
                free(entry_str);
                closedir(dir);
                return;
            }

            // Afegim la nova línia al buffer de sortida
            memcpy(output + output_len, entry_str, entry_len);
            output_len += entry_len;
            output[output_len] = '\0';  // Assegurar-nos que el buffer estigui ben acabat

            free(entry_str);  // Alliberem l'string temporal
        }
    }

    if (count == 0) {
        printF("No media files found\n");
    } else {
        printF(output);
    }

    free(output);  // Alliberar memòria del buffer de sortida
    closedir(dir);
}

void listText(char *directory) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if ((dir = opendir(directory)) == NULL) {
        printF("Error obrint el directori\n");
        return;
    }

    char *output = NULL;  // Inicialitzem sense mida
    size_t output_len = 0;

    printF("Text files available:\n");

    while ((entry = readdir(dir)) != NULL) {
        if (endsWith(entry->d_name, ".txt")) {
            char *entry_str;
            int entry_len = asprintf(&entry_str, "%d. %s\n", ++count, entry->d_name);  // Generem l'string del fitxer

            if (entry_len == -1) {
                printF("Error generant el missatge\n");
                free(output);
                closedir(dir);
                return;
            }

            // Reassignem memòria per al nou missatge
            output = (char *)realloc(output, output_len + entry_len + 1);
            if (output == NULL) {
                printF("Error d'assignació de memòria\n");
                free(entry_str);
                closedir(dir);
                return;
            }

            // Afegim la nova línia al buffer de sortida
            memcpy(output + output_len, entry_str, entry_len);
            output_len += entry_len;
            output[output_len] = '\0';  // Assegurar-nos que el buffer estigui ben acabat

            free(entry_str);  // Alliberem l'string temporal
        }
    }

    if (count == 0) {
        printF("No text files found\n");
    } else {
        printF(output);
    }

    free(output);  // Alliberar memòria del buffer de sortida
    closedir(dir);
}


char *trim(char *str) {
    char *end;

    // Elimina espacios al principio
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // Todas las letras eran espacios
        return str;

    // Elimina espacios al final
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Añade el carácter nulo al final de la cadena
    *(end + 1) = '\0';

    return str;
}

// Función para leer hasta un carácter delimitador
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

        buffer = (char *)realloc(buffer, i + 2);  // Asigna más espacio
        buffer[i++] = c;                
    }

    buffer[i] = '\0';  // Finaliza la cadena con '\0'
    return buffer;
}

// Función para eliminar un char de una cadena
void removeChar(char *string, char charToRemove) {
    char *origen, *dst;
    for (origen = dst = string; *origen != '\0'; origen++) {
        *dst = *origen;
        if (*dst != charToRemove) dst++;
    }
    *dst = '\0';
}

// Función para procesar el archivo de configuración usando open, readUntil y memoria dinámica
void readConfigFile(const char *configFile, FleckConfig *fleckConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error abriendo el archivo de configuración\n");
        exit(1);
    }

    // Leer y asignar memoria para cada campo
    fleckConfig->user = readUntil(fd, '\n');
    removeChar(fleckConfig->user, '&'); // Elimina '&' del nombre del usuario
    fleckConfig->directory = trim(readUntil(fd, '\n')); // Elimina espacios del path del directorio
    fleckConfig->ipGotham = readUntil(fd, '\n');

    char *portStr = readUntil(fd, '\n');
    fleckConfig->portGotham = atoi(portStr);
    free(portStr);  // Libera memoria para portStr después de convertirla
    close(fd);

    // Muestra la configuración leída
    printF("User - ");
    printF(fleckConfig->user);
    printF("\nDirectory - ");
    printF(fleckConfig->directory);
    printF("\nIP - ");
    printF(fleckConfig->ipGotham);

    printF("\nPort - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d\n", fleckConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);
}

// Función para separar palabras sin usar strtok
char* separarParaules(char* string, const char* delimiter, char** context){
    char* posicio = NULL;

    if (string != NULL) {
        posicio = string;
    } else {
        if (*context == NULL) {
            return NULL;
        }
        posicio = *context;
    }

    while (*posicio && strchr(delimiter, *posicio)) {
        posicio++;
    }

    if (*posicio == '\0') {
        *context = NULL;
        return NULL;
    }

    char *iniciParaula = posicio;

    while (*posicio && !strchr(delimiter, *posicio)) {
        posicio++;
    }

    if (*posicio) {
        *posicio = '\0';
        posicio++;
    }

    *context = posicio;
    return iniciParaula;
}

void processCommand(char *command, char *directory) {
    char* context = NULL;
    char *cmd = separarParaules(command, " ", &context);  // Separa la primera paraula del comandament

    if (strcasecmp(cmd, "CONNECT") == 0) {
        printF("Comanda OK: CONNECT\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        printF("Comanda OK: LOGOUT\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = separarParaules(NULL, " ", &context);  // Segona part del comandament
        if (subCmd != NULL) {
            if (strcasecmp(subCmd, "MEDIA") == 0) {
                listMedia(directory);
            } else if (strcasecmp(subCmd, "TEXT") == 0) {
                listText(directory);
            } else {
                printF("Comanda KO: tipus de llista desconegut\n");
            }
        } else {
            printF("Comanda KO: falta especificar MEDIA o TEXT\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = separarParaules(NULL, " ", &context);  // Primer paràmetre
        char *factorStr = separarParaules(NULL, " ", &context);  // Segon paràmetre (factor)

        if (file != NULL && factorStr != NULL) {
            int factor = atoi(factorStr);  // Converteix el factor a enter
            if (factor > 0) {
                printF("Distorsion started!\n");
            } else {
                printF("Comanda KO: factor incorrecte\n");
            }
        } else {
            printF("Comanda KO: arguments incorrectes\n");
        }
    } else {
        printF("ERROR: Please input a valid command.\n");
    }
}


void alliberarMemoria(FleckConfig *fleckConfig){
    if (fleckConfig->user) {
        free(fleckConfig->user);
    }
    if (fleckConfig->directory) {
        free(fleckConfig->directory);
    }

    if(fleckConfig->ipGotham){
        free(fleckConfig->ipGotham);
    }
    
    free(fleckConfig); // Finalmente, liberar el propio struct
}

int main(int argc, char *argv[]) {
    FleckConfig *fleckConfig = (FleckConfig *)malloc(sizeof(FleckConfig));
    
    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    // Lee el archivo de configuración
    readConfigFile(argv[1], fleckConfig);

    // Lògica de línia de comandaments amb buffer dinàmic
    char *command = NULL;
    
    while (1) {
        printF("$montserrat:> ");
        
        command = readUntil(STDIN_FILENO, '\n');
        if (command == NULL) {
            printF("Error al llegir la línia\n");
            break;
        }

        
        
        processCommand(command, fleckConfig->directory);
        free(command);  // Allibera la memòria de la comanda després de cada ús
    }

    // Libera memoria dinámica
    alliberarMemoria(fleckConfig);
    return 0;
}
