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

config-parser:
	cd ../config-parser/ && $(MAKE) all

main: main.c simple-queue config-parser
	$(CC) $(CFLAGS) $< -I ../simple-queue/ -L ../simple-queue/ -lsimple_queue -I ../config-parser/ -L ../config-parser/ -lconfig-parser -o $@

clean:
	rm -rf $(TARGET)
	cd ../simple-queue && make clean
	cd ../config-parser && make clean
