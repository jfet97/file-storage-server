# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -Wno-unused-value

all:
	(cd ./client && make all)
	(cd ./server && make all)
	
debug: CFLAGS += -g
debug: clean debug-d

dist: CFLAGS += -O3
dist: clean all

.PHONY: clean

debug-d:
	(cd ./client && make debug)
	(cd ./server && make debug)

clean: test-clean
	(cd ./client && make clean)
	(cd ./server && make clean)

test-clean:
	((cd ./test/test1 && rm -rf ./clients) || true)
	((cd ./test/test1 && rm -rf ./output) || true)
	((cd ./test/test1 && rm -f log.txt) || true)
	((cd ./test/test2 && rm -rf ./clients) || true)
	((cd ./test/test2 && rm -rf ./output) || true)
	((cd ./test/test2 && rm -f log.txt) || true)
	((cd ./test/test3 && rm -rf ./clients) || true)
	((cd ./test/test3 && rm -rf ./output) || true)
	((cd ./test/test3 && rm -f log.txt) || true)

test1: test-clean all
	(cd test/test1 && bash script.sh)

test2: test-clean all
	(cd test/test2 && bash script.sh)

test3: test-clean all
	(bash test/test3/script.sh)