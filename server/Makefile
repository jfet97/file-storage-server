# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread -Wno-unused-value
TARGET = main

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

simple-queue:
	cd ../simple-queue/ && $(MAKE) all

main: main.c ../simple-queue/libsimple_queue.a ../simple-queue/simple_queue.h
	$(CC) $(CFLAGS) $^ -I ../simple-queue/ -L ../simple-queue/ -lsimple_queue -o $@

clean:
	-rm -f $(TARGET)
