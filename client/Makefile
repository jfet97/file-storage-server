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

api-a:
	cd ./api/ && $(MAKE) all

clp:
	cd ../command-line-parser/ && $(MAKE) all

api-d:
	cd ./api/ && $(MAKE) debug

clp-d:
	cd ../command-line-parser/ && $(MAKE) debug

list:
	cd ../list/ && $(MAKE) all

list-d:
	cd ../list/ && $(MAKE) debug

communication:
	cd ../communication/ && $(MAKE) all

communication-d:
	cd ../communication/ && $(MAKE) debug

bin/main: main.c api-a clp list communication 
	(mkdir bin || true)
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -I ../command-line-parser -L ../command-line-parser/ -lcommand-line-parser -I ../list/ -L../list/ -llist -o $(TARGET)

main-d: main.c api-d clp-d list-d communication-d
	(mkdir bin || true)
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -I ../command-line-parser -L ../command-line-parser/ -lcommand-line-parser -I ../list/ -L../list/ -llist -o $(TARGET)

clean:
	-rm -rf $(BUILDDIR)
	rm -f api.o
	rm -f libapi.a
	cd ./api && make clean
	cd ../command-line-parser && make clean
	cd ../communication && make clean
	cd ../list && make clean
