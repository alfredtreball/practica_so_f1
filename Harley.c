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

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"

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
    if (argc != 2) {
        printF("Ús: ./harley <fitxer de configuració>\n");
        exit(1);
    }

    // Crea la variable local per a la configuració de Harley
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    if (!harleyConfig) {
        printF("Error assignant memòria per a la configuració\n");
        return 1;
    }

    readConfigFileGeneric(argv[1], harleyConfig, CONFIG_HARLEY);

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

    // Bucle per rebre i processar peticions
    char buffer[FRAME_SIZE];
    while (1) {
        int data_length;  // Declara una variable para almacenar la longitud de los datos

        if (receive_frame(gothamSocket, buffer, &data_length) < 0) {
            printF("Error rebent la petició de Gotham\n");
            break;
        }
        printF("Petició rebuda: ");
        printF(buffer);
        printF("\n");

        //Verifiquem si és tipus media
        char *filename = buffer;

        if (esTipoValido(filename, harleyConfig->workerType)) {
            printF("Fitxer acceptat: ");
            printF(filename);
            printF("\n");
        } else {
            printF("Fitxer no acceptat: ");
            printF(filename);
            printF("\nWorker media només accepta tipus de fitxer: .wav, .jpg, .png\n");
            continue;
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
    alliberarMemoria(harleyConfig);
    return 0;
}
