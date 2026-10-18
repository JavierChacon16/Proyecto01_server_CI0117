#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond_lectores;
    pthread_cond_t cond_escritor;

    int lectores_activos;
    int escritor_activo;
    int escritores_esperando;
} rwlock_t;

/* Inicializar el candado */
void inicializar_rwlock(rwlock_t *candado);

/* Liberar los recursos del candado */
void destruir_rwlock(rwlock_t *candado);

/* Permitir la lectura */
void bloquear_lectura(rwlock_t *candado);

/* Permitir la escritura exclusiva */
void bloquear_escritura(rwlock_t *candado);

/* Liberar el acceso */
void desbloquear_rwlock(rwlock_t *candado);

#endif
