
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "http/server.h"
#include "log/log.h"

http_server_t *server = NULL;

void sig_handler(int signum) {
    printf("received signal %d\n", signum);
    free_http_server_t(server);
    exit(0);
}

//ab -n 10 -c 2 localhost:7999/a.txt
//clang -g -Wall -Werror -fsanitize=thread main.c tpool/* http/*
//TSAN_OPTIONS=detect_deadlocks=1:second_deadlock_stack=1 ./a.out
//rm -rf tpool/*.gch http/*.gch a.out

int main() {
    char ip[] = "0.0.0.0";
    int port = 7999, tn = 8;
    char wd[] = "./root";

    setbuf(stdout, NULL);
    printf("pid: %d\n", getpid());

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGHUP, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    if (log_init() < 0) return -1;
    set_log_level(INFO);

    server = new_http_server(ip, port, tn, wd);
    if(server == NULL) return -1;
    if (run_http_server_t(server) < 0) free_http_server_t(server);
    return 0;
}
