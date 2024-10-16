#include "Harley.h" // Incluye el archivo de encabezado




// Función para leer el archivo de configuración de Harley
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error abriendo el archivo de configuración\n");
        exit(1);
    }

    // Lee y asigna memoria para cada campo
    harleyConfig->ipGotham = readUntil(fd, '\n');
    char *portGotham = readUntil(fd, '\n');
    harleyConfig->portGotham = atoi(portGotham);
    free(portGotham);

    harleyConfig->ipFleck = readUntil(fd, '\n');
    char* portFleck = readUntil(fd, '\n');
    harleyConfig->portFleck = atoi(portFleck);
    free(portFleck);

    harleyConfig->directory = readUntil(fd, '\n');
    harleyConfig->workerType = readUntil(fd, '\n');

    close(fd);

    // Mostrar la configuración leída
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

// Función para liberar la memoria asignada dinámicamente para HarleyConfig
void alliberarMemoria(HarleyConfig *harleyConfig) {
    if (harleyConfig->ipGotham) {
        free(harleyConfig->ipGotham);
    }
    if (harleyConfig->ipFleck) {
        free(harleyConfig->ipFleck);
    }
    if (harleyConfig->directory) {
        free(harleyConfig->directory);
    }
    if (harleyConfig->workerType) {
        free(harleyConfig->workerType);
    }
    free(harleyConfig); // Finalmente, libera la estructura en sí
}

int main(int argc, char *argv[]) {
    // Crea la variable local en main
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    
    if (argc != 2) {
        printF("Uso: ./harley <archivo de configuración>\n");
        exit(1);
    }

    // Lee la configuración pasando harleyConfig como argumento
    readConfigFile(argv[1], harleyConfig);

    // Libera memoria dinámica
    alliberarMemoria(harleyConfig);

    return 0;
}
