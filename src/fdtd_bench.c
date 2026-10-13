/* fdtd_bench nx ny pasos hilos k
   Simulacion sin red. Imprime UNA linea CSV (cabecera con --header):
     hilos,init_s,compute_s,barrier_wait_s,io_s,total_s
   - init_s        : fdtd_init (reserva de memoria).
   - compute_s     : promedio por hilo en update_h/update_e.
   - barrier_wait_s: promedio por hilo esperando en barreras (incluye la espera
                     por la E/S del hilo 0 y el desbalance de carga).
   - io_s          : tiempo del hilo 0 en los callbacks (escritura de snapshots).
   - total_s       : tiempo real (pared) de init + fdtd_run + resultado.
*/

#define _POSIX_C_SOURCE 200809L

#include "fdtd_solver.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Devuelve el tiempo actual en segundos para medir duraciones. */
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* Escribe una instantánea binaria temporal con dimensiones, paso y valores Ez.
    El archivo se elimina al terminar porque aquí solo se mide el costo de E/S. */
static int write_snapshot(const double *ez, int nx, int ny, int paso)
{
    const char *dir = getenv("FDTD_BENCH_DIR");
    char path[512];
    FILE *f;
    int32_t hdr[3];
    size_t n = (size_t)nx * (size_t)ny;
    int ok;

    snprintf(path, sizeof path, "%s/job0_step%d.bin", dir ? dir : "/tmp", paso);
    f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }
    hdr[0] = nx; hdr[1] = ny; hdr[2] = paso;
    ok = fwrite(hdr, sizeof hdr[0], 3, f) == 3 && fwrite(ez, sizeof *ez, n, f) == n;
    ok = (fclose(f) == 0) && ok;
    (void)remove(path);
    return ok ? 0 : -1;
}

static int io_errors;

static void on_snapshot(const double *ez, int nx, int ny, int paso, void *ctx)
{
    (void)ctx;
    /* El solver ejecuta este callback en el hilo 0; por eso el contador no requiere sincronización. */
    if (write_snapshot(ez, nx, ny, paso) != 0) {
        io_errors++;   /* solo lo toca el hilo 0 */
    }
}

int main(int argc, char **argv)
{
    fdtd_grid_t g = {0};
    fdtd_stats_t st;
    int nx, ny, pasos, hilos, k;
    double t0, t1, t2, t3;

    if (argc == 2 && strcmp(argv[1], "--header") == 0) {
        puts("hilos,init_s,compute_s,barrier_wait_s,io_s,total_s");
        return EXIT_SUCCESS;
    }
    if (argc != 6) {
        fprintf(stderr, "uso: %s nx ny pasos hilos k   (o --header)\n", argv[0]);
        return EXIT_FAILURE;
    }
    nx = atoi(argv[1]); ny = atoi(argv[2]); pasos = atoi(argv[3]);
    hilos = atoi(argv[4]); k = atoi(argv[5]);

    t0 = now_s();
    if (fdtd_init(&g, nx, ny) != 0) {
        fprintf(stderr, "parametros de malla invalidos o sin memoria\n");
        return EXIT_FAILURE;
    }
    t1 = now_s();
    if (fdtd_run(&g, pasos, hilos, nx / 2, ny / 2, k,
                 NULL, k > 0 ? on_snapshot : NULL, NULL, &st) != 0) {
        fprintf(stderr, "fdtd_run rechazo los parametros\n");
        fdtd_free(&g);
        return EXIT_FAILURE;
    }
    t2 = now_s();
    (void)fdtd_result(&g);   /* parte serial final, incluida en total_s */
    t3 = now_s();
    if (io_errors != 0) {
        fprintf(stderr, "aviso: %d snapshots no se pudieron escribir\n", io_errors);
    }
    printf("%d,%.6f,%.6f,%.6f,%.6f,%.6f\n", hilos, t1 - t0, st.compute_s,
           st.barrier_s, st.callback_s, t3 - t0);
    (void)t2;
    fdtd_free(&g);
    return EXIT_SUCCESS;
}
