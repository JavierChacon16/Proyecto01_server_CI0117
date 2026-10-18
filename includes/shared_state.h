#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "rwlock.h"

int inicializar_estado_compartido(unsigned long id, int nx, int ny);

int consultar_valor(unsigned long id, int i, int j,
                   int *paso, double *valor);

/* Liberar los recursos */
void destruir_estado_compartido(void);

/* Publicar los valores de Ez de la simulacion */
void publicar_estado(int paso, const double *ez, int n);

#endif
