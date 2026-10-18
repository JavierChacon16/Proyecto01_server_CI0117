
#include "dispatcher.h"
#include "queue.h"
#include "job_manager.h"
#include "fdtd_solver.h"
#include "shared_state.h"
#include "snapshot.h"

#include <pthread.h>
#include <time.h>
#include <stdio.h>

typedef struct {
    unsigned long id;
    int error_instantanea;
} contexto_trabajo_t;

static pthread_t hilo_despachador;
static int despachador_iniciado = 0;

/* Actualizar el progreso de la simulacion */
static void informar_progreso(int paso, int total, void *contexto)
{
    (void)total;

    contexto_trabajo_t *datos = contexto;
    actualizar_progreso(datos->id, paso);
}

/* Publicar la matriz Ez y guardar la instantanea */
static void informar_instantanea(const double *ez,
                                int nx, int ny,
                                int paso,
                                void *contexto)
{
    contexto_trabajo_t *datos = contexto;

    publicar_estado(paso, ez, nx * ny);

    if (agregar_instantanea(datos->id, paso, ez, nx, ny) != 0)
        datos->error_instantanea = 1;
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
                trabajo.id, trabajo.nx, trabajo.ny) != 0) {
            actualizar_estado(trabajo.id, JOB_ERROR);
            continue;
        }

        if (fdtd_init(&malla, trabajo.nx, trabajo.ny) != 0) {
            actualizar_estado(trabajo.id, JOB_ERROR);
            continue;
        }

        actualizar_estado(trabajo.id, JOB_RUNNING);

        contexto_trabajo_t contexto = {trabajo.id, 0};

        struct timespec inicio_tiempo;
        struct timespec fin_tiempo;

        clock_gettime(CLOCK_MONOTONIC, &inicio_tiempo);

        int resultado = fdtd_run(
            &malla,
            trabajo.pasos,
            trabajo.hilos,
            trabajo.sx,
            trabajo.sy,
            trabajo.k,
            informar_progreso,
            informar_instantanea,
            &contexto,
            NULL
        );

        clock_gettime(CLOCK_MONOTONIC, &fin_tiempo);

        double segundos =
            (double)(fin_tiempo.tv_sec - inicio_tiempo.tv_sec) +
            (double)(fin_tiempo.tv_nsec - inicio_tiempo.tv_nsec)
            / 1000000000.0;

        actualizar_tiempo(trabajo.id, segundos);

        if (resultado == 0 && !contexto.error_instantanea) {
            double suma = fdtd_result(&malla);

            /* Guardar el ultimo paso cuando k no lo incluye */
            if ((trabajo.k == 0 ||
                 trabajo.pasos % trabajo.k != 0) &&
                agregar_instantanea(
                    trabajo.id,
                    trabajo.pasos,
                    malla.Ez,
                    trabajo.nx,
                    trabajo.ny) != 0) {

                contexto.error_instantanea = 1;
            }

            if (!contexto.error_instantanea &&
                esperar_instantanea(
                    trabajo.id, trabajo.pasos) == 0) {

                char archivo[128];

                snprintf(archivo, sizeof(archivo),
                         "job%lu_step%d.bin",
                         trabajo.id, trabajo.pasos);

                actualizar_resultado(trabajo.id, suma, archivo);
                actualizar_estado(trabajo.id, JOB_DONE);

            } else {
                actualizar_estado(trabajo.id, JOB_ERROR);
            }

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

