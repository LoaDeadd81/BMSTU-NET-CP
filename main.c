#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "http/server.h"

http_server_t *server = NULL;

void sig_handler(int signum)
{
    printf("received signal %d\n", signum);
    free_http_server_t(server);
    exit(0);
}


int main() {
    char ip[] = "0.0.0.0";
    int port = 7999, tn = 8;

    printf("pid: %d\n", getpid());

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    server = new_http_server(ip, port, tn);
    run_http_server_t(server);
    return 0;
}
