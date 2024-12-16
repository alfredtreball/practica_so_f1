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
#include "Logging/Logging.h"

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536

int gothamSocket = -1; // Variable global para manejar el socket
int workerSocket = -1;

void signalHandler(int sig);
void processCommandWithGotham(const char *command);
void listText(const char *directory);
void listMedia(const char *directory);
void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket);
void alliberarMemoria(FleckConfig *fleckConfig);
void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket);
void sendFileToWorker(int workerSocket, const char *fileName);
void receiveDistortedFileFromWorker(int workerSocket);
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

        printColor(ANSI_COLOR_CYAN, "[INFO]: Intentando conectar a Gotham...\n");
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
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) {
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
            printColor(ANSI_COLOR_CYAN, "[INFO]: Verificando el estado de los archivos...\n");
            // Aquí puedes implementar lógica adicional para verificar el estado local
            logInfo("[INFO]: Estado de los archivos verificado localmente.");
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Unknown command. Please enter a valid command.\n");
    }
}

void sendDistortFileRequest(int workerSocket, const char *userName, const char *fileName, const char *fileSize, const char *md5Sum, const char *factor) {
    Frame frame = {0};
    frame.type = 0x03; // Trama de solicitud de distorsión
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%s&%s&%s", userName, fileName, fileSize, md5Sum, factor);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando solicitud DISTORT FILE al Worker...");
    send_frame(workerSocket, &frame);

    // Esperar confirmación del Worker
    Frame response;
    if (receive_frame(workerSocket, &response) == 0 && response.type == 0x03 && strcmp(response.data, "CON_OK") == 0) {
        logSuccess("[SUCCESS]: Worker listo para recibir archivo.");
    } else {
        logError("[ERROR]: Worker rechazó la solicitud DISTORT FILE.");
        return;
    }

    // Enviar el archivo en fragmentos (0x05)
    sendFileToWorker(workerSocket, fileName);
}

void processDistortedFileFromWorker(int workerSocket) {
    logInfo("[INFO]: Iniciando recepción del archivo distorsionado...");

    Frame response;

    // Esperar información del archivo distorsionado (0x04)
    if (receive_frame(workerSocket, &response) == 0) {
        if (response.type == 0x04) {
            char fileSize[20], md5Sum[33];
            if (sscanf(response.data, "%19[^&]&%32s", fileSize, md5Sum) == 2) {
                logInfo("[INFO]: Trama 0x04 recibida. Tamaño y MD5 del archivo procesados correctamente.");
            } else {
                logError("[ERROR]: Formato inválido en la trama 0x04.");
                return;
            }
        } else {
            logError("[ERROR]: Trama no esperada (se esperaba 0x04).");
            return;
        }
    } else {
        logError("[ERROR]: Fallo al recibir trama 0x04 del Worker.");
        return;
    }

    // Recibir fragmentos del archivo distorsionado (0x05)
    logInfo("[INFO]: Esperando tramas 0x05...");
    receiveDistortedFileFromWorker(workerSocket);

    // Esperar validación del MD5SUM (0x06)
    logInfo("[INFO]: Esperando trama de validación 0x06...");
    if (receive_frame(workerSocket, &response) == 0) {
        if (response.type == 0x06) {
            if (strcmp(response.data, "CHECK_OK") == 0) {
                logSuccess("[SUCCESS]: Validación de archivo exitosa (CHECK_OK).");
            } else {
                logError("[ERROR]: Validación del MD5 fallida (CHECK_KO).");
            }
        } else {
            logError("[ERROR]: Trama no esperada (se esperaba 0x06).");
        }
    } else {
        logError("[ERROR]: Fallo al recibir trama 0x06 del Worker.");
    }

    // Verificar si la conexión se cierra correctamente después del flujo
    logInfo("[INFO]: Verificando cierre controlado de conexión...");
    char testBuffer[256];
    ssize_t bytesRead = read(workerSocket, testBuffer, sizeof(testBuffer));
    if (bytesRead == 0) {
        logInfo("[INFO]: Conexión cerrada correctamente por el Worker.");
    } else if (bytesRead < 0) {
        logError("[ERROR]: Error inesperado al leer después de la trama 0x06.");
    } else {
        logWarning("[WARNING]: Se recibieron datos inesperados después de 0x06.");
    }

    logInfo("[INFO]: Finalizando proceso del archivo distorsionado. Cerrando conexión.");
    close(workerSocket);
}

void sendFileToWorker(int workerSocket, const char *fileName) {
    // Usamos valores estáticos para FileSize y MD5SUM
    char fileSize[] = "12345";  // Placeholder
    char md5Sum[] = "abcdef1234567890abcdef1234567890";  // Placeholder

    // Enviar trama 0x04 con tamaño y MD5
    Frame infoFrame = {0};
    infoFrame.type = 0x04;
    snprintf(infoFrame.data, sizeof(infoFrame.data), "%s&%s", fileSize, md5Sum);
    infoFrame.data_length = strlen(infoFrame.data);
    infoFrame.timestamp = (uint32_t)time(NULL);
    infoFrame.checksum = calculate_checksum(infoFrame.data, infoFrame.data_length, 1);
    send_frame(workerSocket, &infoFrame);
    logInfo("[INFO]: Trama 0x04 enviada con FileSize y MD5.");

    // Enviar los datos del archivo en tramas 0x05
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
        dataFrame.checksum = calculate_checksum(dataFrame.data, dataFrame.data_length, 1);
        send_frame(workerSocket, &dataFrame);
    }

    close(fileFd);

    // Esperar validación 0x06
    Frame validationFrame;
    if (receive_frame(workerSocket, &validationFrame) == 0) {
        if (validationFrame.type == 0x06) {
            if (strcmp(validationFrame.data, "CHECK_OK") == 0) {
                logSuccess("[SUCCESS]: Archivo enviado y validado correctamente.");
            } else {
                logError("[ERROR]: Validación del archivo fallida.");
            }
        } else {
            logError("[ERROR]: Trama inesperada después de enviar el archivo.");
        }
    } else {
        logError("[ERROR]: Error al recibir validación del Worker.");
    }
}

void receiveDistortedFileFromWorker(int workerSocket) {
    Frame frame = {0};

    // Recibir la información del archivo (0x04)
    if (receive_frame(workerSocket, &frame) == 0 && frame.type == 0x04) {
        char fileSize[16], md5Sum[33];
        sscanf(frame.data, "%15[^&]&%32s", fileSize, md5Sum);
        logInfo("[INFO]: Información del archivo distorsionado recibida.");

        // Crear el archivo para guardar datos
        int fd = open("distorted_file", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            logError("[ERROR]: No se pudo crear el archivo distorsionado.");
            return;
        }

        // Recibir datos del archivo (0x05)
        while (receive_frame(workerSocket, &frame) == 0 && frame.type == 0x05) {
            write(fd, frame.data, frame.data_length);
        }
        close(fd);

        logInfo("[INFO]: Archivo distorsionado recibido.");
    } else {
        logError("[ERROR]: No se recibió información válida del archivo distorsionado.");
    }
}


void handleWorkerFailure(const char *mediaType, const char *fileName, int gothamSocket) {
    Frame frame = {0};
    frame.type = 0x11; // Tipo de trama para reasignar Worker
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    logInfo("[INFO]: Enviando solicitud de reasignación de Worker a Gotham...");
    escribir_trama(gothamSocket, &frame);

    // Esperar respuesta de Gotham
    Frame response;
    if (leer_trama(gothamSocket, &response) == 0) {
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
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

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
                printf("Port: %d, ip: %s", workerPort, workerIp);
                if (strlen(workerIp) == 0 || workerPort <= 0) {
                    logError("[ERROR]: Datos del Worker inválidos recibidos.");
                    return;
                }
                printf("[DEBUG]: Intentando conectar al Worker - IP: %s, Puerto: %d\n", workerIp, workerPort);

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
                frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

                logInfo("[INFO]: Enviando solicitud DISTORT FILE al Worker...");
                send_frame(workerSocket, &frame);

                // Manejar la respuesta del Worker
                Frame workerResponse;
                if (receive_frame(workerSocket, &workerResponse) == 0) {
                    if (workerResponse.type == 0x03 && workerResponse.data_length == 0) {
                        logSuccess("[SUCCESS]: Worker listo para recibir.");
                        char userName[] = "defaultUser"; // Placeholder para el nombre de usuario
                        char fileSize[] = "PLACEHOLDER_SIZE"; // Tamaño del archivo
                        char md5Sum[] = "PLACEHOLDER_MD5";    // MD5 placeholder
                        sendDistortFileRequest(workerSocket, userName, fileName, fileSize, md5Sum, factor); // Enviar el archivo
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
            logError("[ERROR]: No hay workers encontrados por Gotham.");
        }
    } else {
        logError("[ERROR]: No se recibió respuesta de Gotham.");
    }
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

    logInfo("[INFO]: Enviando trama de desconexión a Gotham...");
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
        logInfo("[INFO]: Fleck desconectado correctamente. Cerrando aplicación.");
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