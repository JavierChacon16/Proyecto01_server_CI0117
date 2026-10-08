#ifndef SERVER_H
#define SERVER_H

#include <signal.h> 


extern volatile sig_atomic_t g_running;


typedef struct {
    int file_descriptor;           
    unsigned long connection_id;   
} connection_t;


void *client_handler(void *arg);

#endif // SERVER_H