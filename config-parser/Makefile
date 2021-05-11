# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic
TARGET = libconfig-parser.a config-parser.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

libconfig-parser.a: config-parser.o
	ar rvs $@ $<

config-parser.o: config-parser.c config-parser.h

main: ../config-parser.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	-rm -f $(TARGET)
