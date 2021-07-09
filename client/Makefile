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

api:
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

bin/main: main.c api clp list
	(mkdir bin || true)
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -I ../command-line-parser -L ../command-line-parser/ -lcommand-line-parser ../list/ -L../list/ -llist -o $@

main-d: main.c api-d clp-d list-d
	(mkdir bin || true)
	((cd bin && mkdir read) || true)
	((cd bin && cd read && mkdir inner) || true)
	cd bin && cd read && touch test1.txt && echo "prova1 prova1" >> test1.txt
	cd bin && cd read && touch test2.txt && echo "prova2 prova2" >> test2.txt
	cd bin && cd read && touch test3.txt && echo "prova3 prova3" >> test3.txt
	cd bin && cd read && cd inner && touch test4.txt && echo "prova4 prova4" >> test4.txt
	cd bin && cd read && cd inner && touch test5.txt && echo "prova5 prova5" >> test5.txt
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -I ../command-line-parser -L ../command-line-parser/ -lcommand-line-parser -I ../list/ -L../list/ -llist -o $(TARGET)

clean:
	-rm -rf $(BUILDDIR)
	rm -f api.o
	rm -f libapi.a
	cd ./api && make clean
	cd ../command-line-parser && make clean
	cd ../list && make clean
