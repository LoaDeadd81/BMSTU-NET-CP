#ifndef BMSTU_NET_CP_NET_H
#define BMSTU_NET_CP_NET_H

#define REQ_SIZE 256
#define RESP_SIZE 1024
#define PATH_SIZE 128

int listen_net(char *host, int port);

int accept_net(int listen_sock);

int close_net(int conn);

#endif
