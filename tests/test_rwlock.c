
#include "rwlock.h"

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

static rwlock_t candado;
static sem_t lectores_listos;
static sem_t permitir_salida;
static pthread_mutex_t mutex_prueba = PTHREAD_MUTEX_INITIALIZER;

static int lectores_activos = 0;
static int dato = 0;

/* Funcion de los lectores */
static void *lector(void *argumento)
{
    (void)argumento;

    bloquear_lectura(&candado);

    pthread_mutex_lock(&mutex_prueba);
    lectores_activos++;
    assert(dato == 0);
    pthread_mutex_unlock(&mutex_prueba);

    sem_post(&lectores_listos);
    sem_wait(&permitir_salida);

    pthread_mutex_lock(&mutex_prueba);
    lectores_activos--;
    pthread_mutex_unlock(&mutex_prueba);

    desbloquear_rwlock(&candado);

    return NULL;
}

/* Funcion del escritor */
static void *escritor(void *argumento)
{
    (void)argumento;

    bloquear_escritura(&candado);

    pthread_mutex_lock(&mutex_prueba);

    assert(lectores_activos == 0);
    dato = 10;

    pthread_mutex_unlock(&mutex_prueba);

    desbloquear_rwlock(&candado);

    return NULL;
}

int main(void)
{
    pthread_t lector1;
    pthread_t lector2;
    pthread_t hilo_escritor;

    inicializar_rwlock(&candado);

    assert(sem_init(&lectores_listos, 0, 0) == 0);
    assert(sem_init(&permitir_salida, 0, 0) == 0);

    /* Iniciar dos lectores */
    assert(pthread_create(&lector1, NULL, lector, NULL) == 0);
    assert(pthread_create(&lector2, NULL, lector, NULL) == 0);

    /* Esperar a que ambos lectores entren */
    assert(sem_wait(&lectores_listos) == 0);
    assert(sem_wait(&lectores_listos) == 0);

    assert(lectores_activos == 2);

    /* Iniciar escritor */
    assert(pthread_create(&hilo_escritor, NULL,
                          escritor, NULL) == 0);

    /* Permitir la salida de los lectores */
    sem_post(&permitir_salida);
    sem_post(&permitir_salida);

    assert(pthread_join(lector1, NULL) == 0);
    assert(pthread_join(lector2, NULL) == 0);
    assert(pthread_join(hilo_escritor, NULL) == 0);

    /* Comprobar escritura */
    assert(lectores_activos == 0);
    assert(dato == 10);

    /* Liberar recursos */
    sem_destroy(&lectores_listos);
    sem_destroy(&permitir_salida);

    destruir_rwlock(&candado);
    pthread_mutex_destroy(&mutex_prueba);

    printf("OK: test_rwlock\n");

    return 0;
}

