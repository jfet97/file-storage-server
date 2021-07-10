echo "-----------------------------------"

pwd

# move into server folder
cd ../../server

pwd

# launch server in background
valgrind --leak-check=full bin/main ../test/test1/config.txt &

# save its PID
SERVER_PID=$!

echo $!

# move into client folder
cd ../client

pwd

# see usage
bin/main -h

# CLIENT 1
bin/main -f ../server/bin/mysocket -w ../test/local-file-system -D ../test/test1/clients/evicted -p

# kill server
kill -2 $SERVER_PID

wait $SERVER_PID