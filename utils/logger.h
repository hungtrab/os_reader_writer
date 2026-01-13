#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>

// Thread types for logging
typedef enum {
    THREAD_READER,
    THREAD_WRITER
} thread_type_t;

// Initialize the logger
void log_init(void);

// Thread-safe logging with timestamp
void log_message(thread_type_t type, int id, const char *action, ...);

// Cleanup
void log_destroy(void);

#endif // LOGGER_H
