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
    snprintf(buffer, FRAME_SIZE, "%c|%04hu|%u|%04hx|%s",
             frame->type, frame->data_length, frame->timestamp, frame->checksum, frame->data);
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
    if (frame.type == 0x20) { // Tipo de trama para ANALYZE_TEXT
        // Procesar análisis de texto
        printColor(ANSI_COLOR_CYAN, "[INFO]: Sol·licitud d'anàlisi de text rebuda.");
        printColor(ANSI_COLOR_YELLOW, frame.data);

        // Aquí implementarías la lógica de análisis de texto
        // Por ejemplo, podrías llamar a una función que procese el texto indicado en frame.data

        // Simular procesamiento
        printColor(ANSI_COLOR_CYAN, "[INFO]: Analitzant el text (encara no implementat).");

        // Enviar ACK de recepción
        Frame ack_frame;
        memset(&ack_frame, 0, sizeof(Frame));
        ack_frame.type = 0x21; // Tipo de ACK para ANALYZE_OK
        strncpy(ack_frame.data, "ANALYZE_OK", sizeof(ack_frame.data) - 1);
        ack_frame.data_length = strlen(ack_frame.data);
        ack_frame.timestamp = (uint32_t)time(NULL);
        ack_frame.checksum = calculate_checksum(ack_frame.data, ack_frame.data_length);

        char ack_buffer[FRAME_SIZE];
        serialize_frame(&ack_frame, ack_buffer);
        if (write(gothamSocket, ack_buffer, FRAME_SIZE) < 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant l'ACK a Gotham.");
        } else {
            printColor(ANSI_COLOR_GREEN, "[SUCCESS]: ACK enviat correctament.");
        }
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Tipus de comanda desconegut.");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Ús correcte: ./enigma <fitxer de configuració>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    EnigmaConfig *enigmaConfig = malloc(sizeof(EnigmaConfig));
    if (!enigmaConfig) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error assignant memòria per a la configuració.");
        return 1;
    }
    readConfigFileGeneric(argv[1], enigmaConfig, CONFIG_ENIGMA);

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuració carregada correctament.");

    gothamSocket = connect_to_server(enigmaConfig->ipGotham, enigmaConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha pogut connectar a Gotham.");
        free(enigmaConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Connectat correctament a Gotham!");

    // Enviar frame de registro
    Frame frame;
    memset(&frame, 0, sizeof(Frame));
    frame.type = 0x02; // Tipo de frame para REGISTER

    // Utilizamos los miembros ipFleck y portFleck como la IP y puerto del propio Enigma
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
             enigmaConfig->workerType, enigmaConfig->ipFleck, enigmaConfig->portFleck);

    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    char buffer[FRAME_SIZE];
    serialize_frame(&frame, buffer);
    if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant el registre a Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    // Esperar respuesta de Gotham
    if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha rebut resposta de Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    Frame response;
    int result = deserialize_frame(buffer, &response);
    if (result != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error al deserialitzar la resposta de Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    // Validar checksum de la respuesta
    uint16_t resp_checksum = calculate_checksum(response.data, response.data_length);
    if (resp_checksum != response.checksum) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Resposta de Gotham amb checksum invàlid");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    if (response.type == 0x02 && strcmp(response.data, "CON_OK") == 0) {
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrat correctament a Gotham.");
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: El registre ha estat rebutjat per Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
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
    free(enigmaConfig);
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Memòria alliberada correctament.");
    return 0;
}
