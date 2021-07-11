echo "-----------------------------------"

pwd

# move into server folder
cd ../../server

pwd

# launch server in background
# valgrind --leak-check=full bin/main ../test/test1/config.txt &
bin/main ../test/test1/config.txt &

# save its PID
SERVER_PID=$!

# move into project folder
cd ../client

pwd

# see usage
bin/main -h

# CLIENT 1
bin/main -f ../server/bin/mysocket -t 200 -w ../test/local-file-system -D ../test/test1/clients/evicted -s ../././test/local-file-system/images/pic1.jpg,../test/local-file-system/images/pic2.jpg,../test/local-file-system/images/pic3.jpg,../test/local-file-system/music/music1.mp3,../test/local-file-system/music/music2.mp3,../test/local-file-system/text-files/lorems/lorem10.txt,../test/local-file-system/text-files/lorems/lorem150.txt,../test/local-file-system/text-files/lorems/lorem513.txt,../test/local-file-system/text-files/lorems/lorem1000.txt -p

# CLIENT 2
bin/main -f ../server/bin/mysocket -t 200 -n  ../test/local-file-system/text-files/lorems/lorem10.txt -c ../test/local-file-system/text-files/lorems/lorem10.txt -p &

# CLIENT 3
bin/main -f ../server/bin/mysocket -t 200 -R n=0 -d ../test/test1/clients/read -p &

# CLIENT 4
bin/main -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -e test/file/casuale.txt -a ../test/local-file-system/text-files/lorems/lorem1000.txt,test/file/casuale.txt -r test/file/casuale.txt -d ../test/test1/clients/read -p &

# CLIENT 5
bin/main -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -c ../test/local-file-system/text-files/lorems/lorem1000.txt -p &

sleep 5

# kill server but wait until all clients have started and have finished their operations
kill -2 $SERVER_PID

wait $SERVER_PID