# make -> compile all the binaries
# make debug -> compile with debug symbol
# make dist -> compile with compiler optimizations
# make clean -> remove the binaries

CFLAGS += -std=c99 -Wall -pedantic -pthread -Wno-unused-value
TARGET = main

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
  
main: main.c api
	$(CC) $(CFLAGS) $^ -I ./api/ -L./api/ -lapi -o $@

main-d: main.c api-d
	$(CC) $< $(CFLAGS) -I ./api/ -L./api/ -lapi -o main

clean:
	-rm -f $(TARGET)
	rm -f ./main
	rm -f api.o
	rm -f libapi.a
	cd ./api && make clean
