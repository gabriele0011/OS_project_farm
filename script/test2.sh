# verifica con invio del segnale SIGUSR1 -> stampa dei risultati ordinati ottenuti fino ad allora
./farm -n 1 -d testdir -q 1 file* -t 200 2>&1 > /dev/null
sleep 1
pkill -SIGUSR1 farm
wait $pid
if [[ $? != 0 ]]; then
    echo "test6 failed"
else
    echo "test6 passed"
fi
