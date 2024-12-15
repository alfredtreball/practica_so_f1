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

void *handleFleckFrames(void *arg);
void processReceivedFrameFromFleck(int clientSocket, const Frame *request);
void processReceivedFrame(int gothamSocket, const Frame *response);
void processBinaryFrameFromFleck(BinaryFrame *binaryFrame);

HarleyConfig *globalharleyConfig = NULL;

// Variable global para el socket
int gothamSocket = -1;

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
    send_frame(gothamSocket, &frame);
}

// Función para manejar señales como SIGINT
void signalHandler(int sig)
{
    if (sig == SIGINT)
    {
        logInfo("[INFO]: Desconectando de Gotham...");
        if (gothamSocket >= 0)
        {
            sendDisconnectFrameToGotham("MEDIA"); // Envía desconexión para tipo MEDIA
            close(gothamSocket);
        }
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
    while (receive_frame(gothamSocket, &frame) == 0)
    {
        processReceivedFrame(gothamSocket, &frame);
    }

    logError("[ERROR]: Gotham se ha desconectado.");
    close(gothamSocket);
    pthread_exit(NULL);
}

int receive_frame_header(int clientSocket, Frame *header)
{
    ssize_t bytesRead = recv(clientSocket, header, sizeof(Frame), MSG_PEEK);
    if (bytesRead != sizeof(Frame))
    {
        logError("[ERROR]: Error al leer la cabecera del frame.");
        return -1;
    }
    return 0;
}

void *handleFleckFrames(void *arg)
{
    int clientSocket = *(int *)arg;

    while (1)
    {
        Frame header;
        if (receive_frame_header(clientSocket, &header) != 0)
        {
            logWarning("[WARNING]: Conexión cerrada por Fleck o error al leer la cabecera.");
            break;
        }

        if (header.type == 0x05)
        {
            // Manejo de trama binaria
            BinaryFrame binaryFrame;
            if (receive_frame_binary(clientSocket, &binaryFrame) == 0)
            {
                processBinaryFrameFromFleck(&binaryFrame);
            }
            else
            {
                logError("[ERROR]: Error al recibir trama binaria.");
                break;
            }
        }
        else
        {
            // Manejo de tramas no binarias
            Frame request;
            if (receive_frame(clientSocket, &request) == 0)
            {
                processReceivedFrameFromFleck(clientSocket, &request);
            }
            else
            {
                logWarning("[WARNING]: Error al recibir trama estándar.");
                break;
            }
        }
    }

    logInfo("[INFO]: Cerrando conexión del cliente Fleck.");
    close(clientSocket);
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
    if (send_frame(clientSocket, &errorFrame) < 0)
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
    if (send_frame(clientSocket, &okFrame) < 0)
    {
        logError("[ERROR]: Error al enviar trama de confirmación.");
    }
}

void processBinaryFrameFromFleck(BinaryFrame *binaryFrame)
{
    if (!binaryFrame)
    {
        logError("[ERROR]: BinaryFrame recibido nulo.");
        return;
    }

    if (tempFileDescriptor < 0)
    {
        logError("[ERROR]: No hay un archivo temporal abierto para escribir.");
        return;
    }

    // Escribir los datos en el archivo temporal
    if (write(tempFileDescriptor, binaryFrame->data, binaryFrame->data_length) != binaryFrame->data_length)
    {
        logError("[ERROR]: Error al escribir en el archivo temporal.");
        return;
    }
}

void sendMD5Response(int clientSocket, const char *status)
{
    Frame response = {0};
    response.type = 0x06; // Tipo de trama para la respuesta MD5
    strncpy(response.data, status, sizeof(response.data) - 1);
    response.data_length = strlen(response.data);
    response.timestamp = (uint32_t)time(NULL);
    response.checksum = calculate_checksum(response.data, response.data_length, 1);

    if (send_frame(clientSocket, &response) < 0)
    {
        logError("[ERROR]: Error al enviar la respuesta MD5 al cliente.");
    }
    else
    {
        logInfo("[INFO]: Respuesta MD5 enviada al cliente.");
    }
}

void processReceivedFrameFromFleck(int clientSocket, const Frame *request)
{
    if (!request)
    {
        logError("[ERROR]: Frame recibido nulo.");
        return;
    }

    switch (request->type)
    {
    case 0x03:
    { // Solicitud DISTORT FILE
        logInfo("[INFO]: Solicitud DISTORT FILE recibida.");
        char userName[64], fileName[256], fileSizeStr[20], md5Sum[33], factor[20];
        if (sscanf(request->data, "%63[^&]&%255[^&]&%19[^&]&%32[^&]&%19s",
                   userName, fileName, fileSizeStr, md5Sum, factor) != 5)
        {
            logError("[ERROR]: Formato inválido en solicitud DISTORT FILE.");
            send_frame_with_error(clientSocket, "CON_KO");
            return;
        }

        strncpy(receivedFileName, fileName, sizeof(receivedFileName) - 1);
        receivedFileName[sizeof(receivedFileName) - 1] = '\0';
        strncpy(expectedMD5, md5Sum, sizeof(expectedMD5) - 1);
        strncpy(receivedFactor, factor, sizeof(receivedFactor) - 1);

        size_t expectedFileSize = strtoull(fileSizeStr, NULL, 10);
        if (expectedFileSize == 0)
        {
            logError("[ERROR]: Error al convertir el tamaño del archivo a número.");
            return;
        }
        // Crear el archivo directamente en HARLEY_PATH_FILES
        char finalFilePath[512];
        snprintf(finalFilePath, sizeof(finalFilePath), "%s%s", HARLEY_PATH_FILES, receivedFileName);
        tempFileDescriptor = open(finalFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        if (tempFileDescriptor < 0)
        {
            logError("[ERROR]: No se pudo crear el archivo en HARLEY_PATH_FILES.");
            perror("Error al abrir el archivo");
            send_frame_with_error(clientSocket, "FILE_ERROR");
            return;
        }

        logInfo("[INFO]: Archivo debería estar guardado en:");
        logInfo(finalFilePath);

        send_frame_with_ok(clientSocket);
        break;
    }

    case 0x06:
    { // Fin de envío de archivo
        logInfo("[INFO]: Trama 0x06 recibida. Fin de envío de archivo por parte de Fleck. Verificando MD5...");

        // Cierra el archivo temporal
        if (tempFileDescriptor >= 0)
        {
            close(tempFileDescriptor);
            tempFileDescriptor = -1;
        }

        char finalFilePath[512];
        snprintf(finalFilePath, sizeof(finalFilePath), "%s%s", HARLEY_PATH_FILES, receivedFileName);

        char calculatedMD5[33] = {0};
        calculate_md5(finalFilePath, calculatedMD5);

        if (strcmp(calculatedMD5, "ERROR") == 0)
        {
            logError("[ERROR]: No se pudo calcular el MD5 del archivo recibido.");
            send_frame_with_error(clientSocket, "CHECK_KO");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
            break;
        }

        logInfo("[INFO]: MD5 calculado del archivo recibido:");
        logInfo(calculatedMD5);

        if (strcmp(expectedMD5, calculatedMD5) == 0)
        {
            logInfo("[SUCCESS]: El MD5 coincide. Procesando compresión...");

            // Procesar la compresión
            int result = process_compression(finalFilePath, atoi(receivedFactor));
            if (result != 0)
            {
                logError("[ERROR]: Fallo en la compresión del archivo.");
                break;
            }

            // Crear la ruta del archivo comprimido dinámicamente
            char *compressedFilePath = NULL;
            if (asprintf(&compressedFilePath, "%s", finalFilePath) == -1)
            {
                logError("[ERROR]: No se pudo asignar memoria para la ruta del archivo comprimido.");
                break;
            }

            // Calcular el MD5 del archivo comprimido
            char compressedMD5[33] = {0};
            calculate_md5(compressedFilePath, compressedMD5);

            if (strcmp(compressedMD5, "ERROR") == 0)
            {
                logError("[ERROR]: No se pudo calcular el MD5 del archivo comprimido.");
                unlink(compressedFilePath); // Eliminar el archivo comprimido
                free(compressedFilePath);
                break;
            }

            logInfo("[INFO]: MD5 calculado del archivo comprimido:");
            logInfo(compressedMD5);

            // Calcular el tamaño del archivo comprimido con lseek
            int fd_media_compressed = open(compressedFilePath, O_RDONLY);
            if (fd_media_compressed < 0)
            {
                logError("[ERROR]: No se pudo abrir el archivo especificado.");
                free(compressedFilePath);
                return;
            }

            off_t fileSizeCompressed = lseek(fd_media_compressed, 0, SEEK_END);
            if (fileSizeCompressed < 0)
            {
                logError("[ERROR]: No se pudo calcular el tamaño del archivo.");
                close(fd_media_compressed);
                free(compressedFilePath);
                return;
            }
            lseek(fd_media_compressed, 0, SEEK_SET);
            close(fd_media_compressed);

            // Convertir tamaño del archivo a string
            char *fileSizeStrCompressed = NULL;
            if (asprintf(&fileSizeStrCompressed, "%ld", fileSizeCompressed) == -1)
            {
                logError("[ERROR]: No se pudo asignar memoria para el tamaño del archivo.");
                free(compressedFilePath);
                return;
            }

            logInfo("\n[INFO]: Path del archivo:");
            logInfo(compressedFilePath);

            logInfo("\n[INFO]: Tamaño del archivo calculado:");
            logInfo(fileSizeStrCompressed);

            // Liberar memoria dinámica asignada
            free(compressedFilePath);
            free(fileSizeStrCompressed);
        }
        else
        {
            logError("[ERROR]: El MD5 no coincide. Archivo recibido está corrupto.");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
            send_frame_with_error(clientSocket, "CHECK_KO");
        }

        break;
    }

    default:
        logWarning("[WARNING]: Frame desconocido recibido.");
        break;
    }
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

        send_frame(gothamSocket, &frame);
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Respuesta DISTORT_OK enviada.");
        break;

    default:
        printColor(ANSI_COLOR_RED, "[ERROR]: Comando desconocido recibido.");
        Frame errorFrame = {.type = 0xFF, .timestamp = time(NULL)};
        strncpy(errorFrame.data, "CMD_KO", sizeof(errorFrame.data) - 1);
        errorFrame.data_length = strlen(errorFrame.data);
        errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);

        send_frame(gothamSocket, &errorFrame);
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

    if (send_frame(gothamSocket, &frame) < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviando el registro a Gotham.");
        close(gothamSocket);
        free(harleyConfig);
        return 1;
    }

    // Esperar respuesta del registro
    Frame response;
    if (receive_frame(gothamSocket, &response) != 0)
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
    while (1)
    {
        int clientSocket = accept_connection(fleckSocket);

        if (clientSocket < 0)
        {
            logError("[ERROR]: No se pudo aceptar la conexión del Fleck.");
            continue;
        }

        if (clientSocket >= 0)
        {
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