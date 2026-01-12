#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "../common/rw_lock.h"
#include "../common/logger.h"

// Shared resource
int prime_count = 0;
rw_lock_t lock;

// Configuration
int num_readers = 10;
int num_writers = 10;
int duration = 20;  // seconds
int max_range = 10000;  // maximum number to check for primes
rw_mode_t mode = READER_PREF;
bool running = true;

// Prime checking function
bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

// Random sleep in milliseconds
void random_sleep(int min_ms, int max_ms) {
    int sleep_ms = min_ms + rand() % (max_ms - min_ms + 1);
    usleep(sleep_ms * 1000);
}

// Reader thread function
void* reader_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_READER, id, "started");
    
    while (running) {
        reader_enter(&lock);
        
        // Critical section: read the shared counter
        int current_count = prime_count;
        log_message(THREAD_READER, id, "read prime_count = %d", current_count);
        
        reader_exit(&lock);
        
        random_sleep(10, 50);
    }
    
    log_message(THREAD_READER, id, "finished");
    return NULL;
}

// Writer thread function
void* writer_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_WRITER, id, "started");
    
    // Each writer checks a range of numbers
    // Total range: [2, max_range) divided among all writers
    int total_range = max_range - 2;
    int range_per_writer = total_range / num_writers;
    int start = 2 + (id - 1) * range_per_writer;
    int end = (id == num_writers) ? max_range : start + range_per_writer;
    
    for (int num = start; num < end && running; num++) {
        if (is_prime(num)) {
            writer_enter(&lock);
            
            // Critical section: increment the shared counter
            prime_count++;
            log_message(THREAD_WRITER, id, "found prime %d, count = %d", num, prime_count);
            
            writer_exit(&lock);
            
            // In vanilla mode, reduce sleep to increase race window
            if (mode == VANILLA) {
                usleep(100);  // Very short sleep to maximize contention
            } else {
                random_sleep(20, 80);
            }
        }
    }
    
    log_message(THREAD_WRITER, id, "finished checking range [%d, %d)", start, end);
    return NULL;
}

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --readers N       Number of reader threads (default: 5)\n");
    printf("  --writers N       Number of writer threads (default: 3)\n");
    printf("  --mode MODE       Synchronization mode: vanilla, reader_pref, writer_pref, fair (default: reader_pref)\n");
    printf("  --duration N      Duration in seconds (default: 10)\n");
    printf("  --range N         Maximum number to check primes up to (default: 1000)\n");
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
        } else if (strcmp(argv[i], "--range") == 0 && i + 1 < argc) {
            max_range = atoi(argv[++i]);
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
    
    printf("=== Prime Counter - Reader-Writer Problem ===\n");
    printf("Mode: %s\n", rw_mode_name(mode));
    printf("Readers: %d, Writers: %d, Duration: %d seconds\n", 
           num_readers, num_writers, duration);
    printf("Checking primes in range: [2, %d)\n\n", max_range);
    
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
    printf("Final prime count: %d\n", prime_count);
    
    // Calculate expected count for verification
    int expected = 0;
    int total_range = max_range - 2;
    int range_per_writer = total_range / num_writers;
    for (int w = 0; w < num_writers; w++) {
        int start = 2 + w * range_per_writer;
        int end = (w == num_writers - 1) ? max_range : start + range_per_writer;
        for (int num = start; num < end; num++) {
            if (is_prime(num)) expected++;
        }
    }
    printf("Expected prime count: %d\n", expected);
    
    if (mode == VANILLA && prime_count != expected) {
        printf("⚠️  RACE CONDITION DETECTED: Lost updates due to no synchronization!\n");
    } else if (prime_count == expected) {
        printf("✓ Count is correct!\n");
    }
    
    // Cleanup
    rw_destroy(&lock);
    log_destroy();
    
    return 0;
}
