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
	((cd ./test/test1 && rm -rf ./clients) || true)
	((cd ./test/test1 && rm -f log.txt) || true)

test1: clean dist
	(cd test/test1 && bash script.sh)