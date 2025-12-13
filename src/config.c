#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../inc/config.h"
#include "../inc/dotenv.h"
#include "../inc/C/errors.h"
#include "../inc/C/macros.h"

config_t *get_config(EMPTY) {
    int found;
    size_t entry_count, i, j;
    dotenv_array_t *env = read_dot_env();
    config_t *config = malloc(sizeof(config_t));
    if (!config) ERROR_EXIT("failed to allocate config");

    config_entry_t entries[] = {
        {"PORT", &config->port, INT_T },
        {"KAFKA_BROKERS", &config->kafka_brokers, STR_T},
        {"KAFKA_CONSUMER_TOPIC", &config->kafka_consumer_topic, STR_T},
        {"KAFKA_PRODUCER_TOPIC", &config->kafka_producer_topic, STR_T},
        {"KAFKA_GROUP_ID", &config->kafka_group_id, STR_T},
        {"INTERFACE", &config->interface, STR_T},
        {"MSG_QUEUE_CAP", &config->message_queue_capacity, INT_T},
        {"KAFKA_POLL", &config->kafka_poll_timeout_ms, INT_T}
    };

    entry_count = GET_ARRAY_LENGTH(entries);

    for (i = 0; i < entry_count; i++) {
        found = 0;

        for (j = 0; j < env->count; j++) {
            if (strcmp(env->items[j].name, entries[i].key) == 0) {
                found = 1;

                if (entries[i].type == INT_T) {
                    *(int *)entries[i].dest = atoi(env->items[j].value);
                } else if (entries[i].type == STR_T) {
                    *(char **)entries[i].dest = strdup(env->items[j].value);
                }

                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Missing required config variable: %s\n", entries[i].key);
            exit(EXIT_FAILURE);
        }
    }

    return config;
}

