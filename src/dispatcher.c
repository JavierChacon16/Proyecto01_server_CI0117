#include "dispatcher.h"
#include "queue.h"
#include "job_manager.h"
#include "fdtd_solver.h"
#include "shared_state.h"
#include "snapshot.h"

#include <pthread.h>

static pthread_t hilo_despachador;
static int despachador_iniciado = 0;

/* Actualizar el progreso de la simulacion */
static void informar_progreso(int paso, int total, void *contexto)
{
    (void)total;

    unsigned long id = *(unsigned long *)contexto;
    actualizar_progreso(id, paso);
}

/* Publicar la matriz Ez y guardar la instantanea */
static void informar_instantanea(const double *ez,
                                int nx, int ny,
                                int paso,
                                void *contexto)
{
    unsigned long id = *(unsigned long *)contexto;

    publicar_estado(paso, ez, nx * ny);
    agregar_instantanea(id, paso, ez, nx, ny);
}

/* Ejecutar los trabajos pendientes */
static void *ejecutar_trabajos(void *argumento)
{
    (void)argumento;

    job_t trabajo;

    while (sacar_cola(&trabajo) == 0) {

        fdtd_grid_t malla;

        /* Preparar el estado para este trabajo */
        if (inicializar_estado_compartido(
                trabajo.nx, trabajo.ny) != 0) {
            actualizar_estado(trabajo.id, JOB_ERROR);
            continue;
        }

        if (fdtd_init(&malla, trabajo.nx, trabajo.ny) != 0) {
            actualizar_estado(trabajo.id, JOB_ERROR);
            continue;
        }

        actualizar_estado(trabajo.id, JOB_RUNNING);

        int resultado = fdtd_run(
            &malla,
            trabajo.pasos,
            trabajo.hilos,
            trabajo.sx,
            trabajo.sy,
            trabajo.k,
            informar_progreso,
            informar_instantanea,
            &trabajo.id,
            NULL
        );

        if (resultado == 0) {
            double suma = fdtd_result(&malla);

            actualizar_resultado(trabajo.id, suma, NULL);
            actualizar_estado(trabajo.id, JOB_DONE);
        } else {
            actualizar_estado(trabajo.id, JOB_ERROR);
        }

        fdtd_free(&malla);
    }

    return NULL;
}

/* Iniciar el hilo despachador */
int iniciar_despachador(void)
{
    if (despachador_iniciado)
        return -1;

    if (pthread_create(&hilo_despachador,
                       NULL,
                       ejecutar_trabajos,
                       NULL) != 0) {
        return -1;
    }

    despachador_iniciado = 1;
    return 0;
}

/* Cerrar la cola y esperar al despachador */
void detener_despachador(void)
{
    if (!despachador_iniciado)
        return;

    cerrar_cola();

    pthread_join(hilo_despachador, NULL);

    despachador_iniciado = 0;
}
