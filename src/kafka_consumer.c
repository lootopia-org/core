#include <librdkafka/rdkafka.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// #include "../inc/config.h"
#include "../inc/kafka_consumer.h"
#include "../inc/log.h"
#include "../inc/message_queue.h"

static int configure_kafka(IN rd_kafka_conf_t *conf, IN const config_t *cfg) {
    char errstr[ERROR_STR_LEN];

    if (rd_kafka_conf_set(conf, "bootstrap.servers", cfg->kafka_brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka config bootstrap.servers failed: %s", errstr);
        return -1;
    }
    if (rd_kafka_conf_set(conf, "group.id", cfg->kafka_group_id, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka config group.id failed: %s", errstr);
        return -1;
    }
    if (rd_kafka_conf_set(conf, "enable.auto.commit", "true", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_WARN("Kafka config enable.auto.commit: %s", errstr);
    }
    if (rd_kafka_conf_set(conf, "auto.offset.reset", "latest", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_WARN("Kafka config auto.offset.reset: %s", errstr);
    }
    return 0;
}

static void cleanup_consumer(IN rd_kafka_t *rk,
                             IN rd_kafka_topic_partition_list_t *topics,
                             IN kafka_thread_args_t *args) {
    if (topics) {
        rd_kafka_topic_partition_list_destroy(topics);
    }
    if (rk) {
        rd_kafka_consumer_close(rk);
        rd_kafka_destroy(rk);
    }
    free(args);
}

static void *kafka_consumer_thread(IN void *arg) {
    char errstr[ERROR_STR_LEN];
    rd_kafka_t *rk;
    rd_kafka_topic_partition_list_t *topics;
    rd_kafka_message_t *rkmessage;
    kafka_thread_args_t *args = (kafka_thread_args_t *)arg;
    const config_t *cfg = args->cfg;
    message_queue_t *queue = args->queue;
    volatile sig_atomic_t *running = args->running;
    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    
    if (configure_kafka(conf, cfg) != 0) {
        rd_kafka_conf_destroy(conf);
        free(args);
        return NULL;
    }

    rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk) {
        LOG_ERROR("Failed to create Kafka consumer: %s", errstr);
        rd_kafka_conf_destroy(conf);
        free(args);
        return NULL;
    }

    rd_kafka_poll_set_consumer(rk);

    topics = rd_kafka_topic_partition_list_new(KAFKA_PARTITION_LIST);
    if (!topics) {
        LOG_ERROR("%s", "Failed to allocate topic partition list");
        cleanup_consumer(rk, topics, args);
        return NULL;
    }
    

    if (!rd_kafka_topic_partition_list_add(topics, cfg->kafka_consumer_topic, RD_KAFKA_PARTITION_UA)) {
        LOG_ERROR("Failed to add topic %s to partition list", cfg->kafka_consumer_topic);
        cleanup_consumer(rk, topics, args);
        return NULL;
    }

    if (rd_kafka_subscribe(rk, topics) != RD_KAFKA_RESP_ERR_NO_ERROR) {
        LOG_ERROR("Failed to subscribe to topic %s", cfg->kafka_consumer_topic);
        cleanup_consumer(rk, topics, args);
        return NULL;
    }
    rd_kafka_topic_partition_list_destroy(topics);
    topics = NULL;

    LOG_INFO("Kafka consumer started for topic %s", cfg->kafka_consumer_topic);

    while (*running) {
        rkmessage = rd_kafka_consumer_poll(rk, cfg->kafka_poll_timeout_ms);
        if (!rkmessage) {
            continue;
        }

        if (rkmessage->err) {
            if (rkmessage->err != RD_KAFKA_RESP_ERR__PARTITION_EOF &&
                rkmessage->err != RD_KAFKA_RESP_ERR__TIMED_OUT) {
                LOG_WARN("Kafka error: %s", rd_kafka_message_errstr(rkmessage));
            }
            rd_kafka_message_destroy(rkmessage);
            continue;
        }

        if (rkmessage->payload && rkmessage->len > 0) {
            if (!message_queue_push(queue, (const char *)rkmessage->payload, rkmessage->len)) {
                LOG_WARN("%s", "Dropping Kafka message; queue unavailable");
            }
        }

        rd_kafka_message_destroy(rkmessage);
    }

    LOG_INFO("%s", "Shutting down Kafka consumer");
    cleanup_consumer(rk, topics, args);
    return NULL;
}

int kafka_consumer_start(IN kafka_consumer_t *consumer,
                         IN const config_t *cfg,
                         IN message_queue_t *queue,
                         IN volatile sig_atomic_t *running_flag) {
    if (!consumer || !cfg || !queue || !running_flag) {
        return -1;
    }
    kafka_thread_args_t *args;

    memset(consumer, 0, sizeof(*consumer));
    consumer->running = running_flag;

    args = calloc(1, sizeof(kafka_thread_args_t));
    if (!args) {
        return -1;
    }
    args->cfg = cfg;
    args->queue = queue;
    args->running = running_flag;

    if (pthread_create(&consumer->thread, NULL, kafka_consumer_thread, args) != 0) {
        free(args);
        return -1;
    }
    return 0;
}

void kafka_consumer_stop(IN kafka_consumer_t *consumer) {
    if (!consumer || !consumer->running) {
        return;
    }
    pthread_join(consumer->thread, NULL);
}
