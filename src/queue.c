
#include "queue.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

static job_t *trabajos = NULL;

static int capacidad;
static int inicio;
static int fin;
static int cantidad;
static int cerrada;
static int inicializada;
static int consumidores_esperando;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t disponibles;

int inicializar_cola(int capacidad_nueva)
{
    if (capacidad_nueva <= 0)
        return -1;

    pthread_mutex_lock(&mutex);

    if (inicializada) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    trabajos = calloc((size_t)capacidad_nueva,
                      sizeof(job_t));

    if (trabajos == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    if (sem_init(&disponibles, 0, 0) != 0) {
        free(trabajos);
        trabajos = NULL;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    capacidad = capacidad_nueva;
    inicio = 0;
    fin = 0;
    cantidad = 0;
    cerrada = 0;
    consumidores_esperando = 0;
    inicializada = 1;

    pthread_mutex_unlock(&mutex);
    return 0;
}

int agregar_cola(const job_t *trabajo)
{
    if (trabajo == NULL)
        return -2;

    pthread_mutex_lock(&mutex);

    if (!inicializada || cerrada) {
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    if (cantidad == capacidad) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    trabajos[fin] = *trabajo;
    fin = (fin + 1) % capacidad;
    cantidad++;

    sem_post(&disponibles);

    pthread_mutex_unlock(&mutex);

    return 0;
}

int sacar_cola(job_t *trabajo)
{
    if (trabajo == NULL)
        return -1;

    for (;;) {
        pthread_mutex_lock(&mutex);

        if (!inicializada ||
            (cerrada && cantidad == 0)) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        consumidores_esperando++;
        pthread_mutex_unlock(&mutex);

        int resultado;

        do {
            resultado = sem_wait(&disponibles);
        } while (resultado == -1 && errno == EINTR);

        pthread_mutex_lock(&mutex);
        consumidores_esperando--;

        if (resultado != 0) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        if (cantidad > 0) {
            *trabajo = trabajos[inicio];
            inicio = (inicio + 1) % capacidad;
            cantidad--;

            pthread_mutex_unlock(&mutex);
            return 0;
        }

        if (cerrada) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        pthread_mutex_unlock(&mutex);
    }
}

void cerrar_cola(void)
{
    pthread_mutex_lock(&mutex);

    if (inicializada && !cerrada) {
        cerrada = 1;

        for (int i = 0; i < consumidores_esperando; i++) {
            sem_post(&disponibles);
        }
    }

    pthread_mutex_unlock(&mutex);
}

void destruir_cola(void)
{
    pthread_mutex_lock(&mutex);

    if (inicializada) {
        /* Los consumidores deben haber terminado */
        sem_destroy(&disponibles);
        free(trabajos);

        trabajos = NULL;
        capacidad = 0;
        inicio = 0;
        fin = 0;
        cantidad = 0;
        cerrada = 0;
        consumidores_esperando = 0;
        inicializada = 0;
    }

    pthread_mutex_unlock(&mutex);
}

