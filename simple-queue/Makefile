# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread
TARGET = libsimple_queue.a simple_queue.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

libsimple_queue.a: simple_queue.o
	ar rvs $@ $<

simple_queue.o: simple_queue.c simple_queue.h

main: ./main.c ../simple_queue.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	-rm -f $(TARGET)
