/***********************************************
* @Fitxer: Enigma.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer implementa la gestió de configuració del sistema Enigma. 
* Conté funcions per a llegir la configuració d'un fitxer i carregar-la en l'estructura 
* EnigmaConfig, així com per alliberar la memòria associada. La configuració inclou 
* informació sobre servidors i el tipus de treballador.
************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "FileReader.h"
#include "StringUtils.h"
#include "Networking.h"

// Definició de l'estructura EnigmaConfig per emmagatzemar la configuració
typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} EnigmaConfig;

// Funció per llegir el fitxer de configuració d'Enigma
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    enigmaConfig->ipGotham = trim(readUntil(fd, '\n'));
    char* portGotham = readUntil(fd, '\n');
    enigmaConfig->portGotham = atoi(portGotham);
    free(portGotham);

    enigmaConfig->ipFleck = trim(readUntil(fd, '\n'));
    char *portFleck = readUntil(fd, '\n');
    enigmaConfig->portFleck = atoi(portFleck);
    free(portFleck);

    enigmaConfig->directory = readUntil(fd, '\n');
    enigmaConfig->workerType = readUntil(fd, '\n');
    close(fd);

    printF("Configuració llegida:\n");
    printF("IP Gotham: ");
    printF(enigmaConfig->ipGotham);
    printF("\nPort Gotham: ");
    char *portGothamStr;
    asprintf(&portGothamStr, "%d\n", enigmaConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);
    
    printF("IP Fleck: ");
    printF(enigmaConfig->ipFleck);
    printF("\nPort Fleck: ");
    char *portFleckStr;
    asprintf(&portFleckStr, "%d\n", enigmaConfig->portFleck);
    printF(portFleckStr);
    free(portFleckStr);
    
    printF("Directori: ");
    printF(enigmaConfig->directory);
    printF("\nTipus de treballador: ");
    printF(enigmaConfig->workerType);
    printF("\n");
}

// Funció per alliberar la memòria de l'estructura EnigmaConfig
void alliberarMemoria(EnigmaConfig *enigmaConfig) {
    if (enigmaConfig->ipGotham) free(enigmaConfig->ipGotham);
    if (enigmaConfig->ipFleck) free(enigmaConfig->ipFleck);
    if (enigmaConfig->directory) free(enigmaConfig->directory);
    if (enigmaConfig->workerType) free(enigmaConfig->workerType);
    free(enigmaConfig);
}

// Funció principal per a Enigma
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Ús: ./enigma <fitxer de configuració>\n");
        return 1;
    }

    EnigmaConfig *enigmaConfig = malloc(sizeof(EnigmaConfig));
    readConfigFile(argv[1], enigmaConfig);

    // Connecta a Gotham
    int gothamSocket = connect_to_server(enigmaConfig->ipGotham, enigmaConfig->portGotham);
    if (gothamSocket < 0) {
        printF("Error connectant a Gotham\n");
        alliberarMemoria(enigmaConfig);
        return 1;
    }

    // Envia un missatge de registre com a treballador de text o media
    char registerMessage[FRAME_SIZE];
    snprintf(registerMessage, FRAME_SIZE, "REGISTER WORKER %s", enigmaConfig->workerType);
    send_frame(gothamSocket, registerMessage, strlen(registerMessage));

    printF("Registrat a Gotham com a worker de tipus: ");
    printF(enigmaConfig->workerType);
    printF("\n");

    // Espera i processa les peticions de Gotham
    char buffer[FRAME_SIZE];
    while (1) {
        if (receive_frame(gothamSocket, buffer) < 0) {
            printF("Error rebent la petició de Gotham\n");
            break;
        }
        printF("Petició rebuda: ");
        printF(buffer);
        printF("\n");

        // Processa la petició (aquí pots afegir distorsions simulades)
        char response[FRAME_SIZE];
        snprintf(response, FRAME_SIZE, "Processed: %.244s", buffer);


        send_frame(gothamSocket, response, strlen(response));
        printF("Resposta enviada a Gotham: ");
        printF(response);
        printF("\n");
    }

    close(gothamSocket);
    alliberarMemoria(enigmaConfig);
    return 0;
}