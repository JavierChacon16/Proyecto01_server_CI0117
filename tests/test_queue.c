
#include "queue.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

typedef struct {
    int resultado;
} datos_consumidor_t;

/* Consumidor que espera por un trabajo */
static void *consumidor(void *argumento)
{
    datos_consumidor_t *datos = argumento;
    job_t trabajo;

    datos->resultado = sacar_cola(&trabajo);

    return NULL;
}

int main(void)
{
    /* Crear cola con capacidad de 2 trabajos */
    assert(inicializar_cola(2) == 0);

    job_t primero = {.id = 1};
    job_t segundo = {.id = 2};
    job_t tercero = {.id = 3};
    job_t salida;

    /* Comprobar capacidad de la cola */
    assert(agregar_cola(&primero) == 0);
    assert(agregar_cola(&segundo) == 0);
    assert(agregar_cola(&tercero) == -1);

    /* Comprobar posiciones */
    assert(posicion_cola(1) == 1);
    assert(posicion_cola(2) == 2);
    assert(posicion_cola(3) == -1);

    /* Comprobar orden FIFO */
    assert(sacar_cola(&salida) == 0);
    assert(salida.id == 1);

    assert(agregar_cola(&tercero) == 0);

    assert(sacar_cola(&salida) == 0);
    assert(salida.id == 2);

    assert(sacar_cola(&salida) == 0);
    assert(salida.id == 3);

    /* Comprobar cierre de la cola */
    datos_consumidor_t datos = {.resultado = 0};
    pthread_t hilo;

    assert(pthread_create(&hilo, NULL,
                          consumidor, &datos) == 0);

    cerrar_cola();

    assert(pthread_join(hilo, NULL) == 0);
    assert(datos.resultado == -1);

    /* No permite nuevos trabajos despues del cierre */
    assert(agregar_cola(&primero) == -2);

    destruir_cola();

    printf("OK: test_queue\n");

    return 0;
}

