
#ifndef DISPATCHER_H
#define DISPATCHER_H

/* Iniciar el hilo encargado de ejecutar los trabajos */
int iniciar_despachador(void);

/* Cerrar la cola y esperar a que termine el despachador */
void detener_despachador(void);

#endif
