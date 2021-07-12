echo "-----------------------------------"

pwd
mkdir output

# move into server folder
cd ../../server

pwd

# launch server in background
# valgrind --leak-check=full bin/server ../test/test2/config.txt >> ../test/test2/output/server.txt & 
bin/server ../test/test2/config.txt >> ../test/test2/output/server.txt &

# save its PID
SERVER_PID=$!

echo $SERVER_PID

# move into project folder
cd ../client

pwd

# see usage
bin/client -h

# CLIENT 1
bin/client -f ../server/bin/mysocket -w ../test/local-file-system/text-files/ -D ../test/test2/clients/evicted -p &> ../test/test2/output/client1.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -w ../test/local-file-system/text-files/ -D ../test/test2/clients/evicted -p &> ../test/test2/output/client1.txt

# CLIENT 2
bin/client -f ../server/bin/mysocket -n  ../test/local-file-system/text-files/lorems/lorem10.txt -c ../test/local-file-system/text-files/lorems/lorem10.txt -p &> ../test/test2/output/client2.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -n  ../test/local-file-system/text-files/lorems/lorem10.txt -c ../test/local-file-system/text-files/lorems/lorem10.txt &> ../test/test2/output/client2.txt &

# CLIENT 3
bin/client -f ../server/bin/mysocket -R n=0 -d ../test/test2/clients/read -p &> ../test/test2/output/client3.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -R n=0 -d ../test/test2/clients/read &> ../test/test2/output/client3.txt &

# CLIENT 4
bin/client -f ../server/bin/mysocket -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -e /test/file/casuale.txt -a ../test/local-file-system/text-files/lorems/lorem1000.txt,/test/file/casuale.txt -r /test/file/casuale.txt -d ../test/test2/clients/read -p &> ../test/test2/output/client4.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -e /test/file/casuale.txt -a ../test/local-file-system/text-files/lorems/lorem1000.txt,test/file/casuale.txt -r /test/file/casuale.txt -d ../test/test2/clients/read &> ../test/test2/output/client4.txt &

# CLIENT 5
bin/client -f ../server/bin/mysocket -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -c ../test/local-file-system/text-files/lorems/lorem1000.txt -p &> ../test/test2/output/client5.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -c ../test/local-file-system/text-files/lorems/lorem1000.txt &> ../test/test2/output/client5.txt &

# CLIENT 6
bin/client -f ../server/bin/mysocket -w ../test/local-file-system/ -D ../test/test2/clients/evicted -p &> ../test/test2/output/client6.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -w ../test/local-file-system/ -D ../test/test2/clients/evicted -p &> ../test/test2/output/client6.txt

# CLIENT 7 
bin/client -f ../server/bin/mysocket -e /test/file/casuale2.txt -l /test/file/casuale2.txt -a ../test/local-file-system/text-files/lorems/lorem150.txt,/test/file/casuale2.txt,/test/file/casuale2.txt -u /test/file/casuale2.txt -p &> ../test/test2/output/client7.txt &

# CLIENT 8 
bin/client -f ../server/bin/mysocket -W ../test/test2/config.txt,../test/test2/script.sh -l /test/file/casuale2.txt -o /test/file/casuale2.txt -r /test/file/casuale2.txt,../test/test2/config.txt,../test/test2/script.sh -d ../test/test2/clients/read -s /test/file/casuale2.txt -u /test/file/casuale2.txt -p &> ../test/test2/output/client8.txt &


sleep 5

# kill server but wait until all clients have started their operations
kill -1 $SERVER_PID

wait $SERVER_PID

exit 0