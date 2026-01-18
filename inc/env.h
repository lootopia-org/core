#pragma once

#include "C/config.h"
#include <stddef.h>

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


static const ConfigEntry entries[] = {
    {"PORT", offsetof(config_t, port), INT_T},
    {"KAFKA_BROKERS", offsetof(config_t, kafka_brokers), STR_T},
    {"KAFKA_CONSUMER_TOPIC", offsetof(config_t, kafka_consumer_topic), STR_T},
    {"KAFKA_PRODUCER_TOPIC", offsetof(config_t, kafka_producer_topic), STR_T},
    {"KAFKA_GROUP_ID", offsetof(config_t, kafka_group_id), STR_T},
    {"INTERFACE", offsetof(config_t, interface), STR_T},
    {"WEBSOCKET_SERVICE_SECRET", offsetof(config_t, websocket_service_secret), STR_T},
    {"MSG_QUEUE_CAP", offsetof(config_t, message_queue_capacity), INT_T},
    {"KAFKA_POLL", offsetof(config_t, kafka_poll_timeout_ms), INT_T}
};
