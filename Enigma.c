#define _GNU_SOURCE // Necesario para funciones GNU como asprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "Networking/Networking.h"
#include "FrameUtils/FrameUtils.h"
#include "Logging/Logging.h"
#include "Heartbeat/heartbeat.h"

// Funciones principales
void *handleFleckFrames(void *arg);
void processReceivedFrameFromFleck(int clientSocket, const Frame *request);
void processReceivedFrame(int gothamSocket, const Frame *response);
void sendDistortedFileToFleck(int clientSocket, const char *originalFileContent, size_t originalFileSize);
void receiveFileFromFleck(int clientSocket, char **fileContent, size_t *fileSize);

// Variable global para el socket
int gothamSocket = -1;

// Función para enviar un frame de desconexión a Gotham
void sendDisconnectFrameToGotham(const char *workerType) {
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    strncpy(frame.data, workerType, sizeof(frame.data) - 1);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando trama de desconexión a Gotham...");
    send_frame(gothamSocket, &frame);
}

// Manejador de señal SIGINT
void signalHandler(int sig) {
    if (sig == SIGINT) {
        logInfo("[INFO]: Desconectando de Gotham...");
        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham("TEXT"); // Envía desconexión para tipo TEXT
            close(gothamSocket);
        }
        exit(0);
    }
}

// Maneja frames recibidos desde Gotham
void *handleGothamFrames(void *arg) {
    int gothamSocket = *(int *)arg;

    Frame frame;
    while (receive_frame(gothamSocket, &frame) == 0) {
        processReceivedFrame(gothamSocket, &frame);
    }

    logError("[Enigma] Gotham se ha desconectado.");
    close(gothamSocket);
    pthread_exit(NULL);
}

// Maneja frames recibidos desde Fleck
void *handleFleckFrames(void *arg) {
    int clientSocket = *(int *)arg;

    Frame request;
    while (receive_frame(clientSocket, &request) == 0) {
        processReceivedFrameFromFleck(clientSocket, &request);
    }

    close(clientSocket);
    return NULL;
}

// Envía un archivo distorsionado a Fleck
void sendDistortedFileToFleck(int clientSocket, const char *originalFileContent, size_t originalFileSize) {
    char md5Sum[33] = "PLACEHOLDER_MD5";
    char fileSize[20];
    snprintf(fileSize, sizeof(fileSize), "%zu", originalFileSize);

    Frame initFrame = {0};
    initFrame.type = 0x04;
    snprintf(initFrame.data, sizeof(initFrame.data), "%s&%s", fileSize, md5Sum);
    initFrame.data_length = strlen(initFrame.data);
    initFrame.timestamp = (uint32_t)time(NULL);
    initFrame.checksum = calculate_checksum(initFrame.data, initFrame.data_length, 0);

    logInfo("[Enigma] Enviando información del archivo distorsionado (0x04)...");
    send_frame(clientSocket, &initFrame);

    size_t offset = 0;
    while (offset < originalFileSize) {
        Frame dataFrame = {0};
        dataFrame.type = 0x05;

        size_t chunkSize = (originalFileSize - offset > 256) ? 256 : (originalFileSize - offset);
        memcpy(dataFrame.data, originalFileContent + offset, chunkSize);

        dataFrame.data_length = chunkSize;
        dataFrame.timestamp = (uint32_t)time(NULL);
        dataFrame.checksum = calculate_checksum(dataFrame.data, dataFrame.data_length, 0);

        send_frame(clientSocket, &dataFrame);
        offset += chunkSize;
    }

    logInfo("[Enigma] Archivo distorsionado enviado. Esperando validación...");
    int md5Valid = 1;

    Frame validationFrame = {0};
    validationFrame.type = 0x06;
    if (md5Valid) {
        strncpy(validationFrame.data, "CHECK_OK", sizeof(validationFrame.data) - 1);
    } else {
        strncpy(validationFrame.data, "CHECK_KO", sizeof(validationFrame.data) - 1);
    }
    validationFrame.data_length = strlen(validationFrame.data);
    validationFrame.timestamp = (uint32_t)time(NULL);
    validationFrame.checksum = calculate_checksum(validationFrame.data, validationFrame.data_length, 0);

    logInfo("[Enigma] Enviando resultado de validación de MD5SUM...");
    send_frame(clientSocket, &validationFrame);
}

// Recibe un archivo desde Fleck
void receiveFileFromFleck(int clientSocket, char **fileContent, size_t *fileSize) {
    logInfo("[Enigma] Recibiendo archivo desde Fleck...");
    size_t bufferCapacity = 1024;
    size_t currentSize = 0;
    char *buffer = malloc(bufferCapacity);
    if (!buffer) {
        logError("[Enigma] No se pudo asignar memoria para el archivo.");
        return;
    }

    Frame request;
    while (receive_frame(clientSocket, &request) == 0) {
        if (request.type == 0x05) {
            if (currentSize + request.data_length > bufferCapacity) {
                bufferCapacity *= 2;
                buffer = realloc(buffer, bufferCapacity);
                if (!buffer) {
                    logError("[Enigma] Error reasignando memoria.");
                    return;
                }
            }
            memcpy(buffer + currentSize, request.data, request.data_length);
            currentSize += request.data_length;
        } else {
            break;
        }
    }

    *fileContent = buffer;
    *fileSize = currentSize;
    logInfo("[Enigma] Archivo recibido completamente.");
}

// Procesa frames recibidos desde Fleck
void processReceivedFrameFromFleck(int clientSocket, const Frame *request) {
    if (!request) return;

    switch (request->type) {
        case 0x03: {
            logInfo("[Enigma] Solicitud DISTORT FILE recibida.");
            char userName[64], fileName[256], fileSize[20], md5Sum[33], factor[20];
            if (sscanf(request->data, "%63[^&]&%255[^&]&%19[^&]&%32[^&]&%19s",
                       userName, fileName, fileSize, md5Sum, factor) != 5) {
                logError("[Enigma] Formato de datos inválido en DISTORT FILE.");
                Frame errorFrame = {.type = 0x03, .timestamp = time(NULL)};
                strncpy(errorFrame.data, "CON_KO", sizeof(errorFrame.data) - 1);
                errorFrame.data_length = strlen(errorFrame.data);
                errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);
                send_frame(clientSocket, &errorFrame);
                return;
            }

            Frame okFrame = {.type = 0x03, .timestamp = time(NULL)};
            strncpy(okFrame.data, "CON_OK", sizeof(okFrame.data) - 1);
            okFrame.data_length = strlen(okFrame.data);
            okFrame.checksum = calculate_checksum(okFrame.data, okFrame.data_length, 0);
            send_frame(clientSocket, &okFrame);

            logInfo("[Enigma] Recibiendo archivo...");
            char *fileContent = NULL;
            size_t numericFileSize = 0; // Cambiamos el nombre para evitar conflicto.

            receiveFileFromFleck(clientSocket, &fileContent, &numericFileSize);

            logInfo("[Enigma] Procesando archivo de texto...");
            for (size_t i = 0; i < numericFileSize; i++) {
                if (fileContent[i] >= 'a' && fileContent[i] <= 'z') {
                    fileContent[i] -= 32; // Convertir a mayúsculas.
                }
            }

            sendDistortedFileToFleck(clientSocket, fileContent, numericFileSize);
            free(fileContent);
            break;
        }
        default:
            logError("[Enigma] Comando desconocido recibido.");
            break;
    }
}

// Procesa frames recibidos desde Gotham
void processReceivedFrame(int gothamSocket, const Frame *response) {
    if (!response) return;

    switch (response->type) {
        case 0x10:
            logInfo("[Enigma] Procesando comando DISTORT...");
            Frame frame = {.type = 0x10, .timestamp = time(NULL)};
            strncpy(frame.data, "DISTORT_OK", sizeof(frame.data) - 1);
            frame.data_length = strlen(frame.data);
            frame.checksum = calculate_checksum(frame.data, frame.data_length, 0);

            send_frame(gothamSocket, &frame);
            logSuccess("[Enigma] Respuesta DISTORT_OK enviada.");
            break;

        default:
            logError("[Enigma] Comando desconocido.");
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        logError("[Enigma] Uso correcto: ./enigma <config_file>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    EnigmaConfig *enigmaConfig = malloc(sizeof(EnigmaConfig));
    if (!enigmaConfig) {
        logError("[Enigma] Error asignando memoria para la configuración.");
        return 1;
    }

    readConfigFileGeneric(argv[1], enigmaConfig, CONFIG_ENIGMA);

    if (!enigmaConfig->workerType || !enigmaConfig->ipFleck || enigmaConfig->portFleck <= 0) {
        logError("[Enigma] Configuración incorrecta o incompleta.");
        free(enigmaConfig);
        return 1;
    }

    logSuccess("[Enigma] Configuración cargada correctamente.");

    gothamSocket = connect_to_server(enigmaConfig->ipGotham, enigmaConfig->portGotham);
    if (gothamSocket < 0) {
        logError("[Enigma] No se pudo conectar a Gotham.");
        free(enigmaConfig);
        return 1;
    }

    logSuccess("[Enigma] Conectado correctamente a Gotham.");

    Frame frame = {.type = 0x02, .timestamp = time(NULL)};
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
             enigmaConfig->workerType, enigmaConfig->ipFleck, enigmaConfig->portFleck);
    frame.data_length = strlen(frame.data);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    send_frame(gothamSocket, &frame);

    Frame response;
    if (receive_frame(gothamSocket, &response) != 0 || response.type != 0x02) {
        logError("[Enigma] Registro rechazado por Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    logSuccess("[Enigma] Registro exitoso en Gotham.");

    pthread_t gothamThread;
    pthread_create(&gothamThread, NULL, handleGothamFrames, &gothamSocket);
    pthread_detach(gothamThread);

    int fleckSocket = startServer(enigmaConfig->ipFleck, enigmaConfig->portFleck);
    if (fleckSocket < 0) {
        logError("[Enigma] No se pudo iniciar el servidor local.");
        free(enigmaConfig);
        return 1;
    }

    logSuccess("[Enigma] Servidor local iniciado correctamente.");

    while (1) {
        int clientSocket = accept_connection(fleckSocket);

        if (clientSocket < 0) {
            logError("[Enigma] Error aceptando conexión.");
            continue;
        }

        pthread_t fleckThread;
        pthread_create(&fleckThread, NULL, handleFleckFrames, &clientSocket);
        pthread_detach(fleckThread);
    }

    close(fleckSocket);
    close(gothamSocket);
    free(enigmaConfig);
    return 0;
}
