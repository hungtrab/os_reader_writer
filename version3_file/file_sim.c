#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "../common/rw_lock.h"
#include "../common/logger.h"

#define BUFFER_CAPACITY 4096

// Simulated file buffer structure
typedef struct {
    char buffer[BUFFER_CAPACITY];
    size_t length;
} FileBuffer;

// Shared resource
FileBuffer file_buffer;
rw_lock_t lock;

// Configuration
int num_readers = 4;
int num_writers = 4;
int duration = 10;  // seconds
rw_mode_t mode = READER_PREF;
bool running = true;

// Statistics
int total_writes = 0;
int total_reads = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// Random sleep in milliseconds
void random_sleep(int min_ms, int max_ms) {
    int sleep_ms = min_ms + rand() % (max_ms - min_ms + 1);
    usleep(sleep_ms * 1000);
}

// Count lines in buffer
int count_lines(const char* buf, size_t len) {
    int count = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') count++;
    }
    return count;
}

// Reader thread function
void* reader_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_READER, id, "started");
    
    while (running) {
        reader_enter(&lock);
        
        // Critical section: read snapshot of file buffer
        char snapshot[BUFFER_CAPACITY];
        size_t snapshot_len = file_buffer.length;
        
        if (snapshot_len > 0) {
            memcpy(snapshot, file_buffer.buffer, snapshot_len);
            snapshot[snapshot_len] = '\0';
        } else {
            snapshot[0] = '\0';
        }
        
        reader_exit(&lock);
        
        int lines = count_lines(snapshot, snapshot_len);
        log_message(THREAD_READER, id, "read snapshot: %zu bytes, %d lines", 
                   snapshot_len, lines);
        
        pthread_mutex_lock(&stats_mutex);
        total_reads++;
        pthread_mutex_unlock(&stats_mutex);
        
        random_sleep(40, 100);
    }
    
    log_message(THREAD_READER, id, "finished");
    return NULL;
}

// Writer thread function
void* writer_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_WRITER, id, "started");
    
    int entry_num = 0;
    
    while (running) {
        // Prepare log line
        char log_line[128];
        snprintf(log_line, sizeof(log_line), 
                "[Writer W%d] Log entry #%d\n", id, ++entry_num);
        size_t line_len = strlen(log_line);
        
        writer_enter(&lock);
        
        // Critical section: append to file buffer
        if (file_buffer.length + line_len < BUFFER_CAPACITY) {
            if (mode == VANILLA) {
                // In vanilla mode, do slow byte-by-byte append to create race window
                for (size_t i = 0; i < line_len; i++) {
                    file_buffer.buffer[file_buffer.length] = log_line[i];
                    file_buffer.length++;
                    usleep(50);  // Small delay to make races more visible
                }
            } else {
                // Normal append
                memcpy(file_buffer.buffer + file_buffer.length, log_line, line_len);
                file_buffer.length += line_len;
            }
            
            log_message(THREAD_WRITER, id, "appended entry #%d, buffer now %zu bytes", 
                       entry_num, file_buffer.length);
        } else {
            log_message(THREAD_WRITER, id, "buffer full, cannot append");
        }
        
        writer_exit(&lock);
        
        pthread_mutex_lock(&stats_mutex);
        total_writes++;
        pthread_mutex_unlock(&stats_mutex);
        
        random_sleep(30, 80);
    }
    
    log_message(THREAD_WRITER, id, "finished, wrote %d entries", entry_num);
    return NULL;
}

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --readers N       Number of reader threads (default: 4)\n");
    printf("  --writers N       Number of writer threads (default: 4)\n");
    printf("  --mode MODE       Synchronization mode: vanilla, reader_pref, writer_pref, fair (default: reader_pref)\n");
    printf("  --duration N      Duration in seconds (default: 10)\n");
    printf("  --help            Show this help message\n");
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--readers") == 0 && i + 1 < argc) {
            num_readers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--writers") == 0 && i + 1 < argc) {
            num_writers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "vanilla") == 0) mode = VANILLA;
            else if (strcmp(argv[i], "reader_pref") == 0) mode = READER_PREF;
            else if (strcmp(argv[i], "writer_pref") == 0) mode = WRITER_PREF;
            else if (strcmp(argv[i], "fair") == 0) mode = FAIR;
            else {
                fprintf(stderr, "Unknown mode: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Initialize
    srand(time(NULL));
    log_init();
    rw_init(&lock, mode);
    
    // Initialize file buffer
    memset(&file_buffer, 0, sizeof(FileBuffer));
    
    printf("=== File Simulation - Reader-Writer Problem ===\n");
    printf("Mode: %s\n", rw_mode_name(mode));
    printf("Readers: %d, Writers: %d, Duration: %d seconds\n", 
           num_readers, num_writers, duration);
    printf("Buffer capacity: %d bytes\n", BUFFER_CAPACITY);
    
    if (mode == VANILLA) {
        printf("⚠️  WARNING: Vanilla mode - expect to see CORRUPTED DATA and INCONSISTENT LENGTH!\n");
    }
    printf("\n");
    
    // Create threads
    pthread_t readers[num_readers];
    pthread_t writers[num_writers];
    
    for (int i = 0; i < num_readers; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&readers[i], NULL, reader_thread, id);
    }
    
    for (int i = 0; i < num_writers; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&writers[i], NULL, writer_thread, id);
    }
    
    // Run for specified duration
    sleep(duration);
    running = false;
    
    // Wait for all threads to finish
    for (int i = 0; i < num_readers; i++) {
        pthread_join(readers[i], NULL);
    }
    
    for (int i = 0; i < num_writers; i++) {
        pthread_join(writers[i], NULL);
    }
    
    // Final report
    printf("\n=== Final Results ===\n");
    printf("Total write operations: %d\n", total_writes);
    printf("Total read operations: %d\n", total_reads);
    printf("Final buffer size: %zu bytes\n", file_buffer.length);
    printf("Final line count: %d\n", count_lines(file_buffer.buffer, file_buffer.length));
    
    // Show last few lines of buffer
    printf("\nLast 200 bytes of buffer:\n");
    printf("---\n");
    size_t start = file_buffer.length > 200 ? file_buffer.length - 200 : 0;
    fwrite(file_buffer.buffer + start, 1, file_buffer.length - start, stdout);
    printf("\n---\n");
    
    // Cleanup
    rw_destroy(&lock);
    log_destroy();
    pthread_mutex_destroy(&stats_mutex);
    
    return 0;
}
