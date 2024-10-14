#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256

#define printF(x) write(1, x, strlen(x)) //Macro per escriure missatges

typedef struct {
    char *gotham_ip;
    int gotham_port;
    char *server_ip;
    int server_port;
    char *directory;
    char *worker_type;
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
    harleyConfig->gotham_ip = readUntil(fd, '\n');
    char *portStr = readUntil(fd, '\n');
    harleyConfig->gotham_port = atoi(portStr);
    free(portStr);  // Alliberar memòria per a portStr després de convertir-la

    harleyConfig->server_ip = readUntil(fd, '\n');
    portStr = readUntil(fd, '\n');
    harleyConfig->server_port = atoi(portStr);
    free(portStr);

    harleyConfig->directory = readUntil(fd, '\n');
    harleyConfig->worker_type = readUntil(fd, '\n');

    close(fd);

    // Mostrar la configuració llegida
    printF("File read correctly:\n");
    printF("Gotham IP - ");
    printF(harleyConfig->gotham_ip);
    printF("\nGotham Port - ");
    
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", harleyConfig->gotham_port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));

    printF("Server IP - ");
    printF(harleyConfig->server_ip);
    printF("\nServer Port - ");
    
    snprintf(portMsg, BUFFER_SIZE, "%d\n", harleyConfig->server_port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));

    printF("Directory - ");
    printF(harleyConfig->directory);
    printF("\nWorker Type - ");
    printF(harleyConfig->worker_type);
    printF("\n");
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
    free(harleyConfig->gotham_ip);
    free(harleyConfig->server_ip);
    free(harleyConfig->directory);
    free(harleyConfig->worker_type);
    free(harleyConfig);

    return 0;
}