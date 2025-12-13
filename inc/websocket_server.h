#ifndef LOOTOPIA_WEBSOCKET_SERVER_H
#define LOOTOPIA_WEBSOCKET_SERVER_H

#include "config.h"
#include "message_queue.h"
#include "kafka_producer.h"
#include "C/arguments.h"
#include <signal.h>
#include <pthread.h>
#include <libwebsockets.h>

#define WEBSOCKET_SERVER_RING_SIZE 64
#define WEBSOCKET_SINGLE_TAIL 1

typedef struct websocket_server websocket_server_t;

typedef struct MSG {
    void *payload;
    size_t len;
} msg_t;

typedef struct Session {
    struct Session *prev;
    struct Session *next;
    struct lws *wsi;
    struct lws_ring *ring;
    uint32_t tail;
} session_t;

typedef struct websocket_server {
    struct lws_context *context;
    const struct lws_protocols *protocol;
    message_queue_t *consumer_queue;
    message_queue_t *producer_queue;
    volatile sig_atomic_t *running;
    pthread_mutex_t clients_lock;
    session_t *clients;
    int port;
} websocket_server_t;

websocket_server_t *websocket_server_create(IN const config_t *cfg,
                                            IN message_queue_t *consumer_queue,
                                            IN message_queue_t *producer_queue,
                                            IN volatile sig_atomic_t *running_flag);

int websocket_server_run(IN websocket_server_t *server);

void websocket_server_stop(IN websocket_server_t *server);

void websocket_server_destroy(IN websocket_server_t *server);

#endif
