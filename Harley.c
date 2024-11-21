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
void processReceivedFrame(int gothamSocket, const Frame *frame) {
    if (!frame) return;

    if (frame->type == 0x10) { // DISTORT
        printColor(ANSI_COLOR_CYAN, "[INFO]: Procesando comando DISTORT...");
        Frame response = { .type = 0x10, .timestamp = time(NULL) };
        strncpy(response.data, "DISTORT_OK", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.checksum = calculate_checksum(response.data, response.data_length);

        send_frame(gothamSocket, &response);
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Respuesta DISTORT_OK enviada.");
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Comando desconocido recibido.");
        Frame response = { .type = 0xFF, .timestamp = time(NULL) };
        strncpy(response.data, "CMD_KO", sizeof(response.data) - 1);
        response.data_length = strlen(response.data);
        response.checksum = calculate_checksum(response.data, response.data_length);

        send_frame(gothamSocket, &response);
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

    if (receive_frame(gothamSocket, &frame) != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se recibió respuesta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (frame.type == 0x02 && strcmp(frame.data, "CON_OK") == 0) {
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrado correctamente en Gotham.");
        
        // Extraemos los datos del worker para mostrarlos (workerType, ipFleck, portFleck)
        char *log_message = NULL;
        asprintf(&log_message, "[INFO]: Tipo de worker: %s\n"
                            "[INFO]: IP registrada: %s\n"
                            "[INFO]: Puerto registrado: %d\n",
                harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
        printF(log_message);
        free(log_message);

    } else if (frame.type == 0xFF) {
        printColor(ANSI_COLOR_YELLOW, "[INFO]: Gotham notifica el cierre del sistema.");
        close(gothamSocket);
        free(harleyConfig);
        return 0;
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: El registro fue rechazado por Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Bucle principal para recibir frames
    while (1) {
        Frame frame;
        if (receive_frame(gothamSocket, &frame) == 0) {
            processReceivedFrame(gothamSocket, &frame);
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error recibiendo frame de Gotham.");
            break;
        }
    }

    // Limpieza y cierre
    close(gothamSocket);
    free(harleyConfig);
    return 0;
}
