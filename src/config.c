#include <stddef.h>
#include <stdlib.h>
#include "../inc/config.h"
#include "../inc/C/macros.h"

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

config_t *create_config(){
    config_t *config = malloc(sizeof(config_t));
    load_config(config, entries, GET_ARRAY_LENGTH(entries));
    return config;
}

void destroy_config(config_t *config){
  free_config(config, entries, GET_ARRAY_LENGTH(entries)); 
}


