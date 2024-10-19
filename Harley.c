#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament
#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Definició de l'estructura HarleyConfig per emmagatzemar la configuració del sistema Harley
typedef struct {
    char *ipGotham;   // Adreça IP del servidor Gotham
    int portGotham;   // Port del servidor Gotham
    char *ipFleck;    // Adreça IP del servidor Fleck
    int portFleck;    // Port del servidor Fleck
    char *directory;  // Directori de treball
    char *workerType; // Tipus de treballador
} HarleyConfig;

// Funció per llegir el fitxer de configuració de Harley
/***********************************************
* @Finalitat: Llegeix el fitxer de configuració especificat i emmagatzema la informació a la estructura HarleyConfig.
* @Paràmetres:
*   in: configFile = nom del fitxer de configuració.
*   out: harleyConfig = estructura on s'emmagatzema la configuració llegida.
* @Retorn: ----
************************************************/
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna memòria per a cada camp de la configuració
    harleyConfig->ipGotham = readUntil(fd, '\n'); // Llegeix la IP del servidor Gotham
    char *portGotham = readUntil(fd, '\n'); // Llegeix el port com a cadena de text
    harleyConfig->portGotham = atoi(portGotham); // Converteix el port a enter
    free(portGotham); // Allibera la memòria de la cadena temporal

    harleyConfig->ipFleck = readUntil(fd, '\n'); // Llegeix la IP del servidor Fleck
    char* portFleck = readUntil(fd, '\n'); // Llegeix el port com a cadena de text
    harleyConfig->portFleck = atoi(portFleck); // Converteix el port a enter
    free(portFleck); // Allibera la memòria de la cadena temporal

    harleyConfig->directory = readUntil(fd, '\n'); // Llegeix el directori de treball
    harleyConfig->workerType = readUntil(fd, '\n'); // Llegeix el tipus de treballador

    close(fd); // Tanca el fitxer

    // Mostra la configuració llegida per a verificació
    printF("Fitxer llegit correctament:\n");
    printF("Gotham IP - ");
    printF(harleyConfig->ipGotham);
    printF("\nGotham Port - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d", harleyConfig->portGotham); // Converteix el port a cadena de text
    printF(portGothamStr);
    free(portGothamStr); // Allibera la memòria de la cadena temporal

    printF("\nIP Fleck - ");
    printF(harleyConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", harleyConfig->portFleck); // Converteix el port a cadena de text
    printF(portFleckStr);
    free(portFleckStr); // Allibera la memòria de la cadena temporal

    printF("\nDirectori - ");
    printF(harleyConfig->directory);
    printF("\nTipus de Treballador - ");
    printF(harleyConfig->workerType);
    printF("\n");
}

// Funció per alliberar la memòria dinàmica utilitzada per HarleyConfig
/***********************************************
* @Finalitat: Allibera la memòria dinàmica associada amb l'estructura HarleyConfig.
* @Paràmetres:
*   in: harleyConfig = estructura HarleyConfig a alliberar.
* @Retorn: ----
************************************************/
void alliberarMemoria(HarleyConfig *harleyConfig) {
    if (harleyConfig->ipGotham) {
        free(harleyConfig->ipGotham); // Allibera la memòria de la IP Gotham
    }
    if (harleyConfig->ipFleck) {
        free(harleyConfig->ipFleck); // Allibera la memòria de la IP Fleck
    }
    if (harleyConfig->directory) {
        free(harleyConfig->directory); // Allibera la memòria del directori
    }
    if (harleyConfig->workerType) {
        free(harleyConfig->workerType); // Allibera la memòria del tipus de treballador
    }
    free(harleyConfig); // Finalment, allibera la memòria de l'estructura principal
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració de Harley
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    
    if (argc != 2) {
        printF("Ús: ./harley <fitxer de configuració>\n"); // Comprova que s'ha passat el fitxer de configuració com a argument
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix el fitxer de configuració passant l'estructura harleyConfig com a argument
    readConfigFile(argv[1], harleyConfig);

    // Allibera la memòria dinàmica abans de finalitzar
    alliberarMemoria(harleyConfig);

    return 0; // Finalitza correctament el programa
}
