#define _GNU_SOURCE // Necesario para funciones GNU como asprintf

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
#include "FrameUtils/FrameUtils.h"

// Variable global para el socket
int gothamSocket = -1;

// Función para manejar señales como SIGINT
void signalHandler(int sig) {
    if (sig == SIGINT) {
        printF(ANSI_COLOR_YELLOW "[INFO]: Desconnectant de Gotham...\n" ANSI_COLOR_RESET);
        if (gothamSocket >= 0) {
            close(gothamSocket);
        }
        exit(0);
    }
}

// Función para imprimir mensajes con color
void printColor(const char *color, const char *message) {
    printF(color);
    printF(message);
    printF(ANSI_COLOR_RESET "\n");
}

// Procesa un frame recibido de Gotham
void processReceivedFrame(int gothamSocket, const Frame *response) {
    if (!response) return;

    // Manejo de comandos
    switch (response->type) {
        case 0x10: // DISTORT
            printColor(ANSI_COLOR_CYAN, "[INFO]: Procesando comando DISTORT...");
            Frame frame = { .type = 0x10, .timestamp = time(NULL) };
            strncpy(frame.data, "DISTORT_OK", sizeof(frame.data) - 1);
            frame.data_length = strlen(frame.data);
            frame.checksum = calculate_checksum(frame.data, frame.data_length);

            send_frame(gothamSocket, &frame);
            printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Respuesta DISTORT_OK enviada.");
            break;

        default:
            printColor(ANSI_COLOR_RED, "[ERROR]: Comando desconocido recibido.");
            Frame errorFrame = { .type = 0xFF, .timestamp = time(NULL) };
            strncpy(errorFrame.data, "CMD_KO", sizeof(errorFrame.data) - 1);
            errorFrame.data_length = strlen(errorFrame.data);
            errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length);

            send_frame(gothamSocket, &errorFrame);
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Ús correcte: ./harley <fitxer de configuració>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    // Carga de configuración
    HarleyConfig *harleyConfig = malloc(sizeof(HarleyConfig));
    if (!harleyConfig) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error asignando memoria para la configuración.");
        return 1;
    }

    readConfigFileGeneric(argv[1], harleyConfig, CONFIG_HARLEY);

    if (!harleyConfig->workerType || !harleyConfig->ipFleck || harleyConfig->portFleck <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Configuración de Harley incorrecta o incompleta.");
        free(harleyConfig);
        return 1;
    }

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuración cargada correctamente.");

    // Conexión a Gotham
    gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo conectar a Gotham.");
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Conectado correctamente a Gotham!");

    // Registro en Gotham
    Frame frame = { .type = 0x02, .timestamp = time(NULL) };
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
             harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
    frame.data_length = strlen(frame.data);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    if (send_frame(gothamSocket, &frame) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviando el registro a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Esperar respuesta del registro
    Frame response;
    if (receive_frame(gothamSocket, &response) != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se recibió respuesta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (response.type == 0x02 && response.data_length == 0) {
        // ACK con DATA vacío
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrado correctamente en Gotham.");
    } else if (response.type == 0x02 && strcmp(response.data, "CON_KO") == 0) {
        // NACK con DATA = "CON_KO"
        printColor(ANSI_COLOR_RED, "[ERROR]: Registro rechazado por Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta inesperada durante el registro.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Inicia el servidor local de Harley
    int localSocket = startServer(harleyConfig->ipFleck, harleyConfig->portFleck);
    if (localSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo iniciar el servidor local de Harley.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Servidor local de Harley iniciado correctamente.");

    // Bucle principal para recibir frames de Gotham
    while (1) {
        Frame response;
        if (receive_frame(gothamSocket, &response) == 0) {
            processReceivedFrame(gothamSocket, &response);
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error recibiendo frame de Gotham.");
            break;
        }
    }

    // Limpieza y cierre
    close(localSocket);
    close(gothamSocket);
    free(harleyConfig);
    return 0;
}
