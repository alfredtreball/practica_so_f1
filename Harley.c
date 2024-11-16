#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Colores para la salida
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define BOLD "\033[1m"

// Includes adicionales
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

// Funció per imprimir missatges amb colors
void printColor(const char *color, const char *message) {
    write(1, color, strlen(color));
    write(1, message, strlen(message));
    write(1, RESET, strlen(RESET));
    write(1, "\n", 1);
}

// Funció principal
int main(int argc, char *argv[]) {
    // Validació de l'entrada
    if (argc != 2) {
        printColor(RED, "[ERROR]: Ús correcte: ./harley <fitxer de configuració>");
        exit(1);
    }

    printColor(CYAN, "[INFO]: Llegint el fitxer de configuració...");

    // Crea la variable local per a la configuració de Harley
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    if (!harleyConfig) {
        printColor(RED, "[ERROR]: Error assignant memòria per a la configuració.");
        return 1;
    }

    readConfigFileGeneric(argv[1], harleyConfig, CONFIG_HARLEY);

    // Mostrem la informació de configuració llegida
    printColor(GREEN, "[SUCCESS]: Configuració carregada correctament.");
    char *configMessage;
    asprintf(&configMessage,
             "[INFO]: IP Gotham: %s\n[INFO]: Port Gotham: %d\n[INFO]: Worker Type: %s\n[INFO]: IP Fleck: %s\n[INFO]: Port Fleck: %d",
             harleyConfig->ipGotham, harleyConfig->portGotham, harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
    printColor(CYAN, configMessage);
    free(configMessage);

    // Connecta a Gotham
    printColor(CYAN, "[INFO]: Connectant a Gotham...");
    int gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(RED, "[ERROR]: No s'ha pogut connectar a Gotham.");
        alliberarMemoria(harleyConfig);
        return 1;
    }
    printColor(GREEN, "[SUCCESS]: Connectat correctament a Gotham!");

    // Envia un missatge de registre com a treballador (amb format correcte)
    char registerMessage[FRAME_SIZE];
    snprintf(registerMessage, FRAME_SIZE, "REGISTER WORKER&%s&%s&%d",
             harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
    if (send_frame(gothamSocket, registerMessage, strlen(registerMessage)) < 0) {
        printColor(RED, "[ERROR]: No s'ha pogut enviar el missatge de registre a Gotham.");
        close(gothamSocket);
        alliberarMemoria(harleyConfig);
        return 1;
    }

    printColor(GREEN, "[SUCCESS]: Worker registrat correctament a Gotham.");
    
    // Rebem la confirmació de registre
    char buffer[FRAME_SIZE];
    int data_length;
    if (receive_frame(gothamSocket, buffer, &data_length) < 0) {
        printColor(RED, "[ERROR]: No s'ha rebut la confirmació de registre des de Gotham.");
        close(gothamSocket);
        alliberarMemoria(harleyConfig);
        return 1;
    }
    printColor(CYAN, "[INFO]: Confirmació de registre rebuda:");

    if (strcmp(buffer, "CON_KO") == 0) {
        printColor(RED, "[ERROR]: Gotham ha rebutjat el registre.");
        close(gothamSocket);
        alliberarMemoria(harleyConfig);
        return 1;
    }
    printColor(GREEN, buffer);

    // Bucle per rebre i processar peticions
    printColor(CYAN, "[INFO]: Esperant peticions de Gotham...");
    while (1) {
        if (receive_frame(gothamSocket, buffer, &data_length) < 0) {
            printColor(RED, "[ERROR]: Error rebent la petició de Gotham.");
            break;
        }

        char *filename = buffer;
        printColor(YELLOW, "[INFO]: Petició rebuda:");
        printColor(MAGENTA, buffer);

        // Verifiquem si és tipus media
        if (esTipoValido(filename, harleyConfig->workerType)) {
            printColor(GREEN, "[SUCCESS]: Fitxer acceptat.");
        } else {
            printColor(RED, "[ERROR]: Fitxer no acceptat.");
            printColor(YELLOW, "Worker media només accepta tipus de fitxer: .wav, .jpg, .png");
            continue;
        }

        // Preparar i enviar la resposta
        char response[FRAME_SIZE];
        snprintf(response, FRAME_SIZE, "Processed: %.244s", buffer);

        send_frame(gothamSocket, response, strlen(response));
        printColor(GREEN, "[SUCCESS]: Resposta enviada a Gotham:");
        printColor(MAGENTA, response);
    }

    printColor(YELLOW, "[INFO]: Desconnectant de Gotham...");
    close(gothamSocket);
    printColor(GREEN, "[SUCCESS]: Desconnectat correctament de Gotham.");

    // Alliberem memòria
    alliberarMemoria(harleyConfig);
    printColor(GREEN, "[SUCCESS]: Memòria alliberada correctament.");
    return 0;
}
