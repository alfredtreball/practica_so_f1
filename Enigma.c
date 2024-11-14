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
#include "StringUtils.h" // Inclou la funció esTipoValido
#include "Networking.h"

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
    if (!enigmaConfig) {
        printF("Error assignant memòria per a la configuració\n");
        return 1;
    }

    readConfigFileGeneric(argv[1], enigmaConfig, CONFIG_ENIGMA);

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

    // Bucle per rebre i processar peticions
    char buffer[FRAME_SIZE];
    while (1) {
        if (receive_frame(gothamSocket, buffer) < 0) {
            printF("Error rebent la petició de Gotham\n");
            break;
        }
        printF("Petició rebuda: ");
        printF(buffer);
        printF("\n");

        //Verifiquem si és tipus text
        char *filename = buffer;

        if (esTipoValido(filename, enigmaConfig->workerType)) {
            printF("Fitxer acceptat: ");
            printF(filename);
            printF("\n");
        } else {
            printF("Fitxer no acceptat: ");
            printF(filename);
            printF("\nWorker text només accepta tipus de fitxer: .txt\n");
        }

        // Preparar i enviar la resposta
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
