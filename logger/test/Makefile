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

../liblogger.a:
	cd ../ && $(MAKE)

bin/main: ./main.c ../liblogger.a ../logger.h
	(mkdir bin || true)
	$(CC) $(CFLAGS) $^ -I ../ -L ../ -llogger -o $@

clean: 
	-rm -rf $(BUILDDIR)
