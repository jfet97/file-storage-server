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

api-d:
	cd ./api/ && $(MAKE) debug
  
bin/main: main.c api
	(mkdir bin || true)
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -o $@

main-d: main.c api-d
	(mkdir bin || true)
	cd bin && touch test2.txt && echo "prova prova" >> test2.txt
	$(CC) $< $(CFLAGS) -I ./api/ -L ./api/ -lapi -I ../communication -L ../communication/ -lcommunication -o $(TARGET)

clean:
	-rm -rf $(BUILDDIR)
	rm -f ./main
	rm -f api.o
	rm -f libapi.a
	cd ./api && make clean
