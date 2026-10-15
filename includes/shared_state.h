#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "rwlock.h"

/* Inicializar el estado compartido */
int inicializar_estado_compartido(int nx, int ny);

/* Liberar los recursos */
void destruir_estado_compartido(void);

/* Publicar los valores de Ez de la simulacion */
void publicar_estado(int paso, const double *ez, int n);

/* Consultar un valor de Ez para PEEK */
int consultar_valor(int i, int j, int *paso, double *valor);

#endif
