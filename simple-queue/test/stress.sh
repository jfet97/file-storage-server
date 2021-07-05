#!/bin/bash
for ((a=1; a <= 200; a++)) ; do
    valgrind --log-file="gino${a}.txt" --leak-check=full --show-leak-kinds=all --track-origins=yes  -s ~/Documents/SOL/file-storage-server/simple-queue/test/bin/main
done
