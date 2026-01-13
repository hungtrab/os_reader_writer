# Reader-Writer Problem - Giải Thích Chi Tiết Code & Thuật Toán

## Tổng Quan Project

Project này triển khai bài toán Reader-Writer với 4 chế độ đồng bộ khác nhau, sử dụng POSIX threads và mutex.

**Cấu trúc thư mục:**
```
Project/
├── utils/              # Các primitive đồng bộ
│   ├── rw_lock.h
│   ├── rw_lock.c
│   ├── logger.h
│   └── logger.c
├── src/                # Application code
│   └── shared_string.c
└── scripts/            # Test automation
    ├── run_tests.sh
    ├── analyze_race.py
    └── analyze_comprehensive.py
```

---

## Phần 1: RW Lock Implementation (utils/rw_lock.h & rw_lock.c)

### 1.1 Cấu Trúc Dữ Liệu

```c
typedef enum {
    VANILLA,      // Không có đồng bộ
    READER_PREF,  // Ưu tiên reader
    WRITER_PREF,  // Ưu tiên writer
    FAIR          // Công bằng
} rw_mode_t;

typedef struct {
    rw_mode_t mode;
    int active_readers;     // Số reader đang hoạt động
    int active_writers;     // Số writer đang hoạt động (0 hoặc 1)
    int waiting_writers;    // Số writer đang đợi
    pthread_mutex_t mutex;        // Bảo vệ counters
    pthread_mutex_t resource_lock; // Khóa tài nguyên thực
    pthread_mutex_t read_try;      // Cho writer pref
    pthread_mutex_t queue_lock;    // Cho fair mode (turnstile)
} rw_lock_t;
```

**Giải thích các trường:**
- `mode`: Chế độ đồng bộ hiện tại
- `active_readers`: Đếm số reader đang đọc (thread-safe với mutex)
- `active_writers`: Luôn là 0 hoặc 1 (tại một thời điểm chỉ có tối đa 1 writer)
- `waiting_writers`: Số writer đang chờ (dùng cho writer_pref)
- `mutex`: Bảo vệ các biến đếm (active_readers, waiting_writers...)
- `resource_lock`: Khóa thực sự bảo vệ shared resource
- `read_try`: Khóa ngăn reader mới vào khi có writer chờ (writer_pref)
- `queue_lock`: "Turnstile" đảm bảo FIFO (fair mode)

### 1.2 Thuật Toán 1: VANILLA MODE (No Synchronization)

**Mục đích:** Chứng minh race condition - không nên dùng trong thực tế!

**Reader Enter:**
```c
void reader_enter(rw_lock_t *lock) {
    if (lock->mode == VANILLA) {
        lock->active_readers++;  // Không có lock!
        return;
    }
    // ...
}
```

**Writer Enter:**
```c
void writer_enter(rw_lock_t *lock) {
    if (lock->mode == VANILLA) {
        lock->active_writers++;  // Không có lock!
        return;
    }
    // ...
}
```

**Vấn đề:**
- `active_readers++` và `active_writers++` KHÔNG atomic
- Assembly: LOAD → ADD → STORE (3 bước)
- Race condition xảy ra khi 2 threads interleave
- Dẫn đến: torn reads, lost updates, data corruption

**Kết quả thực tế:** 367 torn reads trung bình mỗi 8 giây!

---

### 1.3 Thuật Toán 2: READER PREFERENCE

**Ý tưởng:** Reader đầu tiên khóa resource, readers tiếp theo chỉ tăng counter. Writer phải đợi TẤT CẢ readers.

**Reader Enter:**
```c
void reader_enter(rw_lock_t *lock) {
    case READER_PREF:
        pthread_mutex_lock(&lock->mutex);      // [1]
        lock->active_readers++;                 // [2]
        if (lock->active_readers == 1) {       // [3]
            pthread_mutex_lock(&lock->resource_lock); // [4]
        }
        pthread_mutex_unlock(&lock->mutex);    // [5]
        break;
}
```

**Giải thích từng bước:**
1. **[1]** Khóa mutex để bảo vệ counter
2. **[2]** Tăng số reader đang hoạt động
3. **[3]** Nếu là reader ĐẦU TIÊN (1st)
4. **[4]** → Khóa resource (ngăn writer vào)
5. **[5]** Mở mutex (cho readers khác vào)

**Reader Exit:**
```c
void reader_exit(rw_lock_t *lock) {
    case READER_PREF:
        pthread_mutex_lock(&lock->mutex);      // [1]
        lock->active_readers--;                 // [2]
        if (lock->active_readers == 0) {       // [3]
            pthread_mutex_unlock(&lock->resource_lock); // [4]
        }
        pthread_mutex_unlock(&lock->mutex);    // [5]
        break;
}
```

**Giải thích:**
1. **[1]** Khóa mutex
2. **[2]** Giảm counter
3. **[3]** Nếu là reader CUỐI CÙNG
4. **[4]** → Mở resource (cho writer vào)
5. **[5]** Mở mutex

**Writer Enter:**
```c
void writer_enter(rw_lock_t *lock) {
    case READER_PREF:
        pthread_mutex_lock(&lock->resource_lock); // Đợi ALL readers xong
        break;
}
```

**Writer Exit:**
```c
void writer_exit(rw_lock_t *lock) {
    case READER_PREF:
        pthread_mutex_unlock(&lock->resource_lock);
        break;
}
```

**Ưu điểm:**
- Nhiều readers đọc ĐỒNG THỜI (không block nhau)
- Reader latency thấp (chỉ cần kiểm tra counter)
- Throughput cao cho read-heavy workload

**Nhược điểm: WRITER STARVATION**
- Nếu readers liên tục đến: `active_readers` KHÔNG BAO GIỜ về 0
- Writer phải chờ `resource_lock` VÔ HẠN
- Ví dụ: R1 vào → R2 vào → R1 ra → R3 vào → R2 ra → R4 vào...
  - `active_readers` luôn ≥ 1
  - Writer KHÔNG BAO GIỜ vào được!

---

### 1.4 Thuật Toán 3: WRITER PREFERENCE

**Ý tưởng:** Khi có writer chờ, CHẶN readers mới. Ưu tiên writer để tránh starvation.

**Writer Enter:**
```c
void writer_enter(rw_lock_t *lock) {
    case WRITER_PREF:
        pthread_mutex_lock(&lock->mutex);         // [1]
        lock->waiting_writers++;                   // [2]
        pthread_mutex_unlock(&lock->mutex);       // [3]
        
        pthread_mutex_lock(&lock->read_try);      // [4]
        pthread_mutex_lock(&lock->resource_lock); // [5]
        
        pthread_mutex_lock(&lock->mutex);         // [6]
        lock->waiting_writers--;                   // [7]
        pthread_mutex_unlock(&lock->mutex);       // [8]
        break;
}
```

**Giải thích:**
1. **[1-3]** Tăng `waiting_writers` (báo hiệu có writer chờ)
2. **[4]** Khóa `read_try` → CHẶN readers mới vào!
3. **[5]** Đợi `resource_lock` (readers cũ đang đọc xong)
4. **[6-8]** Giảm `waiting_writers`, bắt đầu ghi

**Reader Enter:**
```c
void reader_enter(rw_lock_t *lock) {
    case WRITER_PREF:
        pthread_mutex_lock(&lock->read_try);      // [1]
        pthread_mutex_lock(&lock->mutex);         // [2]
        lock->active_readers++;                    // [3]
        if (lock->active_readers == 1) {          // [4]
            pthread_mutex_lock(&lock->resource_lock);
        }
        pthread_mutex_unlock(&lock->mutex);       // [5]
        pthread_mutex_unlock(&lock->read_try);    // [6]
        break;
}
```

**Giải thích:**
1. **[1]** Phải lấy `read_try` TRƯỚC
   - Nếu writer đang chờ → writer đã khóa `read_try`
   - → Reader BỊ CHẶN tại đây!
2. **[2-5]** Logic giống reader_pref (tăng counter, lock resource nếu cần)
3. **[6]** Mở `read_try` cho reader tiếp theo

**Cơ chế hoạt động:**

**Khi không có writer:**
- `read_try` mở → Readers vào tự do như reader_pref

**Khi có writer chờ:**
1. Writer khóa `read_try` tại bước [4] của writer_enter
2. Readers mới đến → BỊ CHẶN tại [1] của reader_enter
3. Readers cũ (đã vào trước) → Đọc xong và ra
4. Khi `active_readers` = 0 → Writer lấy được `resource_lock`
5. Writer hoàn thành → Mở `read_try` → Readers mới vào

**Ưu điểm:**
- Ngăn writer starvation
- Writer được ưu tiên khi chờ

**Nhược điểm: READER STARVATION**
- Nếu writers liên tục đến → Reader phải chờ lâu
- Readers bị delay bởi `read_try` lock

---

### 1.5 Thuật Toán 4: FAIR MODE (Turnstile Pattern)

**Ý tưởng:** FIFO queue - ai đến trước vào trước. Dùng "turnstile" (cửa xoay) để đảm bảo thứ tự.

**Reader Enter:**
```c
void reader_enter(rw_lock_t *lock) {
    case FAIR:
        pthread_mutex_lock(&lock->queue_lock);    // [1] Vào turnstile
        pthread_mutex_lock(&lock->mutex);         // [2]
        lock->active_readers++;                    // [3]
        if (lock->active_readers == 1) {          // [4]
            pthread_mutex_lock(&lock->resource_lock);
        }
        pthread_mutex_unlock(&lock->mutex);       // [5]
        pthread_mutex_unlock(&lock->queue_lock);  // [6] Ra khỏi turnstile
        break;
}
```

**Writer Enter:**
```c
void writer_enter(rw_lock_t *lock) {
    case FAIR:
        pthread_mutex_lock(&lock->queue_lock);    // [1] Vào turnstile
        pthread_mutex_lock(&lock->resource_lock); // [2] Lấy resource
        pthread_mutex_unlock(&lock->queue_lock);  // [3] Ra khỏi turnstile
        break;
}
```

**Cơ chế Turnstile:**

**Hình dung:** Cửa xoay một chiều - mỗi lần chỉ 1 người qua.

**Khi thread muốn vào:**
1. Thread lấy `queue_lock` (vào cửa xoay)
2. Thread lấy locks cần thiết (mutex, resource_lock...)
3. Thread MỞ `queue_lock` (ra khỏi cửa xoay)
4. Thread xử lý (đọc hoặc ghi)

**Tại sao công bằng?**

Giả sử thứ tự đến: R1 → W1 → R2 → R3

**Không có turnstile (reader_pref):**
- R1 vào
- W1 chờ
- R2 vào (vì `active_readers` > 0)
- R3 vào (vì `active_readers` > 0)
- W1 phải đợi R2, R3 xong → KHÔNG CÔNG BẰNG!

**Có turnstile (fair):**
- R1 lấy `queue_lock` → vào → mở `queue_lock`
- W1 lấy `queue_lock` → BỊ CHẶN (phải đợi R1 mở)
- R2 muốn lấy `queue_lock` → BỊ CHẶN (W1 đang giữ)
- R1 xong → Mở resource → W1 lấy được `queue_lock`
- W1 vào → mở `queue_lock`
- R2 lấy `queue_lock` → vào
- → Đúng thứ tự FIFO!

**Ưu điểm:**
- KHÔNG AI BỊ STARVATION
- Thứ tự FIFO công bằng
- Latency dự đoán được

**Nhược điểm:**
- Overhead thêm 1 mutex (`queue_lock`)
- Throughput giảm nhẹ (~5-10%)

---

## Phần 2: Logger (utils/logger.h & logger.c)

### 2.1 Cấu Trúc

```c
typedef enum {
    THREAD_READER,
    THREAD_WRITER
} thread_type_t;

typedef struct {
    pthread_mutex_t log_mutex;  // Bảo vệ việc ghi log
    bool initialized;
} logger_t;
```

### 2.2 Các Hàm

**log_init():**
```c
void log_init(void) {
    pthread_mutex_init(&logger.log_mutex, NULL);
    logger.initialized = true;
}
```

**log_message():**
```c
void log_message(thread_type_t type, int id, const char* format, ...) {
    pthread_mutex_lock(&logger.log_mutex);  // [1]
    
    // Lấy timestamp
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // In: [HH:MM:SS.mmm] [R/W-id] message
    printf("[%02d:%02d:%02d.%03ld] [%s%d] ",
           hours, mins, secs, ms,
           type == THREAD_READER ? "R" : "W", id);
    
    vprintf(format, args);  // In message với args
    printf("\n");
    
    pthread_mutex_unlock(&logger.log_mutex); // [2]
}
```

**Tại sao cần log_mutex?**
- `printf()` KHÔNG thread-safe
- Nhiều threads gọi printf đồng thời → output bị xen kẽ
- Ví dụ: "Thre[R1ad] 1 rea[R2]d: Hel readlo:  Wor..."
- `log_mutex` đảm bảo mỗi log message được in hoàn chỉnh

---

## Phần 3: Shared String Application (src/shared_string.c)

### 3.1 Cấu Trúc Dữ Liệu

```c
#define STRING_SIZE 256

char shared_string[STRING_SIZE];  // Tài nguyên chia sẻ
rw_lock_t lock;                   // RW lock

// Configuration
int num_readers = 3;
int num_writers = 3;
int duration = 10;
rw_mode_t mode = READER_PREF;
bool running = true;

// Valid sentences (20 câu)
const char* sentences[] = {
    "A",
    "Hello World!",
    "The quick brown fox jumps over the lazy dog.",
    // ... 17 câu khác
};
```

### 3.2 Writer Thread

```c
void* writer_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_WRITER, id, "started");
    
    while (running) {
        // [1] Chọn câu random
        int idx = rand() % num_sentences;
        const char* sentence = sentences[idx];
        
        writer_enter(&lock);  // [2] Lấy writer lock
        
        // [3] CRITICAL SECTION - Copy sentence
        if (mode == VANILLA) {
            // Vanilla: Copy từng ký tự với delay
            for (int i = 0; sentence[i] != '\0' && i < STRING_SIZE - 1; i++) {
                shared_string[i] = sentence[i];
                usleep(100);  // Delay cố ý để tạo race!
            }
            shared_string[strlen(sentence)] = '\0';
        } else {
            // Synchronized: Copy nhanh
            strncpy(shared_string, sentence, STRING_SIZE - 1);
            shared_string[STRING_SIZE - 1] = '\0';
        }
        
        log_message(THREAD_WRITER, id, "wrote: \"%s\"", sentence);
        
        writer_exit(&lock);  // [4] Mở lock
        
        random_sleep(100, 300);  // [5] Ngủ trước khi ghi tiếp
    }
    
    return NULL;
}
```

**Giải thích:**
1. **[1]** Chọn 1 trong 20 câu random
2. **[2]** Vào critical section (với lock nếu synchronized)
3. **[3]** Copy sentence vào `shared_string`
   - **Vanilla:** Copy từng char với `usleep(100)` → TẠO RACE CỐ Ý!
   - **Synchronized:** Copy nhanh bằng `strncpy`
4. **[4]** Ra khỏi critical section
5. **[5]** Ngủ ngẫu nhiên 100-300ms

**Tại sao vanilla copy từng char với delay?**
- Để TĂNG XÁC SUẤT race condition
- Nếu copy nhanh → race ít xảy ra → khó chứng minh
- Với delay → nhiều writers có thời gian "cướp" nhau

### 3.3 Reader Thread

```c
void* reader_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);
    
    log_message(THREAD_READER, id, "started");
    
    while (running) {
        reader_enter(&lock);  // [1] Lấy reader lock
        
        // [2] CRITICAL SECTION - Đọc string
        char buffer[STRING_SIZE];
        strncpy(buffer, shared_string, STRING_SIZE - 1);
        buffer[STRING_SIZE - 1] = '\0';
        
        log_message(THREAD_READER, id, "read: \"%s\"", buffer);
        
        reader_exit(&lock);  // [3] Mở lock
        
        random_sleep(50, 150);  // [4] Ngủ trước khi đọc tiếp
    }
    
    return NULL;
}
```

**Giải thích:**
1. **[1]** Vào critical section
2. **[2]** Copy `shared_string` vào buffer local
   - Tại sao copy? Để log được sau khi exit lock
3. **[3]** Ra khỏi critical section
4. **[4]** Ngủ rồi đọc tiếp

**Torn Read xảy ra khi nào? (Vanilla mode)**

Giả sử:
- `shared_string` = "Hello World!" (12 chars)
- W1 bắt đầu ghi "Operating systems..." (char-by-char)

**Timeline:**
```
t=0:   shared_string = "Hello World!"
t=1:   W1 ghi 'O' → "Oello World!"
t=2:   W1 ghi 'p' → "Opllo World!"
t=3:   R1 ĐỌC   → "Opllo World!"  ← TORN READ!
t=4:   W1 ghi 'e' → "Opelo World!"
...
```

Reader thấy "Opllo World!" - KHÔNG phải câu hợp lệ!

### 3.4 Main Function

```c
int main(int argc, char* argv[]) {
    // [1] Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0) {
            // Parse: vanilla, reader_pref, writer_pref, fair
        }
        // ... readers, writers, duration
    }
    
    // [2] Initialize
    srand(time(NULL));
    strcpy(shared_string, "Initial string content.");
    log_init();
    rw_init(&lock, mode);
    
    // [3] Create threads
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
    
    // [4] Run for duration
    sleep(duration);
    running = false;  // Signal all threads to stop
    
    // [5] Join threads
    for (int i = 0; i < num_readers; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < num_writers; i++) {
        pthread_join(writers[i], NULL);
    }
    
    // [6] Print final result
    printf("\n=== Program Finished ===\n");
    printf("Final shared string: \"%s\"\n", shared_string);
    
    // [7] Cleanup
    rw_destroy(&lock);
    log_destroy();
    
    return 0;
}
```

---

## Phần 4: Test Automation Scripts

### 4.1 run_tests.sh

```bash
#!/bin/bash
SESSION=$(date +%Y%m%d_%H%M%S)
MODES=("vanilla" "reader_pref" "writer_pref" "fair")
RUNS=4

for mode in "${MODES[@]}"; do
    for run in $(seq 1 $RUNS); do
        logfile="logs/${mode}_run${run}_${SESSION}.txt"
        ./src/shared_string --mode $mode \
                            --writers 8 \
                            --readers 5 \
                            --duration 8 > "$logfile" 2>&1
    done
done

python3 analyze_comprehensive.py "$SESSION"
```

**Tổng:** 4 modes × 4 runs = 16 tests

### 4.2 analyze_comprehensive.py

**Cách hoạt động:**

1. **Parse log files:**
```python
for log_file in logs_dir.glob(f"*_{session_id}.txt"):
    # Parse filename: vanilla_run1_SESSION.txt
    mode = extract_mode_from_filename(log_file)
    
    # Analyze
    is_clean, torn_count = analyze_log(log_file)
    results[mode].append((is_clean, torn_count))
```

2. **Analyze single log:**
```python
def analyze_log(log_file):
    torn_reads = 0
    for line in file:
        match = re.search(r'\[R\d+\] read: "(.*?)"', line)
        if match:
            read_string = match.group(1)
            if read_string not in VALID_SENTENCES:
                torn_reads += 1  # FOUND TORN READ!
    
    return (torn_reads == 0, torn_reads)
```

3. **Generate report:**
```python
for mode in ["vanilla", "reader_pref", "writer_pref", "fair"]:
    clean_count = sum(1 for is_clean, _ in results[mode] if is_clean)
    total = len(results[mode])
    
    print(f"{mode}: {clean_count}/{total} clean")
```

---

## Phần 5: Kết Quả Thực Tế & Phân Tích

### 5.1 Session 20260113_004235

**Vanilla Mode:**
```
Run 1: 384 torn reads
Run 2: 352 torn reads
Run 3: 371 torn reads
Run 4: 361 torn reads
Average: 367 torn reads
```

**Ví dụ torn reads thực tế:**
```
"Syncating systems manage..."     ← "Sync" + "ating systems"
"Mutal exclusures..."              ← "Mutual exclu" + "sures"
"Pthread libuduce and..."          ← Character corruption
```

**Synchronized Modes:**
```
reader_pref: 4/4 clean (0 errors)
writer_pref: 4/4 clean (0 errors)
fair:        4/4 clean (0 errors)
```

### 5.2 Tại Sao Kết Quả Này Quan Trọng?

**Vanilla 0/4 clean:**
- Chứng minh race condition XẢY RA THỰC TẾ
- Không phải lý thuyết - là thực nghiệm
- 367 lỗi/8s = ~46 lỗi/giây!

**Synchronized 12/12 clean:**
- Chứng minh implementation ĐÚNG
- Không có false positive
- Tin cậy cao (100% success rate)

---

## Phần 6: Câu Hỏi Thường Gặp

### Q1: Tại sao dùng mutex chứ không phải semaphore?

**A:** Cả hai đều được! Nhưng mutex có:
- Tích hợp tốt với condition variables
- Ownership rõ ràng (thread lock phải unlock)
- POSIX support rộng rãi

### Q2: Reader preference và writer preference có phải là primitive không?

**A:** KHÔNG! Chúng là THUẬT TOÁN (algorithms/strategies). Có thể implement bằng mutex HOẶC semaphore.

### Q3: Tại sao cần `read_try` lock trong writer preference?

**A:** Để CHẶN readers mới. Nếu không có:
- Writer chờ `resource_lock`
- Readers mới vẫn vào → tăng `active_readers`
- Writer phải đợi readers mới này nữa → vẫn bị starve!

### Q4: Turnstile hoạt động như thế nào?

**A:** Giống cửa xoay 1 chiều:
- Mỗi thread phải "qua cửa" (lấy queue_lock)
- Chỉ 1 thread qua một lúc
- Ai đến trước qua trước → FIFO

### Q5: Tại sao vanilla copy char-by-char với delay?

**A:** Để TĂNG xác suất race:
- Copy nhanh → ít race → khó demo
- Copy chậm → nhiều race → chứng minh rõ

---

## Phần 7: Tips Triển Khai

### 7.1 Common Mistakes

**❌ Lỗi 1: Quên unlock**
```c
pthread_mutex_lock(&mutex);
if (error) return;  // ← BUG! Chưa unlock
pthread_mutex_unlock(&mutex);
```

**✓ Đúng:**
```c
pthread_mutex_lock(&mutex);
if (error) {
    pthread_mutex_unlock(&mutex);
    return;
}
pthread_mutex_unlock(&mutex);
```

**❌ Lỗi 2: Lock sai thứ tự (deadlock)**
```c
Thread 1: lock(A) → lock(B)
Thread 2: lock(B) → lock(A)  // ← DEADLOCK!
```

**✓ Đúng:** Luôn lock theo thứ tự nhất quán

**❌ Lỗi 3: Quên check mode**
```c
void reader_enter(rw_lock_t *lock) {
    // Quên check mode → luôn chạy reader_pref!
    pthread_mutex_lock(&lock->mutex);
    ...
}
```

### 7.2 Debug Tips

**1. Thêm logging:**
```c
log_message(READER, id, "Trying to enter...");
reader_enter(&lock);
log_message(READER, id, "Entered!");
```

**2. Kiểm tra invariants:**
```c
assert(lock->active_writers <= 1);  // Tối đa 1 writer
assert(lock->active_readers >= 0);  // Không âm
```

**3. Chạy với Valgrind:**
```bash
valgrind --tool=helgrind ./src/shared_string
```

---

## Tổng Kết

**Key Points:**
1. **4 thuật toán** giải quyết cùng vấn đề, trade-off khác nhau
2. **Mutex** là primitive, **Reader/Writer Pref** là algorithm
3. **Vanilla** chứng minh vấn đề, **Synchronized** giải quyết
4. **Automated testing** đảm bảo correctness
5. **Real data** (367 torn reads) chứng minh impact

**Khi nào dùng gì:**
- **Reader-heavy (90%+ reads):** Reader Preference
- **Write-heavy / fresh data:** Writer Preference  
- **Mixed / SLA requirements:** Fair
- **Production:** KHÔNG BAO GIỜ dùng Vanilla!
