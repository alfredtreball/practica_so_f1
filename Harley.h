#ifndef HARLEYCONFIG_H
#define HARLEYCONFIG_H

#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Macro para imprimir mensajes
#define printF(x) write(1, x, strlen(x))

// Definición de la estructura HarleyConfig que contiene la configuración de Harley
typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} HarleyConfig;

// Prototipos de funciones

// Lee el archivo de configuración de Harley y asigna los valores en la estructura HarleyConfig
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig);

// Libera la memoria asignada dinámicamente para la estructura HarleyConfig
void alliberarMemoria(HarleyConfig *harleyConfig);

#endif // HARLEYCONFIG_H
