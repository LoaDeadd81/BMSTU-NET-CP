#ifndef BMSTU_NET_CP_QUEUE_H
#define BMSTU_NET_CP_QUEUE_H

#include "task.h"

typedef struct q_node_t {
    task_t *task;
    struct q_node_t *next;
} q_node_t;

typedef struct queue_t {
    q_node_t *front, *back;
    int len;
    int t_get, t_push;
} queue_t;

queue_t *new_queue_t();

int push(queue_t *queue, task_t *task);

int pop(queue_t *queue, task_t *task);

int empty(queue_t *queue);

void free_queue_t(queue_t *queue);

int len(queue_t *queue);

#endif
