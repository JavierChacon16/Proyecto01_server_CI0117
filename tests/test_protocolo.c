#include <stdio.h>
#include <string.h>
#include "../includes/protocolo.h"


int fallos = 0;
int total = 0;

// Revisa una condición e imprime OK o FALLO
void revisar(int condicion, const char *nombre)
{
    total++;
    if (condicion) {
        printf("[OK]    %s\n", nombre);
    } else {
        printf("[FALLO] %s\n", nombre);
        fallos++;
    }
}

int main(void)
{
    request_t req;
    char buf[256];

    printf("--- Comandos validos ---\n");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 32 32", &req) == 0, "SUBMIT valido");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 0 32 32", &req) == 0, "SUBMIT con k = 0 (sin instantaneas)");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 32 32\n", &req) == 0, "SUBMIT con salto de linea");
    revisar(protocol_parse_request("STATUS 5", &req) == 0, "STATUS valido");
    revisar(protocol_parse_request("PEEK 5 10 20", &req) == 0, "PEEK valido");
    revisar(protocol_parse_request("RESULT 5", &req) == 0, "RESULT valido");
    revisar(protocol_parse_request("SHUTDOWN", &req) == 0, "SHUTDOWN valido");

    printf("\n--- Comandos invalidos ---\n");
    revisar(protocol_parse_request("", &req) == -1, "linea vacia");
    revisar(protocol_parse_request("HOLA", &req) == -1, "comando desconocido");
    revisar(protocol_parse_request("STATUS", &req) == -1, "STATUS sin id");
    revisar(protocol_parse_request("STATUS abc", &req) == -1, "STATUS con letras");
    revisar(protocol_parse_request("STATUS -1", &req) == -1, "STATUS con id negativo");
    revisar(protocol_parse_request("STATUS 5 extra", &req) == -1, "STATUS con argumento de mas");
    revisar(protocol_parse_request("PEEK 5 10", &req) == -1, "PEEK sin una coordenada");
    revisar(protocol_parse_request("RESULT", &req) == -1, "RESULT sin id");
    revisar(protocol_parse_request("SHUTDOWN extra", &req) == -1, "SHUTDOWN con argumento de mas");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 32", &req) == -1, "SUBMIT con un parametro menos");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 32 32 7", &req) == -1, "SUBMIT con un parametro de mas");
    revisar(protocol_parse_request("SUBMIT 64abc 64 100 4 10 32 32", &req) == -1, "SUBMIT con numero mal escrito");

    printf("\n--- Validacion de SUBMIT ---\n");
    revisar(protocol_parse_request("SUBMIT 0 64 100 4 10 32 32", &req) == -1, "nx = 0");
    revisar(protocol_parse_request("SUBMIT -5 64 100 4 10 32 32", &req) == -1, "nx negativo");
    revisar(protocol_parse_request("SUBMIT 64 0 100 4 10 32 32", &req) == -1, "ny = 0");
    revisar(protocol_parse_request("SUBMIT 64 64 0 4 10 32 32", &req) == -1, "pasos = 0");
    revisar(protocol_parse_request("SUBMIT 64 64 100 0 10 32 32", &req) == -1, "hilos = 0");
    revisar(protocol_parse_request("SUBMIT 64 64 100 63 10 32 32", &req) == -1, "hilos mayor que nx - 2");
    revisar(protocol_parse_request("SUBMIT 64 64 100 62 10 32 32", &req) == 0, "hilos igual a nx - 2");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 -1 32 32", &req) == -1, "k negativo");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 64 32", &req) == -1, "sx fuera de la malla");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 32 64", &req) == -1, "sy fuera de la malla");
    revisar(protocol_parse_request("SUBMIT 64 64 100 4 10 0 32", &req) == -1, "sx en el borde");

    printf("\n--- Datos guardados ---\n");
    protocol_parse_request("SUBMIT 100 120 500 4 10 20 30", &req);
    revisar(req.type == CMD_SUBMIT && req.data.submit.nx == 100 &&
            req.data.submit.ny == 120 && req.data.submit.pasos == 500 &&
            req.data.submit.hilos == 4 && req.data.submit.k == 10 &&
            req.data.submit.sx == 20 && req.data.submit.sy == 30,
            "SUBMIT guarda sus 7 valores");

    protocol_parse_request("STATUS 25", &req);
    revisar(req.type == CMD_STATUS && req.data.status.id == 25, "STATUS guarda el id");

    protocol_parse_request("PEEK 25 10 20", &req);
    revisar(req.type == CMD_PEEK && req.data.peek.id == 25 &&
            req.data.peek.i == 10 && req.data.peek.j == 20,
            "PEEK guarda id, i y j");

    protocol_parse_request("RESULT 30", &req);
    revisar(req.type == CMD_RESULT && req.data.result.id == 30, "RESULT guarda el id");

    protocol_parse_request("SHUTDOWN", &req);
    revisar(req.type == CMD_SHUTDOWN, "SHUTDOWN guarda el tipo");

    printf("\n--- Formato de respuestas ---\n");
    protocol_format_ok(buf, sizeof(buf), 12);
    revisar(strcmp(buf, "OK 12\n") == 0, "OK");

    protocol_format_busy(buf, sizeof(buf));
    revisar(strcmp(buf, "BUSY\n") == 0, "BUSY");

    protocol_format_error(buf, sizeof(buf), "parametro invalido");
    revisar(strcmp(buf, "ERROR parametro invalido\n") == 0, "ERROR");

    protocol_format_queued(buf, sizeof(buf), 3);
    revisar(strcmp(buf, "QUEUED 3\n") == 0, "QUEUED");

    protocol_format_running(buf, sizeof(buf), 50, 500);
    revisar(strcmp(buf, "RUNNING 50/500\n") == 0, "RUNNING");

    protocol_format_done(buf, sizeof(buf), 1.5);
    revisar(strcmp(buf, "DONE 1.500000\n") == 0, "DONE");

    protocol_format_value(buf, sizeof(buf), 50, 0.25);
    revisar(strcmp(buf, "VALUE 50 2.5000000000e-01\n") == 0, "VALUE");

    protocol_format_result(buf, sizeof(buf), 12.5, "job12_step500.bin");
    revisar(strcmp(buf, "RESULT 1.2500000000e+01 job12_step500.bin\n") == 0, "RESULT");

    protocol_format_bye(buf, sizeof(buf));
    revisar(strcmp(buf, "BYE\n") == 0, "BYE");

    printf("\nTotal: %d   Correctas: %d   Fallidas: %d\n", total, total - fallos, fallos);

    if (fallos > 0) {
        return 1;
    }
    return 0;
}