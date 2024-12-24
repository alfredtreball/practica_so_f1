#include "MessageQueue.h"
#include "../Logging/Logging.h"
#include <stdbool.h>

void initQueue(MessageQueue *queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
}

int enqueue(MessageQueue *queue, const Frame *frame) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->count == QUEUE_SIZE) {
        logError("[ERROR]: Cola de mensajes llena.");
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    queue->queue[queue->rear] = *frame;
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int dequeue(MessageQueue *queue, Frame *frame) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    *frame = queue->queue[queue->front];
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void destroyQueue(MessageQueue *queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty); // Cambiado de `queue->cond` a `queue->not_empty`
}


