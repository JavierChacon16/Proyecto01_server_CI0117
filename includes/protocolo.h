#ifndef PROTOCOL_H
#define PROTOCOL_H

//comandos del cliente
typedef enum {
    CMD_SUBMIT, CMD_STATUS, CMD_PEEK, CMD_RESULT, CMD_SHUTDOWN, CMD_UNKNOWN
} cmd_type_t;

// estructura principal del request

typedef struct {
    cmd_type_t type;
    
    union {
        struct { int nx, ny, pasos, hilos, k, sx, sy; } submit;
        struct { int id; } status;
        struct { int id, i, j; } peek;
        struct { int id; } result;
       
    } data;
} request_t;

// Lee el string del socket y llena el struct

int protocol_parse_request(const char *line, request_t *out_req);


void protocol_format_ok(char *buf, int max_len, int id);
void protocol_format_busy(char *buf, int max_len);
void protocol_format_error(char *buf, int max_len, const char *motivo);
void protocol_format_queued(char *buf, int max_len, int pos);
void protocol_format_running(char *buf, int max_len, int paso, int total);
void protocol_format_done(char *buf, int max_len, double segs);
void protocol_format_value(char *buf, int max_len, int paso, double ez);
void protocol_format_result(char *buf, int max_len, double suma, const char *archivo);
void protocol_format_bye(char *buf, int max_len);

#endif