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
void serialize_frame(const Frame *frame, char *buffer) {
    memset(buffer, 0, FRAME_SIZE);
    snprintf(buffer, FRAME_SIZE, "%02x|%04x|%u|%04x|%s",
         frame->type, frame->data_length, frame->timestamp,
         frame->checksum, frame->data);
}

int deserialize_frame(const char *buffer, Frame *frame) {
    if (buffer == NULL || frame == NULL) {
        // Error: Parámetros de entrada inválidos
        return -1;
    }

    memset(frame, 0, sizeof(Frame));

    // Hacemos una copia del buffer para no modificar el original
    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0'; // Aseguramos la terminación nula

    char *token = NULL;

    // Campo TYPE
    token = strtok(localBuffer, "|");
    if (token != NULL && strlen(token) > 0) {
        frame->type = (uint8_t)token[0];
    } else {
        // Error: Campo TYPE faltante o inválido
        return -1;
    }

    // Campo DATA_LENGTH
    token = strtok(NULL, "|");
    if (token != NULL) {
        unsigned int data_length = atoi(token);
        if (data_length > sizeof(frame->data)) {
            // Error: DATA_LENGTH fuera de rango
            return -1;
        }
        frame->data_length = (uint16_t)data_length;
    } else {
        // Error: Campo DATA_LENGTH faltante
        return -1;
    }

    // Campo TIMESTAMP
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->timestamp = (uint32_t)strtoul(token, NULL, 10);
    } else {
        // Error: Campo TIMESTAMP faltante
        return -1;
    }

    // Campo CHECKSUM
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->checksum = (uint16_t)strtoul(token, NULL, 16);
    } else {
        // Error: Campo CHECKSUM faltante
        return -1;
    }

    // Campo DATA
    token = strtok(NULL, "|");
    if (token != NULL) {
        // Aseguramos que no copiamos más datos de los que podemos manejar
        size_t dataToCopy = frame->data_length < sizeof(frame->data) - 1 ? frame->data_length : sizeof(frame->data) - 1;
        strncpy(frame->data, token, dataToCopy);
        frame->data[dataToCopy] = '\0'; // Aseguramos la terminación nula
    } else {
        // Error: Campo DATA faltante
        return -1;
    }

    return 0; // Éxito
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

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuració carregada correctament.");

    gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha pogut connectar a Gotham.");
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Connectat correctament a Gotham!");

    // Enviar frame de registro
    // Crear el frame de registre
    Frame frame;
    memset(&frame, 0, sizeof(Frame));
    frame.type = 0x02; // Tipus REGISTER

    // Serialitzar les dades (WorkerType, IP i Port)
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d", harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);

    // Calcular longitud de les dades
    frame.data_length = strlen(frame.data);
    // Calcular el checksum
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    // Serialitzar el frame complet
    char buffer[FRAME_SIZE];
    serialize_frame(&frame, buffer);

    // Enviar el frame serialitzat a Gotham
    if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant el registre a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant el registre a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Esperar respuesta de Gotham
    if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha rebut resposta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    Frame response;
    int result = deserialize_frame(buffer, &response);
    if (result != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error al deserialitzar la resposta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Validar checksum de la respuesta
    uint16_t resp_checksum = calculate_checksum(response.data, response.data_length);
    if (resp_checksum != response.checksum) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Resposta de Gotham amb checksum invàlid");
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

    // Ahora esperar peticiones de Gotham
    while (1) {
        if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error rebent la petició de Gotham.");
            break;
        }

        // Procesar el frame recibido
        processReceivedFrame(gothamSocket, buffer);
    }

    close(gothamSocket);
    free(harleyConfig);
    return 0;
}
