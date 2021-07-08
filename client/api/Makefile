# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value
TARGET = libapi.a api.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

libapi.a: api.o
	ar rvs $@ $<

communication:
	cd ../../communication/ && $(MAKE) all

api.o: api.c api.h communication
	$(CC) $(CFLAGS) -I ../../communication/ -L../../communication/ -lcommunication  $< -c -o $@

clean:
	-rm -f $(TARGET)
