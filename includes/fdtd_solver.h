#ifndef FDTD_SOLVER_H
#define FDTD_SOLVER_H

/* callbacks ambos se invocan SOLO desde el hilo 0) ----
 * El callback lo usare para reportar el progreso (step_cb) y otro para tomar instantaneas de Ez (snapshot_cb).
 * Se ejecutan despues de la barrera final del paso En ese momento los demas hilos pueden estar
 * ejecutando la fase H del paso siguiente, que solo LEE Ez; el hilo 0 no
 * llega a la siguiente barrera hasta que el callback retorna, asi que nadie
 * escribe Ez mientras el callback lo lee.
 */

/* Progreso: cada paso. Para STATUS / estado publicado. */
typedef void (*step_cb_t)(int paso, int total, void *ctx);

/* Instantanea de Ez: cada k pasos (k > 0). */
typedef void (*snapshot_cb_t)(const double *ez,
                              int nx, int ny,
                              int paso,
                              void *ctx);

typedef struct {
    int nx;
    int ny;
    double *Ez;   /* nx * ny */
    double *Hx;   /* nx * (ny-1) */
    double *Hy;   /* (nx-1) * ny */
} fdtd_grid_t;

int fdtd_init(fdtd_grid_t *grid, int nx, int ny);
void fdtd_free(fdtd_grid_t *grid);

/* Tiempos opcionales (segundos, CLOCK_MONOTONIC). Si fdtd_run recibe
   stats_out == NULL NO se lee el reloj (cero sobrecosto).
   - compute_s : promedio por hilo del tiempo en update_h + update_e.
   - barrier_s : promedio por hilo del tiempo esperando en barreras. Incluye
                 el tiempo que los demas hilos esperan mientras el hilo 0
                 ejecuta los callbacks (E/S) y el desbalance de carga.
   - callback_s: tiempo del hilo 0 dentro de snapshot_cb + step_cb.
   La inicializacion (fdtd_init) la mide quien la va a llamar, hare un test para medirlo */
typedef struct {
    double compute_s;
    double barrier_s;
    double callback_s;
} fdtd_stats_t;

/* Retorna 0 si OK, -1 si los parametros son invalidos o falla un recurso.
   Parametros validos: nx, ny >= 3; pasos >= 0; 1 <= hilos <= nx-2;
   k >= 0 (0 = sin instantaneas); 1 <= sx <= nx-2; 1 <= sy <= ny-2.
   step_cb, snapshot_cb y stats_out pueden ser NULL. Ambos callbacks reciben
   el mismo ctx. Con hilos > nx-2 algun hilo no tendria filas: se rechaza. */
int fdtd_run(fdtd_grid_t *grid,
             int pasos,
             int hilos,
             int sx, int sy,
             int k,
             step_cb_t step_cb,
             snapshot_cb_t snapshot_cb,
             void *ctx,
             fdtd_stats_t *stats_out);

/* Suma de |Ez| sobre toda la malla, calculada secuencialmente. */
double fdtd_result(const fdtd_grid_t *grid);

#endif
