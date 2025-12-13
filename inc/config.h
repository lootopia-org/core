#ifndef LOOTOPIA_CONFIG_H
#define LOOTOPIA_CONFIG_H

#include "../inc/C/arguments.h"
#include <stddef.h>

typedef struct Config {
    int port;
    char *kafka_brokers;
    char *kafka_consumer_topic;
    char *kafka_producer_topic;
    char *kafka_group_id;
    char *interface;
    int message_queue_capacity;
    int kafka_poll_timeout_ms;
} config_t;

typedef struct {
    const char *key;          
    void *dest;               
    enum { INT_T, STR_T } type;
} config_entry_t;


config_t *get_config(EMPTY);

void free_config(IN config_t *config);

#endif 

