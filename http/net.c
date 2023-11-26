// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "net.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "log.h"

int listen_net(char *host, int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        log_fatal("main", ERR_FSTR, "socket create failed", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        log_fatal("main", ERR_FSTR, "bind failed", strerror(errno));
        return -1;
    }

    if (listen(listenfd, SOMAXCONN) != 0) {
        log_fatal("main", ERR_FSTR, "listen failed", strerror(errno));
        return -1;
    }
    return listenfd;
}

int accept_net(int listen_sock) {
    int rc = accept(listen_sock, NULL, 0);
    if (rc < 0) log_error("server", ERR_FSTR, "accept error", strerror(errno));
    return rc;
}

int close_net(int conn) {
    return close(conn);
}

int write_net(int fd, const void *buf, size_t n) {
    log_debug(thread_name, "before write to fd = %d", fd);
    int byte_write = write(fd, buf, n);
    if (byte_write < 0) log_error(thread_name, ERR_FSTR, "write error", strerror(errno));
    log_debug(thread_name, "after write to fd = %d", fd);
    return byte_write;
}

int read_net(int fd, void *buf, size_t n) {
    int byte_read = read(fd, buf, n);
    if (byte_read < 0) log_error("server", ERR_FSTR, "read error", strerror(errno));
    return byte_read;
}


