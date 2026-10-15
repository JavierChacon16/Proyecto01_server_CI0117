#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include "../includes/protocolo.h"
#include "../includes/job_manager.h"
#include "../includes/queue.h"
#include "../includes/dispatcher.h"
#include "../includes/snapshot.h"
#include "../includes/system.h"


#define _POSIX_C_SOURCE 200809L


#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 64
#define LINE_SIZE 1024
#define RESPONSE_SIZE 512

typedef struct {
    int fd; 
    unsigned long connection_id;
} client_args_t;

static volatile sig_atomic_t running = 1;
static int listen_fd = -1;

// Métricas y control de hilos
static unsigned long accepted_connections = 0;
static unsigned long served_connections = 0;
static int active_threads = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void handle_signal(int signal_number) {
    (void)signal_number;
    running = 0;
if (listen_fd >= 0) {
    shutdown(listen_fd, SHUT_RDWR);
}
}


//enviar todos los bytes
static int send_all(int fd, const char *data, size_t length) {
size_t sent = 0;

while (sent < length) {

    ssize_t n = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
 if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return -1;
        sent += (size_t)n;
}

 return 0;
}


//respuesta al lcliente
static int send_response(int fd, const char *response) {
    size_t length = strlen(response);

    if (send_all(fd, response, length) < 0) 
        return -1;

    if (length == 0 || response[length - 1] != '\n') { //salto linea
        if (send_all(fd, "\n", 1) < 0)
            return -1;
    }
    return 0;
}

//lectura segura poll()
static int receive_line_timeout(int fd, char *line, size_t capacity) {
    size_t used = 0;

    if (capacity < 2) return -1;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;

    while (running) {
        int ret = poll(&pfd, 1, 300); // 300 ms

        if (ret < 0) {
            if (errno == EINTR) 
            continue;
            
            return -1;
        }
            // pasaron 300 ms
        if (ret == 0) {
            
            continue;
        }

        if (pfd.revents & POLLIN) {
            char c;
            ssize_t n = recv(fd, &c, 1, 0);

            if (n == 0) {
                if (used == 0) 
                 return 0; 
                 break;
            }
            if (n < 0) {
                 if (errno == EINTR) continue;
                     return -1;
            }
            if (c == '\n')
                 break;
                 
            if (c != '\r') {
                if (used >= capacity - 1) {

                  while (c != '\n') {
                    n = recv(fd, &c, 1, 0);
                    if (n <= 0 || c == '\n') 
                    break;

                    }

                    return -1;
                }
                line[used++] = c;
            }
        } else {
            
            break; //desconexion en el socket
        }
    }

    line[used] = '\0';
    return running ? 1 : 0;
}

//conectar motor fdtd
static int process_command(int fd, const char *line) {
    request_t request;

    char response[RESPONSE_SIZE];

if (protocol_parse_request(line, &request) != 0) {
    protocol_format_error(response, sizeof(response), "comando_invalido");
    return send_response(fd, response);
}

if (request.type == CMD_SUBMIT) {
   //

// integrar

   //
int sim_id = 1; // Stub temporal hasta integrar el módulo 
protocol_format_ok(response, sizeof(response), sim_id);
return send_response(fd, response);
}

if (request.type == CMD_STATUS) {
    
        // Consultar estado real en el gestor de tareas.
    
    protocol_format_running(response, sizeof(response), 0, request.data.status.id);
    return send_response(fd, response);
}

if (request.type == CMD_PEEK) {
    
    // Inspeccionar valor de la malla.
    
    protocol_format_value(response, sizeof(response), 0, 0.0);
    return send_response(fd, response);
}

if (request.type == CMD_RESULT) {
    
// resultados finales del motor.
    
    protocol_format_result(response, sizeof(response), 0.0, "resultado.bin");
    return send_response(fd, response);
}



if (request.type == CMD_SHUTDOWN) {
    protocol_format_bye(response, sizeof(response));
    send_response(fd, response);
    running = 0;
    if (listen_fd >= 0) {
        shutdown(listen_fd, SHUT_RDWR);
    }
    return 1; 
}

protocol_format_error(response, sizeof(response), "comando_desconocido");
return send_response(fd, response);
}

//hilo por cliente
static void *client_thread_handler(void *arg) {
    client_args_t *args = (client_args_t *)arg;

    int fd = args->fd;
    free(args);

    char line[LINE_SIZE];

while (running) {
    int result = receive_line_timeout(fd, line, sizeof(line));

    if (result <= 0)
    break;
    if (line[0] == '\0') 
    continue;

    int cmd_res = process_command(fd, line);
    if (cmd_res != 0) 
    break;
}

    close(fd);

    
    pthread_mutex_lock(&stats_mutex);
    served_connections++;
    active_threads--;
    pthread_mutex_unlock(&stats_mutex);

    return NULL;
}


//validar puerto del servidor
static unsigned short parse_port(int argc, char **argv) {
   
if (argc < 2) 
return DEFAULT_PORT;

char *end = NULL;
errno = 0;
long value = strtol(argv[1], &end, 10);

if (errno != 0 || end == argv[1] || *end != '\0' || value <= 0 || value > 65535) {
    return DEFAULT_PORT;
}
return (unsigned short)value;
}

int main(int argc, char **argv) {
struct sigaction sa;

memset(&sa, 0, sizeof(sa));

sa.sa_handler = handle_signal;

sigemptyset(&sa.sa_mask);

sigaction(SIGINT, &sa, NULL);

signal(SIGPIPE, SIG_IGN);

unsigned short port = parse_port(argc, argv);


/* Inicializar los modulos concurrentes */
int capacidad_cola = 10;
int capacidad_instantaneas = 4;

if (inicializar_trabajos(capacidad_cola) != 0) {
    fprintf(stderr, "Error al inicializar trabajos\n");
    return EXIT_FAILURE;
}

if (inicializar_cola(capacidad_cola) != 0) {
    fprintf(stderr, "Error al inicializar cola\n");
    destruir_trabajos();
    return EXIT_FAILURE;
}

if (inicializar_instantaneas(capacidad_instantaneas) != 0) {
    fprintf(stderr, "Error al inicializar instantaneas\n");
    destruir_cola();
    destruir_trabajos();
    return EXIT_FAILURE;
}

if (iniciar_escritor() != 0) {
    fprintf(stderr, "Error al iniciar escritor\n");
    destruir_instantaneas();
    destruir_cola();
    destruir_trabajos();
    return EXIT_FAILURE;
}

if (iniciar_despachador() != 0) {
    fprintf(stderr, "Error al iniciar despachador\n");
    cerrar_instantaneas();
    detener_escritor();
    destruir_instantaneas();
    destruir_cola();
    destruir_trabajos();
    return EXIT_FAILURE;
}


listen_fd = socket(AF_INET, SOCK_STREAM, 0);
if (listen_fd < 0) {
    perror("socket");
    apagar_sistema();
    return EXIT_FAILURE;
}

int opt = 1;
setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

struct sockaddr_in address;
memset(&address, 0, sizeof(address));
address.sin_family = AF_INET;
address.sin_addr.s_addr = htonl(INADDR_ANY);
address.sin_port = htons(port);

if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    close(listen_fd);
    apagar_sistema();
    return EXIT_FAILURE;
}

if (listen(listen_fd, LISTEN_BACKLOG) < 0) {
    perror("listen");
    close(listen_fd);
    apagar_sistema();
    return EXIT_FAILURE;
}

fflush(stdout);

while (running) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno == EINTR) continue;
        if (!running) break;
        perror("accept");
        continue;
    }

    client_args_t *args = malloc(sizeof(*args));
    if (args == NULL) {
        fprintf(stderr, "Sin memoria para la conexion\n");
        close(client_fd);
        continue;
    }

    args->fd = client_fd;
    args->connection_id = ++accepted_connections;

    pthread_mutex_lock(&stats_mutex);
    active_threads++;
    pthread_mutex_unlock(&stats_mutex);

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread_handler, args) != 0) {
        perror("pthread_create");
        close(client_fd);
        free(args);
        pthread_mutex_lock(&stats_mutex);
        active_threads--;
        pthread_mutex_unlock(&stats_mutex);
    } else {
        pthread_detach(tid);
    }
}

if (listen_fd >= 0) {
    close(listen_fd);
}

// Esperar a que todos los hilos clientes activos terminen
struct timespec espera = {0, 100000000L}; // 100 ms

    while (1) {
        pthread_mutex_lock(&stats_mutex);
        int current_active = active_threads;
        pthread_mutex_unlock(&stats_mutex);

        if (current_active <= 0) {
            break;
        }
        nanosleep(&espera, NULL);
    }

printf("\nConexiones aceptadas: %lu\n", accepted_connections);
printf("Conexiones atendidas: %lu\n", served_connections);
printf("Servidor cerrado.\n");

pthread_mutex_destroy(&stats_mutex);

return EXIT_SUCCESS;
}
