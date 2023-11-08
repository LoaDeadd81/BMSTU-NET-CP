#ifndef BMSTU_NET_CP_TASK_H
#define BMSTU_NET_CP_TASK_H

#include <poll.h>

typedef struct task_t {
    void (*handler)(const int);

    int conn;
} task_t;

#endif //BMSTU_NET_CP_TASK_H
