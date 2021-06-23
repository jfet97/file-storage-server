# make -> compile all the binaries 
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value
TARGETS = liblist.a list.o

nosuppress: CFLAGS += -Wunused-value
nosuppress: clean all

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGETS)

liblist.a: list.o
	 ar rvs $@ $<

list.o: list.c list.h
	$(CC) $(CFLAGS) $< -c -o $@

clean: 
	-rm -f $(TARGETS)