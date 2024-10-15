#define _GNU_SOURCE // Asegura que asprintf esté disponible
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

// Definició de l'estructura que conté la configuració d'Enigma
typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char* directory;
    char* workerType;
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

// Funció per llegir el fitxer de configuració d'Enigma
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    enigmaConfig->ipGotham = readUntil(fd, '\n');
    char* portGotham = readUntil(fd, '\n');
    enigmaConfig->portGotham = atoi(portGotham);
    free(portGotham);

    enigmaConfig->ipFleck = readUntil(fd, '\n');
    char *portFleck = readUntil(fd, '\n');
    enigmaConfig->portFleck = atoi(portFleck);
    free(portFleck);  // Alliberar memòria per a portStr després de convertir-la

    enigmaConfig->directory = readUntil(fd, '\n');
    enigmaConfig->workerType = readUntil(fd, '\n');

    close(fd);

    // Mostrar la configuració llegida
    printF("Ip Gotham - ");
    printF(enigmaConfig->ipGotham);
    printF("\nPort Gotham - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d", enigmaConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);

    printF("\nIp fleck - ");
    printF(enigmaConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", enigmaConfig->portFleck);
    printF(portFleckStr);
    free(portFleckStr);

    printF("\nDirectory Enigma - ");
    printF(enigmaConfig->directory);

    printF("\nWorker Type - ");
    printF(enigmaConfig->workerType);
    printF("\n");

}

void alliberarMemoria(EnigmaConfig *enigmaConfig) {
    if (enigmaConfig->ipGotham) {
        free(enigmaConfig->ipGotham);
    }
    if (enigmaConfig->ipFleck) {
        free(enigmaConfig->ipFleck);
    }
    free(enigmaConfig); // Finalmente, liberar el propio struct
}

int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    EnigmaConfig *enigmaConfig = (EnigmaConfig *)malloc(sizeof(EnigmaConfig));
    
    if (argc != 2) {
        printF("Ús: ./enigma <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir la configuració passant enigmaConfig com a argument
    readConfigFile(argv[1], enigmaConfig);

    // Allibear memòria dnàmica
    alliberarMemoria(enigmaConfig);

    return 0;
}