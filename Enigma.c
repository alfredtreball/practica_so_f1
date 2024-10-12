#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

// Definició de l'estructura que conté la configuració d'Enigma
typedef struct {
    char *server_name;
    char *directory;
    char *ip;
    int port;
} EnigmaConfig;

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

// Funció per llegir el fitxer de configuració d'Enigma
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        writeMessage("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    enigmaConfig->server_name = readUntil(fd, '\n');
    enigmaConfig->directory = readUntil(fd, '\n');
    enigmaConfig->ip = readUntil(fd, '\n');

    char *portStr = readUntil(fd, '\n');
    enigmaConfig->port = stringToInt(portStr);
    free(portStr);  // Alliberar memòria per a portStr després de convertir-la

    close(fd);

    // Mostrar la configuració llegida
    writeMessage("File read correctly:\n");
    writeMessage("Server Name - ");
    writeMessage(enigmaConfig->server_name);
    writeMessage("\nDirectory - ");
    writeMessage(enigmaConfig->directory);
    writeMessage("\nIP - ");
    writeMessage(enigmaConfig->ip);
    writeMessage("\nPort - ");
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", enigmaConfig->port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));
}

int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    EnigmaConfig *enigmaConfig = (EnigmaConfig *)malloc(sizeof(EnigmaConfig));
    
    if (argc != 2) {
        writeMessage("Ús: ./enigma <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir la configuració passant enigmaConfig com a argument
    readConfigFile(argv[1], enigmaConfig);

    // Alliberar memòria dinàmica
    free(enigmaConfig->server_name);
    free(enigmaConfig->directory);
    free(enigmaConfig->ip);
    free(enigmaConfig);

    return 0;
}