#include "server.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "net.h"

void handle_connection(int clientfd);


http_server_t *new_http_server(char host[16], int port, int thread_num) {
    http_server_t *server = calloc(1, sizeof(http_server_t));
    if (server == NULL) return NULL;

    strcpy(server->host, host);
    server->port = port;

    server->cl_num = sysconf(_SC_OPEN_MAX);
    if (server->cl_num < 0) {
        free(server);
        return NULL;
    }
    server->clients = malloc(sizeof(struct pollfd) * server->cl_num);
    if (server->clients == NULL) {
        free(server);
        return NULL;
    }
    for (long i = 0; i < server->cl_num; ++i) server->clients[i].fd = -1;

    server->pool = new_tpool_t(thread_num);
    if (server->pool == NULL) {
        free(server->clients);
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

    long maxi = 0, nready;
    server->clients[0].fd = server->listen_sock;
    server->clients[0].events = POLLIN;

    while (1) {
        nready = poll(server->clients, maxi + 1, -1);
        if (nready < 0) {
            perror("poll error");
            continue;
        }

        if (server->clients[0].revents & POLLIN) {
            int client_sock = accept_net(server->listen_sock);
            if (client_sock < 0) {
                perror("accept_net error");
                continue;
            }

            long i = 0;
            for (i = 1; i < server->cl_num; ++i)
                if (server->clients[i].fd < 0) {
                    server->clients[i].fd = client_sock;
                    break;
                }
            if (i == server->cl_num) {
                perror("too many connections");
                continue;
            }
            server->clients[i].events = POLLIN;

            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;
        }
        for (int i = 1; i <= maxi; ++i) {
            if (server->clients[i].fd < 0) continue;

            if (server->clients[i].revents & (POLLIN | POLLERR)) {
                task_t task = {
                        .conn = server->clients[i].fd,
                        .handler = handle_connection
                };
                if (add_task(server->pool, &task) != 0) {
                    perror("add_task error");
                }
                server->clients[i].fd = -1;
                if (--nready <= 0) break;
            }
        }
    }
}

void free_http_server_t(http_server_t *server) {
    server->clients[0].revents = -1;
    close(server->listen_sock);

    stop(server->pool);
    free_tpool_t(server->pool);

    free(server);
}

void handle_connection(int clientfd) {

    char buff_req[REQ_SIZE] = "", buff_resp[RESP_SIZE] = "";
    long byte_read = 0, msg_size = 0;

    byte_read = read(clientfd, buff_req, sizeof(buff_req) - 1);
    if (byte_read <= 0) {
        if (byte_read < 0) perror("read error: ");
        close(clientfd);
        return;
    }
    msg_size = byte_read;
    buff_req[msg_size - 1] = '\0';
    printf("REQ: %s\n", buff_req);

    char path[PATH_SIZE] = "";
    if (realpath(buff_req, path) == NULL) {
        perror("realpath error: ");
        close(clientfd);
        return;
    }

    int fd = open(path, 0);
    if (fd < 0) {
        perror("open error: ");
        close(clientfd);
        return;
    }

    unsigned long long total_read = 0, total_write = 0;
    long byte_write = 0;
    byte_read = 0;
    while ((byte_read = read(fd, buff_resp, RESP_SIZE)) > 0) {
        printf("read %ld bytes\n", byte_read);
        total_read += byte_read;

        byte_write = write(clientfd, buff_resp, byte_read);
        if(byte_write < 0){
            perror("write error");
            break;
        }
        printf("send %ld bytes\n", byte_write);
        total_write += byte_write;
    }
    printf("total read %llu bytes\n", total_read);
    printf("total sent %llu bytes\n", total_write);
    close(clientfd);
    close(fd);
}
