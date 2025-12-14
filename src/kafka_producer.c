#include <librdkafka/rdkafka.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../inc/config.h"
#include "../inc/kafka_producer.h"
#include "../inc/log.h"
#include "../inc/message_queue.h"

static void delivery_report(IN rd_kafka_t *rk,
                            IN const rd_kafka_message_t *rkmessage,
                            IN void *opaque) {
    (void)rk;
    (void)opaque;
    if (rkmessage->err) {
        LOG_WARN("Kafka delivery failed: %s", rd_kafka_message_errstr(rkmessage));
    }
}

static int configure_kafka(IN rd_kafka_conf_t *conf, IN const config_t *cfg) {
    char errstr[ERROR_STR_LEN];

    if (rd_kafka_conf_set(conf, "bootstrap.servers", cfg->kafka_brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka producer config bootstrap.servers failed: %s", errstr);
        return -1;
    }

    rd_kafka_conf_set(conf, "acks", "1", NULL, 0);
    rd_kafka_conf_set(conf, "linger.ms", "0", NULL, 0);
    rd_kafka_conf_set(conf, "batch.size", "0", NULL, 0);

    rd_kafka_conf_set_log_cb(conf, NULL);
    rd_kafka_conf_set_dr_msg_cb(conf, delivery_report);

    return 0;
}

static void cleanup_producer(IN rd_kafka_t *rk,
                             IN rd_kafka_topic_t *topic,
                             IN producer_thread_args_t *args) {
    if (rk) {
        rd_kafka_flush(rk, KAFKA_PRODUCER_FLUSH);
        if (topic) {
            rd_kafka_topic_destroy(topic);
        }
        rd_kafka_destroy(rk);
    }
    free(args);
}

static void *kafka_producer_thread(IN void *arg) {
    char errstr[ERROR_STR_LEN];
    rd_kafka_t *rk;
    rd_kafka_topic_t *topic;
    producer_thread_args_t *args = (producer_thread_args_t *)arg;
    const config_t *cfg = args->cfg;
    message_queue_t *queue = args->queue;
    volatile sig_atomic_t *running = args->running;
    rd_kafka_conf_t *conf = rd_kafka_conf_new();

    if (configure_kafka(conf, cfg) != 0) {
        rd_kafka_conf_destroy(conf);
        free(args);
        return NULL;
    }

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        LOG_ERROR("Failed to create Kafka producer: %s", errstr);
        rd_kafka_conf_destroy(conf);
        free(args);
        return NULL;
    }

    topic = rd_kafka_topic_new(rk, cfg->kafka_producer_topic, NULL);
    if (!topic) {
        LOG_ERROR("Failed to create Kafka topic object for %s", cfg->kafka_producer_topic);
        cleanup_producer(rk, NULL, args);
        return NULL;
    }

    LOG_INFO("Kafka producer started for topic %s", cfg->kafka_producer_topic);

    while (*running) {
        char *msg = NULL;
        size_t msg_len = 0;

        if (message_queue_try_pop(queue, &msg, &msg_len)) {
            int rc = rd_kafka_produce(
                topic,
                RD_KAFKA_PARTITION_UA,
                RD_KAFKA_MSG_F_COPY,
                msg,
                msg_len,
                NULL,
                0,
                NULL);

            if (rc != 0) {
                LOG_WARN("Failed to enqueue message for topic %s: %s",
                         cfg->kafka_producer_topic,
                         rd_kafka_err2str(rd_kafka_last_error()));
            }

            free(msg);
        }

        rd_kafka_poll(rk, cfg->kafka_poll_timeout_ms);
    }

    LOG_INFO("%s", "Shutting down Kafka producer");
    cleanup_producer(rk, topic, args);
    return NULL;
}

int kafka_producer_start(IN kafka_producer_t *producer,
                         IN const config_t *cfg,
                         IN message_queue_t *queue,
                         IN volatile sig_atomic_t *running_flag) {
    if (!producer || !cfg || !queue || !running_flag) {
        return -1;
    }
    producer_thread_args_t *args;

    memset(producer, 0, sizeof(*producer));
    producer->running = running_flag;

    args = calloc(1, sizeof(producer_thread_args_t));
    if (!args) {
        return -1;
    }
    args->cfg = cfg;
    args->queue = queue;
    args->running = running_flag;

    if (pthread_create(&producer->thread, NULL, kafka_producer_thread, args) != 0) {
        free(args);
        return -1;
    }
    return 0;
}

void kafka_producer_stop(IN kafka_producer_t *producer) {
    if (!producer || !producer->running) {
        return;
    }
    pthread_join(producer->thread, NULL);
}

