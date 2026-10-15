#ifndef QUEUE_H
#define QUEUE_H

#include "job.h"

/* Inicializa la cola con una capacidad determinada */
int inicializar_cola(int capacidad);

/* Agrega un trabajo sin bloquear */
int agregar_cola(const job_t *trabajo);

/* Saca el siguiente trabajo de la cola */
int sacar_cola(job_t *trabajo);

/* Obtener la posicion de un trabajo en la cola */
int posicion_cola(unsigned long id);

/* Cierra la cola y despierta a los hilos en espera */
void cerrar_cola(void);

/* Libera los recursos de la cola */
void destruir_cola(void);

#endif
