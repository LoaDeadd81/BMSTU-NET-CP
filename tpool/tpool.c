#define _GNU_SOURCE

#include "tpool.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "tpool_err.h"

void *routine(void *pool);

tpool_t *new_tpool_t(int thread_num) {
    tpool_t *pool = calloc(1, sizeof(tpool_t));
    if (pool == NULL) return NULL;

    pool->queue = new_queue_t();
    if (pool->queue == NULL) {
        free(pool);
        return NULL;
    }

    pool->th = calloc(thread_num, sizeof(pthread_t));
    if (pool->th == NULL) {
        free_queue_t(pool->queue);
        free(pool);
        return NULL;
    }
    pool->t_sz = thread_num;

    pool->q_mutex = calloc(1, sizeof(pthread_mutex_t));
    if (pool->q_mutex == NULL) {
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool);
        return NULL;
    }

    pool->q_cond = calloc(1, sizeof(pthread_cond_t));
    if (pool->q_cond == NULL) {
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool->q_mutex);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(pool->q_mutex, NULL);
    pthread_cond_init(pool->q_cond, NULL);

    return pool;
}

int run_tpool_t(tpool_t *pool) {
    int flag = 1, i;

    for (i = 0; i < pool->t_sz && flag; ++i) {
        if (pthread_create(&pool->th[i], NULL, routine, pool) != 0) flag = 0;
    }
    if (!flag) {
        for (int j = 0; j < i; ++j) {
            pthread_cancel(pool->th[j]);
        }
        return TPOOL_THREAD_CREATE_ERR;
    }
    return 0;
}

int add_task(tpool_t *pool, task_t *task) {
    int rc = 0;
    pthread_mutex_lock(pool->q_mutex);
    rc = push(pool->queue, task);
    pthread_mutex_unlock(pool->q_mutex);
    if (rc != 0) return rc;
    pthread_cond_signal(pool->q_cond);
    return 0;
}

int stop(tpool_t *pool) {
    pthread_mutex_lock(pool->q_mutex);
    pool->stop = 1;
    pthread_cond_broadcast(pool->q_cond);
    pthread_mutex_unlock(pool->q_mutex);
    printf("stop: broadcasted\n");

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);

    for (int i = 0; i < pool->t_sz; ++i) {
        printf("stop: try join %d = %lu\n", i, pool->th[i]);
        if (pool->th[i] != 0) {
            pthread_cond_broadcast(pool->q_cond);

//            pthread_join(pool->th[i], NULL);
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 5;
            int rc = pthread_timedjoin_np(pool->th[i], NULL, &timeout);
            if (rc == ETIMEDOUT) {
                pthread_cancel(pool->th[i]);
                printf("stop: pthread canceled %d = %lu\n", i, pool->th[i]);
            } else printf("stop: pthread joined %d = %lu\n", i, pool->th[i]);
        }
    }
    printf("stop: all joined\n");
    pool->stopped = 1;
    return 0;
}

void free_tpool_t(tpool_t *pool) {
    if (pool->stopped == 0) return;
    free_queue_t(pool->queue);
    free(pool->th);
    free(pool->q_mutex);
    free(pool->q_cond);
    free(pool);
}

void *routine(void *pool_ptr) {
    tpool_t *pool = (tpool_t *) pool_ptr;
    task_t *task = NULL;

    pthread_t tid = pthread_self();
    printf("[%lu]routine: while started\n", tid);
    int cnt = 0;
    while (1) {
        pthread_mutex_lock(pool->q_mutex);
        printf("[%lu]routine: mutex locked\n", tid);
        while (empty(pool->queue) && pool->stop == 0) {
            cnt++;
            printf("[%lu]routine: while checked\t stop=%d \n", tid, pool->stop);
            pthread_cond_wait(pool->q_cond, pool->q_mutex);
        }

        printf("[%lu]routine: while exit\t stop=%d \n", tid, pool->stop);
        if (pool->stop == 1) {
            pthread_mutex_unlock(pool->q_mutex);
            break;
        }

        task = pop(pool->queue);
        printf("[%lu]routine: pop\n", tid);
        pthread_mutex_unlock(pool->q_mutex);
        printf("[%lu]routine: mutex unlocked\n", tid);

        task->handler(task->conn);
        printf("[%lu]routine: task handled\n", tid);
        free(task);
    }

    printf("[%lu]routine: stoped\n", tid);
    pthread_exit(NULL);
}
