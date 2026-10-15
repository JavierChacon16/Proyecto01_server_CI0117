#ifndef JOB_H
#define JOB_H

typedef struct {
    unsigned long id;
    int nx;
    int ny;
    int pasos;
    int hilos;
    int k;
    int sx;
    int sy;
} job_t;

#endif
