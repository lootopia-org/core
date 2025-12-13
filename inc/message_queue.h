#ifndef LOOTOPIA_MESSAGE_QUEUE_H
#define LOOTOPIA_MESSAGE_QUEUE_H

#include "C/arguments.h"
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

typedef struct message_node {
    char *data;
    size_t len;
    struct message_node *next;
} message_node_t;

typedef struct {
    message_node_t *head;
    message_node_t *tail;
    size_t size;
    size_t capacity;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond_nonempty;
    pthread_cond_t cond_nonfull;
} message_queue_t;

message_queue_t *message_queue_create(IN size_t capacity);
void message_queue_destroy(IN message_queue_t *queue);
bool message_queue_push(IN message_queue_t *queue, IN const char *data, IN size_t len);
bool message_queue_try_pop(IN message_queue_t *queue, OUT char **data, OUT size_t *len);
void message_queue_close(IN message_queue_t *queue);

#endif

