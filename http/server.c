#include "server.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "net.h"

void handle_connection(int *clientfd_p);


http_server_t *new_http_server(char host[16], int port, int thread_num) {
    http_server_t *server = calloc(1, sizeof(http_server_t));
    if (server == NULL) return NULL;

    strcpy(server->host, host);
    server->port = port;

    server->pool = new_tpool_t(thread_num);
    if (server->pool == NULL) {
        free(server);
        return NULL;
    }

    return server;
}

int run_http_server_t(http_server_t *server) {
    server->listen_sock = listen_net(server->host, server->port);
    if (server->listen_sock < 0) {
        perror("listen_net error");
        return -1;
    }
    if (run_tpool_t(server->pool) != 0) {
        perror("run_tpool_t error");
        return -1;
    }

    while (1) {
        int client_sock = accept_net(server->listen_sock);
        if (client_sock < 0) {
            int tmp = errno;
            perror("accept_net error");
            return -1;
        }

        task_t task = {
                .conn = &client_sock,
                .handler = handle_connection
        };
        if (add_task(server->pool, &task) != 0) {
            perror("add_task error");
        }
    }
}

void free_http_server_t(http_server_t *server) {
    close(server->listen_sock);
    stop(server->pool);
    free_tpool_t(server->pool);
    free(server);
}

void handle_connection(int *clientfd_p) {
    int clientfd = *clientfd_p;

    char buff_req[REQ_SIZE] = "", buff_resp[RESP_SIZE] = "";
    size_t byte_read = 0, msg_size = 0;

    while ((byte_read = read(clientfd, buff_req + msg_size, sizeof(buff_req) - msg_size - 1)) > 0) {
        msg_size += byte_read;
        if (msg_size >= RESP_SIZE - 1 || buff_req[msg_size - 1] == '\n') break;
    }
    buff_req[msg_size - 1] = '\0';
    printf("REQ: %s\n", buff_req);

    char path[PATH_SIZE] = "";
    if (realpath(buff_req, path) == NULL) {
        perror("realpath error: ");
        close(clientfd);
        return;
    }

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        perror("fopen error: ");
        close(clientfd);
        return;
    }

    byte_read = 0;
    while ((byte_read = fread(buff_resp, 1, RESP_SIZE, f)) > 0) {
        printf("sending %zu bytes\n", byte_read);
        write(clientfd, buff_resp, byte_read);
    }
    close(clientfd);
    fclose(f);
}
