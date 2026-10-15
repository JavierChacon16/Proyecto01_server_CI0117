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
