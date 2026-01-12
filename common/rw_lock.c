#include "rw_lock.h"
#include <stdlib.h>
#include <string.h>

void rw_init(rw_lock_t *lock, rw_mode_t mode) {
    lock->mode = mode;
    lock->active_readers = 0;
    lock->active_writers = 0;
    lock->waiting_writers = 0;
    
    if (mode != VANILLA) {
        pthread_mutex_init(&lock->mutex, NULL);
        pthread_mutex_init(&lock->resource_lock, NULL);
        pthread_mutex_init(&lock->read_try, NULL);
        pthread_mutex_init(&lock->queue_lock, NULL);
        pthread_cond_init(&lock->readers_proceed, NULL);
        pthread_cond_init(&lock->writers_proceed, NULL);
    }
}

void rw_destroy(rw_lock_t *lock) {
    if (lock->mode != VANILLA) {
        pthread_mutex_destroy(&lock->mutex);
        pthread_mutex_destroy(&lock->resource_lock);
        pthread_mutex_destroy(&lock->read_try);
        pthread_mutex_destroy(&lock->queue_lock);
        pthread_cond_destroy(&lock->readers_proceed);
        pthread_cond_destroy(&lock->writers_proceed);
    }
}

const char* rw_mode_name(rw_mode_t mode) {
    switch (mode) {
        case VANILLA: return "vanilla";
        case READER_PREF: return "reader_pref";
        case WRITER_PREF: return "writer_pref";
        case FAIR: return "fair";
        default: return "unknown";
    }
}

// ============================================================================
// READER OPERATIONS
// ============================================================================

void reader_enter(rw_lock_t *lock) {
    switch (lock->mode) {
        case VANILLA:
            // No synchronization - deliberately allows race conditions
            lock->active_readers++;
            break;
            
        case READER_PREF:
            // Classic reader preference algorithm
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers++;
            if (lock->active_readers == 1) {
                // First reader locks the resource
                pthread_mutex_lock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            break;
            
        case WRITER_PREF:
            // Reader blocked if writers are waiting
            pthread_mutex_lock(&lock->read_try);
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers++;
            if (lock->active_readers == 1) {
                pthread_mutex_lock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            pthread_mutex_unlock(&lock->read_try);
            break;
            
        case FAIR:
            // Turnstile pattern - prevents cutting in line
            pthread_mutex_lock(&lock->queue_lock);
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers++;
            if (lock->active_readers == 1) {
                pthread_mutex_lock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            pthread_mutex_unlock(&lock->queue_lock);
            break;
    }
}

void reader_exit(rw_lock_t *lock) {
    switch (lock->mode) {
        case VANILLA:
            // No synchronization
            lock->active_readers--;
            break;
            
        case READER_PREF:
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers--;
            if (lock->active_readers == 0) {
                // Last reader unlocks the resource
                pthread_mutex_unlock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            break;
            
        case WRITER_PREF:
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers--;
            if (lock->active_readers == 0) {
                pthread_mutex_unlock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            break;
            
        case FAIR:
            pthread_mutex_lock(&lock->mutex);
            lock->active_readers--;
            if (lock->active_readers == 0) {
                pthread_mutex_unlock(&lock->resource_lock);
            }
            pthread_mutex_unlock(&lock->mutex);
            break;
    }
}

// ============================================================================
// WRITER OPERATIONS
// ============================================================================

void writer_enter(rw_lock_t *lock) {
    switch (lock->mode) {
        case VANILLA:
            // No synchronization - deliberately allows race conditions
            lock->active_writers++;
            break;
            
        case READER_PREF:
            // Simple - just lock the resource
            pthread_mutex_lock(&lock->resource_lock);
            lock->active_writers++;
            break;
            
        case WRITER_PREF:
            // Block new readers when writer is waiting
            pthread_mutex_lock(&lock->mutex);
            lock->waiting_writers++;
            pthread_mutex_unlock(&lock->mutex);
            
            pthread_mutex_lock(&lock->read_try);  // Block new readers
            pthread_mutex_lock(&lock->resource_lock);
            
            pthread_mutex_lock(&lock->mutex);
            lock->waiting_writers--;
            lock->active_writers++;
            pthread_mutex_unlock(&lock->mutex);
            break;
            
        case FAIR:
            // Turnstile ensures FIFO-ish behavior
            pthread_mutex_lock(&lock->queue_lock);
            pthread_mutex_lock(&lock->resource_lock);
            lock->active_writers++;
            pthread_mutex_unlock(&lock->queue_lock);
            break;
    }
}

void writer_exit(rw_lock_t *lock) {
    switch (lock->mode) {
        case VANILLA:
            // No synchronization
            lock->active_writers--;
            break;
            
        case READER_PREF:
            lock->active_writers--;
            pthread_mutex_unlock(&lock->resource_lock);
            break;
            
        case WRITER_PREF:
            pthread_mutex_lock(&lock->mutex);
            lock->active_writers--;
            pthread_mutex_unlock(&lock->mutex);
            
            pthread_mutex_unlock(&lock->resource_lock);
            pthread_mutex_unlock(&lock->read_try);  // Allow readers
            break;
            
        case FAIR:
            lock->active_writers--;
            pthread_mutex_unlock(&lock->resource_lock);
            break;
    }
}
