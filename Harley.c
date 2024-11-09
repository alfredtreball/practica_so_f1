/***********************************************
* @Fitxer: Harley.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer implementa les funcions per a la gestió de la 
* configuració del sistema Harley. Inclou funcions per a llegir la configuració 
* des d'un fitxer, emmagatzemar la informació en una estructura de dades, i 
* alliberar la memòria dinàmica associada. La configuració inclou informació 
* sobre les connexions amb els servidors Gotham i Fleck, així com informació 
* del directori de treball i el tipus de treballador.
************************************************/
#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "FileReader.h"
#include "StringUtils.h"
#include "Networking.h" // Inclou la funció connect_to_server

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
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura

    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna memòria per a cada camp de la configuració
    harleyConfig->ipGotham = trim(readUntil(fd, '\n')); // Llegeix la IP del servidor Gotham
    char *portGotham = readUntil(fd, '\n'); // Llegeix el port com a cadena de text
    harleyConfig->portGotham = atoi(portGotham); // Converteix el port a enter
    free(portGotham); // Allibera la memòria de la cadena temporal

    harleyConfig->ipFleck = trim(readUntil(fd, '\n')); // Llegeix la IP del servidor Fleck
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

    printF("\nDirectory - ");
    printF(harleyConfig->directory);
    printF("\nWorker Type - ");
    printF(harleyConfig->workerType);
    printF("\n");
}

// Funció per alliberar la memòria dinàmica utilitzada per HarleyConfig
void alliberarMemoria(HarleyConfig *harleyConfig) {
    if (harleyConfig->ipGotham) free(harleyConfig->ipGotham);
    if (harleyConfig->ipFleck) free(harleyConfig->ipFleck);
    if (harleyConfig->directory) free(harleyConfig->directory);
    if (harleyConfig->workerType) free(harleyConfig->workerType);
    free(harleyConfig);
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració de Harley
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    
    if (argc != 2) {
        printF("Ús: ./harley <fitxer de configuració>\n");
        exit(1);
    }

    // Llegeix el fitxer de configuració passant l'estructura harleyConfig com a argument
    readConfigFile(argv[1], harleyConfig);

    // Connecta a Gotham
    int gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0) {
        printF("Error connectant a Gotham\n");
        alliberarMemoria(harleyConfig);
        return 1;
    }

    // Envia un missatge de registre com a treballador
    char registerMessage[FRAME_SIZE];
    snprintf(registerMessage, FRAME_SIZE, "REGISTER WORKER %s", harleyConfig->workerType);
    send_frame(gothamSocket, registerMessage, strlen(registerMessage));

    printF("Registrat a Gotham com a worker de tipus: ");
    printF(harleyConfig->workerType);
    printF("\n");

    // Aquí pots implementar més funcionalitats si es requereix en el futur
    // Ara només mantindrà la connexió oberta per a rebre i processar peticions

    close(gothamSocket);
    alliberarMemoria(harleyConfig);
    return 0;
}