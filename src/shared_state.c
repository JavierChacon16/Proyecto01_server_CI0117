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
