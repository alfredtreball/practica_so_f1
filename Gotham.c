/***********************************************
* @Fitxer: Gotham.c
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer implementa la gestió de configuració per al sistema 
* Gotham. Inclou funcions per a llegir la configuració des d'un fitxer, 
* emmagatzemar la informació en una estructura de dades i alliberar la memòria 
* dinàmica associada. La configuració inclou informació de les connexions 
* amb els servidors Fleck i Harley Enigma.
************************************************/
#define _GNU_SOURCE // Necessari per a que 'asprintf' funcioni correctament

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>  // Para sockets (INET)
#include <sys/socket.h> // Para funciones de sockets
#include <netinet/in.h> // Para estructura sockaddr_in
#include <pthread.h> // Necesario para hilos

#include "FileReader.h"
#include "StringUtils.h"

// Definició de l'estructura GothamConfig per emmagatzemar la configuració del sistema Gotham
typedef struct {
    char *ipFleck;    // Adreça IP del servidor Fleck
    int portFleck;    // Port del servidor Fleck
    char *ipHarEni;   // Adreça IP del servidor Harley Enigma
    int portHarEni;   // Port del servidor Harley Enigma
} GothamConfig;

//FASE2

// Función para inicializar el servidor de Gotham
int iniciarServidor(char *ip, int puerto) {
    int server_fd;
    struct sockaddr_in direccion;

    // Crear el socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Error al crear el socket");
        exit(1);
    }

    // Configurar la dirección y el puerto del servidor
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);

    // Convertir la IP del archivo de configuración en formato binario
    if (inet_pton(AF_INET, ip, &direccion.sin_addr) <= 0) {
        perror("Error en inet_pton (IP no válida)");
        close(server_fd);
        exit(1);
    }

    // Asociar el socket a la dirección y puerto
    if (bind(server_fd, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind");
        close(server_fd);
        exit(1);
    }

    // Ponemos el socket en modo escucha
    if (listen(server_fd, 10) < 0) {
        perror("Error en listen");
        close(server_fd);
        exit(1);
    }

    printf("Servidor Gotham escuchando en %s:%d\n", ip, puerto);
    return server_fd;
}


/***********************************************
* @Finalitat: Gestionar les connexions entrants de Fleck o workers (Harley/Enigma).
* Cada connexió es tracta en un fil separat amb buffers dinàmics.
* @Paràmetres:
*   in: arg = punter a un descriptor de fitxer del client (client_fd).
* @Retorn: ----
************************************************/
void *gestionarConexion(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char *buffer = NULL;
    ssize_t bytesLeidos;
    size_t totalBytes = 0;

    // Bucle per llegir missatges del client
    while (1) {
        // Realitzem una lectura inicial per a veure si hi ha dades
        char tmpBuffer[128]; // Buffer temporal
        bytesLeidos = read(client_fd, tmpBuffer, sizeof(tmpBuffer));

        if (bytesLeidos <= 0) {
            // Si no es llegeixen bytes o hi ha un error, finalitzem la connexió
            printF("Client desconnectat\n");
            break;
        }

        // Actualitzem el buffer dinàmic amb les dades llegides
        totalBytes += bytesLeidos;
        buffer = (char *)realloc(buffer, totalBytes + 1); // Redimensionem el buffer
        if (buffer == NULL) {
            perror("Error en realloc");
            close(client_fd);
            return NULL;
        }

        // Copiem les dades noves al buffer
        memcpy(buffer + totalBytes - bytesLeidos, tmpBuffer, bytesLeidos);
        buffer[totalBytes] = '\0'; // Afegim el caràcter de final de cadena

        // Mostrem el missatge rebut
        printf("Missatge rebut (%ld bytes): %s\n", totalBytes, buffer);

        // Lògica de processament de la trama aquí...
        // Per exemple, podríem afegir processament de trames, respostes, etc.

        // Alliberem el buffer després de cada processament
        free(buffer);
        buffer = NULL;
        totalBytes = 0;  // Restablim totalBytes després del processament
    }

    close(client_fd);
    return NULL;
}

/***********************************************
* @Finalitat: Bucle principal que accepta connexions entrants i crea un fil per a cada connexió.
* @Paràmetres:
*   in: server_fd = descriptor del servidor per a acceptar connexions.
* @Retorn: ----
************************************************/
void esperarConexiones(int server_fd) {
    struct sockaddr_in direccionCliente;
    socklen_t tamanoDireccion = sizeof(direccionCliente);

    while (1) {
        int *client_fd = malloc(sizeof(int)); // Punter a un descriptor de client
        if (client_fd == NULL) {
            perror("Error en malloc per client_fd");
            exit(1);
        }

        // Acceptar una connexió entrant
        *client_fd = accept(server_fd, (struct sockaddr *)&direccionCliente, &tamanoDireccion);
        if (*client_fd < 0) {
            perror("Error en accept");
            free(client_fd);
            continue;
        }

        // Mostrar la IP i port del client connectat
        char ipCliente[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(direccionCliente.sin_addr), ipCliente, INET_ADDRSTRLEN); //Per convertir de format binari a una legible
        printf("Connexió acceptada de %s:%d\n", ipCliente, ntohs(direccionCliente.sin_port)); //Per convertir de ordre de representació de
                                                                                              //bytes de red BigEndian a la de host
        // Crear un nou fil per gestionar la connexió
        pthread_t thread_fd;
        if (pthread_create(&thread_fd, NULL, gestionarConexion, client_fd) != 0) {
            perror("Error en crear el fil");
            close(*client_fd);
            free(client_fd);
        }

        // Desvincular el fil perquè no bloquegi els recursos després d'acabar, no es necessari esperar a que acabi aquest thread
        pthread_detach(thread_fd); //Per no fer que es bloqueji el thread i poguem acceptar més connexions
    }
}
//FASE1

// Funció per llegir el fitxer de configuració de Gotham
/***********************************************
* @Finalitat: Llegeix el fitxer de configuració especificat i emmagatzema la informació a la estructura GothamConfig.
* @Paràmetres:
*   in: configFile = nom del fitxer de configuració.
*   out: gothamConfig = estructura on s'emmagatzema la configuració llegida.
* @Retorn: ----
************************************************/
void readConfigFile(const char *configFile, GothamConfig *gothamConfig) {
    int fd = open(configFile, O_RDONLY); // Obre el fitxer en mode només lectura
    
    if (fd == -1) {
        printF("Error obrint el fitxer de configuració\n"); // Missatge d'error si no es pot obrir
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix i assigna memòria per a cada camp de la configuració
    gothamConfig->ipFleck = readUntil(fd, '\n'); // Llegeix la IP del servidor Fleck
    gothamConfig->ipFleck = trim(gothamConfig->ipFleck);  // Elimina espacios en blanco
    char* portFleck = readUntil(fd, '\n'); // Llegeix el port com a cadena
    gothamConfig->portFleck = atoi(portFleck); // Converteix el port a enter
    free(portFleck); // Allibera la memòria de la cadena temporal

    gothamConfig->ipHarEni = readUntil(fd, '\n'); // Llegeix la IP del servidor Harley Enigma
    gothamConfig->ipHarEni = trim(gothamConfig->ipHarEni);  // Elimina espacios en blanco
    char *portHarEni = readUntil(fd, '\n'); // Llegeix el port com a cadena
    gothamConfig->portHarEni = atoi(portHarEni); // Converteix el port a enter
    free(portHarEni); // Allibera la memòria de la cadena temporal

    close(fd); // Tanca el fitxer

    // Mostra la configuració llegida per a verificació
    printF("IP Fleck - ");
    printF(gothamConfig->ipFleck);
    printF("\nPort Fleck - ");
    char* portFleckStr = NULL;
    asprintf(&portFleckStr, "%d", gothamConfig->portFleck); // Converteix el port a cadena de text
    printF(portFleckStr);
    free(portFleckStr); // Allibera la memòria de la cadena temporal

    printF("\nIP Harley Enigma - ");
    printF(gothamConfig->ipHarEni);
    printF("\nPort Harley Enigma - ");
    char* portHarEniStr = NULL;
    asprintf(&portHarEniStr, "%d\n", gothamConfig->portHarEni); // Converteix el port a cadena de text
    printF(portHarEniStr);
    free(portHarEniStr); // Allibera la memòria de la cadena temporal
}

// Funció per alliberar la memòria dinàmica utilitzada per la configuració de Gotham
/***********************************************
* @Finalitat: Allibera la memòria dinàmica associada amb l'estructura GothamConfig.
* @Paràmetres:
*   in: gothamConfig = estructura GothamConfig a alliberar.
* @Retorn: ----
************************************************/
void alliberarMemoria(GothamConfig *gothamConfig) {
    if (gothamConfig->ipFleck) {
        free(gothamConfig->ipFleck); // Allibera la memòria de la IP Fleck
    }
    if (gothamConfig->ipHarEni) {
        free(gothamConfig->ipHarEni); // Allibera la memòria de la IP Harley Enigma
    }
    free(gothamConfig); // Finalment, allibera la memòria de l'estructura principal
}

// Funció principal
int main(int argc, char *argv[]) {
    // Crea la variable local per a la configuració de Gotham
    GothamConfig *gothamConfig = (GothamConfig *)malloc(sizeof(GothamConfig));
    
    if (argc != 2) {
        printF("Ús: ./gotham <fitxer de configuració>\n"); // Comprova que s'ha passat el fitxer de configuració com a argument
        exit(1); // Finalitza el programa en cas d'error
    }

    // Llegeix el fitxer de configuració passant l'estructura gothamConfig com a argument
    readConfigFile(argv[1], gothamConfig);
    
    int server_fd = iniciarServidor(gothamConfig->ipFleck, gothamConfig->portFleck);

    //Esperem connexions dels clients
    esperarConexiones(server_fd);

    // Allibera la memòria dinàmica abans de finalitzar
    alliberarMemoria(gothamConfig);

    return 0; // Finalitza correctament el programa
}