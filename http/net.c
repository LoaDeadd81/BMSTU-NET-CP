#include "net.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int listen_net(char *host, int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) return -1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof(addr)) != 0) return -1;

    if (listen(listenfd, SOMAXCONN) != 0) return -1;
    return listenfd;
}

int accept_net(int listen_sock) {
    return accept(listen_sock, NULL, 0);
}

int close_net(int conn) {
    return close(conn);
}


