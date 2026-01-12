# Reader-Writer Problem - OS Project

Dự án môn Hệ điều hành về bài toán **Reader-Writer Problem** được viết bằng C với POSIX threads.

## 📋 Tổng quan

Project này bao gồm **3 phiên bản** minh họa bài toán Reader-Writer với các tài nguyên chia sẻ khác nhau:

1. **Version 1 - Prime Counter**: Đếm số nguyên tố với biến đếm chung
2. **Version 2 - Shared String**: Chuỗi ký tự được cập nhật liên tục 
3. **Version 3 - File Simulation**: Mô phỏng file buffer trong bộ nhớ

Mỗi phiên bản hỗ trợ **4 chế độ đồng bộ**:

- `vanilla` - Không đồng bộ (demo race condition)
- `reader_pref` - Ưu tiên reader (có thể starve writer)
- `writer_pref` - Ưu tiên writer (có thể starve reader)
- `fair` - Công bằng với turnstile pattern

## 🏗️ Cấu trúc thư mục

```
Project/
├── common/
│   ├── rw_lock.h          # API lock thống nhất cho 4 chế độ
│   ├── rw_lock.c          # Triển khai các thuật toán đồng bộ
│   ├── logger.h           # Logging với timestamp
│   └── logger.c
├── version1_prime/
│   ├── prime_counter.c    # Chương trình đếm số nguyên tố
│   └── Makefile
├── version2_string/
│   ├── shared_string.c    # Chương trình chuỗi chung
│   └── Makefile
├── version3_file/
│   ├── file_sim.c         # Chương trình mô phỏng file
│   └── Makefile
├── Makefile               # Makefile tổng
└── README.md
```

## 🔧 Build

### Build tất cả các phiên bản:

```bash
make all
```

### Build từng phiên bản:

```bash
make version1  # Prime counter
make version2  # Shared string
make version3  # File simulation
```

### Clean:

```bash
make clean
```

## 🚀 Cách chạy

### Version 1: Prime Counter

```bash
cd version1_prime

# Reader preference (mặc định)
./prime_counter --readers 5 --writers 3 --duration 10 --mode reader_pref

# Vanilla mode (xem race condition)
./prime_counter --readers 5 --writers 5 --duration 5 --mode vanilla

# Writer preference
./prime_counter --readers 10 --writers 2 --duration 10 --mode writer_pref

# Fair mode
./prime_counter --readers 10 --writers 10 --duration 10 --mode fair
```

**Cách hoạt động:**
- Writer threads: Tìm số nguyên tố trong khoảng được gán, tăng `prime_count` khi tìm thấy
- Reader threads: Đọc và in giá trị `prime_count` định kỳ
- Vanilla mode sẽ cho kết quả sai do lost update trong `prime_count++`

---

### Version 2: Shared String

```bash
cd version2_string

# Reader preference
./shared_string --readers 3 --writers 3 --duration 10 --mode reader_pref

# Vanilla mode (xem torn reads - chuỗi bị xé)
./shared_string --readers 3 --writers 3 --duration 5 --mode vanilla

# Writer preference
./shared_string --readers 2 --writers 20 --duration 10 --mode writer_pref

# Fair mode
./shared_string --readers 20 --writers 20 --duration 10 --mode fair
```

**Cách hoạt động:**
- Writer threads: Copy các câu tiếng Anh vào `shared_string` theo vòng lặp
- Reader threads: Đọc và in chuỗi
- Vanilla mode: Reader có thể thấy "torn reads" (nửa câu cũ + nửa câu mới)

---

### Version 3: File Simulation

```bash
cd version3_file

# Reader preference
./file_sim --readers 4 --writers 4 --duration 10 --mode reader_pref

# Vanilla mode (xem data corruption)
./file_sim --readers 4 --writers 4 --duration 5 --mode vanilla

# Writer preference  
./file_sim --readers 2 --writers 15 --duration 10 --mode writer_pref

# Fair mode
./file_sim --readers 15 --writers 15 --duration 10 --mode fair
```

**Cách hoạt động:**
- Writer threads: Append log lines vào buffer
- Reader threads: Đọc snapshot của buffer và đếm số dòng
- Vanilla mode: Độ dài buffer không nhất quán, dữ liệu bị rách

---

## 📊 Các tham số CLI

Tất cả các chương trình đều hỗ trợ các tham số:

| Tham số | Mô tả | Mặc định |
|---------|-------|----------|
| `--readers N` | Số lượng reader threads | 3-5 |
| `--writers N` | Số lượng writer threads | 3-4 |
| `--mode MODE` | Chế độ: `vanilla`, `reader_pref`, `writer_pref`, `fair` | `reader_pref` |
| `--duration N` | Thời gian chạy (giây) | 10 |
| `--help` | Hiển thị trợ giúp | - |

## 🧪 Test cases quan trọng

### 1. Demo Race Condition (Vanilla Mode)

```bash
# Version 1: Kết quả đếm sẽ SAI
./version1_prime/prime_counter --mode vanilla --readers 5 --writers 5 --duration 5

# Version 2: Sẽ thấy chuỗi BỊ XÉ
./version2_string/shared_string --mode vanilla --readers 3 --writers 3 --duration 5

# Version 3: Buffer length và nội dung KHÔNG NHẤT QUÁN
./version3_file/file_sim --mode vanilla --readers 4 --writers 4 --duration 5
```

### 2. Demo Writer Starvation (Reader Preference)

```bash
# Nhiều readers liên tục → writer phải chờ lâu
./version1_prime/prime_counter --mode reader_pref --readers 50 --writers 2 --duration 10
```

### 3. Demo Reader Starvation (Writer Preference)

```bash
# Nhiều writers liên tục → reader phải chờ lâu
./version2_string/shared_string --mode writer_pref --readers 2 --writers 20 --duration 10
```

### 4. Demo Fairness

```bash
# Cả reader và writer đều có thời gian chờ hợp lý
./version3_file/file_sim --mode fair --readers 20 --writers 20 --duration 10
```

## 📝 Giải thích các chế độ đồng bộ

### 1. Vanilla (No Synchronization)

- **Không có lock nào**
- Cho phép race condition để demo
- Kết quả: Lost updates, torn reads, data corruption

### 2. Reader Preference

**Thuật toán:**
- Reader đầu tiên khóa `resource_lock`
- Reader cuối cùng mở `resource_lock`
- Nhiều reader đọc song song
- Writer phải chờ khi có reader

**Nhược điểm:** Writer starvation nếu reader đến liên tục

### 3. Writer Preference

**Thuật toán:**
- Khi có writer đang chờ, block reader mới
- Sử dụng `read_try` mutex làm "gate"
- Writer được ưu tiên

**Nhược điểm:** Reader starvation nếu writer đến liên tục

### 4. Fair (Turnstile Pattern)

**Thuật toán:**
- Mọi thread phải qua `queue_lock` trước
- Ngăn không cho một bên "chen ngang" liên tục
- Reader vẫn đọc song song sau khi qua cổng

**Ưu điểm:** Giảm starvation, công bằng hơn

## 🎯 Kết quả mong đợi

### Vanilla Mode
- ⚠️ Prime counter: Kết quả < expected (lost updates)
- ⚠️ Shared string: Thấy chuỗi lẫn lộn, bị xé
- ⚠️ File simulation: Độ dài buffer sai, nội dung rách

### Reader/Writer Preference
- ✓ Không có race condition
- ⚠️ Có hiện tượng starvation (một bên chờ quá lâu)

### Fair Mode
- ✓ Không có race condition  
- ✓ Cả hai bên đều tiến triển hợp lý
- ✓ Throughput có thể thấp hơn một chút do overhead

## 📚 Tài liệu tham khảo

- **Readers-Writers Problem**: Classic synchronization problem
- **POSIX Threads**: `pthread_mutex`, `pthread_cond`
- **Turnstile Pattern**: Fair scheduling technique

## 👨‍💻 Tác giả

OS Course Project - 2025

## 📄 License

Educational use only.
