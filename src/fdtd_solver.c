#define _POSIX_C_SOURCE 200809L

#include "fdtd_solver.h"
#include "barrier.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*datos dados*/
#define FDTD_PI 3.14159265358979323846264338327950288
#define SC 0.70710678118654752440084436210484904
#define ETA0 376.730313668
#define ER 1.0
#define PPW 20.0

typedef struct {//struct con los datos compartidos entre los hilos
    fdtd_grid_t *grid;
    int pasos;
    int hilos;
    int sx;
    int sy;
    int k;
    step_cb_t step_cb;
    snapshot_cb_t snapshot_cb;
    void *callback_ctx;
    barrier_t barrier;
    pthread_mutex_t start_mutex;
    pthread_cond_t start_cond;
    int start_ready;
    int abort_start;
    int timing;              /* 1 si el llamador pidio stats */
} run_shared_t;

typedef struct {//struct con los datos privados de cada hilo
    run_shared_t *shared;
    int id;
    int row_begin;
    int row_end;
    /* Acumuladores PRIVADOS del hilo;
       se leen solo despues del join. */
    double t_compute;
    double t_barrier;
    double t_callback;//es el tiempo que tarda el hilo 0 en ejecutar los callbacks
} worker_arg_t;

static double now_s(void)//retorna el tiempo actual en segundos desde un punto fijo en el pasado 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static double ricker(int q)/*calcula la fuente de excitacion */
{
    double x = FDTD_PI * ((SC * (double)q / PPW) - 1.0);
    double arg = x * x;
    return (1.0 - 2.0 * arg) * exp(-arg);
}

static int valid_grid(const fdtd_grid_t *grid)
{
    return grid != NULL && grid->nx >= 3 && grid->ny >= 3 &&
           grid->Ez != NULL && grid->Hx != NULL && grid->Hy != NULL;
}

int fdtd_init(fdtd_grid_t *grid, int nx, int ny)
{
    size_t count_ez, count_hx, count_hy;

    if (grid == NULL || nx < 3 || ny < 3) {
        return -1;
    }
    if ((size_t)nx > SIZE_MAX / (size_t)ny ||/*empezamos a verificar que los tamaños de las matrices no excedan el límite de tamaño */
        (size_t)nx > SIZE_MAX / (size_t)(ny - 1) ||
        (size_t)(nx - 1) > SIZE_MAX / (size_t)ny) {
        return -1;
    }

    count_ez = (size_t)nx * (size_t)ny;//calculamos el tamaño de cada matriz
    count_hx = (size_t)nx * (size_t)(ny - 1);
    count_hy = (size_t)(nx - 1) * (size_t)ny;
    if (count_ez > SIZE_MAX / sizeof(double) ||
        count_hx > SIZE_MAX / sizeof(double) ||
        count_hy > SIZE_MAX / sizeof(double)) {
        return -1;
    }

    grid->nx = nx;//grid lo que hace es almacenar los tamaños de las matrices y reservar memoria para cada una de ellas
    grid->ny = ny;
    grid->Ez = calloc(count_ez, sizeof(double));
    grid->Hx = calloc(count_hx, sizeof(double));
    grid->Hy = calloc(count_hy, sizeof(double));
    if (grid->Ez == NULL || grid->Hx == NULL || grid->Hy == NULL) {//por si no se podemos reservar memoria p
        fdtd_free(grid);
        return -1;
    }
    return 0;
}
/*Liberamos la memoria reservada para las matrices*/
void fdtd_free(fdtd_grid_t *grid)
{
    if (grid == NULL) {
        return;
    }
    free(grid->Ez);
    free(grid->Hx);
    free(grid->Hy);
    grid->Ez = NULL;
    grid->Hx = NULL;
    grid->Hy = NULL;
    grid->nx = 0;
    grid->ny = 0;
}

static void update_h(run_shared_t *s, int row_begin, int row_end)
{
    fdtd_grid_t *g = s->grid;
    int m, n;
    /*Aqui actualizamos los valores de Hx y Hy, que son las componentes del campo magnético, a partir de los valores de Ez, 
    */
    for (m = row_begin; m < row_end; m++) {
        size_t ez_row = (size_t)m * (size_t)g->ny;
        size_t hx_row = (size_t)m * (size_t)(g->ny - 1);
        for (n = 0; n < g->ny - 1; n++) {
            g->Hx[hx_row + (size_t)n] -= (SC / ETA0) *
                (g->Ez[ez_row + (size_t)n + 1] - g->Ez[ez_row + (size_t)n]);
        }
    }

    /* Hy tiene nx-1 filas, la última fila de Ez no tiene fila Hy asociada. */
    for (m = row_begin; m < row_end && m < g->nx - 1; m++) {
        size_t ez_row = (size_t)m * (size_t)g->ny;
        size_t hy_row = (size_t)m * (size_t)g->ny;
        for (n = 0; n < g->ny; n++) {
            g->Hy[hy_row + (size_t)n] += (SC / ETA0) *
                (g->Ez[ez_row + (size_t)g->ny + (size_t)n] -
                 g->Ez[ez_row + (size_t)n]);
        }
    }
}
/*
    Aqui actualizamos los valores de Ez, campo eléctrico, a partir de los valores de Hx y Hy
*/
static void update_e(run_shared_t *s, int row_begin, int row_end, int q)
{
    fdtd_grid_t *g = s->grid;
    int m, n;
    int first = row_begin < 1 ? 1 : row_begin;
    int last = row_end > g->nx - 1 ? g->nx - 1 : row_end;

    for (m = first; m < last; m++) {
        size_t ez_row = (size_t)m * (size_t)g->ny;
        size_t hy_row = (size_t)m * (size_t)g->ny;
        size_t hy_prev_row = (size_t)(m - 1) * (size_t)g->ny;
        size_t hx_row = (size_t)m * (size_t)(g->ny - 1);
        for (n = 1; n < g->ny - 1; n++) {
            double curl = (g->Hy[hy_row + (size_t)n] -
                           g->Hy[hy_prev_row + (size_t)n]) -
                          (g->Hx[hx_row + (size_t)n] -
                           g->Hx[hx_row + (size_t)n - 1]);
            g->Ez[ez_row + (size_t)n] += (SC * ETA0 / ER) * curl;
        }
    }

    /* Solo el hilo dueño de la fila sx aplica la fuente; no hay carrera. */
    if (s->sx >= row_begin && s->sx < row_end) {
        g->Ez[(size_t)s->sx * (size_t)g->ny + (size_t)s->sy] += ricker(q);
    }
}

/*Hilo trabajador, actualiza los campos electromagnéticos en paralelo 
hay que tener cuidado con las barreras, ya que todos los hilos deben esperar a que todos terminen de actualizar H antes de actualizar E, y viceversa.

*/
static void *worker_main(void *ptr)
{
    worker_arg_t *arg = ptr;
    run_shared_t *s = arg->shared;
    int paso;

    (void)pthread_mutex_lock(&s->start_mutex);
    while (!s->start_ready) {
        (void)pthread_cond_wait(&s->start_cond, &s->start_mutex);
    }
    if (s->abort_start) {//si el hilo 0 no pudo crear todos los hilos, no seguirá ejecutando 
        (void)pthread_mutex_unlock(&s->start_mutex);
        return NULL;
    }
    (void)pthread_mutex_unlock(&s->start_mutex);

    const int timing = s->timing;
    double tm = timing ? now_s() : 0.0, tn;
/* Suma el intervalo desde la ultima marca a `acc` (no hace nada sin stats). */
#define LAP(acc) do { if (timing) { tn = now_s(); (acc) += tn - tm; tm = tn; } } while (0)

    for (paso = 1; paso <= s->pasos; paso++) {
        update_h(s, arg->row_begin, arg->row_end);
        LAP(arg->t_compute);
#ifndef SIN_BARRERA
        barrier_wait(&s->barrier);
#endif
        LAP(arg->t_barrier);
        update_e(s, arg->row_begin, arg->row_end, paso - 1); /* q = 0..pasos-1 */
        LAP(arg->t_compute);
#ifndef SIN_BARRERA
        barrier_wait(&s->barrier);
#endif
        LAP(arg->t_barrier);
        /* En este punto todos terminaron Ez y la fuente de este paso.
           Mientras los callbacks leen Ez, la siguiente fase H solo lo lee y
           el hilo 0 no llega a la proxima barrera hasta que retornan.
           Orden: instantanea (PEEK) antes que progreso (STATUS). */
        if (arg->id == 0) {
            if (s->snapshot_cb != NULL && s->k > 0 && paso % s->k == 0) {
                s->snapshot_cb(s->grid->Ez, s->grid->nx, s->grid->ny,
                               paso, s->callback_ctx);
            }
            if (s->step_cb != NULL) {
                s->step_cb(paso, s->pasos, s->callback_ctx);
            }
            LAP(arg->t_callback);
        }
    }
#undef LAP
    return NULL;
}
/*Solver en paralelo, se encarga de inicializar los hilos y coordinar la ejecución */
int fdtd_run(fdtd_grid_t *grid, int pasos, int hilos,
             int sx, int sy, int k,
             step_cb_t step_cb, snapshot_cb_t snapshot_cb, void *ctx,
             fdtd_stats_t *stats_out)
{
    run_shared_t shared;
    pthread_t *threads = NULL;
    worker_arg_t *args = NULL;
    int created = 0;
    int rc = -1;
    int join_failed = 0;
    int i;

    if (!valid_grid(grid) || pasos < 0 || hilos < 1 || hilos > grid->nx - 2 ||//validamos los parametros de entrada, si son invalidos retornamos -1
        k < 0 ||
        sx < 1 || sx >= grid->nx - 1 || sy < 1 || sy >= grid->ny - 1) {
        return -1;
    }
    if ((size_t)hilos > SIZE_MAX / sizeof(*threads) ||
        (size_t)hilos > SIZE_MAX / sizeof(*args)) {
        return -1;
    }
/*Inicializamos la estructura que almacena los datos compartidos */
    memset(&shared, 0, sizeof(shared));
    shared.grid = grid;
    shared.pasos = pasos;
    shared.hilos = hilos;
    shared.sx = sx;
    shared.sy = sy;
    shared.k = k;
    shared.step_cb = step_cb;
    shared.snapshot_cb = snapshot_cb;
    shared.callback_ctx = ctx;
    shared.timing = stats_out != NULL;
    if (stats_out != NULL) {
        memset(stats_out, 0, sizeof(*stats_out));
    }

    if (pthread_mutex_init(&shared.start_mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&shared.start_cond, NULL) != 0) {
        (void)pthread_mutex_destroy(&shared.start_mutex);
        return -1;
    }
#ifndef SIN_BARRERA
    if (barrier_init(&shared.barrier, hilos) != 0) {
        (void)pthread_cond_destroy(&shared.start_cond);
        (void)pthread_mutex_destroy(&shared.start_mutex);
        return -1;
    }
#endif

    threads = calloc((size_t)hilos, sizeof(*threads));
    args = calloc((size_t)hilos, sizeof(*args));
    if (threads == NULL || args == NULL) {
        goto cleanup;
    }

    for (i = 0; i < hilos; i++) {
        args[i].shared = &shared;
        args[i].id = i;
        args[i].row_begin = (int)(((long long)grid->nx * i) / hilos);
        args[i].row_end = (int)(((long long)grid->nx * (i + 1)) / hilos);
        /*Inicializamos los argumentos para el hilo trabajador */
        if (pthread_create(&threads[i], NULL, worker_main, &args[i]) != 0) {
            goto start_abort;
        }
        created++;
    }

    (void)pthread_mutex_lock(&shared.start_mutex);
    shared.start_ready = 1;
    (void)pthread_cond_broadcast(&shared.start_cond);
    (void)pthread_mutex_unlock(&shared.start_mutex);

    for (i = 0; i < created; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            join_failed = 1;
        }
    }
    if (created == hilos && !join_failed) {
        rc = 0;
        if (stats_out != NULL) {
            for (i = 0; i < hilos; i++) {
                stats_out->compute_s += args[i].t_compute;
                stats_out->barrier_s += args[i].t_barrier;
            }
            stats_out->compute_s /= (double)hilos;
            stats_out->barrier_s /= (double)hilos;
            stats_out->callback_s = args[0].t_callback;
        }
    }
    goto cleanup;

start_abort://si no se pudieron crear todos los hilos, abortamos la ejecución
    (void)pthread_mutex_lock(&shared.start_mutex);
    shared.abort_start = 1;
    shared.start_ready = 1;
    (void)pthread_cond_broadcast(&shared.start_cond);
    (void)pthread_mutex_unlock(&shared.start_mutex);
    for (i = 0; i < created; i++) {
        (void)pthread_join(threads[i], NULL);
    }

cleanup:
    free(args);
    free(threads);
#ifndef SIN_BARRERA
    barrier_destroy(&shared.barrier);
#endif
    (void)pthread_cond_destroy(&shared.start_cond);
    (void)pthread_mutex_destroy(&shared.start_mutex);
    return rc;
}
/*Aqui calculamos el resultado final del solver FDTD */
double fdtd_result(const fdtd_grid_t *grid)
{
    double sum = 0.0;
    size_t count, i;

    if (!valid_grid(grid)) {
        return NAN;
    }
    count = (size_t)grid->nx * (size_t)grid->ny;
    for (i = 0; i < count; i++) {
        sum += fabs(grid->Ez[i]);
    }
    return sum;
}
