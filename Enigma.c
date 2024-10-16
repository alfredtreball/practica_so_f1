#include "Enigma.h" // Incluye el archivo de encabezado
#include "Utils.h"

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

// Función para leer el archivo de configuración de Enigma
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error abriendo el archivo de configuración\n");
        exit(1);
    }

    // Lee y asigna memoria para cada campo
    enigmaConfig->ipGotham = readUntil(fd, '\n');
    char* portGotham = readUntil(fd, '\n');
    enigmaConfig->portGotham = atoi(portGotham);
    free(portGotham);

    enigmaConfig->ipFleck = readUntil(fd, '\n');
    char *portFleck = readUntil(fd, '\n');
    enigmaConfig->portFleck = atoi(portFleck);
    free(portFleck);

    enigmaConfig->directory = readUntil(fd, '\n');
    enigmaConfig->workerType = readUntil(fd, '\n');

    close(fd);

    // Muestra la configuración leída
    printF("Ip Gotham - ");
    printF(enigmaConfig->ipGotham);
    printF("\nPort Gotham - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d", enigmaConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);

    printF("\nIp Fleck - ");
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

// Función para liberar la memoria asignada dinámicamente para EnigmaConfig
void alliberarMemoria(EnigmaConfig *enigmaConfig) {
    if (enigmaConfig->ipGotham) {
        free(enigmaConfig->ipGotham);
    }
    if (enigmaConfig->ipFleck) {
        free(enigmaConfig->ipFleck);
    }
    if (enigmaConfig->directory) {
        free(enigmaConfig->directory);
    }
    if (enigmaConfig->workerType) {
        free(enigmaConfig->workerType);
    }
    free(enigmaConfig); // Finalmente, libera la estructura en sí
}

int main(int argc, char *argv[]) {
    // Crea la variable local en main
    EnigmaConfig *enigmaConfig = (EnigmaConfig *)malloc(sizeof(EnigmaConfig));
    
    if (argc != 2) {
        printF("Uso: ./enigma <archivo de configuración>\n");
        exit(1);
    }

    // Lee la configuración pasando enigmaConfig como argumento
    readConfigFile(argv[1], enigmaConfig);

    // Libera memoria dinámica
    alliberarMemoria(enigmaConfig);

    return 0;
}
