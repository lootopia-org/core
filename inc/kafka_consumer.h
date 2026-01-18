#ifndef LOOTOPIA_KAFKA_CONSUMER_H
#define LOOTOPIA_KAFKA_CONSUMER_H

#include "env.h"
#include "message_queue.h"
#include "C/arguments.h"
#include <librdkafka/rdkafka.h>
#include <pthread.h>
#include <signal.h>

#define ERROR_STR_LEN 512
#define KAFKA_PARTITION_LIST 1
#define KAFKA_PARTITION_ASSIGNMENT -1


typedef struct {
    pthread_t thread;
    volatile sig_atomic_t *running;
} kafka_consumer_t;

typedef struct {
    const config_t *cfg;
    message_queue_t *queue;
    volatile sig_atomic_t *running;
} kafka_thread_args_t;


int kafka_consumer_start(IN kafka_consumer_t *consumer,
                         IN const config_t *cfg,
                         IN message_queue_t *queue,
                         IN volatile sig_atomic_t *running_flag);

void kafka_consumer_stop(IN kafka_consumer_t *consumer);

#endif

