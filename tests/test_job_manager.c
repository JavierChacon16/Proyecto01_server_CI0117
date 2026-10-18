
#include "job_manager.h"
#include "queue.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Inicializar gestor y cola */
    assert(inicializar_trabajos(2) == 0);
    assert(inicializar_cola(2) == 0);

    job_t primero = {
        .nx = 12,
        .ny = 12,
        .pasos = 10,
        .hilos = 2,
        .k = 4,
        .sx = 5,
        .sy = 5
    };

    job_t segundo = primero;
    job_t tercero = primero;

    datos_trabajo_t datos;
    job_t salida;

    /* Registrar trabajos */
    assert(enviar_trabajo(&primero) == 0);
    assert(enviar_trabajo(&segundo) == 0);

    assert(primero.id == 1);
    assert(segundo.id == 2);

    /* Comprobar cola llena */
    assert(enviar_trabajo(&tercero) == -1);

    /* Consultar estado inicial */
    assert(consultar_estado(primero.id, &datos) == 0);

    assert(datos.estado == JOB_QUEUED);
    assert(datos.pasos_total == 10);
    assert(datos.paso_actual == 0);

    /* Consultar trabajo inexistente */
    assert(consultar_estado(999, &datos) == -1);

    /* Actualizar estado y progreso */
    actualizar_estado(primero.id, JOB_RUNNING);
    actualizar_progreso(primero.id, 6);
    actualizar_tiempo(primero.id, 0.25);

    assert(consultar_estado(primero.id, &datos) == 0);

    assert(datos.estado == JOB_RUNNING);
    assert(datos.paso_actual == 6);
    assert(datos.segundos == 0.25);

    /* Guardar resultado final */
    actualizar_resultado(primero.id, 3.5, "resultado.bin");
    actualizar_estado(primero.id, JOB_DONE);

    assert(consultar_resultado(primero.id, &datos) == 0);

    assert(datos.estado == JOB_DONE);
    assert(datos.resultado == 3.5);
    assert(strcmp(datos.archivo, "resultado.bin") == 0);

    /* Liberar espacio y registrar otro trabajo */
    assert(sacar_cola(&salida) == 0);
    assert(salida.id == primero.id);

    assert(enviar_trabajo(&tercero) == 0);
    assert(tercero.id == 3);

    assert(consultar_estado(tercero.id, &datos) == 0);
    assert(datos.estado == JOB_QUEUED);

    /* Liberar recursos */
    cerrar_cola();
    destruir_cola();
    destruir_trabajos();

    printf("OK: test_job_manager\n");

    return 0;
}

