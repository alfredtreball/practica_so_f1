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
#include "MD5SUM/md5Sum.h"
#include "File_transfer/file_transfer.h"


void *handleFleckFrames(void *arg);
void processReceivedFrameFromFleck(int clientSocket, const Frame *request);
void processReceivedFrame(int gothamSocket, const Frame *response);

// Variable global para el socket
int gothamSocket = -1;

void sendDisconnectFrameToGotham(const char *mediaType) {
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    strncpy(frame.data, mediaType, sizeof(frame.data) - 1);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando trama de desconexión a Gotham...");
    send_frame(gothamSocket, &frame);
}

// Función para manejar señales como SIGINT
void signalHandler(int sig) {
    if (sig == SIGINT) {
        logInfo("[INFO]: Desconectando de Gotham...");
        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham("MEDIA"); // Envía desconexión para tipo MEDIA
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

void *handleGothamFrames(void *arg) {
    int gothamSocket = *(int *)arg;

    Frame frame;
    while (receive_frame(gothamSocket, &frame) == 0) {
        processReceivedFrame(gothamSocket, &frame);
    }

    logError("[ERROR]: Gotham se ha desconectado.");
    close(gothamSocket);
    pthread_exit(NULL);
}

void *handleFleckFrames(void *arg) {
    int clientSocket = *(int *)arg;

    Frame request;
    while (1) { // Mantener el bucle activo
        if (receive_frame(clientSocket, &request) == 0) {
            logInfo("[INFO]: Frame recibido de Fleck.");
            processReceivedFrameFromFleck(clientSocket, &request);
        } else {
            logWarning("[WARNING]: Conexión cerrada por Fleck o error en receive_frame.");
            break; // Salir del bucle si la conexión es cerrada por el cliente
        }
    }

    logInfo("[INFO]: Cerrando conexión del cliente Fleck.");
    close(clientSocket);
    return NULL;
}

void send_frame_with_error(int clientSocket, const char *errorMessage) {
    Frame errorFrame = {0};
    errorFrame.type = 0x03; // Tipo estándar para error
    strncpy(errorFrame.data, errorMessage, sizeof(errorFrame.data) - 1);
    errorFrame.data_length = strlen(errorFrame.data);
    errorFrame.timestamp = (uint32_t)time(NULL);
    errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);

    logInfo("[INFO]: Enviando trama de error al cliente.");
    if (send_frame(clientSocket, &errorFrame) < 0) {
        logError("[ERROR]: Error al enviar trama de error.");
    }
}

void send_frame_with_ok(int clientSocket) {
    Frame okFrame = {0};
    okFrame.type = 0x03; // Tipo estándar para confirmación
    okFrame.data_length = 0; // Sin datos adicionales
    okFrame.timestamp = (uint32_t)time(NULL);
    okFrame.checksum = calculate_checksum(okFrame.data, okFrame.data_length, 0);

    logInfo("[INFO]: Enviando trama de confirmación OK al cliente.");
    if (send_frame(clientSocket, &okFrame) < 0) {
        logError("[ERROR]: Error al enviar trama de confirmación.");
    }
}

void sendDistortedFileToFleck(int clientSocket, const char *originalFileContent, size_t originalFileSize) {
    logInfo("[INFO]: Procesando el archivo distorsionado.");

    // Calcular MD5SUM del archivo original
    char md5Sum[33];
    calculate_md5("ruta_del_archivo_original", md5Sum);

    // **Trama 0x04**: Enviar información del archivo (FileSize & MD5SUM)
    char fileSize[16];
    snprintf(fileSize, sizeof(fileSize), "%zu", originalFileSize);

    Frame infoFrame = {0};
    infoFrame.type = 0x04;
    snprintf(infoFrame.data, sizeof(infoFrame.data), "%s&%s", fileSize, md5Sum);
    infoFrame.data_length = strlen(infoFrame.data);
    infoFrame.timestamp = (uint32_t)time(NULL);
    infoFrame.checksum = calculate_checksum(infoFrame.data, infoFrame.data_length, 0);

    logInfo("[INFO]: Enviando trama 0x04 con información del archivo.");
    send_frame(clientSocket, &infoFrame);

    // **Tramas 0x05**: Enviar datos del archivo en fragmentos
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

    logInfo("[INFO]: Todos los fragmentos del archivo distorsionado enviados.");

    // **Trama 0x06**: Validar MD5SUM del archivo recibido
    char receivedMd5[33];
    calculate_md5("ruta_del_archivo_distorsionado", receivedMd5);

    Frame validationFrame = {0};
    validationFrame.type = 0x06;
    if (strcmp(md5Sum, receivedMd5) == 0) {
        strncpy(validationFrame.data, "CHECK_OK", sizeof(validationFrame.data) - 1);
    } else {
        strncpy(validationFrame.data, "CHECK_KO", sizeof(validationFrame.data) - 1);
    }
    validationFrame.data_length = strlen(validationFrame.data);
    validationFrame.timestamp = (uint32_t)time(NULL);
    validationFrame.checksum = calculate_checksum(validationFrame.data, validationFrame.data_length, 0);

    logInfo("[INFO]: Enviando resultado de validación de MD5SUM...");
    send_frame(clientSocket, &validationFrame);

    logInfo("[INFO]: Resultado de validación del MD5SUM enviado.");
}


void receiveFileFromFleck(int clientSocket, char **fileContent, size_t *fileSize) {
    logInfo("[INFO]: Recibiendo archivo desde Fleck...");

    Frame request;
    if (receive_frame(clientSocket, &request) != 0 || request.type != 0x04) {
        logError("[ERROR]: Trama inicial 0x04 no recibida o inválida.");
        return;
    }

    // Validar contenido de la trama
    char fileSizeStr[20], md5Sum[33];
    if (sscanf(request.data, "%19[^&]&%32s", fileSizeStr, md5Sum) != 2) {
        logError("[ERROR]: Formato inválido en la trama 0x04.");
        send_frame_with_error(clientSocket, "FORMAT_INVALID");
        return;
    }

    // Aceptar PLACEHOLDER valores para tamaño y MD5
    if (strcmp(fileSizeStr, "PLACEHOLDER_SIZE") == 0) {
        logWarning("[WARNING]: Tamaño del archivo es un marcador de posición.");
        *fileSize = 0; // Valor por defecto para continuar
    } else {
        *fileSize = atol(fileSizeStr); // Convertir tamaño real
    }

    if (strcmp(md5Sum, "PLACEHOLDER_MD5") == 0) {
        logWarning("[WARNING]: MD5 es un marcador de posición.");
    }

    logInfo("[INFO]: Trama 0x04 recibida.");
    logInfo("[INFO]: Tamaño del archivo:");
    logInfo(fileSizeStr);
    logInfo("[INFO]: MD5 del archivo:");
    logInfo(md5Sum);

    // Continuar con la recepción de datos
    size_t bufferCapacity = 1024;
    size_t currentSize = 0;
    char *buffer = malloc(bufferCapacity);
    if (!buffer) {
        logError("[ERROR]: No se pudo asignar memoria para el archivo.");
        return;
    }

    while (1) {
        if (receive_frame(clientSocket, &request) == 0) {
            if (request.type == 0x05) {
                if (currentSize + request.data_length > bufferCapacity) {
                    bufferCapacity *= 2;
                    buffer = realloc(buffer, bufferCapacity);
                    if (!buffer) {
                        logError("[ERROR]: No se pudo reasignar memoria.");
                        free(buffer);
                        return;
                    }
                }
                memcpy(buffer + currentSize, request.data, request.data_length);
                currentSize += request.data_length;
            } else if (request.type == 0x06) {
                logInfo("[INFO]: Trama de validación (0x06) recibida.");
                break;
            } else {
                logWarning("[WARNING]: Tipo de trama inesperado recibido.");
                break;
            }
        } else {
            logError("[ERROR]: Error al recibir datos del archivo.");
            break;
        }
    }

    *fileContent = buffer;
    *fileSize = currentSize;
    logInfo("[INFO]: Archivo recibido completamente.");
}

void processReceivedFrameFromFleck(int clientSocket, const Frame *request) {
    if (!request) {
        logError("[ERROR]: Frame recibido nulo.");
        return;
    }

    switch (request->type) {
        case 0x03: { // Solicitud DISTORT FILE
            logInfo("[INFO]: Solicitud DISTORT FILE recibida.");
            char userName[64], fileName[256], fileSize[20], md5Sum[33], factor[20];
            if (sscanf(request->data, "%63[^&]&%255[^&]&%19[^&]&%32[^&]&%19s",
                       userName, fileName, fileSize, md5Sum, factor) != 5) {
                logError("[ERROR]: Formato inválido en solicitud DISTORT FILE.");
                send_frame_with_error(clientSocket, "CON_KO");
                return;
            }

            // Confirmar recepción
            send_frame_with_ok(clientSocket);

            // Recibir archivo de Fleck (0x04 y 0x05)
            char *fileContent = NULL;
            size_t numericFileSize = 0;

            receiveFileFromFleck(clientSocket, &fileContent, &numericFileSize);

            if (!fileContent || numericFileSize == 0) {
                logError("[ERROR]: No se recibió archivo válido desde Fleck.");
                return;
            }

            logInfo("[INFO]: Archivo recibido correctamente. Procesando...");

            // Enviar archivo distorsionado a Fleck
            sendDistortedFileToFleck(clientSocket, fileContent, numericFileSize);
            free(fileContent);
            break;
        }
        default:
            logWarning("[WARNING]: Frame desconocido recibido.");
            break;
    }
}


// Procesa un frame recibido de Gotham
void processReceivedFrame(int gothamSocket, const Frame *response) {
    if (!response) return;

    // Manejo de comandos
    switch (response->type) {
        case 0x08:
            logInfo("[INFO]: Trama 0x08 recibida. Promovido a Worker principal.");
            break;

        case 0x10: // DISTORT
            printColor(ANSI_COLOR_CYAN, "[INFO]: Procesando comando DISTORT...");
            Frame frame = { .type = 0x10, .timestamp = time(NULL) };
            strncpy(frame.data, "DISTORT_OK", sizeof(frame.data) - 1);
            frame.data_length = strlen(frame.data);
            frame.checksum = calculate_checksum(frame.data, frame.data_length, 0);

            send_frame(gothamSocket, &frame);
            printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Respuesta DISTORT_OK enviada.");
            break;

        default:
            printColor(ANSI_COLOR_RED, "[ERROR]: Comando desconocido recibido.");
            Frame errorFrame = { .type = 0xFF, .timestamp = time(NULL) };
            strncpy(errorFrame.data, "CMD_KO", sizeof(errorFrame.data) - 1);
            errorFrame.data_length = strlen(errorFrame.data);
            errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);

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
    printf("[DEBUG]: Configuración cargada: IP=%s, PUERTO=%d", harleyConfig->ipFleck, harleyConfig->portFleck);

    if (!harleyConfig->workerType || !harleyConfig->ipFleck || harleyConfig->portFleck <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Configuración de Harley incorrecta o incompleta.");
        free(harleyConfig);
        return 1;
    }

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuración cargada correctamente.");

    // Conexión a Gotham para registro
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
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

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
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrado correctamente en Gotham.");
    } else if (response.type == 0x02 && strcmp(response.data, "CON_KO") == 0) {
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

    // Crear hilo para manejar mensajes de Gotham
    pthread_t gothamThread;
    if (pthread_create(&gothamThread, NULL, handleGothamFrames, &gothamSocket) != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo crear el hilo para Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }
    pthread_detach(gothamThread);


    int fleckSocket = startServer(harleyConfig->ipFleck, harleyConfig->portFleck);
    if (fleckSocket < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo iniciar el servidor local de Harley.");
        free(harleyConfig);
        return 1;
    }
    printf("Servidor Harley iniciado en IP: %s, Puerto: %d\n", harleyConfig->ipFleck, harleyConfig->portFleck);
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Servidor local de Harley iniciado correctamente.");

    // Bucle para manejar conexiones de Fleck
    while (1) {
        int clientSocket = accept_connection(fleckSocket);

        if (clientSocket < 0) {
            logError("[ERROR]: No se pudo aceptar la conexión del Fleck.");
            continue;
        }

        if (clientSocket >= 0) {
            pthread_t fleckThread;
            pthread_create(&fleckThread, NULL, (void *(*)(void *))handleFleckFrames, &clientSocket);
            pthread_detach(fleckThread);
        }
    }

    // Limpieza y cierre
    close(fleckSocket);
    close(gothamSocket);
    free(harleyConfig);
    return 0;
}