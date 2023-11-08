#ifndef BMSTU_NET_CP_SERVER_H
#define BMSTU_NET_CP_SERVER_H

#include "../tpool/tpool.h"

#include <poll.h>

#define HOST_SIZE 16

typedef struct http_server_t {
    char host[HOST_SIZE];
    int port;

    struct pollfd *clients;
    long cl_num;

    int listen_sock;
    tpool_t *pool;
} http_server_t;

http_server_t *new_http_server(char *host, int port, int thread_num);

int run_http_server_t(http_server_t *server);

void free_http_server_t(http_server_t *server);

#endif
