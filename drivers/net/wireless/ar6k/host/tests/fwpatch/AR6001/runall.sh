
i=1
while [ "$i" -le 9 ]
do
    ./cleanup.sh
    ./test$i.sh
    i=$(( $i + 1 ))
done
