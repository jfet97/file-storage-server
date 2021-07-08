# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread -Wno-unused-value
TARGET = bin/main
BUILDDIR = bin

debug: CFLAGS += -g
debug: clean d

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

d: main-d

simple-queue:
	cd ../simple-queue/ && $(MAKE) all

config-parser:
	cd ../config-parser/ && $(MAKE) all

logger:
	cd ../logger/ && $(MAKE) all

communication:
	cd ../communication/ && $(MAKE) all

simple-queue-d:
	cd ../simple-queue/ && $(MAKE) debug

config-parser-d:
	cd ../config-parser/ && $(MAKE) debug

logger-d:
	cd ../logger/ && $(MAKE) debug

communication-d:
	cd ../communication/ && $(MAKE) debug

bin/main: main.c simple-queue config-parser logger communication
	(mkdir bin || true)
	$(CC) $(CFLAGS) $< -I ../simple-queue/ -L ../simple-queue/ -lsimple_queue -I ../config-parser/ -L ../config-parser/ -lconfig-parser -I ../logger/ -L ../logger/ -llogger -I ../communication/ -L ../communication/ -lcommunication -o $@

main-d: main.c simple-queue-d config-parser-d logger-d communication-d
	(mkdir bin || true)
	$(CC) $(CFLAGS) $< -I ../simple-queue/ -L ../simple-queue/ -lsimple_queue -I ../config-parser/ -L ../config-parser/ -lconfig-parser -I ../logger/ -L ../logger/ -llogger -I ../communication/ -L ../communication/ -lcommunication -o $(TARGET)

clean:
	rm -rf $(BUILDDIR)
	rm -f ./log.txt
	rm -f ./main
	cd ../simple-queue && make clean
	cd ../config-parser && make clean
	cd ../logger && make clean
	cd ../communication && make clean
