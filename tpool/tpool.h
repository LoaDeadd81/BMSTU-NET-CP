#ifndef BMSTU_NET_CP_TPOOL_H
#define BMSTU_NET_CP_TPOOL_H

#include <pthread.h>

#include "task.h"
#include "queue.h"

typedef struct tpool_t {
    pthread_t *th;
    int t_sz;

    queue_t *queue;
    pthread_mutex_t *q_mutex;
    pthread_cond_t *q_cond;

    int stop;
    int stopped;
} tpool_t;

tpool_t *new_tpool_t(int thread_num);

int run_tpool_t(tpool_t *pool);

int add_task(tpool_t *pool, task_t *task);

int stop(tpool_t *pool);

void free_tpool_t(tpool_t *pool_ptr);

#endif
