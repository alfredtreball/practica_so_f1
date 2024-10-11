#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256
#define COMMAND_SIZE 128

typedef struct {
    char *user;
    char *directory;
    char *ip;
    int port;
} Config;

//tinc caca

Config *fleckConfig;

// Funció per escriure missatges a la consola sense printf
void writeMessage(const char *message) {
    write(STDOUT_FILENO, message, strlen(message));
}

// Funció per llegir línies amb read i memòria dinàmica
ssize_t readLine(int fd, char *buffer, size_t size) {
    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    char ch;
    while (totalBytesRead < size - 1 && (bytesRead = read(fd, &ch, 1)) > 0) {
        if (ch == '\n') {
            break;
        }
        buffer[totalBytesRead++] = ch;
    }
    buffer[totalBytesRead] = '\0'; // Terminar la cadena
    return totalBytesRead;
}

// Funció per convertir un string en enter
int stringToInt(char *str) {
    int result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        }
    }
    return result;
}

// Funció per processar el fitxer de configuració utilitzant open, read i memòria dinàmica
void readConfigFile(const char *configFile) {
    fleckConfig = (Config *)malloc(sizeof(Config)); // Assignar memòria per la configuració
    fleckConfig->user = (char *)malloc(BUFFER_SIZE);
    fleckConfig->directory = (char *)malloc(BUFFER_SIZE);
    fleckConfig->ip = (char *)malloc(BUFFER_SIZE);

    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        writeMessage("Error obrint el fitxer de configuració\n");
        exit(1);
    }
    
    readLine(fd, fleckConfig->user, BUFFER_SIZE);
    readLine(fd, fleckConfig->directory, BUFFER_SIZE);
    readLine(fd, fleckConfig->ip, BUFFER_SIZE);

    char portStr[BUFFER_SIZE];
    readLine(fd, portStr, BUFFER_SIZE);
    fleckConfig->port = stringToInt(portStr); // Convertir a int manualment

    close(fd);

    writeMessage("Arthur user initialized\n");
    writeMessage("File read correctly:\n");
    writeMessage("User - ");
    writeMessage(fleckConfig->user);
    writeMessage("\nDirectory - ");
    writeMessage(fleckConfig->directory);
    writeMessage("\nIP - ");
    writeMessage(fleckConfig->ip);
    writeMessage("\nPort - ");
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", fleckConfig->port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));
}

// Funció per processar les comandes
void processCommand(char *command) {
    char *cmd = strtok(command, " ");  // Separar la primera paraula de la comanda

    if (strcasecmp(cmd, "CONNECT") == 0) {
        writeMessage("Comanda OK\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        writeMessage("Comanda OK\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = strtok(NULL, " ");  // Segona part de la comanda
        if (subCmd != NULL) {
            if (strcasecmp(subCmd, "MEDIA") == 0) {
                writeMessage("Comanda OK\n");
            } else if (strcasecmp(subCmd, "TEXT") == 0) {
                writeMessage("Comanda KO\n");
            } else {
                writeMessage("Unknown command\n");
            }
        } else {
            writeMessage("Unknown command\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = strtok(NULL, " ");  // Primer paràmetre
        char *factorStr = strtok(NULL, " ");  // Segon paràmetre (factor)

        if (file != NULL && factorStr != NULL) {
            int factor = stringToInt(factorStr);  // Convertir el factor a int
            if (factor > 0) {
                writeMessage("Comanda OK\n");
            } else {
                writeMessage("Comanda KO\n");
            }
        } else {
            writeMessage("Comanda KO\n");
        }
    } else {
        writeMessage("Unknown command\n");
    }
}

void freeMemory() {
    free(fleckConfig->user);
    free(fleckConfig->directory);
    free(fleckConfig->ip);
    free(fleckConfig);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        writeMessage("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir fitxer de configuració
    readConfigFile(argv[1]);

    // Línia de comandes
    char command[COMMAND_SIZE];
    while (1) {
        writeMessage("$montserrat:> ");
        if (!fgets(command, COMMAND_SIZE, stdin)) {
            break; // Sortir si EOF
        }
        command[strcspn(command, "\n")] = 0; // Eliminar el salt de línia
        processCommand(command);
    }

    // Alliberar memòria dinàmica
    freeMemory();

    return 0;
}
