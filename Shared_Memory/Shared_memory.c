// shared_memory_utils.c
#include "Shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Inicializa la memoria compartida y su semáforo
int init_shared_memory(SharedMemory *sm, key_t key, size_t size) {
    if (!sm) return -1;

    // Crear segmento de memoria compartida
    sm->shmid = shmget(key, size, IPC_CREAT | 0666);
    if (sm->shmid < 0) {
        perror("Error creando memoria compartida");
        return -1;
    }

    // Asociar la memoria al proceso
    sm->shmaddr = shmat(sm->shmid, NULL, 0);
    if (sm->shmaddr == (void *)-1) {
        perror("Error asociando memoria compartida");
        return -1;
    }

    // Crear e inicializar el semáforo
    if (SEM_constructor_with_name(&sm->sem, key) < 0) {
        perror("Error creando semáforo");
        return -1;
    }
    if (SEM_init(&sm->sem, 1) < 0) {
        perror("Error inicializando semáforo");
        return -1;
    }

    return 0;
}

// Escribir en la memoria compartida
int write_to_shared_memory(SharedMemory *sm, const SharedData *data) {
    if (!sm || !data) return -1;

    lock_shared_memory(sm); // Bloqueo
    memcpy(sm->shmaddr, data, sizeof(SharedData));
    unlock_shared_memory(sm); // Desbloqueo

    return 0;
}

// Leer de la memoria compartida
int read_from_shared_memory(SharedMemory *sm, SharedData *data) {
    if (!sm || !data) return -1;

    lock_shared_memory(sm); // Bloqueo
    memcpy(data, sm->shmaddr, sizeof(SharedData));
    unlock_shared_memory(sm); // Desbloqueo

    return 0;
}

// Libera la memoria compartida y destruye el semáforo
int destroy_shared_memory(SharedMemory *sm) {
    if (!sm) return -1;

    // Desasociar memoria
    if (shmdt(sm->shmaddr) < 0) {
        perror("Error desasociando memoria compartida");
        return -1;
    }

    // Liberar segmento de memoria
    if (shmctl(sm->shmid, IPC_RMID, NULL) < 0) {
        perror("Error destruyendo memoria compartida");
        return -1;
    }

    // Destruir semáforo
    if (SEM_destructor(&sm->sem) < 0) {
        perror("Error destruyendo semáforo");
        return -1;
    }

    return 0;
}

// Bloquear acceso a la memoria compartida
int lock_shared_memory(SharedMemory *sm) {
    if (!sm) return -1;
    return SEM_wait(&sm->sem);
}

// Liberar acceso a la memoria compartida
int unlock_shared_memory(SharedMemory *sm) {
    if (!sm) return -1;
    return SEM_signal(&sm->sem);
}
