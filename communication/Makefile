# make -> compile all the binaries 
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value
TARGETS = libcommunication.a communication.o

nosuppress: CFLAGS += -Wunused-value
nosuppress: clean all

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGETS)

libcommunication.a: communication.o
	 ar rvs $@ $<

communication.o: communication.c communication.h
	$(CC) $(CFLAGS) $< -c -o $@

clean: 
	-rm -f $(TARGETS)