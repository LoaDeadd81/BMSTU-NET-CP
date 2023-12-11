#ifndef BMSTU_NET_CP_LOG_H
#define BMSTU_NET_CP_LOG_H

#define ERR_FSTR "%s: %s"

#include <threads.h>

thread_local extern const char *thread_name; //todo init as main or incapsulate

typedef enum log_level_t {
    FATAL = 0, ERROR, WARN, INFO, DEBUG, TRACE
} log_level_t;

int log_init();

void log_free();

void set_log_fd(int fd);

void set_log_level(log_level_t level);

void log_trace(const char *fstr, ...);

void log_debug(const char *fstr, ...);

void log_info(const char *fstr, ...);

void log_warn(const char *fstr, ...);

void log_error(const char *fstr, ...);

void log_fatal(const char *fstr, ...);

#endif //BMSTU_NET_CP_LOG_H
