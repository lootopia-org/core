#include <stdlib.h>
#include <string.h>

#include "../inc/message_queue.h"
#include "../inc/log.h"

message_queue_t *message_queue_create(IN size_t capacity) {
    message_queue_t *queue = calloc(1, sizeof(message_queue_t));
    if (!queue) {
        return NULL;
    }
    queue->capacity = capacity;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond_nonempty, NULL);
    pthread_cond_init(&queue->cond_nonfull, NULL);
    return queue;
}

void message_queue_destroy(IN message_queue_t *queue) {
    if (!queue) {
        return;
    }
    message_queue_close(queue);
    pthread_mutex_lock(&queue->mutex);
    message_node_t *next;
    message_node_t *node = queue->head;
    while (node) {
        next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_destroy(&queue->cond_nonempty);
    pthread_cond_destroy(&queue->cond_nonfull);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

bool message_queue_push(IN message_queue_t *queue, IN const char *data, IN size_t len) {
    if (!queue || !data || len == 0) {
        return false;
    }

    pthread_mutex_lock(&queue->mutex);
    while (!queue->closed && queue->size >= queue->capacity) {
        pthread_cond_wait(&queue->cond_nonfull, &queue->mutex);
    }
    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    message_node_t *node = calloc(1, sizeof(message_node_t));
    if (!node) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    node->data = malloc(len + 1);
    if (!node->data) {
        free(node);
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    memcpy(node->data, data, len);
    node->data[len] = '\0';
    node->len = len;

    if (!queue->tail) {
        queue->head = queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->size++;
    pthread_cond_signal(&queue->cond_nonempty);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

bool message_queue_try_pop(IN message_queue_t *queue, OUT char **data, OUT size_t *len) {
    if (!queue || !data || !len) {
        return false;
    }

    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    message_node_t *node = queue->head;
    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    queue->size--;
    pthread_cond_signal(&queue->cond_nonfull);
    pthread_mutex_unlock(&queue->mutex);

    *data = node->data;
    *len = node->len;
    free(node);
    return true;
}

void message_queue_close(IN message_queue_t *queue) {
    if (!queue) {
        return;
    }
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->cond_nonempty);
    pthread_cond_broadcast(&queue->cond_nonfull);
    pthread_mutex_unlock(&queue->mutex);
}

