// semaphore_v2.h
#ifndef _MOD_SEMAPHORE_H_
#define _MOD_SEMAPHORE_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

typedef struct {
    int shmid;
} semaphore;

int SEM_constructor_with_name(semaphore *sem, key_t key);
int SEM_constructor(semaphore *sem);
int SEM_init(const semaphore *sem, const int v);
int SEM_destructor(const semaphore *sem);
int SEM_wait(const semaphore *sem);
int SEM_signal(const semaphore *sem);

#endif /* _MOD_SEMAPHORE_H_ */
