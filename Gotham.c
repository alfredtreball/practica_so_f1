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
#include "Networking.h"

//FASE2
void *gestionarConexion(void *arg);
/***********************************************
* @Finalitat: Bucle principal que accepta connexions entrants i crea un fil per a cada connexió.
* @Paràmetres:
*   in: arg = descriptor del servidor passat com a `void *`.
*   in: server_fd = descriptor del servidor per a acceptar connexions.
* @Retorn: ----
************************************************/
void *esperarConexiones(void *arg) {
    // Descriptors de socket per a Fleck, Enigma i Harley (assumeix que es passen al struct de l’argument)
    int server_fd_fleck = ((int *)arg)[0];  
    int server_fd_enigma = ((int *)arg)[1]; 
    int server_fd_harley = ((int *)arg)[2]; 

    // Conjunt de descriptors per utilitzar amb select()
    fd_set read_fds;
    
    // Selecciona el descriptor més gran entre els tres sockets per indicar-lo a select()
    int max_fd = server_fd_fleck;
    if (server_fd_enigma > max_fd) max_fd = server_fd_enigma;
    if (server_fd_harley > max_fd) max_fd = server_fd_harley;

    while (1) {
        // Inicialitza el conjunt de descriptors abans d’afegir-hi nous
        FD_ZERO(&read_fds);
        FD_SET(server_fd_fleck, &read_fds);   // Afegeix el socket de Fleck
        FD_SET(server_fd_enigma, &read_fds);  // Afegeix el socket d’Enigma
        FD_SET(server_fd_harley, &read_fds);  // Afegeix el socket de Harley

        // select() espera que un dels descriptors estigui llest per operar
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) { 
            perror("Error en select"); // Mostra un missatge d’error si select() falla
            continue; 
        }

        // Gestiona les connexions segons el socket que activi l'activitat
        
        // Comprova si hi ha activitat en el socket de Fleck
        if (FD_ISSET(server_fd_fleck, &read_fds)) {
            int *client_fd = malloc(sizeof(int));
            *client_fd = accept(server_fd_fleck, NULL, NULL);
            if (*client_fd >= 0) {
                pthread_t fleck_thread;
                pthread_create(&fleck_thread, NULL, gestionarConexion, client_fd); 
                pthread_detach(fleck_thread);
            }
        }

        // Comprova si hi ha activitat en el socket d’Enigma
        if (FD_ISSET(server_fd_enigma, &read_fds)) {
            int *client_fd = malloc(sizeof(int));
            *client_fd = accept(server_fd_enigma, NULL, NULL);
            if (*client_fd >= 0) {
                pthread_t enigma_thread;
                pthread_create(&enigma_thread, NULL, gestionarConexion, client_fd);
                pthread_detach(enigma_thread);
            }
        }

        // Comprova si hi ha activitat en el socket de Harley
        if (FD_ISSET(server_fd_harley, &read_fds)) {
            int *client_fd = malloc(sizeof(int));
            *client_fd = accept(server_fd_harley, NULL, NULL);
            if (*client_fd >= 0) {
                pthread_t harley_thread;
                pthread_create(&harley_thread, NULL, gestionarConexion, client_fd);
                pthread_detach(harley_thread);
            }
        }
    }
    return NULL;
}

void processCommandInGotham(const char *command, int client_fd) {
    printF("Trama rebuda correctament a Gotham: ");
    printF(command);
    printF("\n");

    if (strcmp(command, "CONNECT") == 0) {
        // Registra la connexió
        printF("Client connectat\n");
        send_frame(client_fd, "ACK", 2);  // Envia resposta de confirmació
    } 
    else if (strncmp(command, "DISTORT", 7) == 0) {
        printF("Processant comanda DISTORT\n");

        // Confirma que DISTORT ha estat rebut i processat
        send_frame(client_fd, "ACK DISTORT", 10);
    } 
    else if (strcmp(command, "CHECK STATUS") == 0) {
        printF("Comprovant estat actual\n");
        send_frame(client_fd, "STATUS OK ACK", 12); 
    } 
    else if (strcmp(command, "CLEAR ALL") == 0) {
        printF("Processant comanda CLEAR ALL\n");

        // Confirma que CLEAR ALL ha estat rebut i processat
        send_frame(client_fd, "ACK CLEAR ALL", 13);
    } 
    else {
        // Comanda desconeguda
        send_frame(client_fd, "ERROR: Comanda desconeguda", 26);
    }
}

/***********************************************
* @Finalitat: Gestiona les connexions entrants de Fleck, Enigma o Harley.
* Cada connexió es tracta en un fil separat, on es reben trames i es processa la comanda.
* @Paràmetres:
*   in: arg = punter a un descriptor de fitxer del client (client_fd).
* @Retorn: ----
************************************************/
void *gestionarConexion(void *arg) {
    int client_fd = *(int *)arg; // Descriptor de fitxer del client
    free(arg); // Alliberem la memòria assignada a l'argument

    char data[FRAME_SIZE]; // Buffer per emmagatzemar les dades de la trama
    int data_length;       // Longitud de les dades rebudes

    while (1) {
        // Recepció de la trama
        if (receive_frame(client_fd, data, &data_length) < 0) {
            printF("Error rebent la trama o client desconnectat\n");
            break; // Si hi ha un error, es tanca la connexió
        }

        // Processar la comanda rebuda a la trama
        printF("Trama rebuda: ");
        printF(data);
        printF("\n");

        // Crida a processCommandInGotham per gestionar la comanda
        processCommandInGotham(data, client_fd);
    }

    close(client_fd); // Tanquem la connexió amb el client
    return NULL;
}

//FASE1
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

int main(int argc, char *argv[]) {
    GothamConfig *gothamConfig = malloc(sizeof(GothamConfig));

    if (argc != 2) {
        printF("Ús: ./gotham <fitxer de configuració>\n");
        free(gothamConfig);  // Liberar memoria en caso de error
        return 1;
    }

    if (!gothamConfig) {
        printF("Error assignant memòria per a la configuració\n");
        return 1;
    }

    readConfigFileGeneric(argv[1], gothamConfig, CONFIG_GOTHAM);

    // Configura y crea el primer socket del servidor (Fleck)
    int server_fd_fleck = startServer(gothamConfig->ipFleck, gothamConfig->portFleck);
    if (server_fd_fleck < 0) {
        printF("No se pudo iniciar el servidor para Fleck\n");
        alliberarMemoria(gothamConfig);
        return 1;
    }

    // Configura y crea el segundo socket del servidor (Harley/Enigma)
    int server_fd_enigma = startServer(gothamConfig->ipHarEni, gothamConfig->portHarEni);
    if (server_fd_enigma < 0) {
        printF("No se pudo iniciar el servidor para Harley/Enigma\n");
        close(server_fd_fleck);  // Cerrar el servidor Fleck antes de salir
        alliberarMemoria(gothamConfig);
        return 1;
    }

    // Iniciar hilos para aceptar conexiones en ambos sockets
    pthread_t fleck_thread, enigma_thread;

    if (pthread_create(&fleck_thread, NULL, (void *(*)(void *))esperarConexiones, &server_fd_fleck) != 0) {
        perror("Error creando el hilo para conexiones de Fleck");
        close(server_fd_fleck);
        close(server_fd_enigma);
        alliberarMemoria(gothamConfig);
        return 1;
    }

    if (pthread_create(&enigma_thread, NULL, (void *(*)(void *))esperarConexiones, &server_fd_enigma) != 0) {
        perror("Error creando el hilo para conexiones de Harley/Enigma");
        pthread_cancel(fleck_thread);  // Cancelar el hilo de Fleck en caso de error
        close(server_fd_fleck);
        close(server_fd_enigma);
        alliberarMemoria(gothamConfig);
        return 1;
    }

    // Esperar a que los hilos terminen
    pthread_join(fleck_thread, NULL);
    pthread_join(enigma_thread, NULL);

    // Liberar recursos y memoria
    close(server_fd_fleck);
    close(server_fd_enigma);
    alliberarMemoria(gothamConfig);

    return 0;
}