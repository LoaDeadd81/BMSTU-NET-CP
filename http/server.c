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
#include "log.h"

#define PATH_MAX 256
#define HEADER_LEN 128
#define TYPE_LEN 110


void handle_connection(int clientfd, char *wd);

int read_req(char *buff, int clientfd);

void send_resp(char *path, int clientfd, request_method_t type);

void send_err(int clientfd, const char *str);

void process_req(int clientfd, request_t *req, char *wd);

int is_prefix(char *prefix, char *str);

void send_file(char *path, int clientfd);

void process_get_req(char *path, int clientfd);

void process_head_req(char *path, int clientfd);

int send_headers(char *path, int clientfd);

char *get_type(char *path);

char *get_content_type(char *path);

http_server_t *new_http_server(char host[16], int port, int thread_num) {
    http_server_t *server = calloc(1, sizeof(http_server_t));
    if (server == NULL) return NULL;

    strcpy(server->host, host);
    server->port = port;

    server->cl_num = sysconf(_SC_OPEN_MAX);
    if (server->cl_num < 0) {
        log_fatal("server", ERR_FSTR, "failed to get max client num", strerror(errno));
        free(server);
        return NULL;
    }
    server->clients = malloc(sizeof(struct pollfd) * server->cl_num);
    if (server->clients == NULL) {
        log_fatal("server", ERR_FSTR, "failed to alloc clients fds", strerror(errno));
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

    server->wd = calloc(PATH_MAX, sizeof(char));
    if (server->pool == NULL) {
        log_fatal("server", ERR_FSTR, "failed to alloc wd", strerror(errno));
        free_tpool_t(server->pool);
        free(server->clients);
        free(server);
        return NULL;
    }

    if (getcwd(server->wd, PATH_MAX) == NULL) {
        log_fatal("server", ERR_FSTR, "failed to get wd", strerror(errno));
        free(server->wd);
        free_tpool_t(server->pool);
        free(server->clients);
        free(server);
        return NULL;
    }

    log_info("server", "work dir: %s", server->wd);
    log_info("server", "server created");

    return server;
}

void free_http_server_t(http_server_t *server) {
    server->clients[0].revents = -1;
    close(server->listen_sock);
//    for (int i = 1; i <= server->cl_num; ++i) {
//        if (server->clients[i].fd != -1) {
//            close(server->clients[i].fd);
//        }
//    }

    stop(server->pool);
    free_tpool_t(server->pool);

    log_info("server", "server stopped");

    free(server->wd);
    free(server->clients);
    free(server);
}

int run_http_server_t(http_server_t *server) {
    server->listen_sock = listen_net(server->host, server->port);
    if (server->listen_sock < 0) return -1;

    if (run_tpool_t(server->pool) != 0) return -1;

    long maxi = 0, nready;
    server->clients[0].fd = server->listen_sock;
    server->clients[0].events = POLLIN;

    while (1) {
        nready = poll(server->clients, maxi + 1, -1);
        if (nready < 0) {
            log_fatal("server", ERR_FSTR, "poll error", strerror(errno));
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
                log_error("server", "too many connections");
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
                        .wd = server->wd,
                        .conn = server->clients[i].fd,
                        .handler = handle_connection
                };
                add_task(server->pool, &task);
                server->clients[i].fd = -1;
                if (--nready <= 0) break;
            }
        }
    }
}

void handle_connection(int clientfd, char *wd) {
    request_t req;
    char buff[REQ_SIZE] = "";
    log_debug(thread_name, "handle_connection");
    if (read_req(buff, clientfd) < 0) {
        send_err(clientfd, INT_SERVER_ERR_STR);
        close(clientfd);
        return;
    }
    log_debug(thread_name, "read_req");

    if (parse_req(&req, buff) < 0) {
        send_err(clientfd, BAD_REQUEST_STR);
        close(clientfd);
        return;
    }
    if (req.method == BAD) {
        log_error(thread_name, "unsupported http method");
        send_err(clientfd, M_NOT_ALLOWED_STR);
        close(clientfd);
        return;
    }

    process_req(clientfd, &req, wd);

    close(clientfd);
}

int read_req(char *buff, int clientfd) {
    long byte_read = 0, msg_size = 0;

    log_debug(thread_name, "read_req in");
    byte_read = read(clientfd, buff, REQ_SIZE - 1);
    if (byte_read <= 0) {
        if (byte_read < 0) log_error(thread_name, ERR_FSTR, "failed to read request", strerror(errno));
        return -1;
    }
    msg_size = byte_read;
    buff[msg_size - 1] = '\0';
    log_debug(thread_name, "%s", buff);

    return 0;
}


void process_req(int clientfd, request_t *req, char *wd) {
    char path[PATH_MAX] = "";

    if (realpath(req->url, path) == NULL) {
        if (errno == ENOENT) send_err(clientfd, NOT_FOUND_STR);
        else send_err(clientfd, INT_SERVER_ERR_STR);
        log_error(thread_name, ERR_FSTR, "realpath error", strerror(errno));
        return;
    }

    log_debug(thread_name, "path: %s", path);

    if (!is_prefix(wd, path)) {
        send_err(clientfd, FORBIDDEN_STR);
        log_error(thread_name, "attempt to access outside the root");
        return;
    }

    send_resp(path, clientfd, GET);
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
            log_error(thread_name, "unsupported http method");
            send_err(clientfd, M_NOT_ALLOWED_STR);
            break;
    }
}

void process_get_req(char *path, int clientfd) {
    log_debug(thread_name, "process as GET");
    if (send_headers(path, clientfd) < 0) return;

    send_file(path, clientfd);
}

void process_head_req(char *path, int clientfd) {
    log_debug(thread_name, "process as HEAD");
    send_headers(path, clientfd);
}

int send_headers(char *path, int clientfd) {
    char status[] = OK_STR,
            connection[] = "Connection: close",
            len[HEADER_LEN] = "",
            type[HEADER_LEN] = "";
    char *res_str;

    struct stat st;
    if (stat(path, &st) < 0) {
        perror("stat error");
        return -1;
    }
    sprintf(len, "Content-Length: %ld", st.st_size);
    char *mime_type = get_content_type(path);

    int rc = 0;
    if (mime_type == NULL) {
        log_warn(thread_name, "could not determine the file type");
        rc = asprintf(&res_str, "%s\r\n%s\r\n%s\r\n\r\n", status, connection, len);
    } else {
        sprintf(type, "Content-Type: %s", mime_type);
        rc = asprintf(&res_str, "%s\r\n%s\r\n%s\r\n%s\r\n\r\n", status, connection, len, type);
    }
    if (rc < 0) {
        log_error(thread_name, "formation of headers of http response failed");
        return -1;
    }

    int byte_write = write_net(clientfd, res_str, rc);
    if (byte_write < 0) return -1;

    log_debug(thread_name, "headers: %s", res_str);
    log_debug(thread_name, "send %d bytes", byte_write);

    return 0;
}

void send_file(char *path, int clientfd) {
    int fd = open(path, 0);
    if (fd < 0) {
        log_error(thread_name, ERR_FSTR, "open error", strerror(errno));
        return;
    }

    char buff_resp[RESP_SIZE] = "";
    unsigned long long total_read = 0, total_write = 0;
    long byte_write = 0, byte_read = 0;

    while ((byte_read = read(fd, buff_resp, RESP_SIZE)) > 0) {
        total_read += byte_read;

        byte_write = write(clientfd, buff_resp, byte_read);
        if (byte_write < 0) {
            log_error(thread_name, ERR_FSTR, "write error", strerror(errno));
            break;
        }
        total_write += byte_write;
    }
    log_debug(thread_name, "total read %llu bytes", total_read);
    log_debug(thread_name, "total sent %llu bytes", total_write);

    close(fd);
    log_info(thread_name, "successful response");
}

void send_err(int clientfd, const char *str) {
    log_info(thread_name, "%s", str);
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