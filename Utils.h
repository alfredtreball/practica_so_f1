/***********************************************
* @Fitxer: Utils.h
* @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
* @Estudis: Enginyeria Electrònica de Telecomunicacions
* @Universitat: Universitat Ramon Llull - La Salle
* @Assignatura: Sistemes Operatius
* @Curs: 2024-2025
* 
* @Descripció: Aquest fitxer d'encapçalament defineix funcions utilitàries per al projecte,
* incloent-hi funcions per a la manipulació de cadenes i la gestió de fitxers. Les utilitats
* inclouen la lectura des d'un fitxer fins a un caràcter específic, la verificació de si una
* cadena acaba amb un sufix determinat, la funció per eliminar espais en blanc d'una cadena, 
* i l'eliminació de caràcters específics dins de les cadenes. També s'inclou una macro per a 
* la sortida de text a la consola.
************************************************/

#ifndef _UTILS_H
#define _UTILS_H

#include <unistd.h>  // Per a la funció write
#include <stdlib.h>  // Per a malloc, realloc, free
#include <string.h>  // Per a strlen, strncmp
#include <stdio.h>   // Per a funcions de gestió d'entrada/sortida
#include <ctype.h>   // Per a isspace

// Macro per imprimir cadenes de text a la sortida estàndard
/***********************************************
* @Finalitat: Escriu la cadena de text 'x' a la sortida estàndard (file descriptor 1).
* @Paràmetres:
*   in: x = cadena de text a imprimir.
* @Retorn: ----
************************************************/
#define printF(x) write(1, x, strlen(x))

// Funcions de utilitat general
/***********************************************
* @Finalitat: Funcions diverses que proporcionen utilitats generals per al projecte, com llegir fins a un caràcter específic, verificar si una cadena acaba amb un sufix, eliminar espais en blanc d'una cadena i eliminar caràcters específics.
************************************************/

// Llegeix des del fitxer descriptor fins trobar un caràcter específic o final de fitxer
char *readUntil(int fd, char cEnd);

// Comprova si una cadena acaba amb un sufix específic
int endsWith(char *str, char *suffix);

// Elimina espais en blanc al començament i al final d'una cadena
char *trim(char *str);

// Elimina totes les aparicions d'un caràcter específic en una cadena
void removeChar(char *string, char charToRemove);

#endif // UTILS_H
