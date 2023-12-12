#ifndef BMSTU_NET_CP_NET_H
#define BMSTU_NET_CP_NET_H

#define REQ_SIZE 2048
#define RESP_SIZE 1024

#include <stdlib.h>

int listen_net(char *host, int port);

int accept_net(int listen_sock);

int close_net(int conn);

int write_net(int fd, const void *buf, size_t n);

int read_net(int fd, void *buf, size_t n);

#endif
