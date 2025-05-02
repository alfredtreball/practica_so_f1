#ifndef HARLEY_SYNC_H
#define HARLEY_SYNC_H

#include <stddef.h>
#include "../Shared_Memory/Shared_memory.h"

#define MAX_DISTORTIONS 10  // Número máximo de distorsiones simultáneas

#define STATUS_PENDING 1
#define STATUS_IN_PROGRESS 2
#define STATUS_DONE 3

// Estructura para almacenar el estado de una distorsión en Harley
typedef struct {
    char fileName[256];   // Nombre del archivo en proceso
    char md5Sum[33];      // MD5 del archivo en proceso
    size_t currentByte;   // Posición actual del archivo
    int factor;           // Factor de compresión
    int fleckSocketFD;    //Socket del fleck que envia la distorsió
    int status;      // Estado de la distorsión (1 -> PENDING, 2 -> IN PROGRESS, 3 -> DONE)
} HarleyDistortionEntry;

// Estructura que almacena todas las distorsiones en curso
typedef struct {
    int count;  // Número de distorsiones activas
    HarleyDistortionEntry distortions[MAX_DISTORTIONS];
} HarleyDistortionState;

// Inicializa la memoria compartida para el estado de Harley
int init_shared_memory(SharedMemory *sm, key_t key, size_t size);

// Guarda el estado actual de una distorsión en memoria compartida
int save_harley_distortion_state(SharedMemory *sm, const char *fileName, size_t currentByte, 
    int factor, const char *md5Sum, int fleckSocketFD, int status);

// Recupera todas las distorsiones en curso
int load_harley_distortion_state(SharedMemory *sm, HarleyDistortionEntry *entries, int *count);

int remove_completed_distortions(SharedMemory *sm);

#endif // HARLEY_SYNC_H
