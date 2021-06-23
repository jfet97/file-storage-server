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

../libfile-system.a:
	cd ../ && $(MAKE) all

bin/main: ./main.c ../libfile-system.a ../file-system.h
	(mkdir bin || true)
	$(CC) $(CFLAGS) $^ -I ../ -I ../../hashmap/ -I ../../list/ -L ../ -lfile-system -L../../hashmap/ -licl_hash -L../../list/ -llist -o $@

clean: 
	-rm -rf $(BUILDDIR)
