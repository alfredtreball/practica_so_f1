#define _GNU_SOURCE //asprintf OK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} HarleyConfig;


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

        buffer = (char *)realloc(buffer, i + 2);  // Alliberar més espai
        buffer[i++] = c;                
    }

    buffer[i] = '\0';  // Finalitzar la cadena amb '\0'
    return buffer;
}

// Funció per llegir el fitxer de configuració utilitzant readUntil
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    harleyConfig->ipGotham = readUntil(fd, '\n');
    char *portGotham = readUntil(fd, '\n');
    harleyConfig->portGotham = atoi(portGotham);
    free(portGotham);  // Alliberar memòria per a portStr després de convertir-la

    harleyConfig->ipFleck = readUntil(fd, '\n');
    char* portFleck = readUntil(fd, '\n');
    harleyConfig->portFleck = atoi(portFleck);
    free(portFleck);

    harleyConfig->directory = readUntil(fd, '\n');
    harleyConfig->workerType = readUntil(fd, '\n');

    close(fd);

    // Mostrar la configuració llegida
    printF("File read correctly:\n");
    printF("Gotham IP - ");
    printF(harleyConfig->ipGotham);
    printF("\nGotham Port - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d", harleyConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);

    printF("\nIp fleck - ");
    printF(harleyConfig->ipFleck);
    printF("\nPortFleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", harleyConfig->portFleck);
    printF(portFleckStr);
    free(portFleckStr);

    printF("\nDirectory - ");
    printF(harleyConfig->directory);
    printF("\nWorker Type - ");
    printF(harleyConfig->workerType);
    printF("\n");
}

void alliberarMemoria(HarleyConfig *harleyConfig) {
    if (harleyConfig->ipGotham) {
        free(harleyConfig->ipGotham);
    }
    if (harleyConfig->ipFleck) {
        free(harleyConfig->ipFleck);
    }
    free(harleyConfig); // Finalmente, liberar el propio struct
}

int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    
    if (argc != 2) {
        printF("Ús: ./harley <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir la configuració passant harleyConfig com a argument
    readConfigFile(argv[1], harleyConfig);

    // Alliberar memòria dinàmica
    alliberarMemoria(harleyConfig);

    return 0;
}