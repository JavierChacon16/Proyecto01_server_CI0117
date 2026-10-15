
#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include "job.h"

typedef enum {
    JOB_QUEUED,
    JOB_RUNNING,
    JOB_DONE,
    JOB_ERROR
} job_state_t;

typedef struct {
    unsigned long id;
    job_state_t estado;
    int paso_actual;
    int pasos_total;
    double resultado;
    double segundos;
    char archivo[128];
} datos_trabajo_t;

/* Inicializar y liberar el administrador */
int inicializar_trabajos(int capacidad);
void destruir_trabajos(void);

/* Registrar un nuevo trabajo */
int registrar_trabajo(const job_t *trabajo);

/* Registrar y encolar un trabajo con identificador unico */
int enviar_trabajo(job_t *trabajo);

/* Actualizar el estado y progreso */
void actualizar_estado(unsigned long id, job_state_t estado);
void actualizar_progreso(unsigned long id, int paso);
void actualizar_resultado(unsigned long id,
                          double resultado,
                          const char *archivo);

/* Consultar estado y resultado */
int consultar_estado(unsigned long id, datos_trabajo_t *salida);
int consultar_resultado(unsigned long id, datos_trabajo_t  *salida);

#endif
