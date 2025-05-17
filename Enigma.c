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
#include <poll.h>

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
#include "EnigmaCompress/EnigmaCompress.h"
#include "EnigmaSync/EnigmaSync.h"

#define ENIGMA_PATH_FILES "enigma_directory/"
#define ENIGMA_TYPE "TEXT"

void *handleFleckFrames(void *arg);
void processReceivedFrame(int gothamSocket, const Frame *response);
void processBinaryFrameFromFleck(BinaryFrame *binaryFrame, size_t expectedFileSize, size_t *currentFileSize, int clientSocket);
void enviaTramaArxiuDistorsionat(int clientSocket, const char *fileSizeCompressed, const char *compressedMD5, const char *compressedFilePath, size_t offset);
void send_frame_with_error(int clientSocket, const char *errorMessage);
void send_frame_with_ok(int clientSocket);
void sendMD5Response(int clientSocket, const char *status);

EnigmaConfig *globalenigmaConfig = NULL;
SharedMemory harleySharedMemory; //LINKEDLIST PER TENIR ARRAY AMB DESCÀRREGUES

// Variable global para el socket
int gothamSocket = -1;
float resultStatus;

int lastFleckSocket = -1;

volatile sig_atomic_t stop = 0;

// Variable para almacenar el último tiempo de recepción del HEARTBEAT
volatile time_t lastHeartbeat = 0;

// Mutex para sincronizar el acceso a la variable de último HEARTBEAT
pthread_mutex_t heartbeatMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char fileName[256];
    char md5Sum[33];
    size_t offset;
    int factor;
    int status;
    int clientSocket;
} ResumeArgs;

// Estructura para los argumentos del hilo
typedef struct {
    int clientSocket;
    char *filePath;
    off_t fileSize;
    char *compressedPath; // Ruta del archivo comprimido
    size_t offset; //Byte per continuar l'enviament
} SendCompressedFileArgs;

char receivedFileName[256] = {0}; // Nombre del archivo recibido
int tempFileDescriptor = -1;      // Descriptor del archivo temporal
char expectedMD5[33];
char receivedFactor[20];

void sendDisconnectFrameToGotham(const char *mediaType)
{
    if (gothamSocket < 0) {
        logWarning("[WARNING]: Intento de desconexión pero Gotham ya estaba cerrado.");
        return;
    }

    Frame frame = {0};
    frame.type = 0x07; // Tipo de desconexión
    strncpy(frame.data, mediaType, sizeof(frame.data) - 1);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (escribirTrama(gothamSocket, &frame) < 0) {
        customPrintf("[ERROR]: No se pudo enviar la trama de desconexión.");
    }

    //Tancar connexió
    if (gothamSocket >= 0) {
        close(gothamSocket);
        gothamSocket = -1;
    }
}

// Función para manejar señales como SIGINT
void signalHandler(int sig) {
    static int already_exiting = 0; // Variable estática para evitar múltiples señales

    if (sig == SIGINT) {
        if (already_exiting) {
            return; // Si ya se está ejecutando, ignorar la segunda señal
        }
        already_exiting = 1;

        if (lastFleckSocket >= 0) {
            shutdown(lastFleckSocket, SHUT_RDWR); // Cierra lectura y escritura
            close(lastFleckSocket);
            lastFleckSocket = -1;
        }
        
        stop = 1;

        sendDisconnectFrameToGotham(ENIGMA_TYPE);

        if (gothamSocket > 0) {
            close(gothamSocket);
            gothamSocket = -1;
        }

        if (tempFileDescriptor >= 0) {
            close(tempFileDescriptor);
            tempFileDescriptor = -1;
        }

        if (globalenigmaConfig) {
            free(globalenigmaConfig->workerType);
            free(globalenigmaConfig->ipFleck);
            free(globalenigmaConfig->ipGotham);
            free(globalenigmaConfig);
            globalenigmaConfig = NULL;
        }

        // Cerrar todos los descriptores de archivo abiertos
        //int maxFD = getdtablesize(); 
        //for (int fd = 3; fd < maxFD; fd++) {
           //close(fd);
        //}
        
         //Alliberar memoria compartida
         if (harleySharedMemory.shmaddr != (void*) -1) {
            shmdt(harleySharedMemory.shmaddr); //Desconecta el segmento de memoria compartida ubicado en la dirección especificada del espacio de direcciones del proceso que realiza la llamada
            shmctl(harleySharedMemory.shmid, IPC_RMID, NULL); //Destruye realmente la zona de memoria compartida
        }

        if (harleySharedMemory.sem.shmid >= 0) {
            SEM_destructor(&(harleySharedMemory.sem));
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
    while (!stop && leerTrama(gothamSocket, &frame) == 0)
    {
        processReceivedFrame(gothamSocket, &frame);
    }

    customPrintf("[ERROR]: Gotham se ha desconectado.");
    close(gothamSocket);
    pthread_exit(NULL);
}

void *handleFleckFrames(void *arg){
    int clientSocket = *(int *)arg;
    free(arg);

    size_t expectedFileSize = 0;   // Tamaño esperado del archivo (de la trama 0x03)
    size_t currentFileSize = 0;    // Tamaño recibido hasta el momento

    //Buscar si hi ha distorsions pending
    EnigmaDistortionEntry recoveredDistortion;
    EnigmaDistortionState *state = (EnigmaDistortionState *) harleySharedMemory.shmaddr;
    int found = 0;

    for (int i = 0; i < state->count; i++) {
        if (state->distortions[i].status == STATUS_PENDING) {
            recoveredDistortion = state->distortions[i];
            found = 1;
            break;
        }
    }

    if (found) {
        expectedFileSize = recoveredDistortion.currentByte; // Tamaño que ya se ha recibido
        strncpy(receivedFileName, recoveredDistortion.fileName, sizeof(receivedFileName) - 1);
        strncpy(expectedMD5, recoveredDistortion.md5Sum, sizeof(expectedMD5) - 1);
        snprintf(receivedFactor, sizeof(receivedFactor), "%d", recoveredDistortion.factor);

        char *finalFilePath;
        if (asprintf(&finalFilePath, "%s%s", ENIGMA_PATH_FILES, receivedFileName) == -1) {
            customPrintf("[ERROR]: No se pudo asignar memoria para el filePath.");
            return NULL;
        }

        tempFileDescriptor = open(finalFilePath, O_WRONLY | O_CREAT, 0666);

        if (tempFileDescriptor < 0) {
            customPrintf("[ERROR]: No se pudo abrir el archivo para continuar la recepción.");
            free(finalFilePath);
            return NULL;
        }
        customPrintf("%d\n\n", expectedFileSize);
        // Posicionarse exactamente donde se quedó la recepción
        if (lseek(tempFileDescriptor, expectedFileSize, SEEK_SET) < 0) {
            customPrintf("[ERROR]: No se pudo posicionar en el offset de continuación.");
            close(tempFileDescriptor);
            tempFileDescriptor = -1;
            free(finalFilePath);
            return NULL;
        }
    }

    while (1) {
        uint8_t type; //Llegir només primer byte
        ssize_t bytesRead = recv(clientSocket, &type, 1, MSG_PEEK);

        if (bytesRead <= 0) {
            customPrintf("[ERROR]: Error al leer el socket de Fleck. Posible desconexión.\n");
            close(clientSocket);
            return NULL;
        }
        
        //TODO MIRAR QUE ENTRI BÉ AQUÍ
        // Procesar trama binaria 0x05
        if (type == 0x05) {
            BinaryFrame binaryFrame;
            if (leerTramaBinaria(clientSocket, &binaryFrame) == 0) {
                processBinaryFrameFromFleck(&binaryFrame, expectedFileSize, &currentFileSize, clientSocket);
            } else {
                customPrintf("[ERROR]: Error al recibir trama binaria.");
                break;
            }
        } else { // Procesar tramas no binarias
            Frame request;
            if (leerTrama(clientSocket, &request) == 0) {
                // Procesar trama 0x03
                if (request.type == 0x03) {
                    char userName[64], fileName[256], fileSizeStr[20], md5Sum[33], factor[20];
                    if (sscanf(request.data, "%63[^&]&%255[^&]&%19[^&]&%32[^&]&%19s",
                            userName, fileName, fileSizeStr, md5Sum, factor) != 5) {
                        customPrintf("[ERROR]: Formato inválido en solicitud DISTORT FILE.");
                        send_frame_with_error(clientSocket, "CON_KO");
                        break;
                    }

                    // Mostrar mensajes personalizados
                    char *logMessage = NULL;
                    asprintf(&logMessage, "\nNew user connected: %s.\n\n", userName);
                    customPrintf(logMessage);
                    free(logMessage);

                    asprintf(&logMessage, "New request - %s wants to distort some media, with factor %s.", userName, factor);
                    customPrintf(logMessage);
                    free(logMessage);

                    customPrintf("\nReceiving original text…\n");

                    //Guardar variables
                    strncpy(receivedFileName, fileName, sizeof(receivedFileName) - 1);
                    strncpy(expectedMD5, md5Sum, sizeof(expectedMD5) - 1);
                    strncpy(receivedFactor, factor, sizeof(receivedFactor) - 1);

                    // Validar tamaño del archivo
                    expectedFileSize = strtoull(fileSizeStr, NULL, 10);
                    
                    if (expectedFileSize == 0) {
                        customPrintf("[ERROR]: Tamaño del archivo inválido.");
                        send_frame_with_error(clientSocket, "CON_KO");
                        break;
                    }
                    char *finalFilePath;
                    if (asprintf(&finalFilePath, "%s%s", ENIGMA_PATH_FILES, receivedFileName) == -1) {
                        customPrintf("[ERROR]: No se pudo asignar memoria para el filePath.");
                        break;
                    }

                    tempFileDescriptor = open(finalFilePath, O_WRONLY | O_CREAT | O_APPEND, 0666);

                    if (tempFileDescriptor < 0) {
                        customPrintf("[ERROR]: No se pudo abrir el archivo para continuar la recepción.");
                        free(finalFilePath);
                        break;
                    }

                    save_enigma_distortion_state(&harleySharedMemory, receivedFileName, 0, atoi(receivedFactor), expectedMD5, clientSocket, STATUS_PENDING);

                    send_frame_with_ok(clientSocket);
                }
                else if (request.type == 0x06) {
                    // Procesar respuesta MD5 recibida desde Fleck
                    if (strcmp(request.data, "CHECK_OK") == 0) {
                        customPrintf("[INFO]: Fleck ha confirmado correctamente el MD5 del archivo comprimido (CHECK_OK).");
                        remove_completed_distortions(&harleySharedMemory); // Limpieza tras éxito
                    } else if (strcmp(request.data, "CHECK_KO") == 0) {
                        customPrintf("[ERROR]: Fleck ha reportado un error en la comprobación MD5 del archivo comprimido (CHECK_KO).");
                        // Opcionalmente, gestionar retransmisión o error aquí.
                    } else {
                        logWarning("[WARNING]: Fleck ha enviado una respuesta MD5 desconocida.");
                    }
                }
            }  else {
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
    size_t offset = sendArgs->offset;

    // Duplicar filePath antes de liberar sendArgs
    char *filePath = strdup(sendArgs->filePath);
    if (!filePath) {
        customPrintf("[ERROR]: No se pudo duplicar filePath.");
        free(sendArgs);
        return NULL;
    }

    free(sendArgs); // Liberar memoria de los argumentos

    if (access(filePath, F_OK) != 0) {
        customPrintf("[ERROR]: El archivo comprimido no existe. Verifica el proceso de compresión.");
        free(filePath);
        return NULL;
    }

    customPrintf("\n[INFO]: El archivo comprimido se creó correctamente: %s\n", filePath);

    int fd = open(filePath, O_RDONLY, 0666);
    if (fd < 0) {
        customPrintf("[ERROR]: No se pudo abrir el archivo comprimido.");
        free(filePath);
        return NULL;
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize <= 0) {
        customPrintf("[ERROR]: El archivo comprimido está vacío o no se pudo calcular su tamaño.");
        close(fd);
        free(filePath);
        return NULL;
    }

    customPrintf("On: %d", offset);

    //Saltar al punto donde lo dejó el anterior Harley
    if (lseek(fd, offset, SEEK_SET) < 0) {
        customPrintf("[ERROR]: No se pudo posicionar en el archivo comprimido.");
        close(fd);
        free(filePath);
        return NULL;
    }

    BinaryFrame frame = {0};

    char buffer[247]; 
    ssize_t bytesRead;

    int bytesAcum = 0;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        customPrintf("\nbytesAcum: %d\n", bytesAcum);
        frame.type = 0x05;
        frame.data_length = bytesRead;
        memcpy(frame.data, buffer, bytesRead);
        frame.timestamp = (uint32_t)time(NULL);
        frame.checksum = calculate_checksum_binary(frame.data, frame.data_length, 1);

        if (escribirTramaBinaria(clientSocket, &frame) < 0) { //AQUÍ ENVIO A FLECK
            customPrintf("[ERROR]: Fallo al enviar trama 0x05.");
            close(fd);
            free(filePath);
            return NULL;
        }

        bytesAcum += bytesRead;
        save_enigma_distortion_state(&harleySharedMemory, receivedFileName, bytesAcum, atoi(receivedFactor),
                                    expectedMD5, clientSocket, STATUS_DONE);

        usleep(3000);
    }

    if (bytesRead < 0) {
        customPrintf("[ERROR]: Fallo al leer el archivo comprimido.");
    } else {
        customPrintf("[INFO]: Envío del archivo comprimido completado.");
    }

    close(fd);
    free(filePath); // Liberar filePath al final
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

    customPrintf("[INFO]: Enviando trama de error al cliente.");
    if (escribirTrama(clientSocket, &errorFrame) < 0)
    {
        customPrintf("[ERROR]: Error al enviar trama de error.");
    }
}

void send_frame_with_ok(int clientSocket){
    if (clientSocket < 0) {
        customPrintf("[ERROR][Harley] ❌ El socket de Fleck no es válido.");
        close(clientSocket);
        return;
    }

    Frame okFrame = {0};
    okFrame.type = 0x03;
    okFrame.data_length = 0;
    okFrame.timestamp = (uint32_t)time(NULL);
    okFrame.checksum = calculate_checksum(okFrame.data, okFrame.data_length, 0);

    // Enviar la trama
    int bytesSent = escribirTrama(clientSocket, &okFrame);
    if (bytesSent < 0) {
        perror("[ERROR][Harley] ❌ Error en send() al enviar la trama de confirmación\n");
    }
}

void processBinaryFrameFromFleck(BinaryFrame *binaryFrame, size_t expectedFileSize, size_t *currentFileSize, int clientSocket) {
    if (!binaryFrame) {
        customPrintf("[ERROR]: BinaryFrame recibido nulo.");
        return;
    }

    if (tempFileDescriptor < 0) {
        customPrintf("[ERROR]: No hay un archivo temporal abierto para escribir.");
        return;
    }

    static int distortionLogged = 0;
    if (!distortionLogged) {
        customPrintf("Distorting...\n");
        distortionLogged = 1;
    }

    // Escribir los datos en el archivo temporal
    ssize_t writtenBytes = write(tempFileDescriptor, binaryFrame->data, binaryFrame->data_length);
    if (writtenBytes == 0) {
        customPrintf("[ERROR]: Error al escribir en el archivo temporal.");
        close(tempFileDescriptor);
        tempFileDescriptor = -1;
        return;
    }

    lseek(tempFileDescriptor, 0, SEEK_SET);
    *currentFileSize = lseek(tempFileDescriptor, 0, SEEK_END);
    save_enigma_distortion_state(&harleySharedMemory, receivedFileName, *currentFileSize, atoi(receivedFactor), expectedMD5, clientSocket, STATUS_PENDING);

    // Verificar si se recibió el archivo completo
    if (*currentFileSize == expectedFileSize) {
        distortionLogged = 0;
        save_enigma_distortion_state(&harleySharedMemory, receivedFileName, *currentFileSize, atoi(receivedFactor), expectedMD5, clientSocket, STATUS_IN_PROGRESS);
        customPrintf("\nARCHIVO COMPLETO RECIBIDO DE FLECK\n");

        // Cerrar el archivo temporal
        close(tempFileDescriptor);
        tempFileDescriptor = -1;

        char *finalFilePath;
        if (asprintf(&finalFilePath, "%s%s", ENIGMA_PATH_FILES, receivedFileName) == -1) {
            customPrintf("[ERROR]: No se pudo asignar memoria para el filePath.");
            return;
        }

        char calculatedMD5[33] = {0};
        calculate_md5(finalFilePath, calculatedMD5);

        if (strcmp(calculatedMD5, "ERROR") == 0) {
            customPrintf("[ERROR]: No se pudo calcular el MD5 del archivo recibido.");
            send_frame_with_error(clientSocket, "CHECK_KO");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
            free(finalFilePath);
            return;
        }

        if (strcmp(expectedMD5, calculatedMD5) == 0) {
            sendMD5Response(clientSocket, "CHECK_OK");
            
            save_enigma_distortion_state(&harleySharedMemory, receivedFileName, *currentFileSize, atoi(receivedFactor), expectedMD5, clientSocket, STATUS_IN_PROGRESS);
            
            // Procesar la compresión
            int result = compress_text_file(finalFilePath, atoi(receivedFactor));
            if (result != 0) {
                customPrintf("[ERROR]: Fallo en la compresión del archivo.");
                free(finalFilePath);
                return;
            }

            // Crear la ruta del archivo comprimido dinámicamente
            char *compressedFilePath = finalFilePath;

            // Validar que el archivo comprimido existe
            if (access(compressedFilePath, F_OK) != 0) {
                customPrintf("[ERROR]: El archivo comprimido no se creó correctamente.");
                unlink(finalFilePath);
                free(finalFilePath);
                return;
            }

            // Calcular el MD5 del archivo comprimido
            char compressedMD5[33] = {0};
            calculate_md5(compressedFilePath, compressedMD5);

            if (strcmp(compressedMD5, "ERROR") == 0) {
                customPrintf("[ERROR]: No se pudo calcular el MD5 del archivo comprimido.");
                unlink(compressedFilePath); // Eliminar el archivo comprimido
                free(finalFilePath);
                return;
            }

            // Calcular el tamaño del archivo comprimido
            int fd_text_compressed = open(compressedFilePath, O_RDONLY);
            if (fd_text_compressed < 0) {
                customPrintf("[ERROR]: No se pudo abrir el archivo especificado.");
                free(finalFilePath);
                return;
            }

            off_t fileSizeCompressed = lseek(fd_text_compressed, 0, SEEK_END);
            if (fileSizeCompressed < 0) {
                customPrintf("[ERROR]: No se pudo calcular el tamaño del archivo.");
                close(fd_text_compressed);
                free(finalFilePath);
                return;
            }
            close(fd_text_compressed);

            // Convertir tamaño del archivo a string
            char *fileSizeStrCompressed = NULL;
            if (asprintf(&fileSizeStrCompressed, "%ld", fileSizeCompressed) == -1) {
                customPrintf("[ERROR]: No se pudo asignar memoria para el tamaño del archivo.");
                free(finalFilePath);
                return;
            }

            // 🛠 Guardar estado como `STATUS_DONE` porque la compresión ha finalizado y está listo para enviarse
            save_enigma_distortion_state(&harleySharedMemory, receivedFileName, *currentFileSize, atoi(receivedFactor), compressedMD5, clientSocket, STATUS_DONE);

            // Enviar la trama del archivo distorsionado
            enviaTramaArxiuDistorsionat(clientSocket, fileSizeStrCompressed, compressedMD5, compressedFilePath, 0);
            remove_completed_distortions(&harleySharedMemory);            
            
            // Liberar memoria dinámica asignada
            free(finalFilePath);
            free(fileSizeStrCompressed);
        } else {
            customPrintf("[ERROR]: El MD5 no coincide. Archivo recibido está corrupto.");
            sendMD5Response(clientSocket, "CHECK_KO");
            unlink(finalFilePath); // Eliminar el archivo en caso de error
            free(finalFilePath);
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

    if (clientSocket < 0) {
        customPrintf("[ERROR]: El socket de Fleck ya está cerrado. No se puede enviar MD5.");
        return;
    }    

    escribirTrama(clientSocket, &response);
}

// Función para enviar la trama del archivo distorsionado
void enviaTramaArxiuDistorsionat(int clientSocket, const char *fileSizeCompressed, const char *compressedMD5, const char *compressedFilePath, size_t offset) {
    Frame frame = {0};
    frame.type = 0x04;
    snprintf(frame.data, sizeof(frame.data), "%s&%s", fileSizeCompressed, compressedMD5);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (escribirTrama(clientSocket, &frame) < 0) {
        customPrintf("[ERROR]: Fallo al enviar la trama del archivo distorsionado.");
    }
    
    customPrintf("[SUCCESS]: Trama del archivo distorsionado 0x04 enviada correctamente.\n");

    // Preparar para enviar el archivo comprimido en tramas binarias
    SendCompressedFileArgs *args = malloc(sizeof(SendCompressedFileArgs));
    if (!args) {
        customPrintf("[ERROR]: No se pudo asignar memoria para los argumentos del hilo de envío.");
        return;
    }

    // Asignar los argumentos necesarios
    args->clientSocket = clientSocket;
    args->filePath = strdup(compressedFilePath);  // Duplicar la ruta
    args->fileSize = strtoull(fileSizeCompressed, NULL, 10);
    args->offset = offset;  

    customPrintf("md5 calculat comprimit: %s\n", compressedMD5);

    // Crear un hilo para enviar el archivo comprimido
    pthread_t sendThread;
    if (pthread_create(&sendThread, NULL, sendCompressedFileToFleck, args) != 0) {
        customPrintf("[ERROR]: No se pudo crear el hilo para enviar el archivo comprimido.");
        free(args->filePath);
        free(args);
        return;
    }

    pthread_detach(sendThread);  // Liberar el hilo automáticamente al finalizar
}

void *resume_distortion(void *arg) {
    ResumeArgs *args = (ResumeArgs *)arg;
    customPrintf("\n[RECOVERY] 🔄 Reanudando distorsión para %s desde byte %ld. Estado previo: %d\n", args->fileName, args->offset, args->status);

    // Construir la ruta completa del archivo
    char *filePath;
    if (asprintf(&filePath, "%s%s", ENIGMA_PATH_FILES, args->fileName) == -1) {
        customPrintf("[ERROR]: No se pudo asignar memoria para filePath.");
        free(args);
        return NULL;
    }    

    // Verificar si el archivo aún existe
    if (access(filePath, F_OK) != 0) {
        customPrintf("[ERROR]: No se encontró el archivo para continuar la distorsión: %s\n", filePath);
        free(filePath);
        free(args);
        return NULL;
    }

    if (args->status == STATUS_PENDING) {
        customPrintf("[RECOVERY] Archivo estaba PENDING, continúo recepción de Fleck...\n");
        save_enigma_distortion_state(&harleySharedMemory, args->fileName, args->offset,
                                     args->factor, args->md5Sum, args->clientSocket, STATUS_PENDING);
        free(filePath);
        free(args);
        return NULL;
    }

    //Cas 2: Estava en STATUS_IN_PROGRESS (mentre comprimeix, iniciar compressió de nou)
    if (args->status == STATUS_IN_PROGRESS) {
        customPrintf("[RECOVERY] 🔄 Estaba en IN_PROGRESS. Reiniciando compresión...\n");

        // 📌 Si el archivo comprimido ya existe, eliminarlo para empezar desde cero
        if (access(filePath, F_OK) != 0) {
            customPrintf("[ERROR]: No se encontró el archivo original para comprimir: %s", filePath);
            free(filePath);
            free(args);
            return NULL;
        }
        
        if (access(filePath, W_OK) != 0) {
            customPrintf("[ERROR]: No hay permisos de escritura en el archivo: %s", filePath);
            free(filePath);
            free(args);
            return NULL;
        }        
    
        // Reiniciar la compresión desde el archivo original
        int result = compress_text_file(filePath, args->factor);
        if (result != 0) {
            customPrintf("[ERROR]: Fallo en la compresión al reanudar distorsión.");
            free(filePath);
            free(args);
            return NULL;
        }
        
        customPrintf("[SUCCESS] ✅ Compresión completada correctamente.\n");
        
        // Guardar el estado como STATUS_DONE para reanudar el envío del archivo
        save_enigma_distortion_state(&harleySharedMemory, args->fileName, 0, args->factor,
            args->md5Sum, args->clientSocket, STATUS_DONE);
        args->status = STATUS_DONE;
        args->offset = 0;
    }

    //Cas 3: Estava en STATUS_DONE (harley cau mentre envia a fleck l'arxiu)
    if (args->status == STATUS_DONE) {
        customPrintf("[RECOVERY] 📤 Estaba en DONE.\n");
    
        if (access(filePath, F_OK) != 0) {
            customPrintf("[ERROR]: No se encontró el archivo comprimido.");
            free(filePath);
            free(args);
            return NULL;
        }
    
        int fd_compressed = open(filePath, O_RDONLY);
        if (fd_compressed < 0) {
            customPrintf("[ERROR]: No se pudo abrir el archivo comprimido.");
            free(filePath);
            free(args);
            return NULL;
        }
        
        off_t fileSizeCompressed = lseek(fd_compressed, 0, SEEK_END);
        close(fd_compressed);

        //Saltar hasta el offset donde lo dejó el Harley anterior
        lseek(fd_compressed, args->offset, SEEK_SET);
    
        char fileSizeStrCompressed[20];
        snprintf(fileSizeStrCompressed, sizeof(fileSizeStrCompressed), "%ld", fileSizeCompressed);

        // Calcular el MD5 del archivo comprimido
        char compressedMD5[33] = {0};
        calculate_md5(filePath, compressedMD5);

        if (strcmp(compressedMD5, "ERROR") == 0) {
            customPrintf("[ERROR]: No se pudo calcular el MD5 del archivo comprimido.");
            unlink(filePath); // Eliminar el archivo comprimido
            free(filePath);
            free(args);
            return NULL;
        }

        enviaTramaArxiuDistorsionat(args->clientSocket, fileSizeStrCompressed, compressedMD5, filePath, args->offset);

        customPrintf("[SUCCESS] ✅ Envío del archivo comprimido reanudado correctamente desde byte %ld.", args->offset);
    }
    
    free(filePath);
    free(arg);
    return NULL;
}

// Procesa un frame recibido de Gotham
void processReceivedFrame(int gothamSocket, const Frame *response){
    if (!response)
        return;

    // Manejo de comandos
    switch (response->type)
    {
        case 0x08:        
            // Recuperar todas las distorsiones en curso desde la memoria compartida, fer-la global per accedir al accept
            EnigmaDistortionEntry recoveredDistortions[MAX_DISTORTIONS];
            int distortionCount = 0;
        
            if (load_enigma_distortion_state(&harleySharedMemory, recoveredDistortions, &distortionCount) == 0) {
                customPrintf("Se encontraron %d distorsiones en memoria compartida.\n", distortionCount);
            } else {
                customPrintf("No hay distorsiones pendientes.");
            }
            
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

    case 0x12: //CASO HEARTBEAT
        pthread_mutex_lock(&heartbeatMutex); // Proteger acceso a la variable compartida
        lastHeartbeat = time(NULL);          // Actualizar el tiempo del último HEARTBEAT recibido
        pthread_mutex_unlock(&heartbeatMutex);
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

int main(int argc, char *argv[]){
    if (argc != 2)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Ús correcte: ./harley <fitxer de configuració>");
        return 1;
    }

    signal(SIGINT, signalHandler);

    // Carga de configuración
    EnigmaConfig *enigmaConfig = malloc(sizeof(EnigmaConfig));
    if (!enigmaConfig)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error asignando memoria para la configuración.");
        return 1;
    }

    globalenigmaConfig = enigmaConfig;

    readConfigFileGeneric(argv[1], enigmaConfig, CONFIG_ENIGMA);

    if (!enigmaConfig->workerType || !enigmaConfig->ipFleck || enigmaConfig->portFleck <= 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Configuración de Harley incorrecta o incompleta.");
        free(enigmaConfig);
        return 1;
    }

    if (init_shared_memory(&harleySharedMemory, 1234, sizeof(EnigmaDistortionState)) < 0) {
        customPrintf("[ERROR] ❌ No se pudo inicializar la memoria compartida.\n");
        return 1;
    } else {
        customPrintf("[DEBUG] ✅ Memoria compartida inicializada correctamente.\n");
    }

    // Conexión a Gotham para registro
    gothamSocket = connect_to_server(enigmaConfig->ipGotham, enigmaConfig->portGotham);
    if (gothamSocket < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo conectar a Gotham.");
        free(enigmaConfig);
        return 1;
    }

    customPrintf("\n\nConnecting Enigma worker to the system..\n");

    // Registro en Gotham
    Frame frame = {.type = 0x02, .timestamp = time(NULL)};
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d",
        enigmaConfig->workerType, enigmaConfig->ipFleck, enigmaConfig->portFleck);
    frame.data_length = strlen(frame.data);
    frame.checksum = calculate_checksum(frame.data, frame.data_length, 1);

    if (escribirTrama(gothamSocket, &frame) < 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviando el registro a Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    // Esperar respuesta del registro
    Frame response;
    if (leerTrama(gothamSocket, &response) != 0)
    {
        printColor(ANSI_COLOR_RED, "[ERROR]: No se recibió respuesta de Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    if (response.type == 0x02 && response.data_length == 0){
        customPrintf("Connected to Mr. J System, ready to listen to Fleck petitions\n\n");
        customPrintf("Waiting for connections...\n");
    }
    else if (response.type == 0x02 && strcmp(response.data, "CON_KO") == 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Registro rechazado por Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }
    else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Respuesta inesperada durante el registro.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }

    // Crear hilo para manejar mensajes de Gotham
    pthread_t gothamThread;
    if (pthread_create(&gothamThread, NULL, handleGothamFrames, &gothamSocket) != 0){
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo crear el hilo para Gotham.");
        close(gothamSocket);
        free(enigmaConfig);
        return 1;
    }
    pthread_detach(gothamThread);

    int fleckSocket = startServer(enigmaConfig->ipFleck, enigmaConfig->portFleck);
    if (fleckSocket < 0){
        printColor(ANSI_COLOR_RED, "[ERROR]: No se pudo iniciar el servidor local de Harley.");
        free(enigmaConfig);
        return 1;
    }

    // Bucle para manejar conexiones de Fleck
    while (1) {
        int clientSocket = accept_connection(fleckSocket);
        lastFleckSocket = clientSocket;

        if (clientSocket < 0) {
            customPrintf("[ERROR]: No se pudo aceptar la conexión del Fleck.");
            continue;
        }

        EnigmaDistortionEntry recoveredDistortions[MAX_DISTORTIONS];
        int distortionCount = 0;

        if (load_enigma_distortion_state(&harleySharedMemory, recoveredDistortions, &distortionCount) == 0) {
            for (int i = 0; i < distortionCount; i++) {
                if (recoveredDistortions[i].fleckSocketFD == clientSocket) {    
                    ResumeArgs *args = malloc(sizeof(ResumeArgs));
                    strncpy(args->fileName, recoveredDistortions[i].fileName, sizeof(args->fileName));
                    strncpy(args->md5Sum, recoveredDistortions[i].md5Sum, sizeof(args->md5Sum));
                    args->offset = recoveredDistortions[i].currentByte;
                    args->factor = recoveredDistortions[i].factor;
                    args->status = recoveredDistortions[i].status;
                    args->clientSocket = clientSocket;

                    pthread_t resumeThread;
                    pthread_create(&resumeThread, NULL, resume_distortion, args);
                    pthread_detach(resumeThread);
                    continue;  
                } else {
                    customPrintf("[ERROR] Un `Fleck` diferente intentó continuar la distorsión de %s.\n",
                                 recoveredDistortions[i].fileName);
                }
            }
        }

        if (clientSocket >= 0) {
            pthread_t fleckThread;
            int *socketArg = malloc(sizeof(int));
            if (!socketArg) {
                customPrintf("[ERROR]: No se pudo asignar memoria para el socket del cliente.");
                close(clientSocket); // Cierra el socket si hay un error
                continue;
            }
            *socketArg = clientSocket;

            if (pthread_create(&fleckThread, NULL, handleFleckFrames, socketArg) != 0) {
                customPrintf("[ERROR]: No se pudo crear el hilo para manejar el cliente.");
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
    free(enigmaConfig);
    return 0;
}