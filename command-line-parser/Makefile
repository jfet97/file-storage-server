# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic
TARGET = libcommand-line-parser.a command-line-parser.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

libcommand-line-parser.a: command-line-parser.o
	ar rvs $@ $<

command-line-parser.o: command-line-parser.c command-line-parser.h

main: ../command-line-parser.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	-rm -f $(TARGET)
