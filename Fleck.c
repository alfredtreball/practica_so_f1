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
#include <errno.h>
#include <poll.h>

#include "GestorTramas/GestorTramas.h"
#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"
#include "FrameUtils/FrameUtils.h"
#include "FrameUtilsBinary/FrameUtilsBinary.h"
#include "Logging/Logging.h"
#include "MD5SUM/md5Sum.h"
#include "CleanFiles/CleanFiles.h"  // Incluir el módulo

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536
#define FILE_PATH "fitxers_prova/"
#define DATA_SIZE 247
#define SEGONA_PART_ENVIAMENT 50
#define MAX_FILES 100

typedef struct {
    int workerSocket;
    const char *filePath;
    off_t fileSize;
} DistortRequestArgs;

typedef struct {
    char mediaType[10];    // Tipo de archivo: "MEDIA" o "TEXT"
    char fileName[256];    // Nombre del archivo
    char md5[33];          // MD5 del archivo
    char factor[10];       // Factor de distorsión
    const char *filePath;
    off_t fileOffset;      // Posición actual en el archivo
    off_t fileSize;        // Tamaño total del archivo
    int workerSocket;      // Socket del Worker
} DistortionState;

// Estructura para progreso de archivos
typedef struct {
    char fileName[256]; // Nombre del archivo
    int progress;       // Progreso en porcentaje
} FileProgress;

// Lista global para progreso
FileProgress fileProgressList[MAX_FILES];
int fileProgressCount = 0;
pthread_mutex_t progressMutex = PTHREAD_MUTEX_INITIALIZER;

int gothamSocket = -1; // Variable global para manejar el socket
int workerSocket = -1;
float statusResult;
char distortedFileName[256] = {0};
char receivedMD5Sum[33] = {0};

void signalHandler(int sig);
void processCommandWithGotham(const char *command);
void listText(const char *directory);
void listMedia(const char *directory);
void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket);
void alliberarMemoria(FleckConfig *fleckConfig);
void sendFileToWorker(int workerSocket, const char *fileName);
void *sendFileChunks(void *args);
DistortRequestArgs* sendDistortFileRequest(int workerSocket, const char *fileName, off_t fileSize, const char *md5Sum, const char *factor);
void sendDisconnectFrameToGotham(const char *userName);
void sendDisconnectFrameToWorker(int workerSocket, const char *userName);
void sendMD5Response(int clientSocket, const char *status);
void processCommand(char *command, int gothamSocket);
int solicitarReasignacionAWorker(DistortionState *globalState);
void releaseResources();

FleckConfig *globalFleckConfig = NULL;
DistortionState *globalState = NULL;

volatile sig_atomic_t stop = 0; // Controla si el programa debe detenerse
volatile time_t lastHeartbeat = 0;
volatile int distortionInProgress = 0; // 1 si hay distorsión en curso, 0 si no

pthread_mutex_t distortionMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t distortionFinished = PTHREAD_COND_INITIALIZER;
pthread_mutex_t heartbeatMutex = PTHREAD_MUTEX_INITIALIZER;

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
        free(line);

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

        free(line);
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
        free(line);

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

        free(line);
        close(tempFd);
    } else {
        printF("Error en fork\n");
    }
}

void *listenToHarley() {
    static int fileDescriptor = -1;
    int fileComplete = 0;

    int receivedChunks = 1;
    int bytesRebuts;

    while (1) {
        union {
            Frame normal;
            BinaryFrame binario;
        } frame;

        int is_binary = -1;
        //customPrintf("Voy a leer de workerSocket: %d", globalState->workerSocket);
        if (receive_any_frame(globalState->workerSocket, &frame, &is_binary) != 0) {
            customPrintf("Error recibiendo trama. Posible caída de Harley.\n");
        
            // Detectar socket cerrado
            char tmp;
            int ret = recv(globalState->workerSocket, &tmp, 1, MSG_PEEK);
            if (ret == 0) {
                customPrintf("Socket cerrado. Reasignando Harley...\n");
        
                if (!solicitarReasignacionAWorker(globalState)) {
                    customPrintf("[ERROR]: No se pudo reasignar el Worker.\n");
                    free(globalState);
                    return NULL;
                }
        
                // Esperar hasta que el nuevo socket esté listo
                while (workerSocket != globalState->workerSocket) {
                    usleep(1000);
                    workerSocket = globalState->workerSocket;
                }
        
                customPrintf("[INFO]: Reasignación completada. Continuando recepción...\n");
                continue; // volver al bucle y seguir recibiendo
            } else {
                break; // error fatal o socket válido pero con error
            }
        }

        customPrintf("[DEBUG] 📨 Trama recibida de tipo: 0x%02X", frame.normal.type);

        //Decidir qué hacer según el type
        if (frame.normal.type == 0x05) {  // Trama binaria
            BinaryFrame *binaryResponse = &frame.binario;

            if (binaryResponse->data_length > DATA_SIZE) {
                customPrintf("[ERROR]: Longitud de datos inválida en trama 0x05.");
                continue;
            }

            if (fileDescriptor == -1) {
                fileDescriptor = open(globalState->filePath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (fileDescriptor < 0) {
                    customPrintf("[ERROR]: No se pudo abrir el archivo para escribir.");
                    continue;
                }
            }

            if (write(fileDescriptor, binaryResponse->data, binaryResponse->data_length) != binaryResponse->data_length) {
                customPrintf("[ERROR]: Fallo al escribir datos en el archivo.");
                close(fileDescriptor);
                fileDescriptor = -1;
                continue;
            }

            receivedChunks++;
            statusResult = 50.0 + ((float)receivedChunks * DATA_SIZE / (float)globalState->fileSize) * 50.0;

            bytesRebuts += binaryResponse->data_length;

            if (binaryResponse->data_length < DATA_SIZE) { 
                close(fileDescriptor);
                fileDescriptor = -1;
                logInfo("[SUCCESS]: Archivo recibido completamente.");
                customPrintf("\n%d\n", bytesRebuts);
                fileComplete = 1;
                pthread_mutex_lock(&distortionMutex);
                distortionInProgress = 0;
                pthread_cond_signal(&distortionFinished);
                pthread_mutex_unlock(&distortionMutex);
            }

            if (fileComplete) {
                char calculatedMD5[33] = {0};
                calculate_md5(globalState->filePath, calculatedMD5);
                
                if (strcmp(calculatedMD5, receivedMD5Sum) == 0) {
                    logInfo("[SUCCESS]: MD5 correcto. Enviando CHECK_OK a Harley.");
                    sendMD5Response(globalState->workerSocket, "CHECK_OK");
                } else {
                    customPrintf("[ERROR]: MD5 incorrecto. Enviando CHECK_KO a Harley.");
                    customPrintf("md5: %s\n md5 expected: %s\nfileComplete: %s\n", calculatedMD5, receivedMD5Sum, fileComplete);
                    sendMD5Response(globalState->workerSocket, "CHECK_KO");
                }
                break;
            }

        } else {  // Trama normal
            Frame *request = &frame.normal;

            if (request->type == 0x04) {
                char fileSizeStr[20];
                char md5Sum[33];
                if (sscanf(request->data, "%19[^&]&%32s", fileSizeStr, md5Sum) != 2) {
                    customPrintf("[ERROR]: Trama 0x04 de Harley con formato inválido.");
                    continue;
                }
                strncpy(receivedMD5Sum, md5Sum, sizeof(receivedMD5Sum) - 1);
                customPrintf("md5: %s\n", md5Sum);
                customPrintf("md5 expected: %s\n", receivedMD5Sum);
            }

            if (request->type == 0x06) { // Confirmación de MD5
                if (strcmp(request->data, "CHECK_OK") == 0) {
                    customPrintf("\n[INFO]: Harley ha confirmado correctamente el MD5 del archivo recibido (CHECK_OK).\n");
                    //Comprovar si harley segueix actiu
                    char buf[1];
                    int ret = recv(globalState->workerSocket, buf, 1, MSG_PEEK);
                    if (ret == 0) {
                        customPrintf("[INFO]: Socket de Harley cerrado tras CHECK_OK. Reasignando...\n");

                        if (!solicitarReasignacionAWorker(globalState)) {
                            customPrintf("[ERROR]: No se pudo reasignar el Worker.");
                            free(globalState);
                            return NULL;
                        }

                        while (workerSocket != globalState->workerSocket) {
                            usleep(1000);
                            workerSocket = globalState->workerSocket;
                        }

                        customPrintf("[INFO]: Nuevo Harley asignado. Esperando archivo distorsionado...\n");
                    }
                } else if (strcmp(request->data, "CHECK_KO") == 0) {
                    customPrintf("[ERROR]: Harley ha reportado un error en la comprobación MD5 del archivo recibido de Fleck (CHECK_KO).");
                }
            }
        }
    }

    if (fileDescriptor != -1) {
        close(fileDescriptor);
        fileDescriptor = -1;
    }

    if (globalState->workerSocket == -1) {
        free(globalState);
    }

    return NULL;
}

void *listenToEnigma() {
    static int fileDescriptor = -1;
    int fileComplete = 0;
    int receivedChunks = 1;
    int bytesReceived;

    while (1) {
        union {
            Frame normal;
            BinaryFrame binario;
        } frame;

        int is_binary = -1;
        //customPrintf("Voy a leer de workerSocket: %d", globalState->workerSocket);
        if (receive_any_frame(globalState->workerSocket, &frame, &is_binary) != 0) {
            customPrintf("Error recibiendo trama. Posible caída de Harley.\n");
        
            // Detectar socket cerrado
            char tmp;
            int ret = recv(globalState->workerSocket, &tmp, 1, MSG_PEEK);
            if (ret == 0) {
                customPrintf("Socket cerrado. Reasignando Harley...\n");
        
                if (!solicitarReasignacionAWorker(globalState)) {
                    customPrintf("[ERROR]: No se pudo reasignar el Worker.\n");
                    free(globalState);
                    return NULL;
                }
        
                // Esperar hasta que el nuevo socket esté listo
                while (workerSocket != globalState->workerSocket) {
                    usleep(1000);
                    workerSocket = globalState->workerSocket;
                }
        
                customPrintf("[INFO]: Reasignación completada. Continuando recepción...\n");
                continue; // volver al bucle y seguir recibiendo
            } else {
                break; // error fatal o socket válido pero con error
            }
        }

        customPrintf("[DEBUG] 📨 Trama recibida de Enigma de tipo: 0x%02X", frame.normal.type);

        //Decidir qué hacer según el type
        if (frame.normal.type == 0x05) {  // Trama binaria
            BinaryFrame *binary = &frame.binario;

            if (binary->data_length > DATA_SIZE) {
                customPrintf("[ERROR]: Longitud de datos inválida en trama 0x05.");
                continue;
            }

            if (fileDescriptor == -1) {
                fileDescriptor = open(globalState->filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fileDescriptor < 0) {
                    customPrintf("[ERROR]: No se pudo abrir el archivo para escribir.");
                    continue;
                }
            }

            if (write(fileDescriptor, binary->data, binary->data_length) != binary->data_length) {
                customPrintf("[ERROR]: Fallo al escribir datos en el archivo.");
                close(fileDescriptor);
                fileDescriptor = -1;
                continue;
            }

            receivedChunks++;
            statusResult = 50.0 + ((float)bytesReceived * DATA_SIZE / (float)globalState->fileSize) * 50.0;

            bytesReceived += binary->data_length;

            if (binary->data_length < DATA_SIZE) { 
                close(fileDescriptor);
                fileDescriptor = -1;
                logInfo("[SUCCESS]: Archivo recibido completamente.");
                customPrintf("\n%d\n", bytesReceived);
                fileComplete = 1;
                pthread_mutex_lock(&distortionMutex);
                distortionInProgress = 0;
                pthread_cond_signal(&distortionFinished);
                pthread_mutex_unlock(&distortionMutex);
            }

            if (fileComplete) {
                char calculatedMD5[33] = {0};
                calculate_md5(globalState->filePath, calculatedMD5);
                
                if (strcmp(calculatedMD5, receivedMD5Sum) == 0) {
                    logInfo("[SUCCESS]: MD5 correcto. Enviando CHECK_OK a Enigma.\n");
                    sendMD5Response(globalState->workerSocket, "CHECK_OK");
                } else {
                    customPrintf("[ERROR]: MD5 incorrecto. Enviando CHECK_KO a Enigma.\n");
                    customPrintf("md5: %s\n md5 expected: %s\nfileComplete: %s\n", calculatedMD5, receivedMD5Sum, fileComplete);
                    sendMD5Response(globalState->workerSocket, "CHECK_KO");
                }
                break;
            }

        } else {  // Trama normal
            if (frame.normal.type == 0x04) {
                char fileSizeStr[20];
                char md5Sum[33];
                if (sscanf(frame.normal.data, "%19[^&]&%32s", fileSizeStr, md5Sum) != 2) {
                    customPrintf("[ERROR]: Trama 0x04 de Enigma con formato inválido.");
                    continue;
                }
                strncpy(receivedMD5Sum, md5Sum, sizeof(receivedMD5Sum) - 1);
                customPrintf("md5: %s\n", md5Sum);
                customPrintf("md5 expected: %s\n", receivedMD5Sum);
            }

            if (frame.normal.type == 0x06) { // Confirmación de MD5
                if (strcmp(frame.normal.data, "CHECK_OK") == 0) {
                    customPrintf("\n[INFO]: Enigma ha confirmado correctamente el MD5 del archivo recibido (CHECK_OK).\n");
                    //Comprovar si harley segueix actiu
                    char buf[1];
                    int ret = recv(globalState->workerSocket, buf, 1, MSG_PEEK);
                    if (ret == 0) {
                        customPrintf("[INFO]: Socket de Enigma cerrado tras CHECK_OK. Reasignando...\n");

                        if (!solicitarReasignacionAWorker(globalState)) {
                            customPrintf("[ERROR]: No se pudo reasignar el Worker.");
                            free(globalState);
                            return NULL;
                        }

                        while (workerSocket != globalState->workerSocket) {
                            usleep(1000);
                            workerSocket = globalState->workerSocket;
                        }

                        customPrintf("[INFO]: Nuevo Enigma asignado. Esperando archivo distorsionado...\n");
                    }
                } else if (strcmp(frame.normal.data, "CHECK_KO") == 0) {
                    customPrintf("[ERROR]: Enigma ha reportado un error en la comprobación MD5 del archivo recibido de Fleck (CHECK_KO).");
                }
            }
        }
    }

    if (fileDescriptor != -1) {
        close(fileDescriptor);
        fileDescriptor = -1;
    }

    if (globalState->workerSocket == -1) {
        free(globalState);
    }

    return NULL;
}


void *listenToGotham(void *arg) {
    int gothamSocket = *(int *)arg;
    free(arg);

    while (1) {
        Frame frame = {0};
        if (leerTrama(gothamSocket, &frame) != 0) {
            customPrintf("\nError al leer el socket de Gotham. Gotham se ha desconectado.\n");
            close(gothamSocket);
            gothamSocket = -1;
            break; // Salir del bucle si hay un error o desconexión
        }

        char workerIp[16] = {0};
        int workerPort = 0;

        switch (frame.type) {
            case 0x01: // Ejemplo: Trama de confirmación de conexión
                if (frame.data_length != 0) { // DATA vacío, longitud 0
                    printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta inesperada de Gotham.\n");
                    close(gothamSocket);
                    gothamSocket = -1;
                    break;
                }
                break;

            case 0x10:
                if (strcmp(frame.data, "DISTORT_KO") == 0) {
                    customPrintf("[ERROR]: Gotham no encontró un Worker disponible.");
                    break;
                } else if (strcmp(frame.data, "MEDIA_KO") == 0) {
                    customPrintf("[ERROR]: Tipo de archivo rechazado por Gotham.");
                    break;
                }

                // Conectar al Worker
                if (sscanf(frame.data, "%15[^&]&%d", workerIp, &workerPort) != 2 || strlen(workerIp) == 0 || workerPort <= 0) {
                    customPrintf("[ERROR]: Datos del Worker inválidos.");
                    break;
                }

                workerSocket = connect_to_server(workerIp, workerPort);
                if (workerSocket < 0) {
                    customPrintf("[ERROR]: No se pudo conectar al Worker.");
                    free(globalState);
                    globalState = NULL;
                    break;
                }

                globalState->workerSocket = workerSocket;

                DistortRequestArgs *args;

                // Enviar solicitud DISTORT FILE al Worker
                args = sendDistortFileRequest(globalState->workerSocket, globalState->fileName, globalState->fileSize, globalState->md5, globalState->factor);

                pthread_t thread;
                if (pthread_create(&thread, NULL, sendFileChunks, (void *) args) != 0) {
                    customPrintf("[ERROR]: No se pudo crear el thread para enviar las tramas.");
                    free(args);
                    return NULL;
                }

                pthread_detach(thread); // Liberar el hilo automáticamente

                break;  
            
            case 0x11:
                if (strcmp(frame.data, "DISTORT_KO") == 0) {
                    customPrintf("[ERROR]: Gotham no pudo reasignar un Worker. Distorsión cancelada.");
                    break; // Fallo en la reasignación
                } else if (strcmp(frame.data, "MEDIA_KO") == 0) {
                    customPrintf("[ERROR]: Gotham indicó que el tipo de media es inválido.");
                    break; // Fallo en la reasignación
                }
            
                // Conectar al Worker
                if (sscanf(frame.data, "%15[^&]&%d", workerIp, &workerPort) != 2 || strlen(workerIp) == 0 || workerPort <= 0) {
                    customPrintf("[ERROR]: Datos del Worker inválidos.");
                    break;
                }
            
                int newWorkerSocket = connect_to_server(workerIp, workerPort);
                if (newWorkerSocket < 0) {
                    customPrintf("[ERROR]: No se pudo conectar al Worker.");
                    free(globalState);
                    globalState = NULL;
                    break;
                }
            
                customPrintf("[SUCCESS]: Conexión establecida con el nuevo Worker. IP = %s, PORT = %d\n", workerIp, workerPort);
                usleep(1000); //1ms d'espera
                globalState->workerSocket = newWorkerSocket; // Actualizar el socket global del Worker
                customPrintf("Reasignació de worker, md5sum: %s\n", globalState->md5);
                customPrintf("FILESIZE: %d\n\n", globalState->fileSize);
                
                sendDistortFileRequest(newWorkerSocket, globalState->fileName, globalState->fileSize, globalState->md5, globalState->factor);  
                
                //AFEGIT - NOU FIL DE RECEPCIÓ
                pthread_t harleyListenerThread;
                int *socketArg = malloc(sizeof(int));
                if (!socketArg) {
                    customPrintf("[ERROR]: No se pudo asignar memoria para socketArg.\n");
                    return NULL;
                }

                *socketArg = newWorkerSocket;
                if (pthread_create(&harleyListenerThread, NULL, listenToHarley, socketArg) != 0) {
                    perror("pthread_create"); // Mostrará el motivo (ej: ENOMEM)
                    customPrintf("[ERROR]: No se pudo crear el hilo para escuchar a Harley.");
                    return NULL;
                }
                pthread_detach(harleyListenerThread);

                break;

            case 0x12: // Ejemplo: Trama de desconexión
                pthread_mutex_lock(&heartbeatMutex);
        
                time_t currentTime = time(NULL);
                if (difftime(currentTime, lastHeartbeat) > 3) { // Si ha pasado más de 1 segundo desde el último heartbeat
                    customPrintf("[ERROR]: Heartbeat de Gotham recibido demasiado tarde. Se asume que ha caído.");
                    pthread_mutex_unlock(&heartbeatMutex);
            
                    pthread_mutex_lock(&distortionMutex);
                    if (distortionInProgress) {
                        logInfo("[INFO]: Esperando a que termine la distorsión en curso antes de cerrar...");
                        while (distortionInProgress) {
                            pthread_cond_wait(&distortionFinished, &distortionMutex);
                        }
                        pthread_mutex_unlock(&distortionMutex);
                    } else {
                        pthread_mutex_unlock(&distortionMutex);
                    }
            
                    releaseResources();
                    exit(0); // Salimos del proceso ya que Gotham ha caído.
                } else {
                    lastHeartbeat = currentTime; // Actualizamos el último heartbeat válido
                    pthread_mutex_unlock(&heartbeatMutex);
                }
                break;

            default:
                logWarning("[WARNING]: Tipo de trama desconocido recibido.");
                break;
        }
    }

    close(gothamSocket);
    gothamSocket = -1;
    return NULL;
}

void calculateOnDemandProgress() {
    pthread_mutex_lock(&progressMutex);

    // Limpiar lista actual
    fileProgressCount = 0;

    // Si hay un archivo activo
    if (strlen(distortedFileName) > 0 && statusResult >= 0 && statusResult <= 100) {
        strncpy(fileProgressList[0].fileName, distortedFileName, sizeof(fileProgressList[0].fileName));
        fileProgressList[0].progress = (int)statusResult;
        fileProgressCount = 1;
    }

    pthread_mutex_unlock(&progressMutex);
}

// Función para mostrar el estado de progreso
void checkStatus() {
    calculateOnDemandProgress();  // 👈 importante

    pthread_mutex_lock(&progressMutex);

    if (fileProgressCount == 0) {
        customPrintf("You have no ongoing or finished distorsions.");
    } else {
        for (int i = 0; i < fileProgressCount; i++) {
            int completed = (int)((fileProgressList[i].progress / 100.0) * 20);

            // Crear la barra dinámicamente
            char *bar = NULL;
            if (asprintf(&bar, "%.*s%.*s", completed, "====================", 20 - completed, "                    ") == -1) {
                customPrintf("[ERROR]: No se pudo crear la barra de progreso.");
                continue;
            }

            // Crear el output con asprintf
            char *output = NULL;
            if (asprintf(&output, "%-20s %6d%% |%s|\n",
                        fileProgressList[i].fileName,
                        fileProgressList[i].progress,
                        bar) == -1) {
                customPrintf("[ERROR]: Fallo al generar la línea de salida.");
                free(bar);
                continue;
            }

            write(STDOUT_FILENO, output, strlen(output));
            free(bar);
            free(output);
        }
    }

    pthread_mutex_unlock(&progressMutex);
}

void clearAllFinishedProgress() {
    pthread_mutex_lock(&progressMutex);

    int i = 0;
    while (i < fileProgressCount) {
        if (fileProgressList[i].progress == 100) {
            // Sustituimos el actual con el último
            fileProgressList[i] = fileProgressList[fileProgressCount - 1];
            fileProgressCount--;
            // No incrementamos i, porque hemos movido un nuevo elemento aquí
        } else {
            i++;
        }
    }

    pthread_mutex_unlock(&progressMutex);

    printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Completed downloads have been cleared from progress list.\n");
}

void processCommandWithGotham(const char *command) {
    Frame frame = {0};
    if (strcasecmp(command, "CONNECT") == 0) {
        if (gothamSocket != -1) {
            return;
        }

        gothamSocket = connect_to_server(globalFleckConfig->ipGotham, globalFleckConfig->portGotham);
        if (gothamSocket < 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo establecer la conexión con Gotham. Comprobar IP y puerto.\n");
            gothamSocket = -1; // Aseguramos que quede desconectado
            return;
        }

        // Eliminar cualquier carácter no deseado de user
        removeChar(globalFleckConfig->user, '\r');
        removeChar(globalFleckConfig->user, '\n');

        frame.type = 0x01; // Tipo de conexión
        frame.timestamp = (uint32_t)time(NULL);

        // Construir la trama con el formato <userName>&<IP>&<Port>
        snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
                 globalFleckConfig->user, globalFleckConfig->ipGotham, globalFleckConfig->portGotham);
        frame.data_length = strlen(frame.data);
        frame.checksum = calculate_checksum(frame.data, frame.data_length, 0);

        // Enviar trama de conexión
        if (escribirTrama(gothamSocket, &frame) < 0) {
            customPrintf("[GestorTramas] Error al enviar la solicitud de conexión a Gotham.");
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }

        customPrintf("%s connected to Mr. J System. Let the chaos begin!:)\n", globalFleckConfig->user);

        pthread_mutex_lock(&heartbeatMutex);
        lastHeartbeat = time(NULL); // Inicializar el tiempo del último heartbeat
        pthread_mutex_unlock(&heartbeatMutex);

        // Iniciar thread para escuchar a Gotham
        pthread_t listenThread;
        int *socketArg = malloc(sizeof(int));
        if (socketArg == NULL) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo asignar memoria para el thread.\n");
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }
        *socketArg = gothamSocket;

        if (pthread_create(&listenThread, NULL, listenToGotham, (void *)socketArg) != 0) {
            printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo crear el thread para escuchar a Gotham.\n");
            free(socketArg);
            close(gothamSocket);
            gothamSocket = -1;
            return;
        }

        pthread_detach(listenThread); // Liberar el hilo automáticamente al finalizar

        /*//Escoltar heartbeat
        pthread_t heartbeatMonitorThread;
        if (pthread_create(&heartbeatMonitorThread, NULL, monitorHeartbeats, NULL) != 0) {
            customPrintf("[ERROR]: No se pudo crear el hilo de monitoreo de heartbeats.");
            exit(1);
        }
        pthread_detach(heartbeatMonitorThread); // Liberar automáticamente al finalizar*/
    }
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
            listMedia(globalFleckConfig->directory);
        } else if (strcasecmp(subCmd, "TEXT") == 0 && extra == NULL) {
            listText(globalFleckConfig->directory);
        } else {
            customPrintf("Unknown command.");
        }
    } else if (strcasecmp(cmd, "CLEAR") == 0 && strcasecmp(subCmd, "ALL") == 0 && extra == NULL) {
        customPrintf("[INFO]: Clearing all completed downloads...\n");
        clearAllFinishedProgress();    
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) {
        logInfo("[INFO]: Procesando comando LOGOUT...");
        // Enviar trama de desconexión a Gotham
        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham(globalFleckConfig->user);
            logInfo("[INFO]: Desconexión de Gotham completada.");
        }
        // Liberar todos los recursos
        releaseResources();

        // Finalizar el programa
        logInfo("[INFO]: Saliendo del programa Fleck.");
        exit(0);
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        if (gothamSocket == -1) {
            customPrintf("Cannot distort, you are not connected to Mr. J System");
        } else {
            processDistortFileCommand(subCmd, extra, gothamSocket);
        }
    } else if (strcasecmp(cmd, "CHECK") == 0 && strcasecmp(subCmd, "STATUS") == 0 && extra == NULL) {
            checkStatus();
    } else {
        printF("ERROR: Please input a valid command.");
    }
}

//LLancem thread per iniciar un nou fil que envïi la trama 0x05 amb longitud data 247 per parts.
void *sendFileChunks(void *args) {
    DistortRequestArgs *requestArgs = (DistortRequestArgs *)args;
    int workerSocket = requestArgs->workerSocket;
    const char *filePath = requestArgs->filePath;
    off_t fileSize = requestArgs->fileSize;
    free(requestArgs); // Liberar memoria de los argumentos

    int fd = open(filePath, O_RDONLY, 0666);
    if (fd < 0) {
        customPrintf("[ERROR]: No se pudo abrir el archivo especificado.");
        return NULL;
    }

    BinaryFrame frame = {0};
    //BinaryFrame lastFrame;
    static int reassigningInProgress = 0; // Variable para controlar la reasignación

    char buffer[DATA_SIZE]; // Tamaño permitido para DATA (247 bytes)
    ssize_t bytesRead;
    ssize_t totalSent = 0;
    int status = 1;

    // Extraer el nombre del archivo para el progreso
    char *fileName = strrchr(filePath, '/');
    if (fileName == NULL) {
        fileName = (char *)filePath; // Si no hay directorio, usar todo el path
    } else {
        fileName++; // Saltar el '/'
    }

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {    
        
        customPrintf("NOU bytes enviats: %d\n", totalSent);

        frame.type = 0x05;
        frame.data_length = bytesRead;
        memcpy(frame.data, buffer, bytesRead);
        frame.timestamp = (uint32_t)time(NULL);
        frame.checksum = calculate_checksum_binary(frame.data, frame.data_length, 1);

        //memcpy(&lastFrame, &frame, sizeof(BinaryFrame)); //Guardem la trama per si hi ha error al enviar-la

        ssize_t sentBytes = escribirTramaBinaria(workerSocket, &frame);
        usleep(5000); // Espera 1 ms (ajustable)

        if (sentBytes < 0) {
            customPrintf("[ERROR]: Fallo al enviar trama 0x05. Verificando reconexión...");
            // Se conectó a un nuevo Worker, continuar desde el offset
            usleep(10000);  // Esperar 100ms antes de reintentar
            totalSent -= 247; // Restar el tamaño del chunk enviado
            lseek(fd, totalSent, SEEK_SET); // Volver al offset anterior
            customPrintf("bytes enviats de moment: %d\n\n", totalSent);
    
            if (errno == EPIPE || errno == ECONNRESET) {
                customPrintf("[ERROR]: Conexión con Harley perdida. Intentando reasignación...\n");
                close(workerSocket);
                workerSocket = -1; 

                if (!reassigningInProgress) {
                    reassigningInProgress = 1;
                    if (!solicitarReasignacionAWorker(globalState)) {
                        customPrintf("[ERROR]: No se pudo reasignar el Worker.");
                        free(globalState);
                        return NULL;
                    }
                }

                //TODO MIRAR AQUÍ EL CODI QUE ESTÀ PASSANT
                while (workerSocket != globalState->workerSocket) {
                    logInfo("[INFO]: Esperando a que el nuevo Harley esté listo para continuar envío...");
                    usleep(1000); // Espera 1ms (ajustable)
                    workerSocket = globalState->workerSocket;
                    customPrintf("\n\n[DEBUG] Reenviando la trama colgada tras reconexión...\n");
                } 
            }
            
        } else {
            totalSent += bytesRead;
            statusResult = ((float)totalSent / (float)fileSize) * 50.0;
            status++;
        }
        usleep(3000);
    }

    // 🚨 Comprobación final
    if (bytesRead < 0) {
        customPrintf("\n[ERROR] ❌ write() devolvió error: errno=%d (%s)", errno, strerror(errno));
    } else if (bytesRead == 0) {
        // Terminó el fichero
        customPrintf("\n[DEBUG] 🟢 write()=0 => fin del fichero. Se enviaron %zd bytes en total.", totalSent);

        // Aquí lanzas de nuevo el hilo 
        if (strcmp(globalState->mediaType, "MEDIA") == 0) {
            pthread_t harleyListenerThread;
            int *socketArg = malloc(sizeof(int));
            *socketArg = workerSocket;
            if (pthread_create(&harleyListenerThread, NULL, listenToHarley, socketArg) != 0) {
                customPrintf("[ERROR]: No se pudo crear el hilo para escuchar a Harley.");
                return NULL;
            }
            pthread_detach(harleyListenerThread);
        } else if (strcmp(globalState->mediaType, "TEXT") == 0) {
            pthread_t enigmaListenerThread;
            int *socketArgEn = malloc(sizeof(int));
            *socketArgEn = workerSocket;
            if (pthread_create(&enigmaListenerThread, NULL, listenToEnigma, socketArgEn) != 0) {
                customPrintf("[ERROR]: No se pudo crear el hilo para escuchar a Enigma.");
                return NULL;
            }
            pthread_detach(enigmaListenerThread);
        }
    }

    close(fd);

    return NULL;
}

void sendMD5Response(int clientSocket, const char *status) {
    Frame response = {0};
    response.type = 0x06; // Tipo de trama para respuesta MD5
    strncpy(response.data, status, sizeof(response.data) - 1);
    response.data_length = strlen(response.data);
    response.timestamp = (uint32_t)time(NULL);
    response.checksum = calculate_checksum(response.data, response.data_length, 1);

    customPrintf("[DEBUG] 🔵 Enviando MD5 a Fleck: %s\n", status);

    if (clientSocket < 0) {
        customPrintf("[ERROR]: El socket de Fleck ya está cerrado. No se puede enviar MD5.");
        return;
    }    

    int result = escribirTrama(clientSocket, &response);
    if (result < 0) {
        customPrintf("[ERROR]: Fallo al enviar la respuesta MD5. Socket cerrado o desconectado.");
    } else {
        logInfo("[SUCCESS]: Trama MD5 enviada correctamente.");
    }
}

DistortRequestArgs* sendDistortFileRequest(int workerSocket, const char *fileName, off_t fileSize, const char *md5Sum, const char *factor) {
    char *filePath = NULL;
    if (asprintf(&filePath, "%s%s", FILE_PATH, fileName) == -1) {
        customPrintf("[ERROR]: No se pudo asignar memoria para el path del archivo.");
        return NULL;
    }

    if (workerSocket < 0) {
        customPrintf("[ERROR]: workerSocket no es válido. Verifica la conexión con el Worker.");
        free(filePath);
        return NULL;
    }

    // Guardar el nombre del archivo en la variable global
    strncpy(distortedFileName, fileName, sizeof(distortedFileName) - 1);

    // Enviar trama inicial de DISTORT FILE (0x03)
    Frame frame = {0};
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%ld&%s&%s", 
             globalFleckConfig->user, fileName, fileSize, md5Sum, factor);
    frame.type = 0x03;
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (workerSocket < 0) {
        customPrintf("[ERROR]: workerSocket no es válido. Verifica la conexión con el Worker.");
        free(filePath);
        return NULL;
    }

    if (escribirTrama(workerSocket, &frame) < 0) {
        customPrintf("[ERROR]: Error al enviar solicitud DISTORT FILE.");
        free(filePath);
        return NULL;
    }

    // Esperar respuesta de Harley
    Frame response = {0};
    if (leerTrama(workerSocket, &response) != 0 || response.type != 0x03) {
        customPrintf("[ERROR]: No se recibió confirmación de conexión del Worker.");
        free(filePath);
        return NULL;
    }

    if (strcmp(response.data, "CON_KO") == 0) {
        customPrintf("[ERROR]: Worker rechazó la conexión.");
        free(filePath);
        return NULL;
    }

    customPrintf("[DEBUG][Fleck]: Confirmación 0x03 de Harley recibida correctamente.\n");
    
    // Cierra el socket anterior para forzar que el hilo anterior salga
    if (globalState->workerSocket != -1 && globalState->workerSocket != workerSocket) {
        close(globalState->workerSocket);  // Esto hace que el read falle y el hilo muera
    }
    // Actualiza el socket en el estado global
    globalState->workerSocket = workerSocket;

    //Dos threads al mateix temps
    //pthread_mutex_lock(&distortionMutex);
    //distortionInProgress = 1; // Distorsión en curso
    //pthread_mutex_unlock(&distortionMutex);

    // Preparar hilo para enviar las tramas 0x05
                
    DistortRequestArgs *args = malloc(sizeof(DistortRequestArgs));
    if (!args) {
        customPrintf("[ERROR]: No se pudo asignar memoria para los argumentos del thread.");
        return NULL;
    }

    args->workerSocket = workerSocket;
    args->filePath = filePath;
    args->fileSize = fileSize;

    return args;
}

int solicitarReasignacionAWorker(DistortionState *globalState) {
    if(!globalState){
        customPrintf("Estado de distorsión no válido.\n");
        return 0;
    }

    Frame frame = {0};
    frame.type = 0x11; // Tipo de trama para reasignar Worker
    size_t maxMediaTypeLen = 9; // Longitud máxima para mediaType
    size_t maxFileNameLen = sizeof(frame.data) - maxMediaTypeLen - 2; // Restamos 2: '&' y '\0'
    snprintf(frame.data, sizeof(frame.data), "%.*s&%.*s",
         (int)maxMediaTypeLen, globalState->mediaType, 
         (int)maxFileNameLen, globalState->fileName);
    frame.data[sizeof(frame.data) - 1] = '\0'; // Garantizar el final nulo
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    customPrintf("[INFO]: Enviando solicitud de reasignación de Worker a Goham... 1");
    if (escribirTrama(gothamSocket, &frame) < 0) {
        customPrintf("[ERROR]: No se pudo enviar la solicitud de reasignación a Gotham.");
        return 0; // Fallo en la reasignación
    }

    return 1; // Reasignación exitosa
}

void processDistortFileCommand(const char *fileName, const char *factor, int gothamSocket) {
    if (!fileName || !factor) {
        customPrintf("[ERROR]: DISTORT command requires a FileName and a Factor.");
        return;
    }

    // Determinar el mediaType según la extensión del archivo
    char *fileExtension = strrchr(fileName, '.');
    if (!fileExtension) {
        customPrintf("[ERROR]: File does not have a valid extension.");
        return;
    }

    char mediaType[10];
    if (strcasecmp(fileExtension, ".txt") == 0) {
        strncpy(mediaType, "TEXT", sizeof(mediaType));
    } else if (strcasecmp(fileExtension, ".wav") == 0 || strcasecmp(fileExtension, ".png") == 0 || strcasecmp(fileExtension, ".jpg") == 0) {
        strncpy(mediaType, "MEDIA", sizeof(mediaType));
    } else {
        customPrintf("[ERROR]: Unsupported file extension.");
        return;
    }

    // Construir el path completo del archivo
    char *filePath = NULL;
    if (asprintf(&filePath, "%s%s", FILE_PATH, fileName) == -1) {
        customPrintf("[ERROR]: No se pudo asignar memoria para el path del archivo.");
        return;
    }

    // Calcular el tamaño del archivo
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        customPrintf("[ERROR]: No se pudo abrir el archivo especificado.");
        free(filePath);
        return;
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < 0) {
        customPrintf("[ERROR]: No se pudo calcular el tamaño del archivo.");
        close(fd);
        free(filePath);
        return;
    }
    close(fd);

    // Calcular el MD5 del archivo
    char md5Sum[33] = {0};
    calculate_md5(filePath, md5Sum);

    // Crear e inicializar el estado de distorsión
    globalState = malloc(sizeof(DistortionState));
    if (!globalState) {
        customPrintf("[ERROR]: Could not allocate memory for distortion state.");
        free(filePath);
        return;
    }

    strncpy(globalState->mediaType, mediaType, sizeof(globalState->mediaType));
    strncpy(globalState->fileName, fileName, sizeof(globalState->fileName));
    strncpy(globalState->md5, md5Sum, sizeof(globalState->md5));
    strncpy(globalState->factor, factor, sizeof(globalState->factor));
    globalState->fileOffset = 0; // Comienza desde el principio
    globalState->fileSize = fileSize;
    globalState->filePath = filePath;
    globalState->workerSocket = -1; // Se actualizará al conectarse al Worker

    // Enviar solicitud DISTORT a Gotham
    Frame frame = {0};
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.type = 0x10;
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    customPrintf("\nDistorsion started!\n");
    logInfo("[INFO]: Enviando solicitud DISTORT a Gotham...");
    escribirTrama(gothamSocket, &frame);
}

// Nueva función para liberar recursos asignados por Fleck
void releaseResources() {
    // Liberar campos de configuración asignados dinámicamente
    if (globalFleckConfig) {
        if (globalFleckConfig->user) {
            free(globalFleckConfig->user);
            globalFleckConfig->user = NULL;
        }
        if (globalFleckConfig->directory) {
            free(globalFleckConfig->directory);
            globalFleckConfig->user = NULL;
        }
        if (globalFleckConfig->ipGotham) {
            free(globalFleckConfig->ipGotham);
            globalFleckConfig->user = NULL;
        }
        free(globalFleckConfig);
        globalFleckConfig = NULL;
    }

    // Cerrar conexiones de socket si están abiertas
    if (gothamSocket >= 0) {
        close(gothamSocket);
        gothamSocket = -1;
    }

    if (workerSocket >= 0) {
        close(workerSocket);
        workerSocket = -1;
    }
}

void sendDisconnectFrameToGotham(const char *userName) {
    if (gothamSocket < 0) {
        logWarning("[INFO]: No se pudo enviar la trama de desconexión porque Gotham no está conectado.");
        return;
    }

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
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 0);

    logInfo("[INFO]: Enviando trama de desconexión a Gotham...");
    if (escribirTrama(gothamSocket, &frame) < 0) {
        customPrintf("[ERROR]: Fallo al enviar trama de desconexión a Gotham.");
    }
}

void sendDisconnectFrameToWorker(int workerSocket, const char *userName) {
    if (workerSocket < 0) {
        logWarning("[INFO]: No se pudo enviar la trama de desconexión porque el socket del Worker no está conectado.");
        return;
    }

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
    if (escribirTrama(workerSocket, &frame) < 0) {
        customPrintf("[ERROR]: Fallo al enviar trama de desconexión al Worker.");
    }
}

// FASE 1
void signalHandler(int sig) {
    if (sig == SIGINT) {
        stop = 1;

        // Desconexión de Gotham
        if (gothamSocket >= 0) {
            sendDisconnectFrameToGotham(globalFleckConfig->user);
            close(gothamSocket);
            gothamSocket = -1;
        }

        // Desconexión de Worker
        if (workerSocket >= 0) {
            sendDisconnectFrameToWorker(workerSocket, globalFleckConfig->user);
            close(workerSocket);
            workerSocket = -1;
        }

        customPrintf("\n\n[INFO]: Fleck desconectado. Saliendo...\n");
        // Liberar recursos asignados
        releaseResources();
        exit(0);
    }
}


int main(int argc, char *argv[]) {
    printF("Arthur user initialized\n");

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

    signal(SIGPIPE, SIG_IGN); // Ignorar senyal SIGPIPE
    signal(SIGINT, signalHandler);

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

        processCommand(command, gothamSocket);
        free(command);
    }

    close(gothamSocket);
    free(fleckConfig);

    return 0;
}