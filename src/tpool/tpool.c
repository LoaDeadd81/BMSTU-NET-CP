
#define _GNU_SOURCE

#include "tpool.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "../log/log.h"

typedef struct routine_args_t {
    tpool_t *pool;
    int num;
} routine_args_t;

void *routine(void *routine_args);

tpool_t *new_tpool_t(int thread_num) {
    tpool_t *pool = calloc(1, sizeof(tpool_t));
    if (pool == NULL) {
        log_fatal(ERR_FSTR, "pool alloc failed", strerror(errno));
        return NULL;
    }

    pool->queue = new_queue_t();
    if (pool->queue == NULL) {
        log_fatal(ERR_FSTR, "queue alloc failed", strerror(errno));
        free(pool);
        return NULL;
    }
    pool->queue->len = 0;

    pool->th = calloc(thread_num, sizeof(pthread_t));
    if (pool->th == NULL) {
        log_fatal(ERR_FSTR, "pthreads alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool);
        return NULL;
    }
    pool->t_sz = thread_num;

    pool->q_mutex = calloc(1, sizeof(pthread_mutex_t));
    if (pool->q_mutex == NULL) {
        log_fatal(ERR_FSTR, "mutex alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool);
        return NULL;
    }

    pool->sem = calloc(1, sizeof(sem_t));
    if (pool->sem == NULL) {
        log_fatal(ERR_FSTR, "cond var alloc failed", strerror(errno));
        free_queue_t(pool->queue);
        free(pool->th);
        free(pool->q_mutex);
        free(pool);
        return NULL;
    }


    pthread_mutex_init(pool->q_mutex, NULL);
    sem_init(pool->sem, 0, 0);

    log_info("tpool created");

    return pool;
}

routine_args_t *new_args(tpool_t *pool, int num) {
    routine_args_t *args = calloc(1, sizeof(routine_args_t));
    if (args == NULL) {
        log_fatal(ERR_FSTR, "failed to alloc args", strerror(errno));
        return NULL;
    }

    args->num = num;
    args->pool = pool;

    return args;
}

int run_tpool_t(tpool_t *pool) {
    int flag = 1, i;

    for (i = 0; i < pool->t_sz && flag; ++i) {
        routine_args_t *args = new_args(pool, i);
        if (args == NULL) {
            flag = 0;
            break;
        }

        if (pthread_create(&pool->th[i], NULL, routine, args) != 0) flag = 0;
    }
    if (!flag) {
        for (int j = 0; j < i; ++j) {
            pthread_cancel(pool->th[j]);
        }
        log_fatal(ERR_FSTR, "failed to create pthreads", strerror(errno));
        return -1;
    }

    log_info("tpool started (thread num = %d)", pool->t_sz);

    return 0;
}

int add_task(tpool_t *pool, task_t *task) {
    int rc = 0;
    pthread_mutex_lock(pool->q_mutex);
    rc = push(pool->queue, task);
    log_debug("work added (q len = %d)", pool->queue->len);
    sem_post(pool->sem);
    pthread_mutex_unlock(pool->q_mutex);
    if (rc != 0) {
        log_fatal(ERR_FSTR, "add_task error", strerror(errno));
        return rc;
    }
    return 0;
}

int stop(tpool_t *pool) {
    pthread_mutex_lock(pool->q_mutex);
    pool->stop = 1;
    for (int i = 0; i < pool->t_sz; ++i) sem_post(pool->sem);
    pthread_mutex_unlock(pool->q_mutex);
    log_info("stop flag is set");

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);

    for (int i = 0; i < pool->t_sz; ++i) {
        if (pool->th[i] != 0) {
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 5;
            int rc = pthread_timedjoin_np(pool->th[i], NULL, &timeout);
            log_debug("try to spot thread-%d", i);
            if (rc == ETIMEDOUT) {
                pthread_cancel(pool->th[i]);
                log_debug("thread-%d canceled", i);
            } else log_debug("thread-%d joined", i);
        }
    }
    log_info("all threads stopped");
    pool->stopped = 1;
    return 0;
}

void free_tpool_t(tpool_t *pool) {
    if (pool->stopped == 0) return;
    sem_destroy(pool->sem);
    free(pool->sem);
    pthread_mutex_destroy(pool->q_mutex);
    free(pool->q_mutex);
    free(pool->th);
    free_queue_t(pool->queue);
    free(pool);
}

void s_wait(sem_t *sem, pthread_mutex_t *mutex) {
    pthread_mutex_unlock(mutex);
    sem_wait(sem);
    pthread_mutex_lock(mutex);
}

void *routine(void *args) {
    routine_args_t *r_args = (routine_args_t *) args;
    tpool_t *pool = r_args->pool;
    int num = r_args->num;
    free(args);

    task_t task;
    char name[15] = "";
    sprintf(name, "thread-%d", num);
    thread_name = name;

    while (1) {
        pthread_mutex_lock(pool->q_mutex);

        s_wait(pool->sem, pool->q_mutex);

        if (pool->stop == 1) {
            pthread_mutex_unlock(pool->q_mutex);
            log_debug("stopped");
            break;
        }

        int rc = pop(pool->queue, &task);
        if (rc != -1) {
            log_debug("work taken (q len = %d)", pool->queue->len);
        }
        pthread_mutex_unlock(pool->q_mutex);
        if (rc < 0) continue;

        task.handler(task.conn, task.wd);
        log_info("routine for task finished");
    }

    pthread_exit(NULL);
}
