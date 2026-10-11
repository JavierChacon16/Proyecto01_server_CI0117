#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include "../includes/protocolo.h"



typedef struct {
    int client_socket;
} client_args_t;


void *client_handler(void *arg) {
    client_args_t *args = (client_args_t *)arg;
    int sock = args->client_socket;
    free(args); 

    char buffer[2048];
    char response[1024];
    ssize_t bytes_read;

    
while ((bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0'; 
 if (strlen(buffer) == 0) {
      continue;
     }

        request_t req;
        int parse_res = protocol_parse_request(buffer, &req);//protocolo.c
 if (parse_res < 0) {
     protocol_format_error(response, sizeof(response), "comando invalido");
     send(sock, response, strlen(response), 0);
     continue;
 }


// Conexión CON EL MOTOR FDTD


if (req.type == CMD_SUBMIT) {
//Enviar los parámetros de la malla   
// Espacio temporal 
    int sim_id = 0; 
 protocol_format_ok(response, sizeof(response), sim_id);
} 
else if (req.type == CMD_STATUS) {
 //Consultar el estado de la simulación     
// Espacio temporal 
 protocol_format_running(response, sizeof(response), 0, 0);
} 
else if (req.type == CMD_PEEK) {
 //Inspeccionar el valor de la celda
// Espacio temporal 
 protocol_format_value(response, sizeof(response), 0, 0.0);
} 
else if (req.type == CMD_RESULT) {
// Obtener el tiempo total de ejecución y el nombre del archivo binario
// Espacio temporal 
 protocol_format_result(response, sizeof(response), 0.0, "resultado.bin");
} 
else if (req.type == CMD_SHUTDOWN) {
protocol_format_bye(response, sizeof(response));
send(sock, response, strlen(response), 0);
//cierre los demás hilos y libere recursos.
break;
}

// Enviamos la respuesta al cliente
send(sock, response, strlen(response), 0);
}
close(sock);
return NULL;
}