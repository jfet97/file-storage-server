clients=(
  '-f ./server/bin/mysocket -w ./test/local-file-system/ -D ./test/test3/clients/evicted -c ./test/local-file-system/images/pic1.jpg -c ./test/local-file-system/images/pic2.jpg -c ./test/local-file-system/images/pic3.jpg &> ./test/test3/output/client1.txt'
  '-f ./server/bin/mysocket -n  ./test/local-file-system/text-files/lorems/lorem10.txt -c ./test/local-file-system/text-files/lorems/lorem10.txt -n  ./test/local-file-system/text-files/lorems/lorem150.txt -c ./test/local-file-system/text-files/lorems/lorem150.txt -n  ./test/local-file-system/text-files/lorems/lorem1000.txt -c ./test/local-file-system/text-files/lorems/lorem513.txt'
  '-f ./server/bin/mysocket -R n=0 -d ./test/test3/clients/read -c ./test/local-file-system/text-files/lorems-3/lorem150.txt -W ./test/local-file-system/music/music1.mp3,./test/local-file-system/music/music2.mp3 -n ./test/local-file-system/music/music2.mp3 -c ./test/local-file-system/music/music2.mp3'
  '-f ./server/bin/mysocket -o ./test/local-file-system/text-files/lorems/lorem1000.txt -l ./test/local-file-system/text-files/lorems/lorem1000.txt -e ./test/file/casuale.txt -a ./test/local-file-system/text-files/lorems/lorem1000.txt,./test/file/casuale.txt -r ./test/file/casuale.txt -d ./test/test3/clients/read'
  '-f ./server/bin/mysocket -o ./test/local-file-system/text-files/lorems/lorem1000.txt -l ./test/local-file-system/text-files/lorems/lorem1000.txt -c ./test/local-file-system/text-files/lorems/lorem1000.txt -r ./test/local-file-system/images/pic3.jpg -d ./test/test3/clients/read -c ./test/local-file-system/text-files/lorems-2/lorem1000.txt'
  '-f ./server/bin/mysocket -e ./test/file/casuale2.txt -l ./test/file/casuale2.txt -a ./test/local-file-system/text-files/lorems/lorem150.txt,./test/file/casuale2.txt,./test/file/casuale2.txt -u ./test/file/casuale2.txt -r ./test/file/casuale2.txt'
  '-f ./server/bin/mysocket -W ./test/test3/config.txt,./test/test3/script.sh -l ./test/file/casuale2.txt -o ./test/file/casuale2.txt -r ./test/file/casuale2.txt,./test/test3/config.txt,./test/test3/script.sh -d ./test/test3/clients/read -s ./test/file/casuale2.txt -u ./test/file/casuale2.txt'
)

MY_ID=${ID}

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ./client/bin/client ${clients[i]} -p &> ./test/test3/output/client${i}_${MY_ID}.txt
done

exit 0