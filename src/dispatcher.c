#include "dispatcher.h"
#include "queue.h"
#include "job_manager.h"
#include "fdtd_solver.h"
#include "shared_state.h"
#include "snapshot.h"

#include <pthread.h>

static pthread_t hilo_despachador;
static int despachador_iniciado = 0;
