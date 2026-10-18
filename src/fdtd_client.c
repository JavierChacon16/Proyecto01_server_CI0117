#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../includes/protocolo.h"

int main(int argc, char *argv[]) {
    int socket_cliente;
    struct sockaddr_in servidor;
    char linea[1024];
    char respuesta[2048];
    request_t solicitud;

    // Valido que me pasen IP y puerto por argumentos
    if (argc != 3) {
        printf("Uso: %s IP PUERTO\n", argv[0]);
        return 1;
    }

    // socket TCP
    socket_cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_cliente < 0) {
        perror("Error");
        return 1;
    }



    //Dirección del servidor
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(atoi(argv[2]));


    if (inet_pton(AF_INET, argv[1], &servidor.sin_addr) != 1) {
        printf("IP invalida\n");
        close(socket_cliente);
        return 1;
    }
    // Me conecto al servidor
    if (connect(socket_cliente, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("Error de conexion");
        close(socket_cliente);
        return 1;
    }
    printf("Conectado al servidor.\n");
    printf("Escriba un comando o EXIT para salir.\n");

    // leer comandos de la terminal y enviarlos
while (1) {
  printf("> ");

 if (fgets(linea, sizeof(linea), stdin) == NULL){
    break;
}

if (strcmp(linea, "EXIT\n") == 0){
  break;
}
        
if (protocol_parse_request(linea, &solicitud) != 0) {
    printf("Comando invalido.\n");
    continue;
}

    // Envío completo asegurándome de mandar todos los bytes
    size_t longitud = strlen(linea);
    size_t enviados = 0;

while (enviados < longitud) {
    ssize_t n = write(socket_cliente, linea + enviados, longitud - enviados);
    if (n <= 0) {
        perror("Error al enviar");
        close(socket_cliente);
        return 1;
    }
        enviados += (size_t)n;
}

// buscar cada byte hasta encntrar salto de linea
size_t usados = 0;
while (usados < sizeof(respuesta) - 1) {
    char c;
    ssize_t n = read(socket_cliente, &c, 1);

if (n <= 0) {
        printf("El servidor cerro la conexion.\n");
        close(socket_cliente);
        return 0;
    }
    respuesta[usados++] = c;
    if (c == '\n'){
        break;
    }
}
respuesta[usados] = '\0';
printf("%s", respuesta);

if (solicitud.type == CMD_SHUTDOWN)
    break;
}
close(socket_cliente);
return 0;
}