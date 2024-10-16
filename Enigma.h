#ifndef ENIGMACONFIG_H
#define ENIGMACONFIG_H

#define _GNU_SOURCE // Asegura que asprintf esté disponible

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Macro para imprimir mensajes
#define printF(x) write(1, x, strlen(x))

// Definición de la estructura EnigmaConfig que contiene la configuración de Enigma
typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} EnigmaConfig;

// Prototipos de funciones

// Lee hasta el carácter delimitador especificado y devuelve una cadena con el contenido leído
char *readUntil(int fd, char cEnd);

// Lee el archivo de configuración de Enigma y asigna los valores en la estructura EnigmaConfig
void readConfigFile(const char *configFile, EnigmaConfig *enigmaConfig);

// Libera la memoria asignada dinámicamente para la estructura EnigmaConfig
void alliberarMemoria(EnigmaConfig *enigmaConfig);

#endif // ENIGMACONFIG_H
