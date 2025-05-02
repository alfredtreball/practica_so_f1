#ifndef ENIGMA_SYNC_H
#define ENIGMA_SYNC_H

#include <stddef.h>
#include "../Shared_Memory/Shared_memory.h"

#define MAX_DISTORTIONS 10  // Número máximo de distorsiones simultáneas

#define STATUS_PENDING 1
#define STATUS_IN_PROGRESS 2
#define STATUS_DONE 3

// Estructura para almacenar el estado de una distorsión en Enigma
typedef struct {
    char fileName[256];   // Nombre del archivo en proceso
    char md5Sum[33];      // MD5 del archivo en proceso
    size_t currentByte;   // Posición actual del archivo
    int factor;           // Factor de compresión
    int fleckSocketFD;    //Socket del fleck que envia la distorsió
    int status;      // Estado de la distorsión (1 -> PENDING, 2 -> IN PROGRESS, 3 -> DONE)
} EnigmaDistortionEntry;

// Estructura que almacena todas las distorsiones en curso
typedef struct {
    int count;  // Número de distorsiones activas
    EnigmaDistortionEntry distortions[MAX_DISTORTIONS];
} EnigmaDistortionState;

// Inicializa la memoria compartida para el estado de Enigma
int init_shared_memory(SharedMemory *sm, key_t key, size_t size);

// Guarda el estado actual de una distorsión en memoria compartida
int save_enigma_distortion_state(SharedMemory *sm, const char *fileName, size_t currentByte, 
    int factor, const char *md5Sum, int fleckSocketFD, int status);

// Recupera todas las distorsiones en curso
int load_enigma_distortion_state(SharedMemory *sm, EnigmaDistortionEntry *entries, int *count);

int remove_completed_distortions(SharedMemory *sm);

#endif // ENIGMA_SYNC_H
