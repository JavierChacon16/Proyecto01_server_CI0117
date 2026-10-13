/* Determinismo bit a bit: T = 1, 2, 3, 4, 8 hilos contra la corrida de 1 hilo.
   Uso: test_determinism [nx] [pasos]
   Con -DSIN_BARRERA debe FALLAR en al menos una de varias ejecuciones. */
#include "fdtd_solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_grid(fdtd_grid_t *g, int nx, int steps, int t)
{
    return fdtd_init(g, nx, nx) == 0 &&
           fdtd_run(g, steps, t, nx / 2, nx / 2, 10, NULL, NULL, NULL, NULL) == 0;
}

int main(int argc, char **argv)
{
    const int thread_counts[] = {2, 3, 4, 8};
    int nx = argc > 1 ? atoi(argv[1]) : 61;
    int steps = argc > 2 ? atoi(argv[2]) : 120;
    fdtd_grid_t ref = {0};
    size_t nez = (size_t)nx * (size_t)nx, nhx = (size_t)nx * (size_t)(nx - 1);
    int bad = 0;

    if (nx < 10 || steps < 1 || !run_grid(&ref, nx, steps, 1)) {
        fprintf(stderr, "No se pudo ejecutar la referencia de 1 hilo\n");
        fdtd_free(&ref);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof thread_counts / sizeof thread_counts[0]; i++) {
        fdtd_grid_t g = {0};
        int t = thread_counts[i];
        if (!run_grid(&g, nx, steps, t)) {
            fprintf(stderr, "Fallo la ejecucion con %d hilos\n", t);
            fdtd_free(&g); fdtd_free(&ref);
            return EXIT_FAILURE;
        }
        if (memcmp(g.Ez, ref.Ez, nez * sizeof(double)) != 0 ||
            memcmp(g.Hx, ref.Hx, nhx * sizeof(double)) != 0 ||
            memcmp(g.Hy, ref.Hy, nhx * sizeof(double)) != 0) {
            fprintf(stderr, "DIFERENCIA bit a bit con %d hilos (nx=%d, pasos=%d)\n", t, nx, steps);
            bad = 1;
        } else {
            printf("OK: %d hilo(s) identico a 1 hilo; suma=%.10e\n", t, fdtd_result(&g));
        }
        fdtd_free(&g);
    }
    fdtd_free(&ref);
    return bad ? EXIT_FAILURE : EXIT_SUCCESS;
}
