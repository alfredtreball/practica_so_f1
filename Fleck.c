#define _GNU_SOURCE //asprintf OK
#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h> // Per a isspace
#include <strings.h> // Necesario para strcasecmp

// Estructura de configuració de Fleck
typedef struct {
    char *user;
    char *directory;
    char *ipGotham;
    int portGotham;
} FleckConfig;


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

void processCommand(char *command) {
    char *cmd = strtok(command, " ");  // Separa la primera palabra del comando

    if (cmd == NULL) {
        printF("ERROR: Please input a valid command.\n");
        return;
    }

    if (strcasecmp(cmd, "CONNECT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = strtok(NULL, " ");  // Segunda parte del comando
        if (subCmd != NULL) {
            if (strcasecmp(subCmd, "MEDIA") == 0) {
                printF("Comanda OK\n");
            } else if (strcasecmp(subCmd, "TEXT") == 0) {
                printF("Comanda OK\n");
            } else {
                printF("Comanda KO\n");
            }
        } else {
            printF("Comanda KO\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = strtok(NULL, " ");  // Primer parámetro
        char *factorStr = strtok(NULL, " ");  // Segundo parámetro (factor)

        if (file != NULL && factorStr != NULL) {
            int factor = atoi(factorStr);  // Convierte el factor a entero
            if (factor > 0) {
                printF("Distorsión iniciada!\n");
            } else {
                printF("Comanda KO\n");
            }
        } else {
            printF("Comanda KO\n");
        }
    } else {
        printF("Comanda KO\n");
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
        printF("$ ");
        
        command = readUntil(STDIN_FILENO, '\n');
        if (command == NULL) {
            printF("Error al llegir la línia\n");
            break;
        }

        processCommand(command);
        free(command);  // Allibera la memòria de la comanda després de cada ús
    }

    // Libera memoria dinámica
    alliberarMemoria(fleckConfig);
    return 0;
}
