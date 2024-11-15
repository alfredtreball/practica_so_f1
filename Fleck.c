#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>

#include "FileReader/FileReader.h"
#include "StringUtils/StringUtils.h"
#include "DataConversion/DataConversion.h"
#include "Networking/Networking.h"

FleckConfig *globalFleckConfig = NULL;

// Funció per llistar els fitxers de text (.txt) en el directori especificat
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

// Funció per llistar els fitxers de tipus mitjà (wav, jpg, png) en el directori especificat
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

// Funció per processar una comanda i enviar-la a Gotham
void processCommandWithGotham(const char *command, int gothamSocket) {
    if (send_frame(gothamSocket, command, strlen(command)) < 0) {
        printF("Error enviant la comanda a Gotham\n");
        return;
    }

    char buffer[FRAME_SIZE];
    int data_length;  // Asegurarse de definir el argumento extra

    if (receive_frame(gothamSocket, buffer, &data_length) < 0) {  // Pasar tres argumentos
        printF("Error rebent la resposta de Gotham\n");
        return;
    }

    printF("Resposta de Gotham: ");
    printF(buffer);
    printF("\n");
}

/*Funció per processar comandes que arriben*/
void processCommand(char *command, FleckConfig *fleckConfig, int gothamSocket) {
    if (command == NULL || strlen(command) == 0 || strcmp(command, "\n") == 0) {
        printF("ERROR: Comanda buida.\n");
        return;
    }

    char *cmd = strtok(command, " \n");
    if (cmd == NULL) {  
        printF("ERROR: Comanda no vàlida.\n");
        return;
    }
    
    char *subCmd = strtok(NULL, " \n");
    char *extra = strtok(NULL, " \n");

    if (strcasecmp(cmd, "CONNECT") == 0 && subCmd == NULL) {
        processCommandWithGotham("CONNECT", gothamSocket);
    } else if (strcasecmp(cmd, "LOGOUT") == 0 && subCmd == NULL) {
        processCommandWithGotham("LOGOUT", gothamSocket);
    } else if (strcasecmp(cmd, "LIST") == 0 && extra == NULL) {
        if (strcasecmp(subCmd, "MEDIA") == 0) {
            listMedia(fleckConfig->directory);
        } else if (strcasecmp(subCmd, "TEXT") == 0) {
            listText(fleckConfig->directory);
        } else {
            printF("Comanda KO\n");
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = subCmd;
        char *factorStr = extra;
        extra = strtok(NULL, " \n");

        if (file && factorStr && extra == NULL) {
            int factor = atoi(factorStr);
            char *distortCommand;
            asprintf(&distortCommand, "DISTORT %s %d", file, factor);
            processCommandWithGotham(distortCommand, gothamSocket);
            free(distortCommand);
        } else {
            printF("Comanda KO\n");
        }
    } else if (strcasecmp(cmd, "CHECK") == 0 && strcasecmp(subCmd, "STATUS") == 0 && extra == NULL) {
        processCommandWithGotham("CHECK STATUS", gothamSocket);
    } else if (strcasecmp(cmd, "CLEAR") == 0 && strcasecmp(subCmd, "ALL") == 0 && extra == NULL) {
        processCommandWithGotham("CLEAR ALL", gothamSocket);
    } else {
        printF("ERROR: Please input a valid command.\n");
    }
}

//FASE 1
void alliberarMemoria(FleckConfig *fleckConfig);

// Funció manejadora per a la senyal SIGINT
void signalHandler(int sig) {
    if (sig == SIGINT) {
        if (globalFleckConfig != NULL) {
            printF("\nAlliberació de memòria OK\n");
            alliberarMemoria(globalFleckConfig);
        }
    }
    exit(0);
}

void alliberarMemoria(FleckConfig *fleckConfig) {
    if (fleckConfig->user) free(fleckConfig->user);
    if (fleckConfig->directory) free(fleckConfig->directory);
    if (fleckConfig->ipGotham) free(fleckConfig->ipGotham);
    free(fleckConfig);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    FleckConfig *fleckConfig = malloc(sizeof(FleckConfig));
    if (!fleckConfig) {
        printF("Error assignant memòria per a la configuració\n");
        return 1;
    }
    globalFleckConfig = fleckConfig;
    signal(SIGINT, signalHandler);
    readConfigFileGeneric(argv[1], fleckConfig, CONFIG_FLECK);

    int gothamSocket = connect_to_server(fleckConfig->ipGotham, fleckConfig->portGotham);
    if (gothamSocket < 0) {
        printF("Error connectant a Gotham\n");
        exit(1);
    }

    char *command = NULL;
    while (1) {
        printF("\n$ ");
        command = readUntil(STDIN_FILENO, '\n');

        // Verificar si `command` es NULL o está vacío para manejar el "Enter" sin entrada
        if (command == NULL || strlen(command) == 0) {
            printF("Comanda buida. Si us plau, introdueix una comanda vàlida.\n");
            if (command != NULL) free(command);  // Liberar `command` si no es NULL
            continue;
        }

        processCommand(command, fleckConfig, gothamSocket);
        free(command);
    }

    close(gothamSocket);
    alliberarMemoria(fleckConfig);
    return 0;
}