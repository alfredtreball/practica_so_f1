#ifndef GOTHAMCONFIG_H
#define GOTHAMCONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Macro para imprimir mensajes
#define printF(x) write(1, x, strlen(x))

// Definición de la estructura GothamConfig que contiene la configuración de Gotham
typedef struct {
    char *ipFleck;
    int portFleck;
    char *ipHarEni;
    int portHarEni;
} GothamConfig;

// Prototipos de funciones

// Lee hasta el carácter delimitador especificado y devuelve una cadena con el contenido leído
char *readUntil(int fd, char cEnd);

// Lee el archivo de configuración de Gotham y asigna los valores en la estructura GothamConfig
void readConfigFile(const char *configFile, GothamConfig *gothamConfig);

// Libera la memoria asignada dinámicamente para la estructura GothamConfig
void alliberarMemoria(GothamConfig *gothamConfig);

#endif // GOTHAMCONFIG_H
