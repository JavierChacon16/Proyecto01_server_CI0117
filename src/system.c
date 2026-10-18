
#include "system.h"
#include "dispatcher.h"
#include "queue.h"
#include "job_manager.h"
#include "shared_state.h"
#include "snapshot.h"

#include <pthread.h>

static pthread_mutex_t mutex_apagado = PTHREAD_MUTEX_INITIALIZER;
static int sistema_apagado = 0;

/* Realizar el apagado ordenado del sistema */
void apagar_sistema(void)
{
    pthread_mutex_lock(&mutex_apagado);

    if (sistema_apagado) {
        pthread_mutex_unlock(&mutex_apagado);
        return;
    }

    sistema_apagado = 1;

    pthread_mutex_unlock(&mutex_apagado);

    /* Cerrar la cola y esperar al despachador */
    cerrar_cola();
    detener_despachador();

    /* Terminar de guardar las instantaneas */
    cerrar_instantaneas();
    detener_escritor();

    /* Liberar recursos */
    destruir_instantaneas();
    destruir_estado_compartido();
    destruir_trabajos();
    destruir_cola();
}
