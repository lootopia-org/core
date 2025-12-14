#ifndef LOOTOPIA_LOG_H
#define LOOTOPIA_LOG_H

#include <stdio.h>
#include <time.h>

static inline void log_timestamp(char *buf, size_t buflen) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);
    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm_info);
}

#define LOG_STREAM(level, fmt, ...)                          \
    do {                                                     \
        char _ts[32];                                        \
        log_timestamp(_ts, sizeof(_ts));                     \
        fprintf(level, "[%s] " fmt "\n", _ts, ##__VA_ARGS__);\
        fflush(level);                                       \
    } while (0)

#define LOG_INFO(fmt, ...)  LOG_STREAM(stdout, "INFO: " fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_STREAM(stdout, "WARN: " fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_STREAM(stderr, "ERROR: " fmt, ##__VA_ARGS__)

#endif

