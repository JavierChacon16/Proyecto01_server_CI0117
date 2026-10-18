#include "job_manager.h"
#include <limits.h>
#include "queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static datos_trabajo_t *trabajos = NULL;
static int cantidad = 0;
static int capacidad = 0;
static int inicializado = 0;
static unsigned long siguiente_id = 1;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Buscar un trabajo por su identificador */
static int buscar_trabajo(unsigned long id)
{
    for (int i = 0; i < cantidad; i++) {
        if (trabajos[i].id == id)
            return i;
    }

    return -1;
}

/* Inicializar el administrador de trabajos */
int inicializar_trabajos(int capacidad_inicial)
{
    if (capacidad_inicial <= 0)
        return -1;

    pthread_mutex_lock(&mutex);

    if (inicializado) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    trabajos = calloc((size_t)capacidad_inicial,
                     sizeof(datos_trabajo_t));

    if (trabajos == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    capacidad = capacidad_inicial;
    cantidad = 0;
    siguiente_id = 1;
    inicializado = 1;

    pthread_mutex_unlock(&mutex);
    return 0;
}

/* Registrar un nuevo trabajo */
int registrar_trabajo(const job_t *trabajo)
{
    if (trabajo == NULL)
        return -2;

    pthread_mutex_lock(&mutex);

    if (!inicializado) {
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    if (buscar_trabajo(trabajo->id) != -1) {
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    if (cantidad == capacidad) {
        int nueva_capacidad = capacidad * 2;

        if (nueva_capacidad <= capacidad) {
            pthread_mutex_unlock(&mutex);
            return -2;
        }

        datos_trabajo_t *nuevos = realloc(
            trabajos,
            (size_t)nueva_capacidad * sizeof(datos_trabajo_t)
        );

        if (nuevos == NULL) {
            pthread_mutex_unlock(&mutex);
            return -2;
        }

        trabajos = nuevos;
        capacidad = nueva_capacidad;
    }

    datos_trabajo_t *nuevo = &trabajos[cantidad];

    memset(nuevo, 0, sizeof(datos_trabajo_t));

    nuevo->id = trabajo->id;
    nuevo->estado = JOB_QUEUED;
    nuevo->paso_actual = 0;
    nuevo->pasos_total = trabajo->pasos;

    cantidad++;

    pthread_mutex_unlock(&mutex);
    return 0;
}


/* Registrar y agregar una simulacion a la cola */
int enviar_trabajo(job_t *trabajo)
{
    if (trabajo == NULL)
        return -2;

    pthread_mutex_lock(&mutex);

    if (!inicializado || siguiente_id > INT_MAX) {
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    /* Preparar espacio para registrar el trabajo */
    if (cantidad == capacidad) {
        int nueva_capacidad = capacidad * 2;

        if (nueva_capacidad <= capacidad) {
            pthread_mutex_unlock(&mutex);
            return -2;
        }

        datos_trabajo_t *nuevos = realloc(
            trabajos,
            (size_t)nueva_capacidad * sizeof(datos_trabajo_t)
        );

        if (nuevos == NULL) {
            pthread_mutex_unlock(&mutex);
            return -2;
        }

        trabajos = nuevos;
        capacidad = nueva_capacidad;
    }

    trabajo->id = siguiente_id;

    /* Agregar a la cola sin bloquear */
    int resultado = agregar_cola(trabajo);

    if (resultado != 0) {
        pthread_mutex_unlock(&mutex);
        return resultado;
    }

    /* Registrar el trabajo aceptado */
    datos_trabajo_t *nuevo = &trabajos[cantidad];

    memset(nuevo, 0, sizeof(datos_trabajo_t));

    nuevo->id = trabajo->id;
    nuevo->estado = JOB_QUEUED;
    nuevo->paso_actual = 0;
    nuevo->pasos_total = trabajo->pasos;

    cantidad++;
    siguiente_id++;

    pthread_mutex_unlock(&mutex);

    return 0;
}


/* Actualizar el estado de un trabajo */
void actualizar_estado(unsigned long id, job_state_t estado)
{
    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion != -1)
        trabajos[posicion].estado = estado;

    pthread_mutex_unlock(&mutex);
}

/* Guardar el tiempo de ejecucion del trabajo */
void actualizar_tiempo(unsigned long id, double segundos)
{
    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion != -1) {
        trabajos[posicion].segundos = segundos;
    }

    pthread_mutex_unlock(&mutex);
}

/* Actualizar el progreso */
void actualizar_progreso(unsigned long id, int paso)
{
    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion != -1)
        trabajos[posicion].paso_actual = paso;

    pthread_mutex_unlock(&mutex);
}

/* Guardar el resultado final */
void actualizar_resultado(unsigned long id,
                          double resultado,
                          const char *archivo)
{
    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion != -1) {
        trabajos[posicion].resultado = resultado;

        if (archivo != NULL) {
            strncpy(trabajos[posicion].archivo,
                    archivo,
                    sizeof(trabajos[posicion].archivo) - 1);

            trabajos[posicion].archivo[
                sizeof(trabajos[posicion].archivo) - 1
            ] = '\0';
        }
    }

    pthread_mutex_unlock(&mutex);
}

/* Consultar el estado de un trabajo */
int consultar_estado(unsigned long id, datos_trabajo_t *salida)
{
    if (salida == NULL)
        return -1;

    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion == -1) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    *salida = trabajos[posicion];

    pthread_mutex_unlock(&mutex);
    return 0;
}

/* Consultar el resultado de un trabajo */
int consultar_resultado(unsigned long id, datos_trabajo_t *salida)
{
    if (salida == NULL)
        return -1;

    pthread_mutex_lock(&mutex);

    int posicion = buscar_trabajo(id);

    if (posicion == -1) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    *salida = trabajos[posicion];

    pthread_mutex_unlock(&mutex);
    return 0;
}

/* Liberar los recursos */
void destruir_trabajos(void)
{
    pthread_mutex_lock(&mutex);

    if (inicializado) {
        free(trabajos);

        trabajos = NULL;
        cantidad = 0;
        capacidad = 0;
        inicializado = 0;
    }

    pthread_mutex_unlock(&mutex);
}
