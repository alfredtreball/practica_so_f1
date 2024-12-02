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

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"
#include "FrameUtils/FrameUtils.h"

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536

int gothamSocket = -1; // Variable global para manejar el socket

void signalHandler(int sig);
void processCommandWithGotham(const char *command);
void listText(const char *directory);
void listMedia(const char *directory);
void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket);
void alliberarMemoria(FleckConfig *fleckConfig);
void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket);


FleckConfig *globalFleckConfig = NULL;

void printColor(const char *color, const char *message) {
    write(1, color, strlen(color));
    write(1, message, strlen(message));
    write(1, ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
}

// Funcions de registre de logs
void logInfo(const char *msg) {
    printF(CYAN "[INFO]: " RESET);
    printF(msg);
    printF("\n");
}

void logWarning(const char *msg) {
    printF(YELLOW "[WARNING]: " RESET);
    printF(msg);
    printF("\n");
}

void logError(const char *msg) {
    printF(RED "[ERROR]: " RESET);
    printF(msg);
    printF("\n");
}

void logSuccess(const char *msg) {
    printF(GREEN "[SUCCESS]: " RESET);
    printF(msg);
    printF("\n");
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

        printColor(ANSI_COLOR_CYAN, "[INFO]: Intentando conectar a Gotham...\n");
        gothamSocket = connect_to_server(globalFleckConfig->ipGotham, globalFleckConfig->portGotham);
        if (gothamSocket < 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo establecer la conexión con Gotham. Comprobar IP y puerto.\n");
            gothamSocket = -1; // Aseguramos que quede desconectado
            return;
        }

        frame.type = 0x01; // Tipo de conexión
        frame.timestamp = (uint32_t)time(NULL);
        strncpy(frame.data, "CONNECT", sizeof(frame.data) - 1);
        frame.data_length = strlen(frame.data);
        frame.checksum = calculate_checksum(frame.data, frame.data_length);

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
        uint16_t resp_checksum = calculate_checksum(response.data, response.data_length);
        if (resp_checksum != response.checksum) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Resposta de Gotham amb checksum invàlid\n");
            return;
        }

        if (response.type == 0x01 && strcmp(response.data, "CONN_OK") == 0) {
            printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Conexión establecida con Gotham.\n");
        } else if (response.type == 0x01 && strcmp(response.data, "CONN_KO") == 0) {
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

void processCommand(char *command, int gothamSocket) {
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
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) {
        if (gothamSocket != -1) {
            printColor(ANSI_COLOR_YELLOW, "[INFO]: Desconectando de Gotham...\n");
            close(gothamSocket);
            gothamSocket = -1;
            printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Desconectado exitosamente.\n");
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: No estás conectado a Gotham.\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        if (gothamSocket == -1) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Debes conectarte a Gotham antes de ejecutar DISTORT.\n");
        } else {
            char debugLog[256];
            snprintf(debugLog, sizeof(debugLog), "[DEBUG]: subCmd: '%s', extra: '%s'", subCmd ? subCmd : "NULL", extra ? extra : "NULL");
            logInfo(debugLog);
            processDistortFileCommand(subCmd, extra, gothamSocket);
        }
    } else if (strcasecmp(cmd, "CHECK") == 0 && strcasecmp(subCmd, "STATUS") == 0 && extra == NULL) {
        if (gothamSocket == -1) {
            printColor(ANSI_COLOR_RED, "[ERROR]: Debes conectarte a Gotham antes de ejecutar CHECK STATUS.\n");
        } else {
            printColor(ANSI_COLOR_CYAN, "[INFO]: Checking status...\n");
            processCommandWithGotham("CHECK STATUS");
        }
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Unknown command. Please enter a valid command.\n");
    }
}

void sendFileToWorker(int workerSocket, const char *fileName) {
    int fileFd = open(fileName, O_RDONLY);
    if (fileFd < 0) {
        logError("[ERROR]: No se pudo abrir el archivo para enviar.");
        return;
    }

    char buffer[256];
    ssize_t bytesRead;

    Frame dataFrame = {0};
    dataFrame.type = 0x05;

    logInfo("[INFO]: Enviando archivo al Worker...");
    while ((bytesRead = read(fileFd, buffer, sizeof(buffer))) > 0) {
        memcpy(dataFrame.data, buffer, bytesRead);
        dataFrame.data_length = bytesRead;
        dataFrame.timestamp = (uint32_t)time(NULL);
        dataFrame.checksum = calculate_checksum(dataFrame.data, dataFrame.data_length);

        send_frame(workerSocket, &dataFrame);
    }

    close(fileFd);
    logInfo("[INFO]: Archivo enviado al Worker.");
}

void receiveDistortedFileFromWorker(int workerSocket) {
    char filePath[] = "distorted_file"; // Guardaremos el archivo aquí
    int fileFd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fileFd < 0) {
        logError("[ERROR]: No se pudo crear el archivo distorsionado.");
        return;
    }

    Frame response;
    while (receive_frame(workerSocket, &response) == 0) {
        if (response.type == 0x04) {
            logInfo("[INFO]: Información del archivo distorsionado recibida.");
            char fileSize[20], md5Sum[33];
            if (sscanf(response.data, "%19[^&]&%32s", fileSize, md5Sum) != 2) {
                logError("[ERROR]: Formato inválido en información del archivo.");
                close(fileFd);
                return;
            }

            logInfo("[INFO]: Tamaño y MD5 del archivo registrado.");
        } else if (response.type == 0x05) {
            logInfo("[INFO]: Recibiendo fragmento de archivo...");
            write(fileFd, response.data, response.data_length);
        } else if (response.type == 0x06) {
            if (strcmp(response.data, "CHECK_OK") == 0) {
                logSuccess("[SUCCESS]: Archivo validado correctamente.");
            } else {
                logError("[ERROR]: MD5SUM no coincide.");
            }
            break;
        }
    }

    close(fileFd);
}

void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket) {
    Frame frame = {0};
    frame.type = 0x11; // Tipo de trama para reasignar Worker
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

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

    int workerSocket = -1;

    char mediaType[10];
    if (strcasecmp(fileExtension, ".txt") == 0) {
        strncpy(mediaType, "TEXT", sizeof(mediaType));
    } else if (strcasecmp(fileExtension, ".wav") == 0 || strcasecmp(fileExtension, ".png") == 0 || strcasecmp(fileExtension, ".jpg") == 0) {
        strncpy(mediaType, "MEDIA", sizeof(mediaType));
    } else {
        logError("[ERROR]: Unsupported file extension.");
        return;
    }

    // Enviar solicitud DISTORT a Gotham
    Frame frame = {0};
    frame.type = 0x10;
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    logInfo("[INFO]: Enviando solicitud DISTORT a Gotham...");
    send_frame(gothamSocket, &frame);

    // Recibir respuesta de Gotham
    Frame response;
    if (receive_frame(gothamSocket, &response) == 0) {
        if (response.type == 0x10) {
            if (strcmp(response.data, "DISTORT_KO") == 0) {
                logError("[ERROR]: Gotham no encontró un worker disponible.");
            } else if (strcmp(response.data, "MEDIA_KO") == 0) {
                logError("[ERROR]: El tipo de archivo solicitado no es válido.");
            } else if(response.type == 0x10){
                // Respuesta exitosa con información del worker
                char workerIp[16];
                int workerPort;
                sscanf(response.data, "%15[^&]&%d", workerIp, &workerPort);

                logInfo("[INFO]: Conectando al Worker...");
                workerSocket = connect_to_server(workerIp, workerPort);
                if (workerSocket < 0) {
                    logError("[ERROR]: No se pudo conectar al Worker.");
                    return;
                }

                // Construir la trama de solicitud DISTORT FILE
                char userName[] = "defaultUser"; // Placeholder para el nombre de usuario
                snprintf(frame.data, sizeof(frame.data), "%s&%s&%s&PLACEHOLDER_MD5&%s", userName, fileName, "PLACEHOLDER_SIZE", factor);
                frame.type = 0x03;
                frame.data_length = strlen(frame.data);
                frame.timestamp = (uint32_t)time(NULL);
                frame.checksum = calculate_checksum(frame.data, frame.data_length);

                logInfo("[INFO]: Enviando solicitud DISTORT FILE al Worker...");
                send_frame(workerSocket, &frame);

                // Manejar la respuesta del Worker
                Frame workerResponse;
                if (receive_frame(workerSocket, &workerResponse) == 0) {
                    if (workerResponse.type == 0x03 && workerResponse.data_length == 0) {
                        logSuccess("[SUCCESS]: Worker listo para recibir.");
                        sendFileToWorker(workerSocket, fileName); // Enviar el archivo
                        receiveDistortedFileFromWorker(workerSocket);
                    } else if (workerResponse.type == 0x03 && strcmp(workerResponse.data, "CON_KO") == 0) {
                        logError("[ERROR]: Worker rechazó la conexión.");
                    } else {
                        logError("[ERROR]: Respuesta inesperada del Worker.");
                    }
                } else {
                    logError("[ERROR]: No se recibió respuesta del Worker.");
                }
            }

            if (workerSocket >= 0){
                close(workerSocket);
            }

        } else {
            logError("[ERROR]: Respuesta inesperada de Gotham.");
        }
    } else {
        logError("[ERROR]: No se recibió respuesta de Gotham.");
    }
}


void sendDisconnectFrame(int socket_fd, const char *userName) {
    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    frame.timestamp = (uint32_t)time(NULL);

    if (userName) {
        snprintf(frame.data, sizeof(frame.data), "%s", userName);
        frame.data_length = strlen(frame.data); // Esto define cuánto se usa de los datos
    } else {
        frame.data_length = 0; // Sin datos
    }
    
    frame.checksum = calculate_checksum(frame.data, frame.data_length);
      printf("[DEBUG][Fleck] Enviando Frame: Type=%02x, DATA_LENGTH=%04x, Checksum=%04x, Data='%s'\n",
           frame.type, frame.data_length, frame.checksum, frame.data);

    if (send_frame(socket_fd, &frame) < 0) {
        logError("[ERROR]: No se pudo enviar el frame de desconexión.");
    } else {
        logInfo("[INFO]: Frame de desconexión enviado correctamente.");
    }
}

// FASE 1
void signalHandler(int sig) {
    if (sig == SIGINT) {
        printColor(ANSI_COLOR_YELLOW, "\n[INFO]: Alliberació de memòria...\n");

        if (gothamSocket != -1) {
            sendDisconnectFrame(gothamSocket, globalFleckConfig->user);
            close(gothamSocket);
            gothamSocket = -1;
        }

        if (globalFleckConfig) {
            alliberarMemoria(globalFleckConfig); // Liberar memoria asignada a la configuración
            globalFleckConfig = NULL; // Evitar accesos posteriores
        }

        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Recursos alliberats correctament.\n");
        exit(0);
    }
}

void alliberarMemoria(FleckConfig *fleckConfig) {
    if (fleckConfig->user) free(fleckConfig->user);
    if (fleckConfig->directory) free(fleckConfig->directory);
    if (fleckConfig->ipGotham) free(fleckConfig->ipGotham);
    free(fleckConfig);
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
        processCommand(command, gothamSocket);
        free(command);
    }

    close(gothamSocket);
    free(fleckConfig);

    return 0;
}
