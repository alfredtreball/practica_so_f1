#ifndef _SHARED_MEMORYH_
#define _SHARED_MEMORYH_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../Semafors/semaphore_v2.h"  // Asegurar que incluimos los semáforos

// Estructura para manejar memoria compartida
typedef struct {
    int shmid;        // ID del segmento de memoria compartida
    void *shmaddr;    // Dirección de la memoria compartida
    semaphore sem;    // Semáforo para sincronización
} SharedMemory;

// Funciones para sincronización con semáforos
int lock_shared_memory(SharedMemory *sm);
int unlock_shared_memory(SharedMemory *sm);

#endif /* _SHARED_MEMORYH_ */
