#include "job_manager.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static datos_trabajo_t *trabajos = NULL;
static int cantidad = 0;
static int capacidad = 0;
static int inicializado = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
