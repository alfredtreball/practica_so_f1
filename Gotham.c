/***********************************************
* @Fitxer: Gotham.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer implementa la gestió de configuració per al sistema 
* Gotham. Inclou funcions per a llegir la configuració des d'un fitxer, 
* emmagatzemar la informació en una estructura de dades i alliberar la memòria 
* dinàmica associada. La configuració inclou informació de les connexions 
* amb els servidors Fleck i Harley Enigma.
************************************************/
#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "Utils.h"

// Definició de l'estructura GothamConfig per emmagatzemar la configuració del sistema Gotham
typedef struct {
    char *ipFleck;    // Adreça IP del servidor Fleck
    int portFleck;    // Port del servidor Fleck
    char *ipHarEni;   // Adreça IP del servidor Harley Enigma
    int portHarEni;   // Port del servidor Harley Enigma
} GothamConfig;

// Funció per llegir el fitxer de configuració de Gotham
/***********************************************
* @Finalitat: Llegeix el fitxer de configuració especificat i emmagatzema la informació a la estructura GothamConfig.
* @Paràmetres:
*   in: configFile = nom del fitxer de configuració.
*   out: gothamConfig = estructura on s'emmagatzema la configuració llegida.
* @Retorn: ----
************************************************/
void readConfigFile(const char *configFile, GothamConfig *gothamConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura
    
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna memòria per a cada camp de la configuració
    gothamConfig->ipFleck = readUntil(fd, '\n'); // Llegeix la IP del servidor Fleck
    char* portFleck = readUntil(fd, '\n'); // Llegeix el port com a cadena
    gothamConfig->portFleck = atoi(portFleck); // Converteix el port a enter
    free(portFleck); // Allibera la memòria de la cadena temporal

    gothamConfig->ipHarEni = readUntil(fd, '\n'); // Llegeix la IP del servidor Harley Enigma
    char *portHarEni = readUntil(fd, '\n'); // Llegeix el port com a cadena
    gothamConfig->portHarEni = atoi(portHarEni); // Converteix el port a enter
    free(portHarEni); // Allibera la memòria de la cadena temporal

    close(fd); // Tanca el fitxer

    // Mostra la configuració llegida per a verificació
    printF("IP Fleck - ");
    printF(gothamConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", gothamConfig->portFleck); // Converteix el port a cadena de text
    printF(portFleckStr);
    free(portFleckStr); // Allibera la memòria de la cadena temporal

    printF("\nIP Harley Enigma - ");
    printF(gothamConfig->ipHarEni);
    printF("\nPort Harley Enigma - ");
    char* portHarEniStr = NULL;
    asprintf(&portHarEniStr, "%d\n", gothamConfig->portHarEni); // Converteix el port a cadena de text
    printF(portHarEniStr);
    free(portHarEniStr); // Allibera la memòria de la cadena temporal
}

// Funció per alliberar la memòria dinàmica utilitzada per la configuració de Gotham
/***********************************************
* @Finalitat: Allibera la memòria dinàmica associada amb l'estructura GothamConfig.
* @Paràmetres:
*   in: gothamConfig = estructura GothamConfig a alliberar.
* @Retorn: ----
************************************************/
void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) {
        free(gothamConfig->ipFleck); // Allibera la memòria de la IP Fleck
    }
    if (gothamConfig->ipHarEni) {
        free(gothamConfig->ipHarEni); // Allibera la memòria de la IP Harley Enigma
    }
    free(gothamConfig); // Finalment, allibera la memòria de l'estructura principal
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració de Gotham
    GothamConfig *gothamConfig = (GothamConfig *)malloc(sizeof(GothamConfig));
    
    if (argc != 2) {
        printF("Ús: ./gotham <fitxer de configuració>\n"); // Comprova que s'ha passat el fitxer de configuració com a argument
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix el fitxer de configuració passant l'estructura gothamConfig com a argument
    readConfigFile(argv[1], gothamConfig);

    // Allibera la memòria dinàmica abans de finalitzar
    alliberarMemoria(gothamConfig);

    return 0; // Finalitza correctament el programa
}
