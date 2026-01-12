# Reader-Writer Problem - OS Project

Dự án môn Hệ điều hành về bài toán **Reader-Writer Problem** được viết bằng C với POSIX threads.

## 📋 Tông quan

Project này bao gồm **2 phiên bản** minh họa bài toán Reader-Writer với các tài nguyên chia sẻ khác nhau:

1. **Version 1 - Prime Counter**: Đếm số nguyên tố với biến đếm chung (demo lost updates)
2. **Version 2 - Shared String**: Chuỗi ký tự được cập nhật liên tục (demo torn reads)

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
├── logs/                  # Thư mục chứa test logs
├── analyze_race.py        # Script phân tích race conditions
├── analyze_comprehensive.py  # Script phân tích toàn diện
├── run_tests.sh           # Chạy 32 tests tự động
├── Makefile               # Makefile tổng
└── README.md
```

## 🔧 Build

### Build tất cả các phiên bản:

```bash
make all
```

###Build từng phiên bản:

```bash
make version1  # Prime counter
make version2  # Shared string
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

## 🧪 Comprehensive Test Results

Project này đã được test toàn diện với **32 runs** (4 modes × 4 runs × 2 versions):

```
======================================================================
VERSION 1: Prime Counter (Lost Updates Detection)
======================================================================

✗ vanilla         : 3/4 runs clean  (avg 2.0 lost updates)
✓ reader_pref     : 4/4 runs clean
✓ writer_pref     : 4/4 runs clean
✓ fair            : 4/4 runs clean

======================================================================
VERSION 2: Shared String (Torn Reads Detection)  
======================================================================

✗ vanilla         : 0/4 runs clean  (avg 362 torn reads)
✓ reader_pref     : 4/4 runs clean
✓ writer_pref     : 4/4 runs clean
✓ fair            : 4/4 runs clean
```

**Kết luận:**
- ✓ Vanilla mode **đúng như mong đợi** xuất hiện race conditions
- ✓ Tất cả synchronized modes (reader_pref, writer_pref, fair) **100% correct**
- ✓ Hệ thống hoạt động chính xác như thiết kế

### Chạy Tests Tự Động

```bash
# Chạy 32 tests và phân tích tự động
./run_tests.sh

# Phân tích một log file cụ thể
python3 analyze_race.py logs/v2_vanilla_run1_SESSION.txt

# Xem kết quả tổng hợp
cat results_SESSION.txt
```

---

## 🧪 Test cases quan trọng

### 1. Demo Race Condition (Vanilla Mode)

```bash
# Version 1: Kết quả đếm sẽ SAI (lost updates)
./version1_prime/prime_counter --mode vanilla --readers 5 --writers 8 --duration 8

# Version 2: Sẽ thấy chuỗi BỊ XÉ (torn reads)
./version2_string/shared_string --mode vanilla --readers 5 --writers 8 --duration 8
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
./version1_prime/prime_counter --mode fair --readers 20 --writers 20 --duration 10
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
- ⚠️ Prime counter: Kết quả < expected (lost updates ~1-3 mỗi run)
- ⚠️ Shared string: Thấy chuỗi lẫn lộn, bị xé (~300-400 torn reads)

### Reader/Writer/Fair Preference
- ✓ Không có race condition (100% correct)
- ⚠️ Reader/Writer pref có thể có starvation
- ✓ Fair mode: Cân đối, không starvation

## 📚 Tài liệu tham khảo

- **Readers-Writers Problem**: Classic synchronization problem
- **POSIX Threads**: `pthread_mutex`, `pthread_cond`
- **Turnstile Pattern**: Fair scheduling technique

## 👨‍💻 Tác giả

OS Course Project - 2025

## 📄 License

Educational use only.
