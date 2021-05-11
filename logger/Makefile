# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread
TARGET = liblogger.a logger.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

liblogger.a: logger.o
	ar rvs $@ $<

logger.o: logger.c logger.h

main: ../logger.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	-rm -f $(TARGET)
