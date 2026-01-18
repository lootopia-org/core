#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "../inc/env.h"
#include "../inc/kafka_consumer.h"
#include "../inc/kafka_producer.h"
#include "../inc/message_queue.h"
#include "../inc/websocket_server.h"
#include "../inc/C/errors.h"
#include "../inc/C/arguments.h"
#include "../inc/C/config.h"
#include "../inc/C/macros.h"

static volatile sig_atomic_t running = 1;

static void handle_signal(IN int sig) {
    (void)sig;
    running = 0;
}

int main(EMPTY) {
    kafka_consumer_t consumer;
    kafka_producer_t producer;
    websocket_server_t *server;
    int entry_count = GET_ARRAY_LENGTH(entries);
    int struct_size = sizeof(config_t);
    config_t *config = load_config(entries, entry_count, struct_size);
    message_queue_t *consumer_queue = message_queue_create((size_t)config->message_queue_capacity);
    message_queue_t *producer_queue = message_queue_create((size_t)config->message_queue_capacity);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
 
    if (!consumer_queue || !producer_queue) {
        if (consumer_queue) message_queue_destroy(consumer_queue);
        if (producer_queue) message_queue_destroy(producer_queue);
        free_config(config, entries, entry_count);
        ERROR_EXIT("Failed to create message queues");
    }
    
    if (kafka_producer_start(&producer, config, producer_queue, &running) != 0) {
        message_queue_destroy(consumer_queue);
        message_queue_destroy(producer_queue);
        free_config(config, entries, entry_count);
        ERROR_EXIT("Failed to start Kafka producer");
    }
    
    if (kafka_consumer_start(&consumer, config, consumer_queue, &running) != 0) {
        kafka_producer_stop(&producer);
        message_queue_destroy(consumer_queue);
        message_queue_destroy(producer_queue);
        free_config(config, entries, entry_count);
        ERROR_EXIT("Failed to start Kafka consumer");
    }
    
    server = websocket_server_create(config, consumer_queue, producer_queue, &running);

    if (!server) {
        running = 0;
        kafka_consumer_stop(&consumer);
        kafka_producer_stop(&producer);
        message_queue_destroy(consumer_queue);
        message_queue_destroy(producer_queue);
        free_config(config, entries, entry_count);
        ERROR_EXIT("Failed to start WebSocket server");
    }

    websocket_server_run(server);

    message_queue_close(consumer_queue);
    message_queue_close(producer_queue);
    kafka_consumer_stop(&consumer);
    kafka_producer_stop(&producer);
    websocket_server_destroy(server);
    message_queue_destroy(consumer_queue);
    message_queue_destroy(producer_queue);
    free_config(config, entries, entry_count);
    return EXIT_SUCCESS;
}

