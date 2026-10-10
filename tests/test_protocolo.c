#include <stdio.h>
#include <string.h>
#include "../includes/protocolo.h"

int main(void)
{
request_t req;
char buf[256];

// Comandos válidos

if (protocol_parse_request("SUBMIT 64 64 100 4 10 32 32", &req) == 0)
    printf("OK: SUBMIT valido\n");
else
    printf("FALLO: SUBMIT valido\n");

if (protocol_parse_request("SUBMIT 64 64 100 4 0 32 32", &req) == 0)
    printf("OK: SUBMIT con k = 0\n");
else
    printf("FALLO: SUBMIT con k = 0\n");

if (protocol_parse_request("STATUS 5", &req) == 0)
    printf("OK: STATUS valido\n");
else
    printf("FALLO: STATUS valido\n");

if (protocol_parse_request("PEEK 5 10 20", &req) == 0)
    printf("OK: PEEK valido\n");
else
    printf("FALLO: PEEK valido\n");


//comandos -1
if (protocol_parse_request("HOLA", &req) == -1)
    printf("OK: comando desconocido\n");
else
    printf("FALLO: comando desconocido\n");

if (protocol_parse_request("STATUS", &req) == -1)
    printf("OK: STATUS sin id\n");
else
    printf("FALLO: STATUS sin id\n");

if (protocol_parse_request("SUBMIT 64 64 100 4 10 32", &req) == -1)
    printf("OK: SUBMIT incompleto\n");
else
    printf("FALLO: SUBMIT incompleto\n");

if (protocol_parse_request("SUBMIT 64 64 100 63 10 32 32", &req) == -1)
    printf("OK: hilos mayor que nx - 2\n");
else
    printf("FALLO: hilos mayor que nx - 2\n");

// Datos guardados
protocol_parse_request("STATUS 25", &req);
if (req.data.status.id == 25)
    printf("OK: STATUS guarda el id\n");
else
    printf("FALLO: STATUS guarda el id\n");

// Respuestas
protocol_format_ok(buf, sizeof(buf), 12);
if (strcmp(buf, "OK 12\n") == 0)
    printf("Ok: formato OK\n");
else
    printf("FALLO: formato OK\n");

protocol_format_busy(buf, sizeof(buf));
if (strcmp(buf, "BUSY\n") == 0)
    printf("OK: formato BUSY\n");
else
    printf("FALLO: formato BUSY\n");

protocol_format_error(buf, sizeof(buf), "malo");
if (strcmp(buf, "ERROR malo\n") == 0)
    printf("OK: formato ERROR\n");
else
    printf("FALLO: formato ERROR\n");

return 0;
}