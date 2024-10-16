#include "Gotham.h" // Incluye el archivo de encabezado




// Función para leer el archivo de configuración de Gotham
void readConfigFile(const char *configFile, GothamConfig *gothamConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error abriendo el archivo de configuración\n");
        exit(1);
    }

    // Lee y asigna memoria para cada campo
    gothamConfig->ipFleck = readUntil(fd, '\n');
    char* portFleck = readUntil(fd, '\n');
    gothamConfig->portFleck = atoi(portFleck);
    free(portFleck);

    gothamConfig->ipHarEni = readUntil(fd, '\n');
    char *portHarEni = readUntil(fd, '\n');
    gothamConfig->portHarEni = atoi(portHarEni);
    free(portHarEni);

    close(fd);

    // Muestra la configuración leída
    printF("IpFleck - ");
    printF(gothamConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", gothamConfig->portFleck);
    printF(portFleckStr);
    free(portFleckStr);

    printF("\nIP Harley Enigma - ");
    printF(gothamConfig->ipHarEni);
    printF("\nPort Harley Enigma - ");
    char* portHarEniStr = NULL;
    asprintf(&portHarEniStr, "%d\n", gothamConfig->portHarEni);
    printF(portHarEniStr);
    free(portHarEniStr);
}

// Función para liberar la memoria de GothamConfig
void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) {
        free(gothamConfig->ipFleck);
    }
    if (gothamConfig->ipHarEni) {
        free(gothamConfig->ipHarEni);
    }
    free(gothamConfig);
}

int main(int argc, char *argv[]) {
    GothamConfig *gothamConfig = (GothamConfig *)malloc(sizeof(GothamConfig));
    
    if (argc != 2) {
        printF("Ús: ./gotham <fitxer de configuració>\n");
        exit(1);
    }

    // Lee la configuración pasando gothamConfig como argumento
    readConfigFile(argv[1], gothamConfig);

    // Libera memoria dinámica
    alliberarMemoria(gothamConfig);

    return 0;
}