#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h> // Necessari per a la funció strcasecmp

#include "Utils.h"

// Definició de l'estructura FleckConfig per emmagatzemar la configuració del sistema Fleck
typedef struct {
    char *user;       // Nom de l'usuari
    char *directory;  // Directori de configuració
    char *ipGotham;   // Adreça IP del servidor Gotham
    int portGotham;   // Port del servidor Gotham
} FleckConfig;

// Funció per llegir el fitxer de configuració i carregar la informació a la estructura FleckConfig
/***********************************************
* @Finalitat: Llegeix el fitxer de configuració especificat i emmagatzema la informació a la estructura FleckConfig.
* @Paràmetres:
*   in: configFile = nom del fitxer de configuració.
*   out: fleckConfig = estructura on s'emmagatzema la configuració llegida.
* @Retorn: ----
************************************************/
void readConfigFile(const char *configFile, FleckConfig *fleckConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna memòria per a cada camp de la configuració
    fleckConfig->user = readUntil(fd, '\n'); // Llegeix el nom de l'usuari
    removeChar(fleckConfig->user, '&'); // Elimina el caràcter '&' del nom de l'usuari
    fleckConfig->directory = trim(readUntil(fd, '\n')); // Elimina espais al voltant del directori
    fleckConfig->ipGotham = readUntil(fd, '\n'); // Llegeix la IP del servidor Gotham

    char *portStr = readUntil(fd, '\n'); // Llegeix el port com a cadena
    fleckConfig->portGotham = atoi(portStr); // Converteix el port a enter
    free(portStr); // Allibera la memòria de la cadena temporal
    close(fd); // Tanca el fitxer
    
    fleckConfig->user = trim(fleckConfig->user); // Crear un buffer temporal per emmagatzemar el missatge
    printF("\n");
    printF(fleckConfig->user);
    printF(" user initialized\n\n");
    printF("File read correctly:");
    printF("\nUser - ");
    printF(fleckConfig->user);
    printF("\nDirectory - ");
    printF(fleckConfig->directory);
    printF("\nIP - ");
    printF(fleckConfig->ipGotham);

    printF("\nPort - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d\n", fleckConfig->portGotham); // Converteix el port a cadena
    printF(portGothamStr);
    free(portGothamStr); // Allibera la memòria de la cadena temporal
}

// Funció per processar una comanda introduïda per l'usuari
/***********************************************
* @Finalitat: Processa la comanda especificada per l'usuari i executa les accions pertinents.
* @Paràmetres:
*   in: command = cadena de text amb la comanda a processar.
* @Retorn: ----
************************************************/
void processCommand(char *command) {
    char *cmd = strtok(command, " "); // Separa la primera paraula del comandament

    if (cmd == NULL) {
        printF("ERROR: Introduïu una comanda vàlida.\n"); // Comprova si la comanda està buida
        return;
    }

    // Comprovació de les comandes vàlides i execució
    if (strcasecmp(cmd, "CONNECT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        printF("Comanda OK\n");
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *subCmd = strtok(NULL, " "); // Llegeix la segona part de la comanda
        if (subCmd != NULL) {
            if (strcasecmp(subCmd, "MEDIA") == 0) {
                printF("Comanda OK\n");
            } else if (strcasecmp(subCmd, "TEXT") == 0) {
                printF("Comanda OK\n");
            } else {
                printF("Comanda KO\n"); // Comanda desconeguda
            }
        } else {
            printF("Comanda KO\n"); // Falta subcomanda
        }
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = strtok(NULL, " ");  // Nom del fitxer
        char *factorStr = strtok(NULL, " "); // Factor de distorsió
        char *extraParam = strtok(NULL, " "); // Intenta llegir un tercer paràmetre

        if (file != NULL && factorStr != NULL && extraParam == NULL) { // Accepta només 2 paràmetres
            int factor = atoi(factorStr);
            if (factor > 0) {
                printF("Comanda OK\n");
            } else {
                printF("Comanda KO\n"); // Factor no vàlid
            }
        } else {
            printF("Comanda KO\n"); // Nombre de paràmetres incorrecte
        }
    } else if (strcasecmp(cmd, "CHECK") == 0) {
        char *subCmd = strtok(NULL, " ");
        if (subCmd != NULL && strcasecmp(subCmd, "STATUS") == 0) {
            printF("Comanda OK\n");
        } else {
            printF("Comanda KO\n"); // Subcomanda incorrecta
        }
    } else if (strcasecmp(cmd, "CLEAR") == 0) {
        char *subCmd = strtok(NULL, " ");
        if (subCmd != NULL && strcasecmp(subCmd, "ALL") == 0) {
            printF("Comanda OK\n");
        } else {
            printF("Comanda KO\n"); // Subcomanda incorrecta
        }
    } else {
        printF("Comanda KO\n"); // Comanda desconeguda
    }
}

// Funció per alliberar la memòria dinàmica utilitzada per la configuració de Fleck
/***********************************************
* @Finalitat: Allibera la memòria dinàmica associada amb l'estructura FleckConfig.
* @Paràmetres:
*   in: fleckConfig = estructura FleckConfig a alliberar.
* @Retorn: ----
************************************************/
void alliberarMemoria(FleckConfig *fleckConfig){
    if (fleckConfig->user) {
        free(fleckConfig->user); // Allibera la memòria del nom d'usuari
    }
    if (fleckConfig->directory) {
        free(fleckConfig->directory); // Allibera la memòria del directori
    }
    if (fleckConfig->ipGotham) {
        free(fleckConfig->ipGotham); // Allibera la memòria de la IP Gotham
    }
    free(fleckConfig); // Finalment, allibera la memòria de l'estructura principal
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració de Fleck
    FleckConfig *fleckConfig = (FleckConfig *)malloc(sizeof(FleckConfig));
    // Lògica de línia de comandes amb memòria dinàmica
    char *command = NULL;
    
    if (argc != 2) {
        printF("Ús: ./fleck <fitxer de configuració>\n"); // Comprova que s'ha passat el fitxer de configuració com a argument
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix el fitxer de configuració passant l'estructura fleckConfig com a argument
    readConfigFile(argv[1], fleckConfig);
    
    while (1) {
        printF("\n$ "); // Mostra el prompt
        
        command = readUntil(STDIN_FILENO, '\n'); // Llegeix la línia de comanda
        if (command == NULL) {
            printF("Error al llegir la línia\n");
            break; // Finalitza el bucle si hi ha un error
        }

        processCommand(command); // Processa la comanda llegida
        free(command); // Allibera la memòria de la comanda després de cada ús
    }

    // Allibera la memòria dinàmica abans de finalitzar
    alliberarMemoria(fleckConfig);
    return 0; // Finalitza correctament el programa
}
