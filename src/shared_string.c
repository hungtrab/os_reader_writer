#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "../utils/rw_lock.h"
#include "../utils/logger.h"

#define STRING_SIZE 256

// Shared resource
rw_lock_t lock;

// Configuration
int num_readers = 10;
int num_writers = 10;
int duration = 10;  // seconds
rw_mode_t mode = READER_PREF;
bool running = true;

char shared_string[STRING_SIZE];
// Predefined sentences for writers to cycle through
const char* sentences[] = {
    "A",
    "Hello World!",
    "The quick brown fox jumps over the lazy dog.",
    "Operating systems manage hardware and software resources.",
    "X",
    "Synchronization prevents race conditions in concurrent programs.",
    "Readers and writers must coordinate access to shared data.",
    "Race!",
    "Mutual exclusion ensures only one writer at a time.",
    "Pthread library provides powerful threading primitives.",
    "Concurrency bugs are difficult to reproduce and debug consistently.",
    "AB",
    "Memory barriers ensure proper ordering of operations across cores.",
    "Deadlock occurs when threads wait indefinitely for each other.",
    "Test",
    "Lock-free data structures use atomic operations for synchronization.",
    "Thread pools improve performance by reusing worker threads efficiently.",
    "!",
    "Context switching between threads has performance overhead costs.",
    "Critical sections must be kept as short as possible for efficiency."
};
const int num_sentences = sizeof(sentences) / sizeof(sentences[0]);
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
        
        // Critical section: read the shared string
        char local_buffer[STRING_SIZE];
        strncpy(local_buffer, shared_string, STRING_SIZE - 1);
        local_buffer[STRING_SIZE - 1] = '\0';
        
        reader_exit(&lock);
        
        log_message(THREAD_READER, id, "read: \"%s\"", local_buffer);
        
        random_sleep(20, 60);
    }
    
    log_message(THREAD_READER, id, "finished");
    return NULL;
}

// Writer thread function
void* writer_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_WRITER, id, "started");
    
    int sentence_idx = 0;
    
    while (running) {
        const char* sentence = sentences[sentence_idx];
        
        writer_enter(&lock);
        
        // Critical section: write to shared string
        // In vanilla mode, this character-by-character copy can create torn reads
        if (mode == VANILLA) {
            // Deliberately slow character-by-character copy to increase race window
            for (int i = 0; sentence[i] != '\0' && i < STRING_SIZE - 1; i++) {
                shared_string[i] = sentence[i];
                // Small delay to make races more visible
                usleep(100);
            }
            shared_string[strlen(sentence)] = '\0';
        } else {
            // Normal copy
            strncpy(shared_string, sentence, STRING_SIZE - 1);
            shared_string[STRING_SIZE - 1] = '\0';
        }
        
        log_message(THREAD_WRITER, id, "wrote: \"%s\"", sentence);
        
        writer_exit(&lock);
        
        sentence_idx = (sentence_idx + 1) % num_sentences;
        random_sleep(30, 100);
    }
    
    log_message(THREAD_WRITER, id, "finished");
    return NULL;
}

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --readers N       Number of reader threads (default: 3)\n");
    printf("  --writers N       Number of writer threads (default: 3)\n");
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
    
    // Initialize shared string
    strncpy(shared_string, "Initial string content.", STRING_SIZE - 1);
    shared_string[STRING_SIZE - 1] = '\0';
    
    printf("=== Shared String - Reader-Writer Problem ===\n");
    printf("Mode: %s\n", rw_mode_name(mode));
    printf("Readers: %d, Writers: %d, Duration: %d seconds\n", 
           num_readers, num_writers, duration);
    
    if (mode == VANILLA) {
        printf("⚠️  WARNING: Vanilla mode - expect to see TORN READS (corrupted strings)!\n");
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
    printf("\n=== Program Finished ===\n");
    printf("Final shared string: \"%s\"\n", shared_string);
    
    // Cleanup
    rw_destroy(&lock);
    log_destroy();
    
    return 0;
}
