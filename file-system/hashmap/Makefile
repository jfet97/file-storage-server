# make -> compile all the binaries 
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value
TARGETS = libicl_hash.a icl_hash.o

nosuppress: CFLAGS += -Wunused-value
nosuppress: clean all

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGETS)

libicl_hash.a: icl_hash.o
	 ar rvs $@ $<

icl_hash.o: icl_hash.c icl_hash.h
	$(CC) $(CFLAGS) $< -c -o $@

clean: 
	-rm -f $(TARGETS)