CC = gcc
CFLAGS = -Wall -Wextra -pthread -I../common
LDFLAGS = -pthread

TARGET = file_sim
SOURCES = file_sim.c ../common/rw_lock.c ../common/logger.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
