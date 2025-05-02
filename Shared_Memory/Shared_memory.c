// shared_memory_utils.c
#include "Shared_memory.h"
#include "../DataConversion/DataConversion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// Bloquear acceso a la memoria compartida
int lock_shared_memory(SharedMemory *sm) {
    if (!sm) return -1;
    return SEM_wait(&sm->sem);
}

// Desbloquear memoria compartida
int unlock_shared_memory(SharedMemory *sm) {
    if (!sm) return -1;
    return SEM_signal(&sm->sem);
}
