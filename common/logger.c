#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

static pthread_mutex_t log_mutex;

void log_init(void) {
    pthread_mutex_init(&log_mutex, NULL);
}

void log_destroy(void) {
    pthread_mutex_destroy(&log_mutex);
}

void log_message(thread_type_t type, int id, const char *action, ...) {
    pthread_mutex_lock(&log_mutex);
    
    // Get timestamp with milliseconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t nowtime = tv.tv_sec;
    struct tm *nowtm = localtime(&nowtime);
    char tmbuf[64];
    strftime(tmbuf, sizeof(tmbuf), "%H:%M:%S", nowtm);
    
    // Print timestamp and thread info
    printf("[%s.%03ld] [%c%d] ", 
           tmbuf, 
           tv.tv_usec / 1000,
           type == THREAD_READER ? 'R' : 'W',
           id);
    
    // Print the actual message
    va_list args;
    va_start(args, action);
    vprintf(action, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
    
    pthread_mutex_unlock(&log_mutex);
}
