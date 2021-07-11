echo "-----------------------------------"

pwd
mkdir output

# move into server folder
cd ../../server

pwd

# launch server in background
valgrind --leak-check=full bin/server ../test/test1/config.txt >> ../test/test1/output/server.txt & 
# bin/server ../test/test1/config.txt >> ../test/test1/output/server.txt &

# save its PID
SERVER_PID=$!

echo $SERVER_PID

# move into project folder
cd ../client

pwd

# see usage
bin/client -h

# CLIENT 1
bin/client -f ../server/bin/mysocket -t 200 -w ../test/local-file-system -D ../test/test1/clients/evicted -s ../././test/local-file-system/images/pic1.jpg,../test/local-file-system/images/pic2.jpg,../test/local-file-system/images/pic3.jpg,../test/local-file-system/music/music1.mp3,../test/local-file-system/music/music2.mp3,../test/local-file-system/text-files/lorems/lorem10.txt,../test/local-file-system/text-files/lorems/lorem150.txt,../test/local-file-system/text-files/lorems/lorem513.txt,../test/local-file-system/text-files/lorems/lorem1000.txt -p &> ../test/test1/output/client1.txt
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -t 200 -w ../test/local-file-system -D ../test/test1/clients/evicted -s ../././test/local-file-system/images/pic1.jpg,../test/local-file-system/images/pic2.jpg,../test/local-file-system/images/pic3.jpg,../test/local-file-system/music/music1.mp3,../test/local-file-system/music/music2.mp3,../test/local-file-system/text-files/lorems/lorem10.txt,../test/local-file-system/text-files/lorems/lorem150.txt,../test/local-file-system/text-files/lorems/lorem513.txt,../test/local-file-system/text-files/lorems/lorem1000.txt &> ../test/test1/output/client1.txt

# CLIENT 2
bin/client -f ../server/bin/mysocket -t 200 -n  ../test/local-file-system/text-files/lorems/lorem10.txt -c ../test/local-file-system/text-files/lorems/lorem10.txt -p &> ../test/test1/output/client2.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -t 200 -n  ../test/local-file-system/text-files/lorems/lorem10.txt -c ../test/local-file-system/text-files/lorems/lorem10.txt &> ../test/test1/output/client2.txt &

# CLIENT 3
bin/client -f ../server/bin/mysocket -t 200 -R n=0 -d ../test/test1/clients/read -p &> ../test/test1/output/client3.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -t 200 -R n=0 -d ../test/test1/clients/read &> ../test/test1/output/client3.txt &

# CLIENT 4
bin/client -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -e /test/file/casuale.txt -a ../test/local-file-system/text-files/lorems/lorem1000.txt,/test/file/casuale.txt -r /test/file/casuale.txt -d ../test/test1/clients/read -p &> ../test/test1/output/client4.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -e /test/file/casuale.txt -a ../test/local-file-system/text-files/lorems/lorem1000.txt,test/file/casuale.txt -r /test/file/casuale.txt -d ../test/test1/clients/read &> ../test/test1/output/client4.txt &

# CLIENT 5
bin/client -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -c ../test/local-file-system/text-files/lorems/lorem1000.txt -p &> ../test/test1/output/client5.txt &
# valgrind --leak-check=full bin/client -f ../server/bin/mysocket -t 200 -o ../test/local-file-system/text-files/lorems/lorem1000.txt -l ../test/local-file-system/text-files/lorems/lorem1000.txt -c ../test/local-file-system/text-files/lorems/lorem1000.txt &> ../test/test1/output/client5.txt &

sleep 0.3

# kill server but wait until all clients have started their operations
kill -1 $SERVER_PID

wait $SERVER_PID