#ifndef BMSTU_NET_CP_TASK_H
#define BMSTU_NET_CP_TASK_H

#include <poll.h>

typedef void (*handler_t)(int, char *);

typedef struct task_t {
    handler_t handler;

    int conn;
    char* wd;
} task_t;

#endif //BMSTU_NET_CP_TASK_H
