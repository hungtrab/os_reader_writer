# Hướng dẫn viết báo cáo (Report Guide)

## Cấu trúc báo cáo đề xuất

### 1. Giới thiệu (Introduction)
- Bài toán Reader-Writer là gì?
- Tại sao quan trọng trong OS? (database, file system, cache...)
- Mục tiêu project: So sánh 4 chính sách đồng bộ

### 2. Lý thuyết (Theory)

#### 2.1 Bài toán Reader-Writer
- Định nghĩa: Nhiều threads truy cập tài nguyên chung
- Yêu cầu:
  - Nhiều reader đọc đồng thời (OK)
  - Writer phải có quyền độc quyền (exclusive)
  - Không có reader khi writer đang ghi

#### 2.2 Vấn đề Starvation
- Reader starvation: Writer đến liên tục
- Writer starvation: Reader đến liên tục

### 3. Thiết kế hệ thống (System Design)

#### 3.1 Kiến trúc chung
```
Common Infrastructure:
  - rw_lock API (4 modes)
  - Thread-safe logger
  
Three Versions:
  - Version 1: Prime counting
  - Version 2: Shared string
  - Version 3: File simulation
```

#### 3.2 API đồng bộ
```c
typedef enum {
    VANILLA, READER_PREF, WRITER_PREF, FAIR
} rw_mode_t;

void reader_enter(rw_lock_t *lock);
void reader_exit(rw_lock_t *lock);
void writer_enter(rw_lock_t *lock);
void writer_exit(rw_lock_t *lock);
```

### 4. Các giải pháp đồng bộ (Synchronization Solutions)

#### 4.1 Vanilla (No Sync)
**Mục đích:** Demo race condition

**Kết quả quan sát được:**
- Prime counter: 178/431 (lost 58% updates)
- Shared string: Torn reads
  - Ví dụ: `"Syncating systems..."` (2 câu lẫn lộn)

#### 4.2 Reader Preference
**Pseudocode:**
```
reader_enter():
    lock(mutex)
    read_count++
    if read_count == 1:
        lock(resource)
    unlock(mutex)

reader_exit():
    lock(mutex)
    read_count--
    if read_count == 0:
        unlock(resource)
    unlock(mutex)

writer_enter():
    lock(resource)

writer_exit():
    unlock(resource)
```

**Ưu điểm:**
- Đơn giản, dễ hiểu
- Reader không block lẫn nhau

**Nhược điểm:**
- Writer có thể bị starve nếu reader liên tục

**Test case minh họa starvation:**
```bash
./prime_counter --readers 50 --writers 2 --duration 10 --mode reader_pref
# => Writer phải chờ lâu
```

#### 4.3 Writer Preference
**Pseudocode:**
```
reader_enter():
    lock(read_try)  # Block nếu có writer
    lock(mutex)
    read_count++
    if read_count == 1:
        lock(resource)
    unlock(mutex)
    unlock(read_try)

writer_enter():
    lock(mutex)
    waiting_writers++
    unlock(mutex)
    lock(read_try)    # Block reader mới
    lock(resource)
    lock(mutex)
    waiting_writers--
    unlock(mutex)

writer_exit():
    unlock(resource)
    unlock(read_try)
```

**Ưu điểm:**
- Writer không bị starve
- Ưu tiên ghi (quan trọng với update)

**Nhược điểm:**
- Reader có thể bị starve

**Test case:**
```bash
./shared_string --readers 2 --writers 20 --duration 10 --mode writer_pref
# => Reader phải chờ lâu
```

#### 4.4 Fair (Turnstile)
**Pseudocode:**
```
reader_enter():
    lock(queue)     # Turnstile
    lock(mutex)
    read_count++
    if read_count == 1:
        lock(resource)
    unlock(mutex)
    unlock(queue)   # Qua cổng

writer_enter():
    lock(queue)     # Turnstile
    lock(resource)
    unlock(queue)   # Qua cổng
```

**Ưu điểm:**
- Công bằng, không starve
- FIFO-like behavior

**Nhược điểm:**
- Overhead cao hơn một chút

**Test case:**
```bash
./file_sim --readers 20 --writers 20 --duration 10 --mode fair
# => Cả 2 bên đều tiến triển
```

### 5. Hiện thực (Implementation)

#### 5.1 Version 1: Prime Counter
**Tài nguyên chung:** `int prime_count`
**Writer:** Tìm số nguyên tố trong khoảng → tăng counter
**Reader:** Đọc giá trị counter

**Code quan trọng:**
```c
// Writer (critical section)
writer_enter(&lock);
prime_count++;  // Race condition nếu không lock!
writer_exit(&lock);

// Reader (critical section)
reader_enter(&lock);
int val = prime_count;
reader_exit(&lock);
```

#### 5.2 Version 2: Shared String
**Tài nguyên chung:** `char shared_string[256]`
**Writer:** Copy câu vào string
**Reader:** Đọc và in string

**Torn read demo (vanilla):**
- Copy chậm từng ký tự → reader thấy nửa câu cũ + nửa câu mới

#### 5.3 Version 3: File Simulation
**Tài nguyên chung:** File buffer + length
**Writer:** Append log lines
**Reader:** Snapshot buffer

### 6. Thực nghiệm (Experiments)

#### 6.1 Cấu hình test
| Test | Readers | Writers | Mode | Duration |
|------|---------|---------|------|----------|
| Race demo | 3 | 3 | vanilla | 3s |
| Writer starve | 50 | 2 | reader_pref | 10s |
| Reader starve | 2 | 20 | writer_pref | 10s |
| Fair test | 20 | 20 | fair | 10s |

#### 6.2 Kết quả

**Vanilla mode:**
- Version 1: 178/431 primes (41% correct)
- Version 2: Nhiều torn reads quan sát được
- Version 3: Data corruption

**Synchronized modes:**
- Reader/Writer pref: Đúng nhưng có starvation
- Fair: Đúng và cân bằng

### 7. Phân tích (Analysis)

#### 7.1 Correctness
Tất cả các chế độ có lock đều đảm bảo:
- ✓ Mutual exclusion cho writer
- ✓ Không có reader khi writer đang ghi
- ✓ Deadlock-free

#### 7.2 Fairness
| Mode | Reader Fairness | Writer Fairness |
|------|----------------|-----------------|
| Reader Pref | ★★★ | ★ (có thể starve) |
| Writer Pref | ★ (có thể starve) | ★★★ |
| Fair | ★★★ | ★★★ |

#### 7.3 Performance
- Reader pref: Throughput cao cho read-heavy workload
- Writer pref: Tốt cho write-heavy workload
- Fair: Cân bằng nhưng overhead cao hơn

### 8. Kết luận (Conclusion)

**Đã hoàn thành:**
- ✓ 3 phiên bản với tài nguyên khác nhau
- ✓ 4 chính sách đồng bộ
- ✓ Demo được race condition
- ✓ So sánh fairness vs performance

**Bài học:**
- Synchronization cần thiết để tránh race
- Không có giải pháp "tốt nhất" - tuỳ workload
- Trade-off giữa fairness và throughput

**Ứng dụng thực tế:**
- Database: Read-write locks
- File systems: Concurrent access
- Cache: Read-dominated workload

### 9. Tài liệu tham khảo (References)

1. Operating System Concepts (Silberschatz)
2. POSIX Threads Programming
3. The Little Book of Semaphores

---

## Phụ lục: Log examples

### Example 1: Lost Update (Vanilla)
```
[Time] [W1] found prime 1009, count = 50
[Time] [W2] found prime 2003, count = 50  # Lost W1's update!
```

### Example 2: Torn Read (Vanilla)  
```
Original: "Operating systems manage hardware..."
Read:     "Syncating systems manage hardware..."
          ^^^^^ Mixed from "Synchronization..."
```

### Example 3: Fair Scheduling
```
[Time] [W1] enters (via turnstile)
[Time] [R1] enters (via turnstile) - waited for W1
[Time] [R2] enters (parallel with R1)
[Time] [W2] enters (via turnstile) - waited for R1/R2
```
