# make -> compile all the binaries 
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic
TARGET = bin/main
BUILDDIR = bin

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

../../liblist.a:
	cd ../../ && $(MAKE)

bin/main: ./main.c ../../liblist.a ../../list.h
	(mkdir bin || true)
	$(CC) $(CFLAGS) $^ -I ../../ -L ../..// -llist -o $@

clean: 
	-rm -rf $(BUILDDIR)
