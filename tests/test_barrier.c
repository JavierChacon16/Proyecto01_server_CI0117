#include "barrier.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define N_THREADS 8
#define N_ROUNDS 1000

static barrier_t barrier;
static int arrived[N_ROUNDS];   /* hilos que ENTRARON a la ronda r  */
static int finished[N_ROUNDS];  /* hilos que TERMINARON la ronda r  */
static int failures;
static pthread_mutex_t check_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
   - tras la barrera A, todos entraron a la ronda r;
   - al entrar a la ronda r+1, todos terminaron la ronda r (nadie se adelanta). */
static void *worker(void *arg)
{
    (void)arg;
    for (int r = 0; r < N_ROUNDS; r++) {
        (void)pthread_mutex_lock(&check_mutex);
        if (r > 0 && finished[r - 1] != N_THREADS) {
            failures++;                 /* un hilo se adelanto una ronda */
        }
        arrived[r]++;
        (void)pthread_mutex_unlock(&check_mutex);

        barrier_wait(&barrier);

        (void)pthread_mutex_lock(&check_mutex);
        if (arrived[r] != N_THREADS) {
            failures++;                 /* paso la barrera sin esperar a todos */
        }
        finished[r]++;
        (void)pthread_mutex_unlock(&check_mutex);

        barrier_wait(&barrier);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[N_THREADS];
    int created = 0;
    int rc = EXIT_SUCCESS;

    if (barrier_init(&barrier, N_THREADS) != 0) {
        fprintf(stderr, "No se pudo inicializar la barrera\n");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < N_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL) != 0) {
            fprintf(stderr, "Fallo pthread_create\n");
            rc = EXIT_FAILURE;
            break;
        }
        created++;
    }
    if (rc != EXIT_SUCCESS) {
        /* Los hilos ya creados esperarian para siempre en la barrera
           (faltan participantes): se cancelan para poder liberar recursos. */
        for (int i = 0; i < created; i++) {
            (void)pthread_cancel(threads[i]);
        }
    }
    for (int i = 0; i < created; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Fallo pthread_join\n");
            rc = EXIT_FAILURE;
        }
    }
    if (rc != EXIT_SUCCESS) {
        return rc;   /* no se destruye la barrera si hubo hilos cancelados */
    }
    barrier_destroy(&barrier);
    (void)pthread_mutex_destroy(&check_mutex);

    if (failures != 0) {
        fprintf(stderr, "Fallo la prueba: %d errores\n", failures);
        return EXIT_FAILURE;
    }
    printf("OK: barrera reutilizable, %d hilos x %d rondas\n", N_THREADS, N_ROUNDS);
    return EXIT_SUCCESS;
}
