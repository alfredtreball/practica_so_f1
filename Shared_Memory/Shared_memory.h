// shared_memory_utils.h
#ifndef _SHARED_MEMORYH_
#define _SHARED_MEMORYH_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "semaphore_v2.h"

// Estructura para manejar memoria compartida
typedef struct {
    int shmid;        // ID del segmento de memoria compartida
    void *shmaddr;    // Dirección de la memoria compartida
    semaphore sem;    // Semáforo para sincronización
} SharedMemory;

typedef struct {
    char fileName[256];   // Nombre del archivo
    char user[64];        // Usuario asociado al archivo
    char status[16];      // Estado del procesamiento ("PENDING", "DONE", etc.)
    char result[1024];    // Resultado (MD5, validación, etc.)
} SharedData;

// Funciones para manejar memoria compartida
int init_shared_memory(SharedMemory *sm, key_t key, size_t size);
int destroy_shared_memory(SharedMemory *sm);

//Llegir i escriure a la memòria compartida
int read_from_shared_memory(SharedMemory *sm, SharedData *data);
int write_to_shared_memory(SharedMemory *sm, const SharedData *data);

// Funciones para operar sobre la memoria compartida
int lock_shared_memory(SharedMemory *sm);
int unlock_shared_memory(SharedMemory *sm);

#endif /* _SHARED_MEMORY_UTILS_H_ */
