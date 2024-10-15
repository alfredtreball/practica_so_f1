#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

// Definició de l'estructura que conté la configuració de Gotham
typedef struct {
    char *ipFleck;
    int portFleck;
    char *ipHarEni;
    int portHarEni;
} GothamConfig;

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

// Funció per llegir el fitxer de configuració de Gotham
void readConfigFile(const char *configFile, GothamConfig *gothamConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    gothamConfig->ipFleck = readUntil(fd, '\n');
    char* portFleck = readUntil(fd, '\n');
    gothamConfig->portFleck = atoi(portFleck);
    free(portFleck); // Alliberar memòria port fleck

    gothamConfig->ipHarEni = readUntil(fd, '\n');
    char *portHarEni = readUntil(fd, '\n');
    gothamConfig->port = atoi(portHarEni);
    free(portStr);  // Alliberar memòria port Harley Enigma

    close(fd);

    // Mostrar la configuració llegida
    printF("IpFleck - ");
    printF(gothamConfig->ipFleck);
    printF("\nPort fleck - ");
    printF(gothamConfig->portFleck);
    printF("\nIP Harley Engima - ");
    printF(gothamConfig->ipHarEni);
    printF("\nPort Harley Enigma - ");
    printF(gothamConfig->portHarEni);
}

// Función para liberar la memoria de GothamConfig
void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) {
        free(gothamConfig->ipFleck);
    }
    if (gothamConfig->ipHarEni) {
        free(gothamConfig->ipHarEni);
    }
    free(gothamConfig); // Finalmente, liberar el propio struct
}


int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    GothamConfig *gothamConfig = (GothamConfig *)malloc(sizeof(GothamConfig));
    
    if (argc != 2) {
        printF("Ús: ./gotham <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir la configuració passant gothamConfig com a argument
    readConfigFile(argv[1], gothamConfig);

    // Alliberar memòria dinàmica
    alliberarMemoria(gothamConfig);

    return 0;
}