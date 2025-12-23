#include <libwebsockets.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../inc/config.h"
#include "../inc/log.h"
#include "../inc/message_queue.h"
#include "../inc/websocket_server.h"

static websocket_server_t *g_server = NULL;

static void destroy_message(IN void *ptr) {
    msg_t *m = (msg_t *)ptr;
    free(m->payload);
}

static void append_client(IN session_t *pss) {
    pthread_mutex_lock(&g_server->clients_lock);
    pss->next = g_server->clients;
    if (g_server->clients) {
        g_server->clients->prev = pss;
    }
    g_server->clients = pss;
    pthread_mutex_unlock(&g_server->clients_lock);
}

static void remove_client(IN session_t *pss) {
    pthread_mutex_lock(&g_server->clients_lock);
    if (pss->prev) {
        pss->prev->next = pss->next;
    } else {
        g_server->clients = pss->next;
    }
    if (pss->next) {
        pss->next->prev = pss->prev;
    }
    pthread_mutex_unlock(&g_server->clients_lock);
}

static int broadcast_to_clients(IN const char *data, IN size_t len) {
    if (!data || len == 0) {
        return 0;
    }
    int delivered = 0;
    msg_t amsg;
    pthread_mutex_lock(&g_server->clients_lock);
    session_t *pss = g_server->clients;
    while (pss) {
        amsg.len = len;
        amsg.payload = malloc(LWS_PRE + len);
        if (!amsg.payload) {
            pss = pss->next;
            continue;
        }
        
        memset(amsg.payload, 0, LWS_PRE);
        memcpy((unsigned char *)amsg.payload + LWS_PRE, data, len);
        if (lws_ring_insert(pss->ring, &amsg, 1) != 1) {
            destroy_message(&amsg);
        } else {
            delivered++;
        }
        pss = pss->next;
    }
    pthread_mutex_unlock(&g_server->clients_lock);

    if (delivered > 0) {
        lws_callback_on_writable_all_protocol(g_server->context, g_server->protocol);
    }
    return delivered;
}

static int callback_ws(IN struct lws *wsi, IN enum lws_callback_reasons reason,
                       IN void *user, IN void *in, IN size_t len) {
    session_t *pss = (session_t *)user;
    int m;
    const msg_t *pmsg;

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            LOG_INFO("%s","WebSocket protocol initialized");
            break;

        case LWS_CALLBACK_ESTABLISHED:
            pss->ring = lws_ring_create(sizeof(msg_t), WEBSOCKET_SERVER_RING_SIZE, destroy_message);
            pss->tail = 0;
            pss->wsi = wsi;
            append_client(pss);
            LOG_INFO("%s", "Client connected");
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (!pss->ring) {
                break;
            }
            pmsg = lws_ring_get_element(pss->ring, &pss->tail);
            if (!pmsg) {
                break;
            }

            m = lws_write(wsi,
                              (unsigned char *)pmsg->payload + LWS_PRE,
                              pmsg->len,
                              LWS_WRITE_TEXT);
            if (m < (int)pmsg->len) {
                LOG_WARN("%s", "Short write on WebSocket");
            }
            lws_ring_consume_single_tail(pss->ring, &pss->tail, WEBSOCKET_SINGLE_TAIL);
            if (lws_ring_get_element(pss->ring, &pss->tail)) {
                lws_callback_on_writable(wsi);
            }
            break;
        }

        case LWS_CALLBACK_RECEIVE:   
            if (!g_server || !in || len == 0) {
                break;
            }
        
            broadcast_to_clients((const char *)in, len);
            if (!g_server && g_server->producer_queue) {
                if (!message_queue_push(g_server->producer_queue, (const char *)in, len)) {
                    LOG_WARN("%s", "Failed to forward message to Kafka producer queue");
                }
            }
            break;

        case LWS_CALLBACK_CLOSED:
            remove_client(pss);
            if (pss->ring) {
                lws_ring_destroy(pss->ring);
            }
            LOG_INFO("%s", "Client disconnected");
            break;

        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    {
        .name = "lootopia-ws",
        .callback = callback_ws,
        .per_session_data_size = sizeof(session_t)
    },
    LWS_PROTOCOL_LIST_TERM
};

websocket_server_t *websocket_server_create(IN const config_t *cfg,
                                            IN message_queue_t *consumer_queue,
                                            IN message_queue_t *producer_queue,
                                            IN volatile sig_atomic_t *running_flag) {
    websocket_server_t *server = calloc(1, sizeof(websocket_server_t));
    struct lws_context_creation_info info;
    if (!server) {
        return NULL;
    }
    server->consumer_queue = consumer_queue;
    server->producer_queue = producer_queue;
    server->running = running_flag;
    server->port = cfg->port;
    pthread_mutex_init(&server->clients_lock, NULL);

    memset(&info, 0, sizeof(info));
    info.port = cfg->port;
    info.iface = cfg->interface;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_DISABLE_IPV6 | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.ssl_cert_filepath = cfg->ssl_cert_path;
    info.ssl_private_key_filepath = cfg->ssl_priv_path;
    info.user = server;

    server->context = lws_create_context(&info);
    if (!server->context) {
        LOG_ERROR("%s", "Failed to create WebSocket context");
        pthread_mutex_destroy(&server->clients_lock);
        free(server);
        return NULL;
    }

    server->protocol = &protocols[0];
    g_server = server;
    return server;
}

int websocket_server_run(IN websocket_server_t *server) {
    size_t len = 0;
    char *payload = NULL;
    
    if (!server) {
        return -1;
    }

    LOG_INFO("WebSocket server listening on %d", server->port);

    while (*server->running) {
        while (message_queue_try_pop(server->consumer_queue, &payload, &len)) {
            broadcast_to_clients(payload, len);
            free(payload);
        }
        lws_service(server->context, WEBSOCKET_SINGLE_TAIL);
    }
    return 0;
}

void websocket_server_stop(IN websocket_server_t *server) {
    if (!server) {
        return;
    }
    *server->running = 0;
}

void websocket_server_destroy(IN websocket_server_t *server) {
    if (!server) {
        return;
    }
    pthread_mutex_lock(&server->clients_lock);
    session_t *pss = server->clients;
    session_t *next;
    while (pss) {
        next = pss->next;
        if (pss->ring) {
            lws_ring_destroy(pss->ring);
        }
        pss = next;
    }
    pthread_mutex_unlock(&server->clients_lock);
    lws_context_destroy(server->context);
    pthread_mutex_destroy(&server->clients_lock);
    free(server);
    g_server = NULL;
}

