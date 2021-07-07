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

libapi.a: api.o
	ar rvs $@ $<

api.o: api.c api.h
  
main: main.c libapi.a
	$(CC) $(CFLAGS) $^ -o $@

clean:
	-rm -f $(TARGET)
	rm -f ./main
	rm -f api.o
	rm -f libapi.a
