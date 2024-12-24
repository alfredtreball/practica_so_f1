#define _GNU_SOURCE // Necesario para funciones GNU como asprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>  // Para stat y mkdir
#include <sys/types.h> // Para stat y mkdir

#include "GestorTramas/GestorTramas.h"
#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "Networking/Networking.h"
#include "DataConversion/DataConversion.h"
#include "FrameUtils/FrameUtils.h"
#include "FrameUtilsBinary/FrameUtilsBinary.h"
#include "Logging/Logging.h"
#include "MD5SUM/md5Sum.h"
#include "File_transfer/file_transfer.h"
#include "Compression/so_compression.h"
#include "HarleyCompression/compression_handler.h"

#define HARLEY_PATH_FILES "harley_directory/"
#define HARLEY_TYPE "MEDIA"

void *handleFleckFrames(void *arg);
void processReceivedFrameFromFleck(int clientSocket, const Frame *request);
void processReceivedFrame(int gothamSocket, const Frame *response);
void processBinaryFrameFromFleck(BinaryFrame *binaryFrame, size_t expectedFileSize, size_t *currentFileSize, int clientSocket);
void enviaTramaArxiuDistorsionat(int clientSocket, const char *fileSizeCompressed, const char *compressedMD5, const char *compressedFilePath);
void send_frame_with_error(int clientSocket, const char *errorMessage);
void send_frame_with_ok(int clientSocket);
void sendMD5Response(int clientSocket, const char *status);

HarleyConfig *globalharleyConfig = NULL;

// Variable global para el socket
int gothamSocket = -1;
float resultStatus;
volatile sig_atomic_t stop = 0;

// Estructura para los argumentos del hilo
typedef struct {
    int clientSocket;
    char *filePath;
    off_t fileSize;
    char *compressedPath; // Ruta del archivo comprimido
} SendCompressedFileArgs;

char receivedFileName[256] = {0}; // Nombre del archivo recibido
int tempFileDescriptor = -1;      // Descriptor del archivo temporal
char expectedMD5[33];
char receivedFactor[20];

void sendDisconnectFrameToGotham(const char *mediaType)
{
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    strncpy(frame.data, mediaType, sizeof(frame.data) - 1);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando trama de desconexión a Gotham...");
    escribirTrama(gothamSocket, &frame);
}

// Función para manejar señales como SIGINT
void signalHandler(int sig) {
    if (sig == SIGINT) {
        logInfo("[INFO]: Señal SIGINT recibida. Cerrando conexiones...");
        sendDisconnectFrameToGotham(HARLEY_TYPE);
         stop = 1;

        if (gothamSocket >= 0) {
            close(gothamSocket);
            gothamSocket = -1;
        }

        if (tempFileDescriptor >= 0) {
            close(tempFileDescriptor);
            tempFileDescriptor = -1;
        }

        if (globalharleyConfig) {
            if (globalharleyConfig->workerType) free(globalharleyConfig->workerType);
            if (globalharleyConfig->ipFleck) free(globalharleyConfig->ipFleck);
            if (globalharleyConfig->ipGotham) free(globalharleyConfig->ipGotham);
            free(globalharleyConfig);
        }

        pthread_exit(NULL); // Asegura que los hilos finalicen
        exit(0);
    }
}

// Función para imprimir mensajes con color
void printColor(const char *color, const char *message)
{
    printF(color);
    printF(message);
    printF(ANSI_COLOR_RESET "\n");
}

void *handleGothamFrames(void *arg)
{
    int gothamSocket = *(int *)arg;

    Frame frame;
    while (!stop && leerTrama(gothamSocket, &frame) == 0)
    {
        processReceivedFrame(gothamSocket, &frame);
    }

    logError("[ERROR]: Gotham se ha desconectado.");
    close(gothamSocket);
    pthread_exit(NULL);
}

int receive_frame_header(int clientSocket, Frame *header) {
    size_t totalBytesRead = 0; // Cambiado a size_t
    while (totalBytesRead < sizeof(Frame)) {
        ssize_t bytesRead = recv(clientSocket, ((char *)header) + totalBytesRead, sizeof(Frame) - totalBytesRead, MSG_PEEK);
        if (bytesRead <= 0) {
            logError("[ERROR]: Error al leer la cabecera del frame.");
            return -1;
        }
        totalBytesRead += bytesRead;
    }
    return 0;
}

void *handleFleckFrames(void *arg) {
    int clientSocket = *(int *)arg;
    free(arg);

    size_t expectedFileSize = 0;   // Tamaño esperado del archivo (de la trama 0x03)
    size_t currentFileSize = 0;    // Tamaño recibido hasta el momento

    while (1) {
        Frame header;

        // Leer encabezado y manejar errores de desconexión
        if (receive_frame_header(clientSocket, &header) != 0) {
            logWarning("[WARNING]: Conexión cerrada por Fleck o error al leer la cabecera.");
            break;
        }

        // Procesar trama binaria 0x05
        if (header.type == 0x05) {
            BinaryFrame binaryFrame;
            if (leerTramaBinaria(clientSocket, &binaryFrame) == 0) {
                processBinaryFrameFromFleck(&binaryFrame, expectedFileSize, &currentFileSize, clientSocket);
            } else {
                logError("[ERROR]: Error al recibir trama binaria.");
                break;
            }
        } else { // Procesar tramas no binarias
            Frame request;
            if (leerTrama(clientSocket, &request) == 0) {
                // Procesar trama 0x03
                if (request.type == 0x03) {
                    logInfo("[INFO]: Solicitud DISTORT FILE recibida.");
                    char userName[64], fileName[256], fileSizeStr[20], md5Sum[33], factor[20];
                    if (sscanf(request.data, "%63[^&]&%255[^&]&%19[^&]&%32[^&]&%19s",
                            userName, fileName, fileSizeStr, md5Sum, factor) != 5) {
                        logError("[ERROR]: Formato inválido en solicitud DISTORT FILE.");
                        send_frame_with_error(clientSocket, "CON_KO");
                        break;
                    }

                    // Guardar metadatos globales
                    strncpy(receivedFileName, fileName, sizeof(receivedFileName) - 1);
                    strncpy(expectedMD5, md5Sum, sizeof(expectedMD5) - 1);
                    strncpy(receivedFactor, factor, sizeof(receivedFactor) - 1);

                    // Validar tamaño del archivo
                    expectedFileSize = strtoull(fileSizeStr, NULL, 10);
                    if (expectedFileSize == 0) {
                        logError("[ERROR]: Tamaño del archivo inválido.");
                        send_frame_with_error(clientSocket, "CON_KO");
                        break;
                    }

                    // Crear archivo en HARLEY_PATH_FILES
                    char finalFilePath[512];
                    snprintf(finalFilePath, sizeof(finalFilePath), "%s%s", HARLEY_PATH_FILES, receivedFileName);
                    tempFileDescriptor = open(finalFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

                    if (tempFileDescriptor < 0) {
                        logError("[ERROR]: No se pudo crear el archivo en HARLEY_PATH_FILES.");
                        perror("Error al abrir el archivo");
                        break;
                    }

                    logInfo("[INFO]: Archivo temporal creado correctamente:");
                    logInfo(finalFilePath);

                    // Responder a Fleck con OK
                    send_frame_with_ok(clientSocket);
                }
            } else {
                logWarning("[WARNING]: Error al recibir trama estándar.");
                break;
            }
        }
    }

    return NULL;
}

// Función para enviar el archivo comprimido en tramas binarias
void *sendCompressedFileToFleck(void *args) {
    SendCompressedFileArgs *sendArgs = (SendCompressedFileArgs *)args;
    int clientSocket = sendArgs->clientSocket;
    const char *filePath = sendArgs->filePath;
    free(sendArgs); // Liberar memoria de los argumentos

    printf("[DEBUG]: Intentando abrir el archivo comprimido en la ruta: %s", filePath);

    if (access(filePath, F_OK) != 0) {
        logError("[ERROR]: El archivo comprimido no existe. Verifica el proceso de compresión.");
        return NULL;
    }

    printf("[INFO]: El archivo comprimido se creó correctamente: %s", filePath);

    int fd = open(filePath, O_RDONLY, 0666);
    if (fd < 0) {
        logError("[ERROR]: No se pudo abrir el archivo comprimido.");
        return NULL;
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize <= 0) {
        logError("[ERROR]: El archivo comprimido está vacío o no se pudo calcular su tamaño.");
        close(fd);
        return NULL;
    }
    logInfo("[DEBUG]: Tamaño del archivo comprimido: ");
    char sizeStr[32]; // Buffer para almacenar la cadena del tamaño
    snprintf(sizeStr, sizeof(sizeStr), "%ld", fileSize); // Convertir off_t a cadena
    logInfo(sizeStr); // Pasar la cadena a logInfo
    lseek(fd, 0, SEEK_SET);

    BinaryFrame frame = {0};
    frame.type = 0x05; // Tipo de trama binaria

    char buffer[247]; // Máxima longitud permitida para DATA
    ssize_t bytesRead;

    logInfo("[INFO]: Iniciando envío del archivo comprimido por tramas 0x05...");

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        printf("[DEBUG]: Bytes leídos del archivo comprimido: %zd", bytesRead);
        frame.data_length = bytesRead;
        memcpy(frame.data, buffer, bytesRead);
        frame.timestamp = (uint32_t)time(NULL);
        frame.checksum = calculate_checksum_binary(frame.data, frame.data_length, 1);

        if (escribirTramaBinaria(clientSocket, &frame) < 0) {
            logError("[ERROR]: Fallo al enviar trama 0x05.");
            close(fd);
            return NULL;
        }

        usleep(1000); // Pausa para evitar congestión
        
    }

    if (bytesRead < 0) {
        logError("[ERROR]: Fallo al leer el archivo comprimido.");
    } else {
        logInfo("[INFO]: Envío del archivo comprimido completado.");
    }

    close(fd);
    return NULL;
}

void send_frame_with_error(int clientSocket, const char *errorMessage)
{
    Frame errorFrame = {0};
    errorFrame.type = 0x03; // Tipo estándar para error
    strncpy(errorFrame.data, errorMessage, sizeof(errorFrame.data) - 1);
    errorFrame.data_length = strlen(errorFrame.data);
    errorFrame.timestamp = (uint32_t)time(NULL);
    errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);

    logInfo("[INFO]: Enviando trama de error al cliente.");
    if (escribirTrama(clientSocket, &errorFrame) < 0)
    {
        logError("[ERROR]: Error al enviar trama de error.");
    }
}

void send_frame_with_ok(int clientSocket)
{
    Frame okFrame = {0};
    okFrame.type = 0x03;     // Tipo estándar para confirmación
    okFrame.data_length = 0; // Sin datos adicionales
    okFrame.timestamp = (uint32_t)time(NULL);
    okFrame.checksum = calculate_checksum(okFrame.data, okFrame.data_length, 0);

    logInfo("[INFO]: Enviando trama de confirmación OK al cliente.");
    if (escribirTrama(clientSocket, &okFrame) < 0)
    {
        logError("[ERROR]: Error al enviar trama de confirmación.");
    }
}

void processBinaryFrameFromFleck(BinaryFrame *binaryFrame, size_t expectedFileSize, size_t *currentFileSize, int clientSocket) {
    if (!binaryFrame) {
        logError("[ERROR]: BinaryFrame recibido nulo.");
        return;
    }

    if (tempFileDescriptor < 0) {
        logError("[ERROR]: No hay un archivo temporal abierto para escribir.");
        return;
    }

    // Escribir los datos en el archivo temporal
    if (write(tempFileDescriptor, binaryFrame->data, binaryFrame->data_length) != binaryFrame->data_length) {
        logError("[ERROR]: Error al escribir en el archivo temporal.");
        close(tempFileDescriptor);
        tempFileDescriptor = -1;
        return;
    }

    *currentFileSize += binaryFrame->data_length;

    // Verificar si se recibió el archivo completo
    if (*currentFileSize >= expectedFileSize) {
        logInfo("[INFO]: Archivo completo recibido. Procesando...");

        // Cerrar el archivo temporal
        close(tempFileDescriptor);
        tempFileDescriptor = -1;

        char finalFilePath[512];
        snprintf(finalFilePath, sizeof(finalFilePath), "%s%s", HARLEY_PATH_FILES, receivedFileName);

        char calculatedMD5[33] = {0};
        calculate_md5(finalFilePath, calculatedMD5);

        if (strcmp(calculatedMD5, "ERROR") == 0) {
            logError("[ERROR]: No se pudo calcular el MD5 del archivo recibido.");
            send_frame_with_error(clientSocket, "CHECK_KO");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
            return;
        }

        logInfo("[INFO]: MD5 calculado del archivo recibido:");
        logInfo(calculatedMD5);

        if (strcmp(expectedMD5, calculatedMD5) == 0) {
            logInfo("[SUCCESS]: El MD5 coincide. Procesando compresión...");
            sendMD5Response(clientSocket, "CHECK_OK");
            // Procesar la compresión
            int result = process_compression(finalFilePath, atoi(receivedFactor));
            if (result != 0) {
                logError("[ERROR]: Fallo en la compresión del archivo.");
                return;
            }

            printf("[INFO]: Proceso de compresión completado exitosamente para el archivo: %s", finalFilePath);

            // Crear la ruta del archivo comprimido dinámicamente
            char *compressedFilePath = NULL;
            if (asprintf(&compressedFilePath, "%s", finalFilePath) == -1) {
                logError("[ERROR]: No se pudo asignar memoria para la ruta del archivo comprimido.");
                return;
            }

            // Validar que el archivo comprimido existe
            if (access(compressedFilePath, F_OK) != 0) {
                logError("[ERROR]: El archivo comprimido no se creó correctamente.");
                unlink(finalFilePath);
                free(compressedFilePath);
                return;
            }

            // Calcular el MD5 del archivo comprimido
            char compressedMD5[33] = {0};
            calculate_md5(compressedFilePath, compressedMD5);

            if (strcmp(compressedMD5, "ERROR") == 0) {
                logError("[ERROR]: No se pudo calcular el MD5 del archivo comprimido.");
                unlink(compressedFilePath); // Eliminar el archivo comprimido
                free(compressedFilePath);
                return;
            }

            logInfo("[INFO]: MD5 calculado del archivo comprimido:");
            logInfo(compressedMD5);

            // Calcular el tamaño del archivo comprimido
            int fd_media_compressed = open(compressedFilePath, O_RDONLY);
            if (fd_media_compressed < 0) {
                logError("[ERROR]: No se pudo abrir el archivo especificado.");
                free(compressedFilePath);
                return;
            }

            off_t fileSizeCompressed = lseek(fd_media_compressed, 0, SEEK_END);
            if (fileSizeCompressed < 0) {
                logError("[ERROR]: No se pudo calcular el tamaño del archivo.");
                close(fd_media_compressed);
                free(compressedFilePath);
                return;
            }
            close(fd_media_compressed);

            // Convertir tamaño del archivo a string
            char *fileSizeStrCompressed = NULL;
            if (asprintf(&fileSizeStrCompressed, "%ld", fileSizeCompressed) == -1) {
                logError("[ERROR]: No se pudo asignar memoria para el tamaño del archivo.");
                free(compressedFilePath);
                return;
            }

            logInfo("\n[INFO]: Path del archivo:");
            logInfo(compressedFilePath);

            logInfo("\n[INFO]: Tamaño del archivo calculado:");
            logInfo(fileSizeStrCompressed);

            // Enviar la trama del archivo distorsionado
            enviaTramaArxiuDistorsionat(clientSocket, fileSizeStrCompressed, compressedMD5, compressedFilePath);

            // Liberar memoria dinámica asignada
            free(compressedFilePath);
            free(fileSizeStrCompressed);
        } else {
            logError("[ERROR]: El MD5 no coincide. Archivo recibido está corrupto.");
            sendMD5Response(clientSocket, "CHECK_KO");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
        }
    }
}

void sendMD5Response(int clientSocket, const char *status) {
    Frame response = {0};
    response.type = 0x06; // Tipo de trama para respuesta MD5
    strncpy(response.data, status, sizeof(response.data) - 1);
    response.data_length = strlen(response.data);
    response.timestamp = (uint32_t)time(NULL);
    response.checksum = calculate_checksum(response.data, response.data_length, 1);

    logInfo("[INFO]: Enviando respuesta de MD5 al cliente.");
    if (escribirTrama(clientSocket, &response) < 0) {
        logError("[ERROR]: Error al enviar la respuesta MD5.");
    }
}

// Función para enviar la trama del archivo distorsionado
void enviaTramaArxiuDistorsionat(int clientSocket, const char *fileSizeCompressed, const char *compressedMD5, const char *compressedFilePath) {
    Frame frame = {0};
    frame.type = 0x04;
    snprintf(frame.data, sizeof(frame.data), "%s&%s", fileSizeCompressed, compressedMD5);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando trama del archivo distorsionado...");
    if (escribirTrama(clientSocket, &frame) < 0) {
        logError("[ERROR]: Fallo al enviar la trama del archivo distorsionado.");
    }
    
    logInfo("[SUCCESS]: Trama del archivo distorsionado enviada correctamente.");

    // Preparar para enviar el archivo comprimido en tramas binarias
    SendCompressedFileArgs *args = malloc(sizeof(SendCompressedFileArgs));
    if (!args) {
        logError("[ERROR]: No se pudo asignar memoria para los argumentos del hilo de envío.");
        return;
    }

    // Asignar los argumentos necesarios
    args->clientSocket = clientSocket;
    args->filePath = strdup(compressedFilePath  ); // Asegúrate de duplicar la ruta
    args->fileSize = strtoull(fileSizeCompressed, NULL, 10);

    // Crear un hilo para enviar el archivo comprimido
    pthread_t sendThread;
    if (pthread_create(&sendThread, NULL, sendCompressedFileToFleck, args) != 0) {
        logError("[ERROR]: No se pudo crear el hilo para enviar el archivo comprimido.");
        free(args->filePath);
        free(args);
        return;
    }

    pthread_detach(sendThread); // Liberar el hilo automáticamente al finalizar
}

// Procesa un frame recibido de Gotham
void processReceivedFrame(int gothamSocket, const Frame *response)
{
    if (!response)
        return;

    // Manejo de comandos
    switch (response->type)
    {
    case 0x08:
        logInfo("[INFO]: Trama 0x08 recibida. Promovido a Worker principal.");
        break;

    case 0x10: // DISTORT
        printColor(ANSI_COLOR_CYAN, "[INFO]: Procesando comando DISTORT...");
        Frame frame = {.type = 0x10, .timestamp = time(NULL)};
        strncpy(frame.data, "DISTORT_OK", sizeof(frame.data) - 1);
        frame.data_length = strlen(frame.data);
        frame.checksum = calculate_checksum(frame.data, frame.data_length, 0);

        escribirTrama(gothamSocket, &frame);
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Respuesta DISTORT_OK enviada.");
        break;

    default:
        Frame errorFrame = {0};
        errorFrame.type = 0x09; // Tipo de trama de error
        errorFrame.data_length = 0; // Sin datos adicionales
        errorFrame.timestamp = (uint32_t)time(NULL);
        errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 1);

        // Enviar la trama de error
        escribirTrama(gothamSocket, &errorFrame);
        break;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Ús correcte: ./harley <fitxer de configuració>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    // Carga de configuración
    HarleyConfig *harleyConfig = malloc(sizeof(HarleyConfig));
    if (!harleyConfig)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error asignando memoria para la configuración.");
        return 1;
    }

    globalharleyConfig = harleyConfig;

    readConfigFileGeneric(argv[1], harleyConfig, CONFIG_HARLEY);

    if (!harleyConfig->workerType || !harleyConfig->ipFleck || harleyConfig->portFleck <= 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Configuración de Harley incorrecta o incompleta.");
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Configuración cargada correctamente.");

    // Conexión a Gotham para registro
    gothamSocket = connect_to_server(harleyConfig->ipGotham, harleyConfig->portGotham);
    if (gothamSocket < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo conectar a Gotham.");
        free(harleyConfig);
        return 1;
    }
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Conectado correctamente a Gotham!");

    // Registro en Gotham
    Frame frame = {.type = 0x02, .timestamp = time(NULL)};
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
             harleyConfig->workerType, harleyConfig->ipFleck, harleyConfig->portFleck);
    frame.data_length = strlen(frame.data);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (escribirTrama(gothamSocket, &frame) < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviando el registro a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Esperar respuesta del registro
    Frame response;
    if (leerTrama(gothamSocket, &response) != 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se recibió respuesta de Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    if (response.type == 0x02 && response.data_length == 0)
    {
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Worker registrado correctamente en Gotham.");
    }
    else if (response.type == 0x02 && strcmp(response.data, "CON_KO") == 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Registro rechazado por Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }
    else
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta inesperada durante el registro.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Crear hilo para manejar mensajes de Gotham
    pthread_t gothamThread;
    if (pthread_create(&gothamThread, NULL, handleGothamFrames, &gothamSocket) != 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo crear el hilo para Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }
    pthread_detach(gothamThread);

    int fleckSocket = startServer(harleyConfig->ipFleck, harleyConfig->portFleck);
    if (fleckSocket < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo iniciar el servidor local de Harley.");
        free(harleyConfig);
        return 1;
    }
    printf("Servidor Harley iniciado en IP: %s, Puerto: %d\n", harleyConfig->ipFleck, harleyConfig->portFleck);
    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Servidor local de Harley iniciado correctamente.");

    // Bucle para manejar conexiones de Fleck
    while (1) {
        while (!stop) {
            // Continuar aceptando conexiones de Fleck
            int clientSocket = accept_connection(fleckSocket);

            if (clientSocket < 0) {
                logError("[ERROR]: No se pudo aceptar la conexión del cliente.");
                continue;
            }

            pthread_t clientThread;
            int *socketArg = malloc(sizeof(int));
            if (!socketArg) {
                logError("[ERROR]: No se pudo asignar memoria para el socket del cliente.");
                close(clientSocket);
                continue;
            }
            *socketArg = clientSocket;

            if (pthread_create(&clientThread, NULL, handleFleckFrames, socketArg) != 0) {
                logError("[ERROR]: No se pudo crear el hilo para manejar al cliente.");
                close(clientSocket);
                free(socketArg);
                continue;
            }

            pthread_detach(clientThread);
        }

        int clientSocket = accept_connection(fleckSocket);

        if (clientSocket < 0) {
            logError("[ERROR]: No se pudo aceptar la conexión del Fleck.");
            continue;
        }

        if (clientSocket >= 0) {
            pthread_t fleckThread;
            int *socketArg = malloc(sizeof(int));
            if (!socketArg) {
                logError("[ERROR]: No se pudo asignar memoria para el socket del cliente.");
                close(clientSocket); // Cierra el socket si hay un error
                continue;
            }
            *socketArg = clientSocket;

            if (pthread_create(&fleckThread, NULL, handleFleckFrames, socketArg) != 0) {
                logError("[ERROR]: No se pudo crear el hilo para manejar el cliente.");
                close(clientSocket); // Cierra el socket si hay un error
                free(socketArg);
                continue;
            }

            pthread_detach(fleckThread);
        }
    }

    // Limpieza y cierre
    close(fleckSocket);
    close(gothamSocket);
    free(harleyConfig);
    return 0;
}