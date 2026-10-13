#include "barrier.h"

#include <errno.h>
#include <stddef.h>

/* Implementación de barrera de sincronización para hilos en C. */


static int sem_wait_nointr(sem_t *sem)//espera en semáforo, manejando interrupciones de señal
{
    int rc;
    do {
        rc = sem_wait(sem);
    } while (rc == -1 && errno == EINTR);//si la llamada a sem_wait es interrumpida por una señal, se vuelve a intentar
    return rc;
}

int barrier_init(barrier_t *b, int n)//inicializo la barrera con el número total de hilos que deben esperar
{
    int rc;

    if (b == NULL || n < 1) {
        return -1;
    }

    b->total = n;
    b->waiting = 0;
    b->phase = 0;

    rc = pthread_mutex_init(&b->mutex, NULL);
    if (rc != 0) {
        return -1;
    }
    if (sem_init(&b->turnstile[0], 0, 0) == -1) {//inicializo el semáforo para bloquear hilos
        (void)pthread_mutex_destroy(&b->mutex);
        return -1;
    }
    if (sem_init(&b->turnstile[1], 0, 0) == -1) {//inicializo el semáforo para desbloquear hilos
        (void)sem_destroy(&b->turnstile[0]);
        (void)pthread_mutex_destroy(&b->mutex);
        return -1;
    }
    return 0;
}

void barrier_destroy(barrier_t *b)
{
    if (b == NULL) {
        return;
    }
    (void)sem_destroy(&b->turnstile[0]);
    (void)sem_destroy(&b->turnstile[1]);
    (void)pthread_mutex_destroy(&b->mutex);
}

void barrier_wait(barrier_t *b)
{
    int my_phase;
    int i;
    int is_last = 0;

    if (b == NULL || b->total <= 1) {//si la barrera es nula o el número total de hilos es menor o igual a 1
        return;
    }

    (void)pthread_mutex_lock(&b->mutex);//bloqueo con mutex para proteger el acceso a la barrera
    my_phase = b->phase;
    b->waiting++;
    if (b->waiting == b->total) {//si todos los hilos han llegado a la barrera, se desbloquean y continúan ejecutando
        b->waiting = 0;
        b->phase = 1 - b->phase;//cambio de fase para la siguiente barrera
        is_last = 1;
    }
    (void)pthread_mutex_unlock(&b->mutex);

    if (is_last) {//si es el último hilo en llegar a la barrera, desbloquea a todos los demás hilos
        for (i = 0; i < b->total - 1; i++) {
            (void)sem_post(&b->turnstile[my_phase]);
        }
    } else {
        (void)sem_wait_nointr(&b->turnstile[my_phase]);//si no es el último hilo,debe esperar a que se desbloquee
    }
}
