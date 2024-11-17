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

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536

int gothamSocket = -1; // Variable global para manejar el socket

void signalHandler(int sig);
void processCommandWithGotham(const char *command, int gothamSocket);
void listText(const char *directory);
void listMedia(const char *directory);

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

void serialize_frame(const Frame *frame, char *buffer) {
    memset(buffer, 0, FRAME_SIZE);
    snprintf(buffer, FRAME_SIZE, "%02x|%04x|%u|%04x|%s",
             frame->type, frame->data_length, frame->timestamp, frame->checksum, frame->data);
}

int deserialize_frame(const char *buffer, Frame *frame) {
    if (buffer == NULL || frame == NULL) {
        // Error: Invalid input parameters
        return -1;
    }

    memset(frame, 0, sizeof(Frame));

    char localBuffer[FRAME_SIZE];
    strncpy(localBuffer, buffer, FRAME_SIZE - 1);
    localBuffer[FRAME_SIZE - 1] = '\0'; // Ensure null termination

    char *token = NULL;

    // FIELD: TYPE
    token = strtok(localBuffer, "|");
    if (token != NULL && strlen(token) > 0) {
        frame->type = (uint8_t)strtoul(token, NULL, 16);
    } else {
        // Error: Missing or invalid TYPE field
        return -1;
    }

    // FIELD: DATA_LENGTH
    token = strtok(NULL, "|");
    if (token != NULL) {
        unsigned int data_length = strtoul(token, NULL, 16);
        if (data_length > sizeof(frame->data)) {
            // Error: DATA_LENGTH out of range
            return -1;
        }
        frame->data_length = (uint16_t)data_length;
    } else {
        // Error: Missing DATA_LENGTH field
        return -1;
    }

    // FIELD: TIMESTAMP
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->timestamp = (uint32_t)strtoul(token, NULL, 10);
    } else {
        // Error: Missing TIMESTAMP field
        return -1;
    }

    // FIELD: CHECKSUM
    token = strtok(NULL, "|");
    if (token != NULL) {
        frame->checksum = (uint16_t)strtoul(token, NULL, 16);
    } else {
        // Error: Missing CHECKSUM field
        return -1;
    }

    // FIELD: DATA
    token = strtok(NULL, "|");
    if (token != NULL) {
        size_t dataToCopy = frame->data_length < sizeof(frame->data) - 1
            ? frame->data_length
            : sizeof(frame->data) - 1;
        strncpy(frame->data, token, dataToCopy);
        frame->data[dataToCopy] = '\0'; // Ensure null termination
    } else {
        // Error: Missing DATA field
        return -1;
    }

    return 0; // Success
}

void processCommandWithGotham(const char *command, int gothamSocket) {
    Frame frame = {0};
    frame.type = 0x01; // Tipo de trama
    strncpy(frame.data, command, sizeof(frame.data) - 1);
    frame.data_length = strlen(frame.data);
    frame.timestamp = (uint32_t)time(NULL);
    frame.checksum = calculate_checksum(frame.data, frame.data_length);

    char buffer[FRAME_SIZE];
    serialize_frame(&frame, buffer);

    if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant la comanda a Gotham\n");
        return;
    }

    if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error rebent la resposta de Gotham\n");
        return;
    }

    Frame response;
    int result = deserialize_frame(buffer, &response);
    if (result != 0) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Error al deserialitzar la resposta de Gotham.\n");
        return;
    }

    // Validar checksum de la respuesta
    uint16_t resp_checksum = calculate_checksum(response.data, response.data_length);
    if (resp_checksum != response.checksum) {
        printColor(ANSI_COLOR_RED, "[ERROR]: Resposta de Gotham amb checksum invàlid\n");
        return;
    }

    if (response.type == 0x01 && strcmp(response.data, "CONN_OK") == 0) {
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Connexió establerta amb Gotham!\n");
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Connexió rebutjada: ");
        printColor(ANSI_COLOR_CYAN, response.data);
        printColor(ANSI_COLOR_RED, "\n");
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
        printColor(ANSI_COLOR_CYAN, "[INFO]: Connecting to Gotham...\n");
        processCommandWithGotham("CONNECT", gothamSocket);
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) {
        printColor(ANSI_COLOR_YELLOW, "[INFO]: Disconnecting from Gotham...\n");
        processCommandWithGotham("LOGOUT", gothamSocket);
        close(gothamSocket);
        printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Disconnected successfully.\n");
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = subCmd;
        char *factorStr = extra;

        if (file && factorStr) {
            Frame frame = {0};
            frame.type = 0x10;
            snprintf(frame.data, sizeof(frame.data), "%s&%s", file, factorStr);
            frame.data_length = strlen(frame.data);
            frame.timestamp = (uint32_t)time(NULL);
            frame.checksum = calculate_checksum(frame.data, frame.data_length);

            char buffer[FRAME_SIZE];
            serialize_frame(&frame, buffer);

            if (write(gothamSocket, buffer, FRAME_SIZE) < 0) {
                printColor(ANSI_COLOR_RED, "[ERROR]: Error enviant la comanda DISTORT a Gotham\n");
                return;
            }

            if (read(gothamSocket, buffer, FRAME_SIZE) <= 0) {
                printColor(ANSI_COLOR_RED, "[ERROR]: No hi ha resposta de Gotham.\n");
                return;
            }

            printColor(ANSI_COLOR_CYAN, "[DEBUG]: Buffer rebut de Gotham:");
            printColor(ANSI_COLOR_YELLOW, buffer);

            Frame response;
            if (deserialize_frame(buffer, &response) != 0) {
                printColor(ANSI_COLOR_RED, "[ERROR]: Error deserialitzant la resposta de Gotham.\n");
                return;
            }

           if (strcmp(response.data, "DISTORT_OK") == 0) {
                printColor(ANSI_COLOR_GREEN, "[SUCCESS]: Fitxer distorsionat correctament.\n");
            } else {
                printColor(ANSI_COLOR_RED, "[ERROR]: No s'ha pogut distorsionar el fitxer.\n");
            }
        } else {
            printColor(ANSI_COLOR_RED, "[ERROR]: La comanda DISTORT necessita un fitxer i un factor.\n");
        }
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
    } else if (strcasecmp(cmd, "CHECK") == 0 && strcasecmp(subCmd, "STATUS") == 0 && extra == NULL) {
        printColor(ANSI_COLOR_CYAN, "[INFO]: Checking Gotham status...\n");
        processCommandWithGotham("CHECK STATUS", gothamSocket);
    } else {
        printColor(ANSI_COLOR_RED, "[ERROR]: Unknown command. Please enter a valid command.\n");
    }
}

// FASE 1
void alliberarMemoria(FleckConfig *fleckConfig);

void signalHandler(int sig) {
    if (sig == SIGINT) {
        printColor(ANSI_COLOR_YELLOW, "\n[INFO]: Alliberació de memòria...\n");

        if (gothamSocket != -1) {
            Frame frame = {.type = 0x02, .data_length = 0, .timestamp = (uint32_t)time(NULL)};
            frame.checksum = calculate_checksum(frame.data, frame.data_length);

            char buffer[FRAME_SIZE];
            serialize_frame(&frame, buffer);
            write(gothamSocket, buffer, FRAME_SIZE);

            close(gothamSocket);
        }
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

    printF("\033[1;36m[INFO]: Intentant connectar a Gotham...\n\033[0m");
    gothamSocket = connect_to_server(fleckConfig->ipGotham, fleckConfig->portGotham);
    if (gothamSocket < 0) {
        printF("\033[1;31m[ERROR]: No es pot connectar a Gotham. Comprova la IP i el port.\n\033[0m");
        free(fleckConfig);
        return 1;
    }

    printF("\033[1;32m[SUCCESS]: Connectat correctament a Gotham!\n\033[0m");

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
