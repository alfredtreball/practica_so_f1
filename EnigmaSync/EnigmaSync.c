#include "EnigmaSync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Shared_Memory/Shared_memory.h"
#include "../DataConversion/DataConversion.h"

// Inicializa la memoria compartida para Enigma
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
int save_enigma_distortion_state(SharedMemory *sm, const char *fileName, size_t currentByte, 
    int factor, const char *md5Sum, int fleckSocketFD, int status) {
    
    if (!sm || !sm->shmaddr) {
        customPrintf("[ERROR] ❌ Memoria compartida no inicializada antes de guardar estado.\n");
        return -1;
    }

    lock_shared_memory(sm);
    EnigmaDistortionState *state = (EnigmaDistortionState *)sm->shmaddr;

    int found = 0;

    for (int i = 0; i < state->count; i++) {
        if (strcmp(state->distortions[i].fileName, fileName) == 0) {
            // 📌 Actualizar distorsión existente
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
            state->distortions[state->count].currentByte = currentByte;
            state->distortions[state->count].factor = factor;
            state->distortions[state->count].fleckSocketFD = fleckSocketFD;
            state->distortions[state->count].status = status; // Se asigna el estado correctamente

            customPrintf("[DEBUG] 💾 Nueva distorsión agregada: %s, MD5=%s, byte=%ld, factor=%d, status=%d\n",
                   fileName, md5Sum, currentByte, factor, state->distortions[state->count].status);

            state->count++;
        } else {
            customPrintf("[ERROR] ❌ No se pueden agregar más distorsiones, memoria llena.\n");
        }
    }

    unlock_shared_memory(sm);
    return 0;
}


// Recupera el estado de la distorsión en caso de caída
int load_enigma_distortion_state(SharedMemory *sm, EnigmaDistortionEntry *entries, int *count) {
    if (!sm || !sm->shmaddr) {
        customPrintf("[ERROR] ❌ Memoria compartida no inicializada antes de recuperar estado.\n");
        return -1;
    }

    lock_shared_memory(sm);
    EnigmaDistortionState *state = (EnigmaDistortionState *)sm->shmaddr;

    *count = 0;
    for (int i = 0; i < state->count; i++) {
        if (state->distortions[i].status == STATUS_IN_PROGRESS || 
            state->distortions[i].status == STATUS_DONE || 
            state->distortions[i].status == STATUS_PENDING) {

            entries[*count] = state->distortions[i]; 
            (*count)++;
        }
    }

    for (int i = 0; i < *count; i++) {
        const char *status_str = "UNKNOWN";
        switch (entries[i].status) {
            case STATUS_PENDING:    status_str = "PENDING"; break;
            case STATUS_IN_PROGRESS:status_str = "IN_PROGRESS"; break;
            case STATUS_DONE:       status_str = "DONE"; break;
        }

        customPrintf("[DEBUG] 📄 Archivo: %s | Byte actual: %ld | Estado: %s\n",
                    entries[i].fileName,
                    entries[i].currentByte,
                    status_str);
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
    EnigmaDistortionState *state = (EnigmaDistortionState *)sm->shmaddr;

    int newCount = 0;
    for (int i = 0; i < state->count; i++) {
        if (state->distortions[i].status != STATUS_DONE) {
            state->distortions[newCount] = state->distortions[i]; // Mover solo los que no estén completados
            newCount++;
        } else {
            customPrintf("[DEBUG] 🗑 Eliminando distorsión completada: %s\n", state->distortions[i].fileName);
        }
    }

    state->count = newCount; // Actualizar el número de distorsiones activas

    unlock_shared_memory(sm);
    return 0;
}


