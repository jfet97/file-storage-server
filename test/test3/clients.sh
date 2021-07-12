clients=(
  '-f ./server/bin/mysocket -w ./test/local-file-system/ -D ./test/test3/clients/evicted &> ./test/test3/output/client1.txt'
  '-f ./server/bin/mysocket -n  ./test/local-file-system/text-files/lorems/lorem10.txt -c ./test/local-file-system/text-files/lorems/lorem10.txt'
  '-f ./server/bin/mysocket -R n=0 -d ./test/test3/clients/read'
  '-f ./server/bin/mysocket -o ./test/local-file-system/text-files/lorems/lorem1000.txt -l ./test/local-file-system/text-files/lorems/lorem1000.txt -e /test/file/casuale.txt -a ./test/local-file-system/text-files/lorems/lorem1000.txt,/test/file/casuale.txt -r /test/file/casuale.txt -d ./test/test3/clients/read'
  '-f ./server/bin/mysocket -o ./test/local-file-system/text-files/lorems/lorem1000.txt -l ./test/local-file-system/text-files/lorems/lorem1000.txt -c ./test/local-file-system/text-files/lorems/lorem1000.txt'
  '-f ./server/bin/mysocket -w ./test/local-file-system/ -D ./test/test3/clients/evicted'
  '-f ./server/bin/mysocket -e /test/file/casuale2.txt -l /test/file/casuale2.txt -a ./test/local-file-system/text-files/lorems/lorem150.txt,/test/file/casuale2.txt,/test/file/casuale2.txt -u /test/file/casuale2.txt'
  '-f ./server/bin/mysocket -W ./test/test3/config.txt,./test/test3/script.sh -l /test/file/casuale2.txt -o /test/file/casuale2.txt -r /test/file/casuale2.txt,./test/test3/config.txt,./test/test3/script.sh -d ./test/test3/clients/read -s /test/file/casuale2.txt -u /test/file/casuale2.txt'
)

MY_ID=${ID}

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ./client/bin/client ${clients[i]} -p &> ./test/test3/output/client${i}_${MY_ID}.txt
done

exit 0