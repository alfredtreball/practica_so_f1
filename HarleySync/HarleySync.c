#include "HarleySync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Shared_Memory/Shared_memory.h"
#include "../DataConversion/DataConversion.h"


// Inicializa la memoria compartida para Harley
int init_shared_memory(SharedMemory *sm, key_t key, size_t size) {
    if (!sm) return -1;

    sm->shmid = shmget(key, size, IPC_CREAT | 0666);
    if (sm->shmid < 0) {
        perror("Error creando memoria compartida");
        return -1;
    }

    sm->shmaddr = shmat(sm->shmid, NULL, 0);
    if (sm->shmaddr == (void *)-1) {
        perror("Error asociando memoria compartida");
        return -1;
    }

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

// Guarda el estado exacto de la distorsión en curso
int save_harley_distortion_state(SharedMemory *sm, const char *fileName, size_t currentByte, 
    int factor, const char *md5Sum, int fleckSocketFD, int status, const char *userName) {
    
    if (!sm || !sm->shmaddr) {
        customPrintf("[ERROR] ❌ Memoria compartida no inicializada antes de guardar estado.\n");
        return -1;
    }

    lock_shared_memory(sm);
    HarleyDistortionState *state = (HarleyDistortionState *)sm->shmaddr;

    int found = 0;

    for (int i = 0; i < state->count; i++) {
        if (strcmp(state->distortions[i].fileName, fileName) == 0 && strcmp(state->distortions[i].userName, userName) == 0) {
            //Actualizar distorsión existente
            state->distortions[i].currentByte = currentByte;
            state->distortions[i].factor = factor;
            strncpy(state->distortions[i].md5Sum, md5Sum, sizeof(state->distortions[i].md5Sum));
            state->distortions[i].fleckSocketFD = fleckSocketFD;
            state->distortions[i].status = status; //Se asigna el estado correctamente
            found = 1;
            break;  //No necesitamos seguir buscando
        }
    }

    // 📌 Si no se encontró la distorsión, la agregamos
    if (!found) {
        if (state->count < MAX_DISTORTIONS) {
            strncpy(state->distortions[state->count].fileName, fileName, sizeof(state->distortions[state->count].fileName) - 1);
            strncpy(state->distortions[state->count].md5Sum, md5Sum, sizeof(state->distortions[state->count].md5Sum));
            strncpy(state->distortions[state->count].userName, userName, sizeof(state->distortions[state->count].userName));
            state->distortions[state->count].currentByte = currentByte;
            state->distortions[state->count].factor = factor;
            state->distortions[state->count].fleckSocketFD = fleckSocketFD;
            state->distortions[state->count].status = status; // Se asigna el estado correctamente

            state->count++;
        } else {
            customPrintf("[ERROR] ❌ No se pueden agregar más distorsiones, memoria llena.\n");
        }
    }

    unlock_shared_memory(sm);
    return 0;
}


// Recupera el estado de la distorsión en caso de caída
int load_harley_distortion_state(SharedMemory *sm, HarleyDistortionEntry *entries, int *count) {
    if (!sm || !sm->shmaddr) {
        customPrintf("[ERROR] Memoria compartida no inicializada antes de recuperar estado.\n");
        return -1;
    }

    lock_shared_memory(sm);
    HarleyDistortionState *state = (HarleyDistortionState *)sm->shmaddr;

    *count = 0;
    for (int i = 0; i < state->count; i++) {
        if (state->distortions[i].status == STATUS_IN_PROGRESS || 
            state->distortions[i].status == STATUS_DONE || 
            state->distortions[i].status == STATUS_PENDING) {

            entries[*count] = state->distortions[i]; 
            (*count)++;
        }
    }

    unlock_shared_memory(sm);
    return (*count > 0) ? 0 : -1;
}

// Elimina todas las distorsiones con STATUS_DONE de la memoria compartida
int remove_completed_distortions(SharedMemory *sm) {
    if (!sm || !sm->shmaddr) {
        customPrintf("[ERROR] ❌ Memoria compartida no inicializada antes de eliminar distorsiones completadas.\n");
        return -1;
    }

    lock_shared_memory(sm);
    HarleyDistortionState *state = (HarleyDistortionState *)sm->shmaddr;

    int newCount = 0;
    for (int i = 0; i < state->count; i++) {
        if (state->distortions[i].status != STATUS_DONE) {
            state->distortions[newCount] = state->distortions[i]; // Mover solo los que no estén completados
            newCount++;
        }
    }

    state->count = newCount; // Actualizar el número de distorsiones activas

    unlock_shared_memory(sm);
    return 0;
}


