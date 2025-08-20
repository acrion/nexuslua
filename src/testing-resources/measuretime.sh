#!/usr/bin/env bash
startTime=$(date +%s.%N)
eval "$1"
endTime=$(date +%s.%N)
elapsedTime=$(echo "$endTime - $startTime" | bc)
echo $elapsedTime

