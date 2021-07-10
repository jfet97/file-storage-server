# launch server in background
valgrind --leak-check=full bin/server ../test/test1/config.txt &
# save its PID
SERVER_PID=$!