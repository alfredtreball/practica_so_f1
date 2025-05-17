#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "FileReader.h"
#include "StringUtils.h" //trim()

#define printF(x) write(1, x, strlen(x))

// Funció per llegir descripcions del fitxer fins a trobar un caràcter específic o el final del fitxer
/***********************************************
* @Finalitat: Llegeix caràcters d'un fitxer descriptor fins que es troba un caràcter específic (cEnd) o el final del fitxer.
* @Paràmetres:
*   in: fd = descriptor del fitxer des del qual es vol llegir.
*   in: cEnd = caràcter fins al qual es vol llegir.
* @Retorn: Retorna un punter a una cadena de caràcters llegida o NULL en cas d'error o EOF.
************************************************/
char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = malloc(1); // Asignación inicial mínima

    if (!buffer) {
        return NULL;
    }

    while (1) {
        chars_read = read(fd, &c, sizeof(char));
        if (chars_read == 0) { // EOF
            if (i == 0) { // No se leyeron caracteres
                free(buffer);
                return NULL;
            }
            break;
        } else if (chars_read < 0) { // Error de lectura
            free(buffer);
            return NULL;
        }

        if (c == cEnd) { // Encontró el delimitador
            break;
        }

        char *temp = realloc(buffer, i + 2); // Incrementar espacio para un carácter más y el terminador nulo
        if (!temp) { // Verificación de fallos en realloc
            free(buffer);
            return NULL;
        }

        buffer = temp;
        buffer[i++] = c;
    }

    buffer[i] = '\0'; // Terminar la cadena

    // Si solo se leyó un salto de línea o la cadena está vacía, liberamos y retornamos NULL
    if (i == 0 || (i == 1 && buffer[0] == '\n')) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/***********************************************
* @Finalitat: Llegeix el fitxer de configuració genèricament segons el tipus.
* @Paràmetres:
*   in: configFile = nom del fitxer de configuració.
*   out: configStruct = estructura on s'emmagatzema la configuració llegida.
*   in: configType = tipus de configuració (ConfigType).
************************************************/
void readConfigFileGeneric(const char *configFile, void *configStruct, ConfigType configType) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    char *line;

    switch (configType) {
        case CONFIG_ENIGMA: {
            EnigmaConfig *config = (EnigmaConfig *)configStruct;
            config->ipGotham = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portGotham = atoi(line);
            free(line);

            config->ipFleck = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portFleck = atoi(line);
            free(line);

            config->directory = readUntil(fd, '\n');
            config->workerType = readUntil(fd, '\n');
            break;
        }
        case CONFIG_HARLEY: {
            HarleyConfig *config = (HarleyConfig *)configStruct;
            config->ipGotham = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portGotham = atoi(line);
            free(line);

            config->ipFleck = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portFleck = atoi(line);
            free(line);

            config->directory = readUntil(fd, '\n');
            config->workerType = readUntil(fd, '\n');
            break;
        }
        case CONFIG_GOTHAM: {
            GothamConfig *config = (GothamConfig *)configStruct;
            config->ipFleck = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portFleck = atoi(line);
            free(line);

            config->ipHarEni = trim(readUntil(fd, '\n'));
            line = readUntil(fd, '\n');
            config->portHarEni = atoi(line);
            free(line);
            break;
        }
        case CONFIG_FLECK: {
            FleckConfig *config = (FleckConfig *)configStruct;
            config->user = readUntil(fd, '\n');
            config->directory = trim(readUntil(fd, '\n'));
            config->ipGotham = trim(readUntil(fd, '\n'));

            line = readUntil(fd, '\n');
            config->portGotham = atoi(line);
            free(line);
            break;
        }
        default:
            printF("Error: Tipus de configuració no vàlid.\n");
            close(fd);
            exit(1);
    }

    close(fd);

    switch (configType) {
        case CONFIG_ENIGMA: {
            printF("\nReading configuration file\n");
            printF("Connecting Enigma worker to the system..\n");
            printF("Connected to Mr. J System, ready to listen to Fleck petitions\n\n");
            printF("Waiting for connections...\n\n");
            EnigmaConfig *config = (EnigmaConfig *)configStruct;
            printF("IP Gotham - "); printF(config->ipGotham);

            printF("\nPort Gotham - ");
            char *portStr; asprintf(&portStr, "%d", config->portGotham); printF(portStr); free(portStr);

            printF("\nIP Fleck - "); printF(config->ipFleck);

            printF("\nPort Fleck - ");
            asprintf(&portStr, "%d", config->portFleck); printF(portStr); free(portStr);

            printF("\nDirectori - "); printF(config->directory);

            printF("\nTipus de treballador - "); printF(config->workerType);
            printF("\n");
            break;
        }
        case CONFIG_HARLEY: {
            HarleyConfig *config = (HarleyConfig *)configStruct;
            printF("\nReading configuration file\n");
            printF("IP Gotham - "); printF(config->ipGotham);

            printF("\nPort Gotham - ");
            char *portStr; asprintf(&portStr, "%d", config->portGotham); printF(portStr); free(portStr);

            printF("\nIP Fleck - "); printF(config->ipFleck);

            printF("\nPort Fleck - ");
            asprintf(&portStr, "%d", config->portFleck); printF(portStr); free(portStr);

            printF("\nDirectori - "); printF(config->directory);

            printF("\nTipus de treballador - "); printF(config->workerType);
            printF("\n");
            break;
        }
        case CONFIG_GOTHAM: {
            GothamConfig *config = (GothamConfig *)configStruct;
            printF("Reading configuration file\n\n");
            printF("Gotham server initialized\n\n");
            printF("Waiting for connections...\n");
            printF("IP Fleck - "); printF(config->ipFleck);

            printF("\nPort Fleck - ");
            char *portStr; asprintf(&portStr, "%d", config->portFleck); printF(portStr); free(portStr);

            printF("\nIP Harley Enigma - "); printF(config->ipHarEni);

            printF("\nPort Harley Enigma - ");
            asprintf(&portStr, "%d", config->portHarEni); printF(portStr); free(portStr);

            printF("\n");
            break;
        }
        case CONFIG_FLECK: {
            FleckConfig *config = (FleckConfig *)configStruct;
            printF("Usuari - "); printF(config->user);

            printF("\nDirectori - "); printF(config->directory);

            printF("\nIP Gotham - "); printF(config->ipGotham);

            printF("\nPort Gotham - ");
            char *portStr; asprintf(&portStr, "%d", config->portGotham); printF(portStr); free(portStr);

            printF("\n");
            break;
        }
    }
}
