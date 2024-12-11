// semaphore_v2.c
#include "semaphore_v2.h"
#include <stdio.h> // Por si acaso necesitas funciones como perror

int SEM_constructor_with_name(semaphore *sem, key_t key) {
    sem->shmid = semget(key, 1, IPC_CREAT | 0644);
    if (sem->shmid < 0) return sem->shmid;
    return 0;
}

int SEM_constructor(semaphore *sem) {
    sem->shmid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (sem->shmid < 0) return sem->shmid;
    return 0;
}

int SEM_init(const semaphore *sem, const int v) {
    unsigned short _v[1] = {v};
    return semctl(sem->shmid, 0, SETALL, _v);
}

int SEM_destructor(const semaphore *sem) {
    return semctl(sem->shmid, 0, IPC_RMID, NULL);
}

int SEM_wait(const semaphore *sem) {
    struct sembuf o = {0, -1, SEM_UNDO};
    return semop(sem->shmid, &o, 1);
}

int SEM_signal(const semaphore *sem) {
    struct sembuf o = {0, 1, SEM_UNDO};
    return semop(sem->shmid, &o, 1);
}
