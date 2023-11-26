#define _GNU_SOURCE

#include "tpool.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "../http/log.h"

typedef struct routine_args_t {
    tpool_t *pool;
    int num;
} routine_args_t;

void *routine(void *routine_args);

tpool_t *new_tpool_t(int thread_num) {
    tpool_t *pool = calloc(1, sizeof(tpool_t));
    if (pool == NULL) {
        log_fatal("tpool", ERR_FSTR, "pool alloc failed", strerror(errno));
        return NULL;
    }

    pool->queue = new_queue_t();
    if (pool->queue == NULL) {
        log_fatal("tpool", ERR_FSTR, "queue alloc failed", strerror(errno));
        free(pool);
        return NULL;
    }

    pool->th = calloc(thread_num, sizeof(pthread_t));
    if (pool->th == NULL) {
        log_fatal("tpool", ERR_FSTR, "pthreads alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool);
        return NULL;
    }
    pool->t_sz = thread_num;

    pool->q_mutex = calloc(1, sizeof(pthread_mutex_t));
    if (pool->q_mutex == NULL) {
        log_fatal("tpool", ERR_FSTR, "mutex alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool);
        return NULL;
    }

    pool->q_cond = calloc(1, sizeof(pthread_cond_t));
    if (pool->q_cond == NULL) {
        log_fatal("tpool", ERR_FSTR, "cond var alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool->q_mutex);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(pool->q_mutex, NULL);
    pthread_cond_init(pool->q_cond, NULL);

    log_info("tpool", "tpool created");

    return pool;
}

int run_tpool_t(tpool_t *pool) {
    int flag = 1, i;

    for (i = 0; i < pool->t_sz && flag; ++i) {
        routine_args_t args = {
                .pool = pool,
                .num = i
        };
        if (pthread_create(&pool->th[i], NULL, routine, &args) != 0) flag = 0;
    }
    if (!flag) {
        for (int j = 0; j < i; ++j) {
            pthread_cancel(pool->th[j]);
        }
        log_fatal("tpool", ERR_FSTR, "failed to create pthreads", strerror(errno));
        return -1;
    }

    log_info("tpool", "tpool started (thread num = %d)", 10);

    return 0;
}

int add_task(tpool_t *pool, task_t *task) {
    int rc = 0;
    pthread_mutex_lock(pool->q_mutex);
    rc = push(pool->queue, task);
    pthread_mutex_unlock(pool->q_mutex);
    if (rc != 0) {
        log_fatal("tpool", ERR_FSTR, "add_task error", strerror(errno));
        return rc;
    }
    pthread_cond_signal(pool->q_cond);
    return 0;
}

int stop(tpool_t *pool) {
    pthread_mutex_lock(pool->q_mutex);
    pool->stop = 1;
    pthread_cond_broadcast(pool->q_cond);
    pthread_mutex_unlock(pool->q_mutex);
    log_info("tpool", "stop flag is set");

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);

    for (int i = 0; i < pool->t_sz; ++i) {
        if (pool->th[i] != 0) {
//            pthread_cond_broadcast(pool->q_cond);
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 5;
            int rc = pthread_timedjoin_np(pool->th[i], NULL, &timeout);
            log_debug("tpool", "try to spot thread-%d", i);
            if (rc == ETIMEDOUT) {
                pthread_cancel(pool->th[i]);
                log_debug("tpool", "thread-%d canceled", i);
            } else log_debug("tpool", "thread-%d joined", i);
        }
    }
    log_info("tpool", "all threads stopped");
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

void *routine(void *args) {
    routine_args_t *r_args = (routine_args_t *) args;
    tpool_t *pool = r_args->pool;
    task_t *task = NULL;

    char name[15] = "";
    sprintf(name, "thread-%d", r_args->num);
    thread_name = name;

    setbuf(stdout, NULL);

    int cnt = 0;
    while (1) {
        pthread_mutex_lock(pool->q_mutex);
        while (empty(pool->queue) && pool->stop == 0) {
            cnt++;
            pthread_cond_wait(pool->q_cond, pool->q_mutex);
        }

        if (pool->stop == 1) {
            pthread_mutex_unlock(pool->q_mutex);
            log_debug(name, "stopped");
            break;
        }

        task = pop(pool->queue);
        pthread_mutex_unlock(pool->q_mutex);
        log_debug(name, "took the task");

        task->handler(task->conn, task->wd);
        log_debug(name, "launched task");
        free(task);
    }

    pthread_exit(NULL);
}
