/* Pruebas del nucleo FDTD Jorge
   Cubre: referencia serial independiente (8x8, 5 pasos) y a mano (1 paso),
   simetria, bordes PEC, filas no divisibles, validacion de hilos y callbacks.
   El determinismo bit a bit vive en test_determinism.c ( tambien para
   la deteccion de la carrera con -DSIN_BARRERA). */
#include "fdtd_solver.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_ 3.14159265358979323846
#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FALLO: %s (linea %d)\n", msg, __LINE__); return 0; } } while (0)

/* Referencia serial (escrita aparte del solver)*/
static double ref_ricker(int q)
{
    const double sc = 1.0 / sqrt(2.0);
    double a = PI_ * ((sc * q) / 20.0 - 1.0);
    double arg = a * a;
    return (1.0 - 2.0 * arg) * exp(-arg);
}

static void ref_run(int nx, int ny, int pasos, int sx, int sy,
                    double *Ez, double *Hx, double *Hy)
{
    const double sc = 1.0 / sqrt(2.0), eta0 = 376.730313668;
#define EZ(m, n) Ez[(m) * ny + (n)]
#define HX(m, n) Hx[(m) * (ny - 1) + (n)]
#define HY(m, n) Hy[(m) * ny + (n)]
    for (int q = 0; q < pasos; q++) {
        for (int m = 0; m < nx; m++)
            for (int n = 0; n < ny - 1; n++)
                HX(m, n) = HX(m, n) - (sc / eta0) * (EZ(m, n + 1) - EZ(m, n));
        for (int m = 0; m < nx - 1; m++)
            for (int n = 0; n < ny; n++)
                HY(m, n) = HY(m, n) + (sc / eta0) * (EZ(m + 1, n) - EZ(m, n));
        for (int m = 1; m < nx - 1; m++)
            for (int n = 1; n < ny - 1; n++)
                EZ(m, n) = EZ(m, n) + (sc * eta0) *
                    ((HY(m, n) - HY(m - 1, n)) - (HX(m, n) - HX(m, n - 1)));
        EZ(sx, sy) = EZ(sx, sy) + ref_ricker(q);
    }
#undef EZ
#undef HX
#undef HY
}

/* Tolerancia relativa al maximo. */
static double max_rel_diff(const double *a, const double *b, size_t n)
{
    double maxv = 0.0, maxd = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (fabs(b[i]) > maxv) maxv = fabs(b[i]);
        if (fabs(a[i] - b[i]) > maxd) maxd = fabs(a[i] - b[i]);
    }
    return maxv > 0.0 ? maxd / maxv : maxd;
}

static int test_reference(int nx, int ny, int pasos, int hilos, int sx, int sy)
{
    fdtd_grid_t g = {0};
    size_t nez = (size_t)nx * ny, nhx = (size_t)nx * (ny - 1), nhy = (size_t)(nx - 1) * ny;
    double *rez = calloc(nez, sizeof(double)), *rhx = calloc(nhx, sizeof(double)),
           *rhy = calloc(nhy, sizeof(double));
    int ok = 0;
    if (rez && rhx && rhy && fdtd_init(&g, nx, ny) == 0) {
        ref_run(nx, ny, pasos, sx, sy, rez, rhx, rhy);
        if (fdtd_run(&g, pasos, hilos, sx, sy, 0, NULL, NULL, NULL, NULL) == 0) {
            double e = max_rel_diff(g.Ez, rez, nez), hx = max_rel_diff(g.Hx, rhx, nhx),
                   hy = max_rel_diff(g.Hy, rhy, nhy);
            ok = e < 1e-12 && hx < 1e-12 && hy < 1e-12;
            if (!ok) fprintf(stderr, "ref %dx%d p=%d T=%d: dif Ez=%g Hx=%g Hy=%g\n",
                             nx, ny, pasos, hilos, e, hx, hy);
        }
    }
    fdtd_free(&g); free(rez); free(rhx); free(rhy);
    return ok;
}

/* todo en cero, el paso 1 solo deja la fuente ricker(0) en (sx,sy). */
static int test_hand_one_step(void)
{
    fdtd_grid_t g = {0};
    CHECK(fdtd_init(&g, 8, 8) == 0, "init");
    CHECK(fdtd_run(&g, 1, 1, 4, 4, 0, NULL, NULL, NULL, NULL) == 0, "run");
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            double v = g.Ez[i * 8 + j];
            if (i == 4 && j == 4) CHECK(fabs(v - ref_ricker(0)) <= 1e-12 * fabs(ref_ricker(0)), "Ez fuente = ricker(0)");
            else CHECK(v == 0.0, "Ez resto = 0 tras 1 paso");
        }
    for (int i = 0; i < 8 * 7; i++) CHECK(g.Hx[i] == 0.0 && g.Hy[i] == 0.0, "H = 0 tras 1 paso");
    fdtd_free(&g);
    return 1;
}

static int test_symmetry(int threads)
{
    const int n = 61, steps = 120, c = 30;
    fdtd_grid_t g = {0};
    double maxv = 0.0, d1 = 0.0, d2 = 0.0, d3 = 0.0;
    CHECK(fdtd_init(&g, n, n) == 0, "init");
    CHECK(fdtd_run(&g, steps, threads, c, c, 0, NULL, NULL, NULL, NULL) == 0, "run");
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double v = g.Ez[i * n + j];
            if (fabs(v) > maxv) maxv = fabs(v);
            double a = fabs(v - g.Ez[(n - 1 - i) * n + j]);   /* eje horizontal */
            double b = fabs(v - g.Ez[i * n + (n - 1 - j)]);   /* eje vertical   */
            double d = fabs(v - g.Ez[j * n + i]);             /* diagonal       */
            if (a > d1) d1 = a;
            if (b > d2) d2 = b;
            if (d > d3) d3 = d;
        }
    CHECK(maxv > 0.0, "campo no nulo");
    CHECK(d1 <= 1e-12 * maxv && d2 <= 1e-12 * maxv && d3 <= 1e-12 * maxv, "simetria");
    fdtd_free(&g);
    return 1;
}

static int test_borders(int threads)
{
    const int nx = 47, ny = 53;
    fdtd_grid_t g = {0};
    CHECK(fdtd_init(&g, nx, ny) == 0, "init");
    CHECK(fdtd_run(&g, 100, threads, 20, 25, 0, NULL, NULL, NULL, NULL) == 0, "run");
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1)
                CHECK(g.Ez[i * ny + j] == 0.0, "borde PEC != 0");
    CHECK(isfinite(fdtd_result(&g)) && fdtd_result(&g) > 0.0, "resultado finito > 0");
    fdtd_free(&g);
    return 1;
}

/* Filas no divisibles: 61/4, 61/7, 10/8 (bloques de 1-2 filas), 13/11 (= nx-2). */
static int test_uneven_rows(void)
{
    static const int cases[][2] = {{61, 4}, {61, 7}, {10, 8}, {13, 11}, {17, 3}};
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        int nx = cases[c][0], t = cases[c][1];
        CHECK(nx % t != 0, "el caso debe ser no divisible");
        CHECK(test_reference(nx, nx + 2, 40, t, nx / 2, 5), "referencia con filas no divisibles");
    }
    return 1;
}

static int test_invalid_args(void)
{
    fdtd_grid_t g = {0};
    CHECK(fdtd_init(&g, 20, 20) == 0, "init");
    CHECK(fdtd_run(&g, 5, 0, 10, 10, 0, NULL, NULL, NULL, NULL) != 0, "hilos=0 rechazado");
    CHECK(fdtd_run(&g, 5, -3, 10, 10, 0, NULL, NULL, NULL, NULL) != 0, "hilos<0 rechazado");
    CHECK(fdtd_run(&g, 5, 19, 10, 10, 0, NULL, NULL, NULL, NULL) != 0, "hilos>nx-2 rechazado");
    CHECK(fdtd_run(&g, 5, 18, 10, 10, 0, NULL, NULL, NULL, NULL) == 0, "hilos=nx-2 aceptado");
    CHECK(fdtd_run(&g, 5, 2, 0, 10, 0, NULL, NULL, NULL, NULL) != 0, "fuente en borde rechazada");
    CHECK(fdtd_run(&g, 5, 2, 10, 19, 0, NULL, NULL, NULL, NULL) != 0, "fuente en borde rechazada");
    CHECK(fdtd_run(&g, -1, 2, 10, 10, 0, NULL, NULL, NULL, NULL) != 0, "pasos<0 rechazado");
    CHECK(fdtd_run(&g, 5, 2, 10, 10, -1, NULL, NULL, NULL, NULL) != 0, "k<0 rechazado");
    fdtd_free(&g);
    CHECK(fdtd_init(&g, 2, 10) != 0, "malla < 3 rechazada");
    return 1;
}

/* ---------- Callbacks ---------- */
typedef struct {
    int steps_seen, snaps_seen, last_step, last_snap, order_ok, ez_ok;
} cb_log_t;

static void on_step(int paso, int total, void *ctx)
{
    cb_log_t *l = ctx;
    if (paso != l->steps_seen + 1 || total != 60) l->order_ok = 0;
    /* si hubo snapshot en este paso, debio llegar antes que el progreso */
    if (paso % 10 == 0 && l->last_snap != paso) l->order_ok = 0;
    l->steps_seen++; l->last_step = paso;
}

static void on_snap(const double *ez, int nx, int ny, int paso, void *ctx)
{
    cb_log_t *l = ctx;
    if (nx != 31 || ny != 31 || ez == NULL) l->ez_ok = 0;
    if (paso % 10 != 0) l->ez_ok = 0;
    l->snaps_seen++; l->last_snap = paso;
}

static int test_callbacks(int threads)
{
    fdtd_grid_t g = {0};
    cb_log_t l = {0, 0, 0, 0, 1, 1};
    CHECK(fdtd_init(&g, 31, 31) == 0, "init");
    CHECK(fdtd_run(&g, 60, threads, 15, 15, 10, on_step, on_snap, &l, NULL) == 0, "run");
    CHECK(l.steps_seen == 60 && l.last_step == 60, "step_cb en cada paso");
    CHECK(l.snaps_seen == 6 && l.last_snap == 60, "snapshot_cb cada k pasos");
    CHECK(l.order_ok && l.ez_ok, "orden y argumentos de callbacks");
    fdtd_free(&g);

    /* k=0: sin instantaneas, pero el progreso sigue llegando (caso E1/E3). */
    memset(&l, 0, sizeof l); l.order_ok = 1; l.ez_ok = 1; l.last_snap = 60;
    CHECK(fdtd_init(&g, 31, 31) == 0, "init");
    CHECK(fdtd_run(&g, 60, threads, 15, 15, 0, on_step, on_snap, &l, NULL) == 0, "run k=0");
    CHECK(l.steps_seen == 60 && l.snaps_seen == 0, "k=0: solo progreso");
    fdtd_free(&g);
    return 1;
}

/* Las estadisticas no alteran el resultado y son coherentes. */
static int test_stats(int threads)
{
    fdtd_grid_t a = {0}, b = {0};
    fdtd_stats_t st;
    cb_log_t l = {0, 0, 0, 0, 1, 1};
    size_t nez = 31u * 31u;
    CHECK(fdtd_init(&a, 31, 31) == 0 && fdtd_init(&b, 31, 31) == 0, "init");
    CHECK(fdtd_run(&a, 60, threads, 15, 15, 10, NULL, NULL, NULL, NULL) == 0, "run sin stats");
    CHECK(fdtd_run(&b, 60, threads, 15, 15, 10, on_step, on_snap, &l, &st) == 0, "run con stats");
    CHECK(memcmp(a.Ez, b.Ez, nez * sizeof(double)) == 0, "stats no cambia el resultado");
    CHECK(st.compute_s > 0.0 && st.barrier_s >= 0.0 && st.callback_s >= 0.0, "stats >= 0");
    CHECK(st.compute_s < 60.0 && st.barrier_s < 60.0, "stats en rango razonable");
    fdtd_free(&a); fdtd_free(&b);
    return 1;
}

#define RUN(name, expr) do { if (!(expr)) { fprintf(stderr, "FALLO en %s\n", name); return EXIT_FAILURE; } \
    printf("OK: %s\n", name); } while (0)

int main(void)
{
    RUN("a mano: 1 paso deja solo ricker(0) en la fuente", test_hand_one_step());
    RUN("referencia serial 8x8, 5 pasos, 1 hilo", test_reference(8, 8, 5, 1, 4, 4));
    RUN("referencia serial 8x8, 5 pasos, 4 hilos", test_reference(8, 8, 5, 4, 4, 4));
    RUN("referencia serial 8x8, 5 pasos, 6 hilos (=nx-2)", test_reference(8, 8, 5, 6, 3, 4));
    RUN("referencia serial 61x61, 120 pasos, 4 hilos", test_reference(61, 61, 120, 4, 30, 30));
    RUN("filas no divisibles (61/4, 61/7, 10/8, 13/11, 17/3)", test_uneven_rows());
    RUN("simetria (ejes y diagonal), 1 hilo", test_symmetry(1));
    RUN("simetria (ejes y diagonal), 4 hilos", test_symmetry(4));
    RUN("bordes PEC en cero (malla 47x53, 3 hilos)", test_borders(3));
    RUN("parametros invalidos rechazados", test_invalid_args());
    RUN("callbacks step/snapshot (4 hilos)", test_callbacks(4));
    RUN("estadisticas de tiempo (stats_out)", test_stats(4));
    puts("OK: pruebas del nucleo FDTD");
    return EXIT_SUCCESS;
}
