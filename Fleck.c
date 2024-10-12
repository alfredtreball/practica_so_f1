#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256
#define COMMAND_SIZE 128

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

typedef struct {
    char *user;
    char *directory;
    char *ip;
    int port;
} Config;


// Funció per llegir fins a un caràcter delimitador
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

        buffer = (char *)realloc(buffer, i + 2);  // Assignar més espai
        buffer[i++] = c;                
    }

    buffer[i] = '\0';  // Finalitzar la cadena amb '\0'
    return buffer;
}

//Funció per eliminar un char d'un string
void removeChar(char *string, char charToRemove) {
    char *origen, *dst;
    for (origen = dst = string; *origen != '\0'; origen++) {
        *dst = *origen;
        if (*dst != charToRemove) dst++;
    }
    *dst = '\0';
}

// Funció per processar el fitxer de configuració utilitzant open, readUntil i memòria dinàmica
void readConfigFile(const char *configFile, Config *fleckConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    fleckConfig->user = readUntil(fd, '\n');
    removeChar(fleckConfig->user, '&'); //Eliminar '&' del nom de l'usuari (Consideració a tenir en compte)
    fleckConfig->directory = readUntil(fd, '\n');
    fleckConfig->ip = readUntil(fd, '\n');

    char *portStr = readUntil(fd, '\n');
    fleckConfig->port = atoi(portStr);
    free(portStr);  // Alliberar memòria per a portStr després de convertir-la
    close(fd);

    // Mostrar la configuració llegida
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

// Funció per processar les comandes
void processCommand(char *command) {
    char *cmd = strtok(command, " ");  // Separar la primera paraula de la comanda

    if (strcasecmp(cmd, "CONNECT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = strtok(NULL, " ");  // Segona part de la comanda
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
        char *file = strtok(NULL, " ");  // Primer paràmetre
        char *factorStr = strtok(NULL, " ");  // Segon paràmetre (factor)

        if (file != NULL && factorStr != NULL) {
            int factor = stringToInt(factorStr);  // Convertir el factor a int
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

int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    Config *fleckConfig = (Config *)malloc(sizeof(Config));
    
    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir el fitxer de configuració passant fleckConfig com a argument
    readConfigFile(argv[1], fleckConfig);

    // Línia de comandes
    char command[COMMAND_SIZE];
    while (1) {
        printF("$montserrat:> ");
        if (!fgets(command, COMMAND_SIZE, stdin)) {
            break; // Sortir si EOF
        }
        command[strcspn(command, "\n")] = 0; // Eliminar el salt de línia
        processCommand(command);
    }

    // Alliberar memòria dinàmica
    free(fleckConfig->user);
    free(fleckConfig->directory);
    free(fleckConfig->ip);
    free(fleckConfig);

    return 0;
}
