
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

typedef struct {
    unsigned long id_trabajo;
    int paso;
    int nx;
    int ny;
    double *ez;
} snapshot_t;

/* Inicializar el bufer de instantaneas */
int inicializar_instantaneas(int capacidad);

/* Agregar una instantanea al bufer */
int agregar_instantanea(unsigned long id_trabajo,
                       int paso,
                       const double *ez,
                       int nx,
                       int ny);

/* Sacar la siguiente instantanea */
int sacar_instantanea(snapshot_t *salida);

/* Iniciar el hilo que escribe los archivos */
int iniciar_escritor(void);

/* Cerrar el bufer */
void cerrar_instantaneas(void);

/* Esperar a que termine el hilo escritor */
void detener_escritor(void);

/* Liberar los recursos */
void destruir_instantaneas(void);

#endif
