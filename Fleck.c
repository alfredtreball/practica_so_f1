#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>

#include "FileReader.h"
#include "StringUtils.h"
#include "DataConversion.h"
#include "Networking.h"

// Definició de l'estructura FleckConfig
typedef struct {
    char *user;
    char *directory;
    char *ipGotham;
    int portGotham;
} FleckConfig;

FleckConfig *globalFleckConfig = NULL;

// Funció per llistar els fitxers de text (.txt) en el directori especificat
/***********************************************
* @Finalitat: Llista els fitxers amb l'extensió .txt en el directori especificat 
*             utilitzant el comandament find del sistema.
* @Paràmetres:
*   in: directory = el directori on buscar els fitxers.
* @Retorn: ----
************************************************/
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

        // Executa el comandament find per buscar fitxers de text
        char *args[] = {"/usr/bin/find", (char *)directory, "-type", "f", "-name", "*.txt", "-exec", "basename", "{}", ";", NULL};
        execv(args[0], args);

        // Si execv falla
        printF("Error executant find\n");
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
        // Llegeix línies del fitxer temporal
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            count++;
            free(line);
        }

        // Mostra el nombre de fitxers trobats
        char *countStr = intToStr(count);
        printF("There are ");
        printF(countStr);
        printF(" text files available:\n");
        free(countStr);

        lseek(tempFd, 0, SEEK_SET); // Torna al començament del fitxer temporal

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
/***********************************************
* @Finalitat: Llista els fitxers amb extensions .wav, .jpg, o .png en el directori especificat 
*             utilitzant un script de bash per a executar el comandament find.
* @Paràmetres:
*   in: directory = el directori on buscar els fitxers.
* @Retorn: ----
************************************************/


void listMedia(const char *directory) {
    pid_t pid = fork();

    if (pid == 0) { // Procés fill
        int tempFd = open("media_files.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            exit(1);
        }
        dup2(tempFd, STDOUT_FILENO); // Redirigeix la sortida estàndard al fitxer temporal
        close(tempFd);

        // Executa el comandament find per buscar fitxers de mitjans
        char *args[] = {
            "/bin/bash", "-c", // Executa bash amb l'opció -c per passar un script com a cadena
            "find \"$1\" -type f \\( -name '*.wav' -o -name '*.jpg' -o -name '*.png' \\) -exec basename {} \\;", // Comandament find per buscar fitxers amb extensions especificades
            "bash", (char *)directory, NULL // Arguments addicionals per bash
        };
        execv(args[0], args);

        // Si execv falla
        printF("Error executant find\n");
        exit(1);
    } else if (pid > 0) { // Procés pare
        wait(NULL);

        int tempFd = open("media_files.txt", O_RDONLY);
        if (tempFd == -1) {
            printF("Error obrint el fitxer temporal\n");
            return;
        }

        int count = 0;
        char *line;
        // Llegeix línies del fitxer temporal
        while ((line = readUntil(tempFd, '\n')) != NULL) {
            count++;
            free(line);
        }

        // Mostra el nombre de fitxers trobats
        char *countStr = intToStr(count);
        printF("There are ");
        printF(countStr);
        printF(" media files available:\n");
        free(countStr);

        lseek(tempFd, 0, SEEK_SET); // Torna al començament del fitxer temporal

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
    if (receive_frame(gothamSocket, buffer) < 0) {
        printF("Error rebent la resposta de Gotham\n");
        return;
    }

    printF("Resposta de Gotham: ");
    printF(buffer);
    printF("\n");
}

/*Funció per processar comandes que arriben*/
void processCommand(char *command, FleckConfig *fleckConfig, int gothamSocket) {
    char *cmd = strtok(command, " \n");  // Primer paraula
    char *subCmd = strtok(NULL, " \n");  // Segona paraula
    char *extra = strtok(NULL, " \n");   // Comprova per a paraules extra

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
        extra = strtok(NULL, " \n"); // Busquem més paraules

        if (file && factorStr && extra == NULL) {  // Assegura que només hi ha 2 paràmetres
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

// Funció per llegir el fitxer de configuració
void readConfigFile(const char *configFile, FleckConfig *fleckConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    fleckConfig->user = readUntil(fd, '\n');
    if (fleckConfig->user == NULL) {
        printF("Error llegint l'usuari\n");
        exit(1);
    }
    fleckConfig->directory = trim(readUntil(fd, '\n'));
    if (fleckConfig->directory == NULL) {
        printF("Error llegint el directori\n");
        exit(1);
    }
    fleckConfig->ipGotham = trim(readUntil(fd, '\n'));
    if (fleckConfig->ipGotham == NULL) {
        printF("Error llegint la IP de Gotham\n");
        exit(1);
    }

    char *portStr = readUntil(fd, '\n');
    if (portStr == NULL) {
        printF("Error llegint el port de Gotham\n");
        exit(1);
    }
    fleckConfig->portGotham = atoi(portStr);
    free(portStr);
    close(fd);

    printF("Configuració llegida:\n");
    printF("Usuari: ");
    printF(fleckConfig->user);
    printF("\nDirectori: ");
    printF(fleckConfig->directory);
    printF("\nIP Gotham: ");
    printF(fleckConfig->ipGotham);
    printF("\nPort Gotham: ");
    char *portGothamStr;
    asprintf(&portGothamStr, "%d\n", fleckConfig->portGotham);
    printF(portGothamStr);
    free(portGothamStr);
}

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
    FleckConfig *fleckConfig = (FleckConfig *)malloc(sizeof(FleckConfig));
    if (fleckConfig == NULL) {
        printF("Error assignant memòria per a la configuració\n");
        exit(1);
    }
    globalFleckConfig = fleckConfig;
    signal(SIGINT, signalHandler);

    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    readConfigFile(argv[1], fleckConfig);

    int gothamSocket = connect_to_server(fleckConfig->ipGotham, fleckConfig->portGotham);
    if (gothamSocket < 0) {
        printF("Error connectant a Gotham\n");
        exit(1);
    }

    char *command = NULL;
    while (1) {
        printF("\n$ ");
        command = readUntil(STDIN_FILENO, '\n');
        if (command == NULL) {
            printF("Error al llegir la línia\n");
            break;
        }

        processCommandWithGotham(command, gothamSocket);
        free(command);
    }

    close(gothamSocket);
    alliberarMemoria(fleckConfig);
    return 0;
}