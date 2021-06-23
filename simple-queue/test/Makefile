# make -> compile all the binaries 
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread
TARGET = bin/main
BUILDDIR = bin

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

../libsimple_queue.a:
	cd ../ && $(MAKE)

bin/main: ./main.c ../libsimple_queue.a ../simple_queue.h
	(mkdir bin || true)
	$(CC) $(CFLAGS) $^ -I ../ -L ../ -lsimple_queue -o $@

clean: 
	-rm -rf $(BUILDDIR)
