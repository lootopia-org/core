#pragma once

#include "C/config.h"

typedef struct Config {
    int port;
    char *kafka_brokers;
    char *kafka_consumer_topic;
    char *kafka_producer_topic;
    char *kafka_group_id;
    char *interface;
    char *websocket_service_secret;
    int message_queue_capacity;
    int kafka_poll_timeout_ms;
} config_t;

config_t *create_config();
void destroy_config(config_t *config);
