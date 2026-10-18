
#include "rwlock.h"

/* Inicializar el candado */
void inicializar_rwlock(rwlock_t *candado)
{
    pthread_mutex_init(&candado->mutex, NULL);
    pthread_cond_init(&candado->cond_lectores, NULL);
    pthread_cond_init(&candado->cond_escritor, NULL);

    candado->lectores_activos = 0;
    candado->escritor_activo = 0;
    candado->escritores_esperando = 0;
}

/* Permitir que varios lectores accedan al mismo tiempo */
void bloquear_lectura(rwlock_t *candado)
{
    pthread_mutex_lock(&candado->mutex);

    while (candado->escritor_activo ||
           candado->escritores_esperando > 0) {
        pthread_cond_wait(&candado->cond_lectores,
                          &candado->mutex);
    }

    candado->lectores_activos++;

    pthread_mutex_unlock(&candado->mutex);
}

/* Permitir que un escritor acceda de forma exclusiva */
void bloquear_escritura(rwlock_t *candado)
{
    pthread_mutex_lock(&candado->mutex);

    candado->escritores_esperando++;

    while (candado->lectores_activos > 0 ||
           candado->escritor_activo) {
        pthread_cond_wait(&candado->cond_escritor,
                          &candado->mutex);
    }

    candado->escritores_esperando--;
    candado->escritor_activo = 1;

    pthread_mutex_unlock(&candado->mutex);
}

/* Liberar el candado */
void desbloquear_rwlock(rwlock_t *candado)
{
    pthread_mutex_lock(&candado->mutex);

    if (candado->escritor_activo) {
        candado->escritor_activo = 0;
    } else {
        candado->lectores_activos--;
    }

    if (candado->escritores_esperando > 0) {
        if (candado->lectores_activos == 0 &&
            !candado->escritor_activo) {
            pthread_cond_signal(&candado->cond_escritor);
        }
    } else {
        pthread_cond_broadcast(&candado->cond_lectores);
    }

    pthread_mutex_unlock(&candado->mutex);
}

/* Liberar recursos */
void destruir_rwlock(rwlock_t *candado)
{
    pthread_mutex_destroy(&candado->mutex);
    pthread_cond_destroy(&candado->cond_lectores);
    pthread_cond_destroy(&candado->cond_escritor);
}

