CC = gcc
CFLAGS = -Wall -Wextra -pthread -I../utils
LDFLAGS = -pthread

TARGET = shared_string
SOURCES = shared_string.c ../utils/rw_lock.c ../utils/logger.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
