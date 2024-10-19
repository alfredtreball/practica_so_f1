#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "Utils.h" // Inclou les funcions utilitàries necessàries

// Definició de la estructura EnigmaConfig per emmagatzemar la configuració del sistema Enigma
typedef struct {
    char *ipGotham; // Adreça IP del servidor Gotham
    int portGotham; // Port del servidor Gotham
    char *ipFleck;  // Adreça IP del servidor Fleck
    int portFleck;  // Port del servidor Fleck
    char *directory; // Directori per a la configuració d'Enigma
    char *workerType; // Tipus de treballador per a la configuració d'Enigma
} EnigmaConfig;

// Funció per llegir el fitxer de configuració d'Enigma
/***********************************************
* @Finalitat: Llegeix el fitxer de configuració especificat i carrega la informació en una estructura EnigmaConfig.
* @Paràmetres: 
*   in: configFile = nom del fitxer de configuració.
*   out: enigmaConfig = estructura on s'emmagatzema la configuració llegida.
* @Retorn: ----
************************************************/
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura
    
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna la memòria per a cada camp de la configuració
    enigmaConfig->ipGotham = readUntil(fd, '\n'); // Llegeix la IP del servidor Gotham
    char* portGotham = readUntil(fd, '\n'); // Llegeix el port com a cadena de text
    enigmaConfig->portGotham = atoi(portGotham); // Converteix el port a enter
    free(portGotham); // Allibera la memòria de la cadena temporal

    enigmaConfig->ipFleck = readUntil(fd, '\n'); // Llegeix la IP del servidor Fleck
    char *portFleck = readUntil(fd, '\n'); // Llegeix el port com a cadena de text
    enigmaConfig->portFleck = atoi(portFleck); // Converteix el port a enter
    free(portFleck); // Allibera la memòria de la cadena temporal

    enigmaConfig->directory = readUntil(fd, '\n'); // Llegeix el directori de configuració
    enigmaConfig->workerType = readUntil(fd, '\n'); // Llegeix el tipus de treballador

    close(fd); // Tanca el fitxer

    // Mostra la configuració llegida per a verificació
    printF("Ip Gotham - ");
    printF(enigmaConfig->ipGotham);
    printF("\nPort Gotham - ");
    char* portGothamStr = NULL;
    asprintf(&portGothamStr, "%d", enigmaConfig->portGotham); // Converteix el port a cadena de text
    printF(portGothamStr);
    free(portGothamStr); // Allibera la memòria de la cadena temporal

    printF("\nIp Fleck - ");
    printF(enigmaConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", enigmaConfig->portFleck); // Converteix el port a cadena de text
    printF(portFleckStr);
    free(portFleckStr); // Allibera la memòria de la cadena temporal

    printF("\nDirectori Enigma - ");
    printF(enigmaConfig->directory);

    printF("\nTipus de Treballador - ");
    printF(enigmaConfig->workerType);
    printF("\n");
}

// Funció per alliberar la memòria dinàmica utilitzada per la configuració d'Enigma
/***********************************************
* @Finalitat: Allibera la memòria dinàmica associada amb l'estructura EnigmaConfig.
* @Paràmetres: 
*   in: enigmaConfig = estructura EnigmaConfig a alliberar.
* @Retorn: ----
************************************************/
void alliberarMemoria(EnigmaConfig *enigmaConfig) {
    if (enigmaConfig->ipGotham) {
        free(enigmaConfig->ipGotham); // Allibera la memòria de la IP Gotham
    }
    if (enigmaConfig->ipFleck) {
        free(enigmaConfig->ipFleck); // Allibera la memòria de la IP Fleck
    }
    if (enigmaConfig->directory) {
        free(enigmaConfig->directory); // Allibera la memòria del directori
    }
    if (enigmaConfig->workerType) {
        free(enigmaConfig->workerType); // Allibera la memòria del tipus de treballador
    }
    free(enigmaConfig); // Finalment, allibera la memòria de l'estructura principal
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració d'Enigma
    EnigmaConfig *enigmaConfig = (EnigmaConfig *)malloc(sizeof(EnigmaConfig));
    
    if (argc != 2) {
        printF("Ús: ./enigma <fitxer de configuració>\n"); // Comprova que s'ha passat el fitxer de configuració com a argument
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix la configuració passant l'estructura enigmaConfig com a argument
    readConfigFile(argv[1], enigmaConfig);

    // Allibera la memòria dinàmica per evitar fuites de memòria
    alliberarMemoria(enigmaConfig);

    return 0; // Finalitza correctament el programa
}
