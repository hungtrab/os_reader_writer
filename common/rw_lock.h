#ifndef RW_LOCK_H
#define RW_LOCK_H

#include <pthread.h>

// Synchronization modes
typedef enum {
    VANILLA,      // No synchronization (demonstrates race conditions)
    READER_PREF,  // Reader preference (may starve writers)
    WRITER_PREF,  // Writer preference (may starve readers)
    FAIR          // Fair/FIFO policy (reduces starvation)
} rw_mode_t;

// Reader-Writer lock structure
typedef struct {
    rw_mode_t mode;
    
    // Counters
    int active_readers;
    int active_writers;
    int waiting_writers;
    
    // Synchronization primitives
    pthread_mutex_t mutex;           // Protects counters
    pthread_mutex_t resource_lock;   // Writer exclusion
    pthread_mutex_t read_try;        // For writer preference
    pthread_mutex_t queue_lock;      // For fair mode (turnstile)
    pthread_cond_t readers_proceed;  // Condition variable for readers
    pthread_cond_t writers_proceed;  // Condition variable for writers
} rw_lock_t;

// Initialize the lock with specified mode
void rw_init(rw_lock_t *lock, rw_mode_t mode);

// Reader operations
void reader_enter(rw_lock_t *lock);
void reader_exit(rw_lock_t *lock);

// Writer operations
void writer_enter(rw_lock_t *lock);
void writer_exit(rw_lock_t *lock);

// Cleanup
void rw_destroy(rw_lock_t *lock);

// Get mode name as string
const char* rw_mode_name(rw_mode_t mode);

#endif // RW_LOCK_H
