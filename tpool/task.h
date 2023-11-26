#ifndef BMSTU_NET_CP_TASK_H
#define BMSTU_NET_CP_TASK_H

#include <poll.h>

typedef struct task_t {
    void (*handler)(int, char *);

    int conn;
    char* wd;
} task_t;

#endif //BMSTU_NET_CP_TASK_H
