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

../libcommand-line-parser.a:
	cd ../ && $(MAKE)

bin/main: ./main.c ../libcommand-line-parser.a ../command-line-parser.h
	(mkdir bin || true)
	$(CC) $(CFLAGS) $^ -I ../ -L ../ -lcommand-line-parser -o $@

clean: 
	-rm -rf $(BUILDDIR)
