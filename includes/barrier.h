#ifndef BARRIER_H
#define BARRIER_H

#include <pthread.h>
#include <semaphore.h>

typedef struct {//barrier todos los threads se desbloquean y continúan ejecutando
    int total;
    int waiting;
    int phase;
    sem_t turnstile[2];//semaforo para bloquear y desbloquear threads
    //turnstile[0] para bloquear threads y turnstile[1] para desbloquear threads
    pthread_mutex_t mutex;
} barrier_t;

int barrier_init(barrier_t *b, int n);
void barrier_destroy(barrier_t *b);
void barrier_wait(barrier_t *b);

#endif
