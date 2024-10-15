#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h> // Para isspace

#define BUFFER_SIZE 256
#define COMMAND_SIZE 128

#define printF(x) write(1, x, strlen(x)) // Macro para escribir mensajes

typedef struct {
    char *user;
    char *directory;
    char *ip;
    int port;
} Config;

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
void readConfigFile(const char *configFile, Config *fleckConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error abriendo el archivo de configuración\n");
        exit(1);
    }

    // Leer y asignar memoria para cada campo
    fleckConfig->user = readUntil(fd, '\n');
    removeChar(fleckConfig->user, '&'); // Elimina '&' del nombre del usuario
    fleckConfig->directory = trim(readUntil(fd, '\n')); // Elimina espacios del path del directorio
    fleckConfig->ip = readUntil(fd, '\n');

    char *portStr = readUntil(fd, '\n');
    fleckConfig->port = atoi(portStr);
    free(portStr);  // Libera memoria para portStr después de convertirla
    close(fd);

    // Muestra la configuración leída
    printF("Arthur user initialized\n");
    printF("File read correctly:\n");
    printF("User - ");
    printF(fleckConfig->user);
    printF("\nDirectory - ");
    printF(fleckConfig->directory);
    printF("\nIP - ");
    printF(fleckConfig->ip);
    printF("\nPort - ");
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", fleckConfig->port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg)); 
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

// Función para procesar los comandos
void processCommand(char *command) {
    char* context = NULL;
    char *cmd = separarParaules(command, " ", &context);  // Separa la primera palabra del comando

    if (strcasecmp(cmd, "CONNECT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = separarParaules(NULL, " ", &context);  // Segunda parte del comando
        if (subCmd != NULL) {
            if (strcasecmp(subCmd, "MEDIA") == 0) {
                printF("Comanda OK\n");
            } else if (strcasecmp(subCmd, "TEXT") == 0) {
                printF("Comanda KO\n");
            } else {
                printF("Unknown command\n");
            }
        } else {
            printF("Unknown command\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = separarParaules(NULL, " ", &context);  // Primer parámetro
        char *factorStr = separarParaules(NULL, " ", &context);  // Segundo parámetro (factor)

        if (file != NULL && factorStr != NULL) {
            int factor = atoi(factorStr);  // Convierte el factor a int
            if (factor > 0) {
                printF("Comanda OK\n");
            } else {
                printF("Comanda KO\n");
            }
        } else {
            printF("Comanda KO\n");
        }
    } else {
        printF("Unknown command\n");
    }
}

ssize_t readLine(char *buffer, size_t max_size) {
    ssize_t total_read = read(STDIN_FILENO, buffer, max_size - 1);
    if (total_read < 0) {
        return -1; // Error de lectura
    } else if (total_read == 0) {
        return 0; // EOF o no se leyó nada
    }

    for (ssize_t i = 0; i < total_read; i++) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            return total_read;
        }
    }

    buffer[total_read] = '\0';
    return total_read;
}

int main(int argc, char *argv[]) {
    Config *fleckConfig = (Config *)malloc(sizeof(Config));
    
    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    // Lee el archivo de configuración
    readConfigFile(argv[1], fleckConfig);

    // Lógica de línea de comandos
    char command[COMMAND_SIZE];
    while (1) {
        printF("$montserrat:> ");
        
        ssize_t len = readLine(command, COMMAND_SIZE);
        if (len == -1) {
            printF("Error al leer la línea\n");
            break;
        } else if (len == 0) {
            break; // EOF, salir del bucle
        }
        
        processCommand(command);
    }

    // Libera memoria dinámica
    free(fleckConfig->user);
    free(fleckConfig->directory);
    free(fleckConfig->ip);
    free(fleckConfig);

    return 0;
}
