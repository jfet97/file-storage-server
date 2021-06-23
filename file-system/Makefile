# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value
TARGET = libfile-system.a file-system.o

debug: CFLAGS += -g
debug: clean all

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: $(TARGET)

libfile-system.a: file-system.o
	ar rvs $@ $<

hashmap:
	cd ../hashmap/ && $(MAKE) all

list:
	cd ../list/ && $(MAKE) all

file-system.o: file-system.c file-system.h hashmap list
	$(CC) $(CFLAGS) -I ../hashmap/ -I ../list/ -L../hashmap/ -licl_hash -L../list/ -llist $< -c -o $@

clean:
	-rm -f $(TARGET)
