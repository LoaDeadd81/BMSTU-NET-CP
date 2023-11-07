#ifndef BMSTU_NET_CP_TASK_H
#define BMSTU_NET_CP_TASK_H

typedef struct task_t {
    void (*handler)(int *);

    int *conn;
} task_t;

#endif //BMSTU_NET_CP_TASK_H
