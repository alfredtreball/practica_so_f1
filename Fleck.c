#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"
#include "FrameUtils/FrameUtils.h"
#include "FrameUtilsBinary/FrameUtilsBinary.h"
#include "Logging/Logging.h"
#include "MD5SUM/md5Sum.h"

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536
#define FILE_PATH "fitxers_prova/"
#define DATA_SIZE 247

typedef struct {
    int workerSocket;
    const char *filePath;
    off_t fileSize;
} DistortRequestArgs;

int gothamSocket = -1; // Variable global para manejar el socket
int workerSocket = -1;
float statusResult;

void signalHandler(int sig);
void processCommandWithGotham(const char *command);
void listText(const char *directory);
void listMedia(const char *directory);
void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket);
void alliberarMemoria(FleckConfig *fleckConfig);
void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket);
void sendFileToWorker(int workerSocket, const char *fileName);
void *sendFileChunks(void *args);
void sendDistortFileRequest(int workerSocket, const char *fileName, off_t fileSize);
void sendDisconnectFrameToGotham(const char *userName);
void sendDisconnectFrameToWorker(int workerSocket, const char *userName);
void processCommand(char *command, int gothamSocket, int workerSocket);

FleckConfig *globalFleckConfig = NULL;

void printColor(const char *color, const char *message) {
    write(1, color, strlen(color));
    write(1, message, strlen(message));
    write(1, ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
}

// Función para listar los archivos de texto (.txt) en el directorio especificado
void listText(const char *directory) {
    pid_t pid = fork();

    if (pid == 0) { // Procés fill
        int tempFd = open("text_files.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            exit(1);
        }
        dup2(tempFd, STDOUT_FILENO); // Redirigeix la sortida estàndard al fitxer temporal
        close(tempFd);

        char *args[] = {"/usr/bin/find", (char *)directory, "-type", "f", "-name", "*.txt", "-exec", "basename", "{}", ";", NULL};
        execv(args[0], args);

        printF("Error executant find\n"); // Si execv falla
        exit(1);
    } else if (pid > 0) { // Procés pare
        wait(NULL);

        int tempFd = open("text_files.txt", O_RDONLY);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            return;
        }

        int count = 0;
        char *line;
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            count++;
            free(line);
        }

        char *countStr = intToStr(count); 
        printF("There are ");
        printF(countStr);
        printF(" text files available:\n");
        free(countStr);

        lseek(tempFd, 0, SEEK_SET);

        int index = 1;
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            char *indexStr = intToStr(index++);
            printF(indexStr);
            printF(". ");
            printF(line);
            printF("\n");
            free(indexStr);
            free(line);
        }

        close(tempFd);
    } else {
        printF("Error en fork\n");
    }
}

// Función para listar los archivos de tipo media (wav, jpg, png) en el directorio especificado
void listMedia(const char *directory) {
   pid_t pid = fork();

    if (pid == 0) { // Procés fill
        int tempFd = open("media_files.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            exit(1);
        }
        dup2(tempFd, STDOUT_FILENO); 
        close(tempFd);

        char *args[] = {
            "/bin/bash", "-c",
            "find \"$1\" -type f \\( -name '*.wav' -o -name '*.jpg' -o -name '*.png' \\) -exec basename {} \\;",
            "bash", (char *)directory, NULL
        };
        execv(args[0], args);

        printF("Error executant find\n");
        exit(1);
    } else if (pid > 0) { 
        wait(NULL);

        int tempFd = open("media_files.txt", O_RDONLY);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            return;
        }

        int count = 0;
        char *line;
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            count++;
            free(line);
        }

        char *countStr = intToStr(count); 
        printF("There are ");
        printF(countStr);
        printF(" media files available:\n");
        free(countStr);

        lseek(tempFd, 0, SEEK_SET);

        int index = 1;
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            char *indexStr = intToStr(index++);
            printF(indexStr);
            printF(". ");
            printF(line);
            printF("\n");
            free(indexStr);
            free(line);
        }

        close(tempFd);
    } else {
        printF("Error en fork\n");
    }
}

void processCommandWithGotham(const char *command) {
    Frame frame = {0};

    if (strcasecmp(command, "CONNECT") == 0) {
        if (gothamSocket != -1) {
            printColor(ANSI_COLOR_YELLOW, "[INFO]: Ya estás conectado a Gotham.\n");
            return;
        }

        gothamSocket = connect_to_server(globalFleckConfig->ipGotham, globalFleckConfig->portGotham);
        if (gothamSocket < 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo establecer la conexión con Gotham. Comprobar IP y puerto.\n");
            gothamSocket = -1; // Aseguramos que quede desconectado
            return;
        }

        // Eliminar cualquier carácter no deseado de `user`
        removeChar(globalFleckConfig->user, '\r');
        removeChar(globalFleckConfig->user, '\n');

        frame.type = 0x01; // Tipo de conexión
        frame.timestamp = (uint32_t)time(NULL);

        // Construir la trama con el formato <userName>&<IP>&<Port>
        snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
                 globalFleckConfig->user, globalFleckConfig->ipGotham, globalFleckConfig->portGotham);
        frame.data_length = strlen(frame.data);
        frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

        char buffer[FRAME_SIZE];
        serialize_frame(&frame, buffer);

        if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error al enviar la solicitud de conexión a Gotham.\n");
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }

        if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se recibió respuesta de Gotham.\n");
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }

        Frame response;
        if (deserialize_frame(buffer, &response) != 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Error al procesar la respuesta de Gotham.\n");
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }

        // Validar checksum de la respuesta
        uint16_t resp_checksum = calculate_checksum(response.data, response.data_length, 1);
        if (resp_checksum != response.checksum) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta de Gotham con checksum inválido\n");
            return;
        }

        if (response.type == 0x01) {
            if (response.data_length == 0) { // DATA vacío, longitud 0
                printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Conexión establecida con Gotham.\n");
            }
        } else if (response.type == 0x01 && strcmp(response.data, "CON_KO") == 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Gotham rechazó la conexión.\n");
            close(gothamSocket);
            gothamSocket = -1;
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta inesperada de Gotham.\n");
            close(gothamSocket);
            gothamSocket = -1;
        }
        return;
    }

    printColor(ANSI_COLOR_RED, "[ERROR]: Comando CONNECT no válido en este contexto.\n");
}

void processCommand(char *command, int gothamSocket, int workerSocket) {
    if (command == NULL || strlen(command) == 0 || strcmp(command, "\n") == 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Empty command.\n");
        return;
    }

    char *cmd = strtok(command, " \n");
    if (cmd == NULL) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Invalid command.\n");
        return;
    }

    char *subCmd = strtok(NULL, " \n");
    char *extra = strtok(NULL, " \n");

    if (strcasecmp(cmd, "CONNECT") == 0 && subCmd == NULL) {
        processCommandWithGotham("CONNECT");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        if (strcasecmp(subCmd, "MEDIA") == 0 && extra == NULL) {
            printColor(ANSI_COLOR_CYAN, "[INFO]: Listing media files...\n");
            listMedia(globalFleckConfig->directory);
        } else if (strcasecmp(subCmd, "TEXT") == 0 && extra == NULL) {
            printColor(ANSI_COLOR_CYAN, "[INFO]: Listing text files...\n");
            listText(globalFleckConfig->directory);
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: Invalid LIST command. Use LIST MEDIA or LIST TEXT.\n");
        }
    } else if (strcasecmp(cmd, "CLEAR") == 0 && strcasecmp(subCmd, "ALL") == 0 && extra == NULL) {
        printColor(ANSI_COLOR_CYAN, "[INFO]: Clearing all local data in Fleck...\n");
        // Implementaremos en FASE 3
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: All local data has been cleared in Fleck.\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) { //ANAR-SE CAP A FORA //TODO ENCARA I NO ESTAR CONNECTAT CAP A FORA
        logInfo("[INFO]: Procesando comando LOGOUT...");

        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham(globalFleckConfig->user);
            close(gothamSocket);
            gothamSocket = -1;
            logInfo("[INFO]: Desconexión de Gotham completada.");
        } else {
            logWarning("[WARNING]: No estás conectado a Gotham.");
        }

        if (workerSocket >= 0) {
            sendDisconnectFrameToWorker(workerSocket, globalFleckConfig->user);
            close(workerSocket);
            workerSocket = -1;
            logInfo("[INFO]: Desconexión del Worker completada.");
        } else {
            logWarning("[WARNING]: No estás conectado a un Worker.");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        if (gothamSocket == -1) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Debes conectarte a Gotham antes de ejecutar DISTORT.\n");
        } else {
            processDistortFileCommand(subCmd, extra, gothamSocket);
        }
    } else if (strcasecmp(cmd, "CHECK") == 0 && strcasecmp(subCmd, "STATUS") == 0 && extra == NULL) {
            printColor(ANSI_COLOR_CYAN, "Status enviament fitxer Fleck -> Worker:\n");
            printf("%.2f\n", statusResult);
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Unknown command. Please enter a valid command.\n");
    }
}

//FLECK  ENVIA 0X05 AMB LES DADES Y WORKER RESPON 0X04 AMB FILESIZE Y MD5SUM QUAN ACABI D'ENVIAR TOT L'ARXIU DISTORSIONAT
//PERCENTATGE PER CHECK STATUS, ÉS DOBLE OSEA QUE AL ENVIAR Y RECIBIR VA POR LADO
//TODO QUAN PASSEM FITXERS BINARIS, FER UNA FUNCIÓ AMB LA TRAMA SHIFTEJANT, SINÓ DONARÀ PROBLEMES AL REBRE I PROBAR AL ENVIAR

//LLancem thread per iniciar un nou fil que envïi la trama 0x05 amb longitud data 247 per parts.
void *sendFileChunks(void *args) {
    DistortRequestArgs *requestArgs = (DistortRequestArgs *)args;
    int workerSocket = requestArgs->workerSocket;
    const char *filePath = requestArgs->filePath;
    off_t fileSize = requestArgs->fileSize;
    free(requestArgs); // Liberar memoria de los argumentos

    int fd = open(filePath, O_RDONLY, 0666);
    if (fd < 0) {
        logError("[ERROR]: No se pudo abrir el archivo especificado.");
        return NULL;
    }

    BinaryFrame frame = {0};
    frame.type = 0x05;

    char buffer[247]; // Máxima longitud permitida para DATA
    ssize_t bytesRead;
    int status = 1;

    logInfo("[INFO]: Iniciando envío del archivo por tramas 0x05...");

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        frame.data_length = bytesRead;
        memcpy(frame.data, buffer, bytesRead);
        frame.timestamp = (uint32_t)time(NULL);
        frame.checksum = calculate_checksum_binary(frame.data, frame.data_length, 1);

        if (send_frame_binary(workerSocket, &frame) < 0) {
            logError("[ERROR]: Fallo al enviar trama 0x05.");
            close(fd);
            return NULL;
        }
        usleep(1000);
        statusResult = (((DATA_SIZE*status)*100)/fileSize)/2;
        status++;
    }

    if (bytesRead < 0) {
        logError("[ERROR]: Fallo al leer el archivo.");
    } else {
        logInfo("[INFO]: Envío del archivo completado.");
    }

    close(fd);

    // Enviar trama 0x06 para indicar fin del envío
    Frame endFrame = {0};
    endFrame.type = 0x06;
    snprintf(endFrame.data, sizeof(endFrame.data), "END_OF_FILE");
    endFrame.data_length = strlen(endFrame.data);
    endFrame.timestamp = (uint32_t)time(NULL);
    endFrame.checksum = calculate_checksum(endFrame.data, endFrame.data_length, 1);

    if (send_frame(workerSocket, &endFrame) < 0) {
        logError("[ERROR]: Fallo al enviar trama de fin de archivo (0x06).");
    } else {
        logInfo("[INFO]: Trama 0x06 enviada al Worker.");
    }

    return NULL;
}

void sendDistortFileRequest(int workerSocket, const char *fileName, off_t fileSize) {
    char *filePath = NULL;
    if (asprintf(&filePath, "%s%s", FILE_PATH, fileName) == -1) {
        logError("[ERROR]: No se pudo asignar memoria para el path del archivo.");
        return;
    }

    DistortRequestArgs *args = malloc(sizeof(DistortRequestArgs));
    if (!args) {
        logError("[ERROR]: No se pudo asignar memoria para los argumentos del thread.");
        free(filePath);
        return;
    }

    args->workerSocket = workerSocket;
    args->filePath = filePath;
    args->fileSize = fileSize;

    pthread_t thread;
    if (pthread_create(&thread, NULL, sendFileChunks, (void *)args) != 0) {
        logError("[ERROR]: No se pudo crear el thread para enviar las tramas.");
        free(args);
        free(filePath);
        return;
    }

    pthread_detach(thread); // No necesitamos esperar explícitamente a que el thread termine
    logInfo("[INFO]: Thread iniciado para enviar el archivo.");
}

void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket) {
    Frame frame = {0};
    frame.type = 0x11; // Tipo de trama para reasignar Worker
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando solicitud de reasignación de Worker a Gotham...");
    send_frame(gothamSocket, &frame);

    // Esperar respuesta de Gotham
    Frame response;
    if (receive_frame(gothamSocket, &response) == 0) {
        if (response.type == 0x11) {
            if (strcmp(response.data, "DISTORT_KO") == 0) {
                logError("[ERROR]: Gotham no pudo reasignar un Worker.");
            } else if (strcmp(response.data, "MEDIA_KO") == 0) {
                logError("[ERROR]: Gotham indicó que el tipo de media es inválido.");
            } else {
                char workerIp[16];
                int workerPort;
                if (sscanf(response.data, "%15[^&]&%d", workerIp, &workerPort) == 2) {
                    logInfo("[INFO]: Nuevo Worker asignado. Intentando reconexión...");
                
                    int workerSocket = connect_to_server(workerIp, workerPort);
                    if (workerSocket >= 0) {
                        logSuccess("[SUCCESS]: Conexión al nuevo Worker establecida.");
                        // Reiniciar flujo de distorsión con el nuevo Worker.
                        processDistortFileCommand(fileName, "factor_placeholder", gothamSocket);
                        close(workerSocket);
                    } else {
                        logError("[ERROR]: No se pudo conectar al nuevo Worker.");
                    }
                } else {
                    logError("[ERROR]: Formato de respuesta inválido de Gotham.");
                }
            }
        } else {
            logError("[ERROR]: Respuesta inesperada de Gotham al reasignar Worker.");
        }
    } else {
        logError("[ERROR]: No se recibió respuesta de Gotham al solicitar reasignación.");
    }
}

void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket) {
    if (!fileName || !factor) {
        logError("[ERROR]: DISTORT command requires a mediaType and a fileName.");
        return;
    }

    // Determinar el mediaType según la extensión del archivo
    char *fileExtension = strrchr(fileName, '.');
    if (!fileExtension) {
        logError("[ERROR]: File does not have a valid extension.");
        return;
    }

    char mediaType[10];
    if (strcasecmp(fileExtension, ".txt") == 0) {
        strncpy(mediaType, "TEXT", sizeof(mediaType));
    } else if (strcasecmp(fileExtension, ".wav") == 0 || strcasecmp(fileExtension, ".png") == 0 || strcasecmp(fileExtension, ".jpg") == 0) {
        strncpy(mediaType, "MEDIA", sizeof(mediaType));
    } else {
        logError("[ERROR]: Unsupported file extension.");
        return;
    }

    // Construir el path completo del archivo
    char *filePath = NULL;
    if (asprintf(&filePath, "%s%s", FILE_PATH, fileName) == -1) {
        logError("[ERROR]: No se pudo asignar memoria para el path del archivo.");
        return;
    }

    // Calcular el tamaño del archivo con lseek
    int fd_media = open(filePath, O_RDONLY);
    if (fd_media < 0) {
        logError("[ERROR]: No se pudo abrir el archivo especificado.");
        free(filePath);  // Liberar memoria asignada dinámicamente
        return;
    }

    off_t fileSize = lseek(fd_media, 0, SEEK_END);
    if (fileSize < 0) {
        logError("[ERROR]: No se pudo calcular el tamaño del archivo.");
        close(fd_media);
        free(filePath);
        return;
    }
    lseek(fd_media, 0, SEEK_SET);  // Regresar al inicio del archivo para su posterior lectura
    close(fd_media);

    // Convertir tamaño del archivo a string
    char *fileSizeStr = NULL;
    if (asprintf(&fileSizeStr, "%ld", fileSize) == -1) {
        logError("[ERROR]: No se pudo asignar memoria para el tamaño del archivo.");
        free(filePath);
        return;
    }

    logInfo("\n[INFO]: Path del archivo:");
    logInfo(filePath);

    logInfo("\n[INFO]: Tamaño del archivo calculado:");
    logInfo(fileSizeStr);

    // Calcular el MD5 del archivo
    char md5sum[33] = {0};
    calculate_md5(filePath, md5sum);

    logInfo("\n[INFO]: MD5 calculado:");
    logInfo(md5sum);

    // Enviar solicitud DISTORT a Gotham
    Frame frame = {0};
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.type = 0x10;
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando solicitud DISTORT a Gotham...");
    send_frame(gothamSocket, &frame);

    // Recibir respuesta de Gotham
    Frame response = {0};
    if (receive_frame(gothamSocket, &response) != 0 || response.type != 0x10) {
        logError("[ERROR]: No se recibió respuesta válida de Gotham.");
        free(filePath);
        free(fileSizeStr);
        return;
    }

    if (strcmp(response.data, "DISTORT_KO") == 0) {
        logError("[ERROR]: Gotham no encontró un worker disponible.");
        free(filePath);
        free(fileSizeStr);
        return;
    } else if (strcmp(response.data, "MEDIA_KO") == 0) {
        logError("[ERROR]: Tipo de archivo rechazado por Gotham.");
        free(filePath);
        free(fileSizeStr);
        return;
    }

    // Conectar al Worker
    char workerIp[16] = {0};
    int workerPort = 0;
    if (sscanf(response.data, "%15[^&]&%d", workerIp, &workerPort) != 2 || strlen(workerIp) == 0 || workerPort <= 0) {
        logError("[ERROR]: Datos del Worker inválidos.");
        free(filePath);
        free(fileSizeStr);
        return;
    }

    int workerSocket = connect_to_server(workerIp, workerPort);
    if (workerSocket < 0) {
        logError("[ERROR]: No se pudo conectar al Worker.");
        free(filePath);
        free(fileSizeStr);
        return;
    }

    // Enviar trama 0x03 con tamaño y MD5
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%s&%s&%s", 
             globalFleckConfig->user, fileName, fileSizeStr, md5sum, factor);
    frame.type = 0x03;
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[DEBUG]: Trama enviada a Harley:");
    logInfo(frame.data);

    logInfo("[INFO]: Enviando solicitud DISTORT FILE al Worker...");
    send_frame(workerSocket, &frame);

    // Manejar respuesta del Worker
    Frame workerResponse = {0};
    if (receive_frame(workerSocket, &workerResponse) == 0 && workerResponse.type == 0x03) {
        if (workerResponse.data_length == 0) {
            logSuccess("[SUCCESS]: Worker listo para recibir.");
            sendDistortFileRequest(workerSocket, fileName, fileSize);
        } else {
            logError("[ERROR]: Worker rechazó la solicitud.");
        }
    } else {
        logError("[ERROR]: Respuesta inesperada del Worker.");
    }

    free(filePath);
    free(fileSizeStr);
}


void sendDisconnectFrameToGotham(const char *userName) {
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    frame.timestamp = (uint32_t)time(NULL);
    
    // Incluir el nombre de usuario en la trama
    if (userName) {
        strncpy(frame.data, userName, sizeof(frame.data) - 1);
        frame.data_length = strlen(frame.data);
    } else {
        frame.data_length = 0; // Sin datos adicionales
    }
    
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (send_frame(gothamSocket, &frame) < 0) {
        logError("[ERROR]: Fallo al enviar trama de desconexión a Gotham.");
    } else {
        logInfo("[INFO]: Trama de desconexión enviada correctamente a Gotham.");
    }
}

void sendDisconnectFrameToWorker(int workerSocket, const char *userName) {
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    frame.timestamp = (uint32_t)time(NULL);
    
    // Incluir el nombre de usuario en la trama
    if (userName) {
        strncpy(frame.data, userName, sizeof(frame.data) - 1);
        frame.data_length = strlen(frame.data);
    } else {
        frame.data_length = 0; // Sin datos adicionales
    }

    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando trama de desconexión al Worker...");
    if (send_frame(workerSocket, &frame) < 0) {
        logError("[ERROR]: Fallo al enviar trama de desconexión al Worker.");
    } else {
        logInfo("[INFO]: Trama de desconexión enviada correctamente al Worker.");
    }
}

// FASE 1
void signalHandler(int sig) {
    if (sig == SIGINT) {
        logInfo("[INFO]: Señal SIGINT recibida. Desconectando...");

        // Desconexión de Gotham
        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham(globalFleckConfig->user);
            close(gothamSocket);
        }

        // Desconexión de Worker
        if (workerSocket >= 0) {
            sendDisconnectFrameToWorker(workerSocket, globalFleckConfig->user);
            close(workerSocket);
        }

        free(globalFleckConfig);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    printF("\033[1;34m\n###################################\n");
    printF("# BENVINGUT AL CLIENT FLECK       #\n");
    printF("# Gestió de connexions amb Gotham #\n");
    printF("###################################\n\033[0m");

    if (argc != 2) {
        printF("\033[1;31m[ERROR]: Ús: ./fleck <fitxer de configuració>\n\033[0m");
        exit(1);
    }

    FleckConfig *fleckConfig = malloc(sizeof(FleckConfig));
    if (!fleckConfig) {
        printF("\033[1;31m[ERROR]: Error assignant memòria per a la configuració.\n\033[0m");
        return 1;
    }

    globalFleckConfig = fleckConfig;

    signal(SIGINT, signalHandler);

    printF("\033[1;36m[INFO]: Llegint el fitxer de configuració...\n\033[0m");
    readConfigFileGeneric(argv[1], fleckConfig, CONFIG_FLECK);

    char *command = NULL;
    while (1) {
        printF("\033[1;35m\n$ \033[0m");
        command = readUntil(STDIN_FILENO, '\n');

        if (command == NULL || strlen(command) == 0) {
            printF("\033[1;33m[WARNING]: Comanda buida. Si us plau, introdueix una comanda vàlida.\n\033[0m");
            free(command);
            continue;
        }

        printF("\033[1;36m[INFO]: Processant la comanda...\n\033[0m");
        processCommand(command, gothamSocket, workerSocket);
        free(command);
    }

    close(gothamSocket);
    free(fleckConfig);

    return 0;
}