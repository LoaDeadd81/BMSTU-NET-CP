#include "queue.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../http/log.h"

q_node_t *new_q_node_t(task_t *task);

task_t *extract_q_node_t(q_node_t *node);

void free_q_node_t(q_node_t *node);

queue_t *new_queue_t() {
    return calloc(1, sizeof(queue_t));
}

int push(queue_t *queue, task_t *task) {
    q_node_t *node = new_q_node_t(task);
    if (node == NULL) return -1;

    if (queue->back == NULL) {
        queue->front = node;
        queue->back = node;
    } else {
        queue->back->next = node;
    }

    queue->len++;
    log_debug("queue", "task pushed to queue; len = %d", queue->len);

    return 0;
}

task_t *pop(queue_t *queue) {
    if (queue->front == NULL) {
        log_warn("queue", "pop from empty queue");
        return NULL;
    }

    q_node_t *node = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL) queue->back = NULL;

    queue->len--;
    log_debug("queue", "task extracted from queue; len = %d", queue->len);

    return extract_q_node_t(node);
}

int empty(queue_t *queue) {
    return queue->front == NULL;
}

void free_queue_t(queue_t *queue) {
    q_node_t *node = queue->front;
    while (node != NULL) {
        q_node_t *next = node->next;
        free_q_node_t(node);
        node = next;
    }
    free(queue);
}

q_node_t *new_q_node_t(task_t *task) {
    q_node_t *node = calloc(1, sizeof(q_node_t));
    if (node == NULL) {
        log_error("queue", ERR_FSTR, "node alloc failed", strerror(errno));
        return NULL;
    }

    node->task = calloc(1, sizeof(task_t));
    if (node->task == NULL) {
        free_q_node_t(node);
        log_error("queue", ERR_FSTR, "task alloc failed", strerror(errno));
        return NULL;
    }
    *(node->task) = *task;

    return node;
}

task_t *extract_q_node_t(q_node_t *node) {
    task_t *task = node->task;
    free(node);
    return task;
}

void free_q_node_t(q_node_t *node) {
    free(node->task);
    free(node);
}
