#include <stdio.h>
#include <string.h>
#include "../includes/protocolo.h"

// Tamaño para la malla
#define MAX_DIM 6767

 

// Convierte la línea del cliente en un struct request_t.
// Retorna 0
// Retorna -1 ai hay un error
int protocol_parse_request(const char *line, request_t *out_req) {

    char cmd[32];
    char extra[2];
    int n;
    out_req->type = CMD_UNKNOWN; //entrada desconocida
if (sscanf(line, "%31s", cmd) != 1) {
    
    return -1;

}

// Comando SUBMIT
if (strcmp(cmd, "SUBMIT") == 0) {

    int nx, ny, pasos, hilos, k, sx, sy;    
    n = sscanf(line, "%*s %d %d %d %d %d %d %d %1s", &nx, &ny, &pasos, &hilos, &k, &sx, &sy, extra);
    
    if (n != 7) {
     
        return -1;
    }

    // Restricciones
    if (nx < 3 || nx > MAX_DIM) 
    return -1;
    if (ny < 3 || ny > MAX_DIM) 
    return -1;
    if (pasos <= 0) 
    return -1;
    if (hilos < 1 || hilos > nx - 2)
        return -1;
    if (k < 0) 
    return -1;
    
    if (sx < 1 || sx > nx - 2) 
    return -1;
    if (sy < 1 || sy > ny - 2) 
    return -1;

    
    out_req->type = CMD_SUBMIT;
    out_req->data.submit.nx = nx;
    out_req->data.submit.ny = ny;
    out_req->data.submit.pasos = pasos;
    out_req->data.submit.hilos = hilos;
    out_req->data.submit.k = k;
    out_req->data.submit.sx = sx;
    out_req->data.submit.sy = sy;
    return 0;
}

// STATUS
if (strcmp(cmd, "STATUS") == 0) {
    int id;
    
    n = sscanf(line, "%*s %d %1s", &id, extra);
    if (n != 1 || id < 0) {
        return -1;
    }
    out_req->type = CMD_STATUS;
    out_req->data.status.id = id;
    return 0;
}

// PEEK
if (strcmp(cmd, "PEEK") == 0) {
    int id, i, j;
    n = sscanf(line, "%*s %d %d %d %1s", &id, &i, &j, extra);
    if (n != 3 || id < 0 || i < 0 || j < 0) {
        return -1;
    }
        out_req->type = CMD_PEEK;
    out_req->data.peek.id = id;
    out_req->data.peek.i = i;
    out_req->data.peek.j = j;
    
    return 0;
}

// RESULT
if (strcmp(cmd, "RESULT") == 0) {
    int id;
    
    n = sscanf(line, "%*s %d %1s", &id, extra);
    if (n != 1 || id < 0) {
        return -1;
    }
    out_req->type = CMD_RESULT;
    out_req->data.result.id = id;
    
    return 0;
}

    //SHUTDOWN
 if (strcmp(cmd, "SHUTDOWN") == 0) {
    n = sscanf(line, "%*s %1s", extra);
    if (n > 0) {
        return -1; // SHUTDOWN no debe llevar parametros extras
    }
        
    out_req->type = CMD_SHUTDOWN;
    return 0;
    }
    return -1; // Si no calza con ninguno
}

//respuestas tcp
void protocol_format_ok(char *buf, int max_len, int id) {
    snprintf(buf, max_len, "OK %d\n", id);
}
void protocol_format_busy(char *buf, int max_len) {
    snprintf(buf, max_len, "BUSY\n");
}
void protocol_format_error(char *buf, int max_len, const char *motivo) {
    snprintf(buf, max_len, "ERROR %s\n", motivo);
}
void protocol_format_queued(char *buf, int max_len, int pos) {
    snprintf(buf, max_len, "QUEUED %d\n", pos);
}
void protocol_format_running(char *buf, int max_len, int paso, int total) {
    snprintf(buf, max_len, "RUNNING %d/%d\n", paso, total);
}
void protocol_format_done(char *buf, int max_len, double segs) {
    snprintf(buf, max_len, "DONE %.6f\n", segs);
}
void protocol_format_value(char *buf, int max_len, int paso, double ez) {
    snprintf(buf, max_len, "VALUE %d %.10e\n", paso, ez);
}
void protocol_format_result(char *buf, int max_len, double suma, const char *archivo) {
       snprintf(buf, max_len, "RESULT %.10e %s\n", suma, archivo);
}
void protocol_format_bye(char *buf, int max_len) {
    snprintf(buf, max_len, "BYE\n");
}