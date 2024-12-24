#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <pthread.h>
#include "../GestorTramas/GestorTramas.h"

#define QUEUE_SIZE 100

typedef struct {
    Frame queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} MessageQueue;

void initQueue(MessageQueue *queue);
int enqueue(MessageQueue *queue, const Frame *frame);
int dequeue(MessageQueue *queue, Frame *frame);
void destroyQueue(MessageQueue *queue);

#endif // MESSAGE_QUEUE_H
