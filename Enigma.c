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

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"

// Defineix els colors ANSI
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"
#define YELLOW "\033[33m"

// Funció per alliberar la memòria de l'estructura EnigmaConfig
void alliberarMemoria(EnigmaConfig *enigmaConfig) {
    if (enigmaConfig->ipGotham) free(enigmaConfig->ipGotham);
    if (enigmaConfig->ipFleck) free(enigmaConfig->ipFleck);
    if (enigmaConfig->directory) free(enigmaConfig->directory);
    if (enigmaConfig->workerType) free(enigmaConfig->workerType);
    free(enigmaConfig);
}

// Funció per imprimir missatges amb color
void printColor(const char *color, const char *message) {
    write(1, color, strlen(color));
    write(1, message, strlen(message));
    write(1, RESET, strlen(RESET));
    write(1, "\n", 1);
}

// Funció principal per a Enigma
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printColor(RED, "[ERROR]: Ús correcte: ./enigma <fitxer de configuració>");
        return 1;
    }

    // Assignem memòria per a la configuració
    EnigmaConfig *enigmaConfig = malloc(sizeof(EnigmaConfig));
    if (!enigmaConfig) {
        printColor(RED, "[ERROR]: Error assignant memòria per a la configuració.");
        return 1;
    }

    printColor(CYAN, "[INFO]: Llegint el fitxer de configuració...");
    readConfigFileGeneric(argv[1], enigmaConfig, CONFIG_ENIGMA);

    // Mostrem la informació de configuració llegida
    printColor(GREEN, "[SUCCESS]: Configuració carregada correctament.");
    char *configMessage;
    asprintf(&configMessage,
             "[INFO]: IP Gotham: %s\n[INFO]: Port Gotham: %d\n[INFO]: Worker Type: %s",
             enigmaConfig->ipGotham, enigmaConfig->portGotham, enigmaConfig->workerType);
    printColor(CYAN, configMessage);
    free(configMessage);

    // Connectem a Gotham
    printColor(CYAN, "[INFO]: Connectant a Gotham...");
    int gothamSocket = connect_to_server(enigmaConfig->ipGotham, enigmaConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(RED, "[ERROR]: No s'ha pogut connectar a Gotham.");
        alliberarMemoria(enigmaConfig);
        return 1;
    }
    printColor(GREEN, "[SUCCESS]: Connectat correctament a Gotham!");

    // Enviem un missatge de registre a Gotham amb separadors "&"
    char *registerMessage;
    asprintf(&registerMessage, "REGISTER WORKER&%s&%s&%d",
         enigmaConfig->workerType, enigmaConfig->ipFleck, enigmaConfig->portFleck);
    printf("[DEBUG]: Mensaje a enviar: %s\n", registerMessage);

    if (send_frame(gothamSocket, registerMessage, strlen(registerMessage)) < 0) {
        printColor(RED, "[ERROR]: No s'ha pogut enviar el missatge de registre a Gotham.");
        close(gothamSocket);
        free(registerMessage);
        alliberarMemoria(enigmaConfig);
        return 1;
    }
    free(registerMessage);
    printColor(GREEN, "[SUCCESS]: Worker registrat correctament a Gotham.");

    // Rebem la confirmació de registre o error
    char buffer[FRAME_SIZE];
    int data_length;
    if (receive_frame(gothamSocket, buffer, &data_length) < 0) {
        printColor(RED, "[ERROR]: No s'ha pogut rebre la confirmació de registre.");
        close(gothamSocket);
        alliberarMemoria(enigmaConfig);
        return 1;
    }
    if (strcmp(buffer, "CON_KO") == 0) {
        printColor(RED, "[ERROR]: Gotham ha rebutjat el registre.");
        close(gothamSocket);
        alliberarMemoria(enigmaConfig);
        return 1;
    }
    printColor(GREEN, "[SUCCESS]: Confirmació de registre rebuda de Gotham.");

    // Enigma es desconnecta després del registre
    printColor(YELLOW, "[INFO]: Desconnectant de Gotham...");
    close(gothamSocket);
    printColor(GREEN, "[SUCCESS]: Desconnectat correctament de Gotham.");

    // Alliberem memòria
    alliberarMemoria(enigmaConfig);
    printColor(GREEN, "[SUCCESS]: Memòria alliberada correctament.");
    return 0;
}
