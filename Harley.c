#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "Networking/Networking.h"

// Variable global para el socket
int gothamSocket = -1;

// Funciones auxiliares
void signalHandler(int sig) {
    if (sig == SIGINT) {
        printF(ANSI_COLOR_YELLOW "[INFO]: Desconnectant de Gotham...\n" ANSI_COLOR_RESET);
        if (gothamSocket >= 0) {
            close(gothamSocket);
        }
        exit(0);
    }
}

void printColor(const char *color, const char *message) {
    printF(color);
    printF(message);
    printF(ANSI_COLOR_RESET "\n");
}

// Serialización y deserialización consistentes
// Serialización con depuración para asegurar datos completos
void serialize_frame(const Frame *frame, char *buffer) {
    memset(buffer, 0, FRAME_SIZE);
    snprintf(buffer, FRAME_SIZE, "%02x|%04d|%u|%04x|%s",
             frame->type, frame->data_length, frame->timestamp,
             frame->checksum, frame->data);
}

int deserialize_frame(const char *buffer, Frame *frame) {
    if (buffer == NULL || frame == NULL) {
        return -1;
    }

    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0'; // Asegurar terminación nula

    char *token = strtok(localBuffer, "|");
    if (!token) return -1;
    frame->type = (uint8_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->data_length = (uint16_t)strtoul(token, NULL, 10);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->timestamp = (uint32_t)strtoul(token, NULL, 10);

    token = strtok(NULL, "|");
    if (!token) return -1;
    frame->checksum = (uint16_t)strtoul(token, NULL, 16);

    token = strtok(NULL, "|");
    if (!token) return -1;
    strncpy(frame->data, token, frame->data_length);
    frame->data[frame->data_length] = '\0'; // Terminación nula

    return 0; // Frame válido
}

// Implementación de processReceivedFrame
void processReceivedFrame(int gothamSocket, const char *buffer) {
    Frame frame;
    int result = deserialize_frame(buffer, &frame);
    if (result != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error al deserialitzar el frame rebut.");
        return;
    }

    // Validar el checksum
    uint16_t calculated_checksum = calculate_checksum(frame.data, frame.data_length);
    if (calculated_checksum != frame.checksum) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Trama rebuda amb checksum invàlid.");
        return;
    }

    // Procesar según el tipo de frame
    if (frame.type == 0x10) {
        printColor(ANSI_COLOR_CYAN, "[INFO]: DISTORT rebuda. Processant...");

        // Simula processament
        sleep(2); // Simula el temps de processament

        // Resposta DISTORT_OK
        Frame response;
        memset(&response, 0, sizeof(Frame));
        response.type = 0x10;
        strncpy(response.data, "DISTORT_OK", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.timestamp = (uint32_t)time(NULL);
        response.checksum = calculate_checksum(response.data, response.data_length);

        char responseBuffer[FRAME_SIZE];
        serialize_frame(&response, responseBuffer);

        printColor(ANSI_COLOR_CYAN, "[DEBUG]: Resposta enviada a Gotham:");
        printColor(ANSI_COLOR_YELLOW, responseBuffer);

        write(gothamSocket, responseBuffer, FRAME_SIZE);
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: DISTORT completat correctament.");
    }
    else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Tipus de comanda desconegut.");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Ús correcte: ./harley <fitxer de configuració>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    HarleyConfig *harleyConfig = malloc(sizeof(HarleyConfig));
    if (!harleyConfig) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error assignant memòria per a la configuració.");
        return 1;
    }
    readConfigFileGeneric(argv[1], harleyConfig, CONFIG_HARLEY);

    if (!harleyConfig->workerType || !harleyConfig->ipFleck || harleyConfig->portFleck <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Configuració de Harley incorrecta o incompleta.");
        free(harleyConfig);
        return 1;
    }

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuració carregada correctament.");

    gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha pogut connectar a Gotham.");
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Connectat correctament a Gotham!");

    Frame frame;
    char buffer[FRAME_SIZE];

    frame.type = 0x02; // Tipo REGISTER
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
             harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    serialize_frame(&frame, buffer);

    if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant el registre a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha rebut resposta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    Frame response;
    if (deserialize_frame(buffer, &response) != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error al deserialitzar la resposta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    uint16_t resp_checksum = calculate_checksum(response.data, response.data_length);
    if (resp_checksum != response.checksum) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Resposta de Gotham amb checksum invàlid.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (response.type == 0x02 && strcmp(response.data, "CON_OK") == 0) {
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrat correctament a Gotham.");
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: El registre ha estat rebutjat per Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    while (1) {
        if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error rebent la petició de Gotham.");
            break;
        }
        processReceivedFrame(gothamSocket, buffer);
    }

    close(gothamSocket);
    free(harleyConfig);
    return 0;
}

