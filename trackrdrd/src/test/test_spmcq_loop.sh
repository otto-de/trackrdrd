#! /bin/bash

N=500

echo
echo "TEST: $0"
echo "... running test_spmcq $N times"

I=1
while [[ $I -le $N ]]
do
    # echo -en "Test $N\r"
    ./test_spmcq > /dev/null
    if [ $? -ne 0 ]; then
        echo "Test $I FAILED"
        exit 1
    fi
    ((I++))
done

exit 0