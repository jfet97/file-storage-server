# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value

debug: CFLAGS += -g
debug: clean debug-d

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

all: clean
	(cd ./client && make all)
	(cd ./server && make all)

debug-d:
	(cd ./client && make debug)
	(cd ./server && make debug)

clean:
	(cd ./client && make clean)
	(cd ./server && make clean)
