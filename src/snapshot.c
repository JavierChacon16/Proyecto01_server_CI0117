#include "snapshot.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static snapshot_t *instantaneas = NULL;

static int capacidad = 0;
static int inicio = 0;
static int fin = 0;
static int cantidad = 0;
static int cerrada = 0;
static int inicializada = 0;
static int productores_esperando = 0;
static int consumidores_esperando = 0;
static int escritor_iniciado = 0;

static pthread_t hilo_escritor;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static sem_t espacios;
static sem_t disponibles;


/* Guardar una instantanea en formato binario */
static void guardar_instantanea(const snapshot_t *dato)
{
    char nombre[128];

    snprintf(nombre, sizeof(nombre),
             "job%lu_step%d.bin",
             dato->id_trabajo, dato->paso);

    FILE *archivo = fopen(nombre, "wb");

    if (archivo == NULL) {
        perror("Error al abrir instantanea");
        return;
    }

    int32_t encabezado[3] = {
        (int32_t)dato->nx,
        (int32_t)dato->ny,
        (int32_t)dato->paso
    };

    size_t total = (size_t)dato->nx * (size_t)dato->ny;

    int correcto =
        fwrite(encabezado, sizeof(int32_t), 3, archivo) == 3 &&
        fwrite(dato->ez, sizeof(double), total, archivo) == total;

    if (fclose(archivo) != 0)
        correcto = 0;

    if (!correcto) {
        fprintf(stderr, "Error al guardar %s\n", nombre);
        remove(nombre);
    }
}

/* Funcion del hilo escritor */
static void *escribir_instantaneas(void *argumento)
{
    (void)argumento;

    snapshot_t dato;

    while (sacar_instantanea(&dato) == 0) {
        guardar_instantanea(&dato);
        free(dato.ez);
    }

    return NULL;
}

/* Inicializar el bufer */
int inicializar_instantaneas(int capacidad_nueva)
{
    if (capacidad_nueva <= 0)
        return -1;

    pthread_mutex_lock(&mutex);

    if (inicializada) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    instantaneas = calloc((size_t)capacidad_nueva,
                          sizeof(snapshot_t));

    if (instantaneas == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    if (sem_init(&espacios, 0,
                 (unsigned int)capacidad_nueva) != 0) {
        free(instantaneas);
        instantaneas = NULL;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    if (sem_init(&disponibles, 0, 0) != 0) {
        sem_destroy(&espacios);
        free(instantaneas);
        instantaneas = NULL;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    capacidad = capacidad_nueva;
    inicio = 0;
    fin = 0;
    cantidad = 0;
    cerrada = 0;
    inicializada = 1;
    productores_esperando = 0;
    consumidores_esperando = 0;
    escritor_iniciado = 0;

    pthread_mutex_unlock(&mutex);

    return 0;
}

/* Agregar una copia de Ez al bufer */
int agregar_instantanea(unsigned long id_trabajo,
                       int paso,
                       const double *ez,
                       int nx,
                       int ny)
{
    if (ez == NULL || nx <= 0 || ny <= 0)
        return -2;

    pthread_mutex_lock(&mutex);

    if (!inicializada || cerrada) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    productores_esperando++;
    pthread_mutex_unlock(&mutex);

    int resultado;

    do {
        resultado = sem_wait(&espacios);
    } while (resultado == -1 && errno == EINTR);

    pthread_mutex_lock(&mutex);
    productores_esperando--;

    if (resultado != 0) {
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    if (cerrada) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    size_t total = (size_t)nx * (size_t)ny;

    if ((size_t)nx > SIZE_MAX / (size_t)ny ||
        total > SIZE_MAX / sizeof(double)) {
        sem_post(&espacios);
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    double *copia = malloc(total * sizeof(double));

    if (copia == NULL) {
        sem_post(&espacios);
        pthread_mutex_unlock(&mutex);
        return -2;
    }

    memcpy(copia, ez, total * sizeof(double));

    snapshot_t dato = {
        .id_trabajo = id_trabajo,
        .paso = paso,
        .nx = nx,
        .ny = ny,
        .ez = copia
    };

    instantaneas[fin] = dato;
    fin = (fin + 1) % capacidad;
    cantidad++;

    sem_post(&disponibles);
    pthread_mutex_unlock(&mutex);

    return 0;
}

/* Sacar la siguiente instantanea */
int sacar_instantanea(snapshot_t *salida)
{
    if (salida == NULL)
        return -1;

    for (;;) {
        pthread_mutex_lock(&mutex);

        if (!inicializada ||
            (cerrada && cantidad == 0)) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        consumidores_esperando++;
        pthread_mutex_unlock(&mutex);

        int resultado;

        do {
            resultado = sem_wait(&disponibles);
        } while (resultado == -1 && errno == EINTR);

        pthread_mutex_lock(&mutex);
        consumidores_esperando--;

        if (resultado != 0) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        if (cantidad > 0) {
            *salida = instantaneas[inicio];
            inicio = (inicio + 1) % capacidad;
            cantidad--;

            sem_post(&espacios);
            pthread_mutex_unlock(&mutex);
            return 0;
        }

        if (cerrada) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        pthread_mutex_unlock(&mutex);
    }
}

/* Iniciar el hilo escritor */
int iniciar_escritor(void)
{
    pthread_mutex_lock(&mutex);

    if (!inicializada || cerrada || escritor_iniciado) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    if (pthread_create(&hilo_escritor, NULL,
                       escribir_instantaneas, NULL) != 0) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    escritor_iniciado = 1;

    pthread_mutex_unlock(&mutex);
    return 0;
}

/* Cerrar el bufer y despertar a los hilos */
void cerrar_instantaneas(void)
{
    pthread_mutex_lock(&mutex);

    if (inicializada && !cerrada) {
        cerrada = 1;

        for (int i = 0; i < productores_esperando; i++)
            sem_post(&espacios);

        for (int i = 0; i < consumidores_esperando; i++)
            sem_post(&disponibles);
    }

    pthread_mutex_unlock(&mutex);
}

/* Esperar a que termine el escritor */
void detener_escritor(void)
{
    pthread_mutex_lock(&mutex);

    int iniciado = escritor_iniciado;

    pthread_mutex_unlock(&mutex);

    if (iniciado) {
        pthread_join(hilo_escritor, NULL);

        pthread_mutex_lock(&mutex);
        escritor_iniciado = 0;
        pthread_mutex_unlock(&mutex);
    }
}

/* Liberar los recursos */
void destruir_instantaneas(void)
{
    pthread_mutex_lock(&mutex);

    if (inicializada) {
        for (int i = 0; i < cantidad; i++) {
            int posicion = (inicio + i) % capacidad;
            free(instantaneas[posicion].ez);
        }

        sem_destroy(&espacios);
        sem_destroy(&disponibles);

        free(instantaneas);
        instantaneas = NULL;

        capacidad = 0;
        inicio = 0;
        fin = 0;
        cantidad = 0;
        cerrada = 0;
        inicializada = 0;
        productores_esperando = 0;
        consumidores_esperando = 0;
        escritor_iniciado = 0;
    }

    pthread_mutex_unlock(&mutex);
}
