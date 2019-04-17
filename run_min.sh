#!/bin/bash

MIN_PORT=1024
MAX_PORT=65500

if [ "$#" -ne 1 ]; then
    echo "Please provide base port to the script. Value between [$MIN_PORT - $MAX_PORT]"
    exit
fi

if [ $1 -lt $MIN_PORT ] || [ $1 -gt $MAX_PORT ]; then
    echo "Port $1 is not in the range [$MIN_PORT, $MAX_PORT]"
    exit
fi

BASE_PORT=$1
./routing_server $BASE_PORT 8          &

# Run all nodes
./node $BASE_PORT 1 11:2 103:6         &
./node $BASE_PORT 11 1:2 13:7 19:2     &
./node $BASE_PORT 13 11:7 17:3 101:4   &
./node $BASE_PORT 17 13:3 107:2        &
./node $BASE_PORT 19 11:2 101:2 103:1  &
./node $BASE_PORT 101 13:4 19:2 107:2  &
./node $BASE_PORT 103 1:6 19:1 107:4   &
./node $BASE_PORT 107 17:2 101:2 103:4 &

# Terminate all processes in case of failure
trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM

# Wait for processes to finish
echo "Waiting for processes to finish"
wait
exit 0

