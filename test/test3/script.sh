echo "-----------------------------------"

mkdir ./test/test3/output

# move into server folder

# launch server in background
./server/bin/server ./test/test3/config.txt &> ./test/test3/output/server.txt &
# save its PID
SERVER_PID=$!

sleep 3

pwd

CLIENTS_PID=$!

ID=0

CLS=()
for i in {1..10}; do
    ID=${i}
    export ID
    ./test/test3/clients.sh &
    CLS+=($!)
done

sleep 30

kill -2 $SERVER_PID
wait $CLIENTS_PID

for i in "${CLS[@]}"; do
    kill -9 ${i}
    wait ${i}
done

exit 0