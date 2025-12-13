#ifndef LOOTOPIA_KAFKA_PRODUCER_H
#define LOOTOPIA_KAFKA_PRODUCER_H

#include "C/arguments.h"
#include "config.h"
#include "message_queue.h"
#include <pthread.h>
#include <signal.h>
#include <librdkafka/rdkafka.h>

#define KAFKA_PRODUCER_FLUSH 5000
#define ERROR_STR_LEN 512

typedef struct {
    pthread_t thread;
    volatile sig_atomic_t *running;
} kafka_producer_t;

typedef struct {
    const config_t *cfg;
    message_queue_t *queue;
    volatile sig_atomic_t *running;
} producer_thread_args_t;

int kafka_producer_start(IN kafka_producer_t *producer,
                         IN const config_t *cfg,
                         IN message_queue_t *queue,
                         IN volatile sig_atomic_t *running_flag);

void kafka_producer_stop(IN kafka_producer_t *producer);

#endif

