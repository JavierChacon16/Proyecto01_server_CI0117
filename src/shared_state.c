#include "shared_state.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static double *valores_ez = NULL;
static int filas = 0;
static int columnas = 0;
static int paso_publicado = 0;
static int hay_datos = 0;

static rwlock_t candado;
static pthread_once_t una_vez = PTHREAD_ONCE_INIT;

/* Inicializar el candado una sola vez */
static void preparar_candado(void)
{
    inicializar_rwlock(&candado);
}

/* Preparar la matriz de una simulacion */
int inicializar_estado_compartido(int nx, int ny)
{
    if (nx <= 0 || ny <= 0)
        return -1;

    if ((size_t)nx > SIZE_MAX / (size_t)ny)
        return -1;

    size_t total = (size_t)nx * (size_t)ny;

    if (total > SIZE_MAX / sizeof(double))
        return -1;

    pthread_once(&una_vez, preparar_candado);

    double *nuevos = calloc(total, sizeof(double));

    if (nuevos == NULL)
        return -1;

    bloquear_escritura(&candado);

    free(valores_ez);
    valores_ez = nuevos;

    filas = nx;
    columnas = ny;
    paso_publicado = 0;
    hay_datos = 0;

    desbloquear_rwlock(&candado);

    return 0;
}

/* Publicar una copia de Ez */
void publicar_estado(int paso, const double *ez, int n)
{
    if (ez == NULL)
        return;

    pthread_once(&una_vez, preparar_candado);

    bloquear_escritura(&candado);

    if (valores_ez != NULL &&
        (size_t)n == (size_t)filas * (size_t)columnas) {

        memcpy(valores_ez, ez,
               (size_t)n * sizeof(double));

        paso_publicado = paso;
        hay_datos = 1;
    }

    desbloquear_rwlock(&candado);
}

/* Consultar una celda para PEEK */
int consultar_valor(int i, int j, int *paso, double *valor)
{
    if (paso == NULL || valor == NULL)
        return -1;

    pthread_once(&una_vez, preparar_candado);

    bloquear_lectura(&candado);

    if (valores_ez == NULL || !hay_datos ||
        i < 0 || i >= filas ||
        j < 0 || j >= columnas) {

        desbloquear_rwlock(&candado);
        return -1;
    }

    *paso = paso_publicado;
    *valor = valores_ez[(size_t)i * columnas + j];

    desbloquear_rwlock(&candado);

    return 0;
}

/* Liberar recursos durante el apagado */
void destruir_estado_compartido(void)
{
    pthread_once(&una_vez, preparar_candado);

    bloquear_escritura(&candado);

    free(valores_ez);
    valores_ez = NULL;

    filas = 0;
    columnas = 0;
    paso_publicado = 0;
    hay_datos = 0;

    desbloquear_rwlock(&candado);
}
