
#include "server.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "net.h"
#include "request.h"
#include "responses_str.h"
#include "content_type.h"
#include "../log/log.h"

#define PATH_MAX 256
#define HEADER_LEN 128


void handle_connection(int clientfd);

int read_req(char *buff, int clientfd);

void send_resp(char *path, int clientfd, request_method_t type);

void send_err(int clientfd, const char *str);

void process_req(int clientfd, request_t *req);

int is_prefix(char *prefix, char *str);

void send_file(char *path, int clientfd);

void process_get_req(char *path, int clientfd);

void process_head_req(char *path, int clientfd);

int send_headers(char *path, int clientfd);

char *get_type(char *path);

char *get_content_type(char *path);

http_server_t *new_http_server(char *host, int port, int thread_num, char *wd) {
    if(chdir(wd)){
        log_fatal(ERR_FSTR, "failed to change work dir", strerror(errno));
        return NULL;
    }
    if(chroot(".")){
        log_fatal(ERR_FSTR, "chroot error", strerror(errno));
        return NULL;
    }

    http_server_t *server = calloc(1, sizeof(http_server_t));
    if (server == NULL) return NULL;

    strcpy(server->host, host);
    server->port = port;

    server->cl_num = sysconf(_SC_OPEN_MAX);
    if (server->cl_num < 0) {
        log_fatal(ERR_FSTR, "failed to get max client num", strerror(errno));
        free(server);
        return NULL;
    }
    server->clients = malloc(sizeof(struct pollfd) * server->cl_num);
    if (server->clients == NULL) {
        log_fatal(ERR_FSTR, "failed to alloc clients fds", strerror(errno));
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

    log_info("server created");

    return server;
}

void free_http_server_t(http_server_t *server) {
    server->clients[0].revents = -1;
    close(server->listen_sock);

    stop(server->pool);
    free_tpool_t(server->pool);

    free(server->clients);
    free(server);

    log_info("server destroyed");
}

task_t *new_task_t(void (*handler)(int), int conn) {
    task_t *task = calloc(1, sizeof(task_t));
    if (task == NULL) {
        log_error(ERR_FSTR, "task alloc failed", strerror(errno));
        return NULL;
    }
    task->conn = conn;
    task->handler = handler;
    return task;
}

int run_http_server_t(http_server_t *server) {

    char work_dir[PATH_MAX] = "";
    getcwd(work_dir, PATH_MAX);

    log_info("Server starting");
    log_info("Host: %s", server->host);
    log_info("Port: %d", server->port);
    log_info("Work dir: %s", work_dir);

    server->listen_sock = listen_net(server->host, server->port);
    if (server->listen_sock < 0) return -1;

    if (run_tpool_t(server->pool) != 0) return -1;

    long maxi = 0, nready;
    server->clients[0].fd = server->listen_sock;
    server->clients[0].events = POLLIN;

    while (1) {
        nready = poll(server->clients, maxi + 1, -1);
        if (nready < 0) {
            log_fatal(ERR_FSTR, "poll error", strerror(errno));
            return -1;
        }

        if (server->clients[0].revents & POLLIN) {
            int client_sock = accept_net(server->listen_sock);
            if (client_sock < 0) continue;

            long i = 0;
            for (i = 1; i < server->cl_num; ++i) {
                if (server->clients[i].fd < 0) {
                    server->clients[i].fd = client_sock;
                    break;
                }
            }
            if (i == server->cl_num) {
                log_error("too many connections");
                continue;
            }
            server->clients[i].events = POLLIN;

            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;
        }
        for (int i = 1; i <= maxi; ++i) {
            if (server->clients[i].fd < 0) continue;

            if (server->clients[i].revents & (POLLIN | POLLERR)) {
                task_t *task = new_task_t(handle_connection, server->clients[i].fd);
                if (task == NULL) continue;

                add_task(server->pool, task);
                server->clients[i].fd = -1;

                if (--nready <= 0) break;
            }
        }
    }
}

void handle_connection(int clientfd) {
    request_t req;
    char *buff = calloc(REQ_SIZE, sizeof(char));
    if (buff == NULL) {
        log_error(ERR_FSTR, "failed alloc req buf", strerror(errno));
        return;
    }

    log_debug("handle_connection started");
    if (read_req(buff, clientfd) < 0) {
        send_err(clientfd, INT_SERVER_ERR_STR);
        close(clientfd);
        free(buff);
        return;
    }
    log_debug("read_req");

    if (parse_req(&req, buff) < 0) {
        send_err(clientfd, BAD_REQUEST_STR);
        close(clientfd);
        free(buff);
        return;
    }
    if (req.method == BAD) {
        log_error("unsupported http method");
        send_err(clientfd, M_NOT_ALLOWED_STR);
        close(clientfd);
        free(buff);
        return;
    }

    process_req(clientfd, &req);

    close(clientfd);
    log_debug("handle_connection finished");

    free(buff);
}

int read_req(char *buff, int clientfd) {
    long byte_read = 0, msg_size = 0;

    log_debug("read_req in");
    byte_read = read_net(clientfd, buff, REQ_SIZE - 1);
    if (byte_read <= 0) return -1;
    msg_size = byte_read;
    buff[msg_size - 1] = '\0';
    log_debug("%s", buff);

    return 0;
}


void process_req(int clientfd, request_t *req) {
    char *path = calloc(PATH_MAX, sizeof(char));
    if (path == NULL) {
        log_error(ERR_FSTR, "failed alloc path buf", strerror(errno));
        send_err(clientfd, INT_SERVER_ERR_STR);
        return;
    }

    send_resp(req->url, clientfd, GET);
    free(path);
}

void send_resp(char *path, int clientfd, request_method_t type) {
    switch (type) {
        case GET:
            process_get_req(path, clientfd);
            break;
        case HEAD:
            process_head_req(path, clientfd);
            break;
        default:
            log_error("unsupported http method");
            send_err(clientfd, M_NOT_ALLOWED_STR);
            break;
    }
}

void process_get_req(char *path, int clientfd) {
    log_debug("process as GET");
    if (send_headers(path, clientfd) < 0) return;

    send_file(path, clientfd);
}

void process_head_req(char *path, int clientfd) {
    log_debug("process as HEAD");
    send_headers(path, clientfd);
}

int send_headers(char *path, int clientfd) {
    char status[] = OK_STR,
            connection[] = "Connection: close";
    char *len = calloc(HEADER_LEN, sizeof(char)),
            *type = calloc(HEADER_LEN, sizeof(char));
    char *res_str;

    if (len == NULL || type == NULL) {
        log_error(ERR_FSTR, "failed to alloc headers buffs", strerror(errno));
        send_err(clientfd, INT_SERVER_ERR_STR);
        return -1;
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        send_err(clientfd, NOT_FOUND_STR);
        perror("stat error");
        free(len);
        free(type);
        return -1;
    }
    sprintf(len, "Content-Length: %ld", st.st_size);
    char *mime_type = get_content_type(path);

    int rc = 0;
    if (mime_type == NULL) {
        log_warn("could not determine the file type");
        rc = asprintf(&res_str, "%s\r\n%s\r\n%s\r\n\r\n", status, connection, len);
    } else {
        sprintf(type, "Content-Type: %s", mime_type);
        rc = asprintf(&res_str, "%s\r\n%s\r\n%s\r\n%s\r\n\r\n", status, connection, len, type);
    }
    if (rc < 0) {
        log_error("formation of headers of http response failed");
        send_err(clientfd, INT_SERVER_ERR_STR);
        free(len);
        free(type);
        return -1;
    }

    int byte_write = write_net(clientfd, res_str, rc);
    if (byte_write < 0) {
        free(res_str);
        free(len);
        free(type);
        return -1;
    }

    log_debug("headers: %s", res_str);
    log_debug("send %d bytes", byte_write);

    free(res_str);
    free(len);
    free(type);
    return 0;
}

void send_file(char *path, int clientfd) {
    int fd = open(path, 0);
    if (fd < 0) {
        log_error(ERR_FSTR, "open error", strerror(errno));
        send_err(clientfd, NOT_FOUND_STR);
        return;
    }

    char *buff_resp = calloc(RESP_SIZE, sizeof(char));
    if (buff_resp == NULL) {
        log_error(ERR_FSTR, "failed alloc resp buf", strerror(errno));
        send_err(clientfd, INT_SERVER_ERR_STR);
        return;
    }
    unsigned long long total_read = 0, total_write = 0;
    long byte_write = 0, byte_read = 0;

    while ((byte_read = read_net(fd, buff_resp, RESP_SIZE)) > 0) {
        total_read += byte_read;

        byte_write = write(clientfd, buff_resp, byte_read);
        if (byte_write < 0) {
            log_error(ERR_FSTR, "write error", strerror(errno));
            break;
        }
        total_write += byte_write;
    }
    log_debug("total read %llu bytes", total_read);
    log_debug("total sent %llu bytes", total_write);

    close(fd);
    log_info("successful response");

    free(buff_resp);
}

void send_err(int clientfd, const char *str) {
    log_info("%s", str);
    write_net(clientfd, str, strlen(str));
}

int is_prefix(char *prefix, char *str) {
    while (*prefix && *str && *prefix++ == *str++);

    if (*prefix == '\0') return 1;
    return 0;
}

char *get_type(char *path) {
    char *res = path + strlen(path) - 1;
    while (res >= path && *res != '.' && *res != '/') res--;

    if (res < path || *res == '/') return NULL;
    return ++res;
}

char *get_content_type(char *path) {
    char *ext = get_type(path);
    if (ext == NULL) return NULL;

    int i = 0;
    for (i = 0; i < TYPE_NUM && strcmp(TYPE_EXT[i], ext) != 0; i++);
    if (i >= TYPE_NUM) return NULL;

    return MIME_TYPE[i];
}