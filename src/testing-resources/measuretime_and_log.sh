#!/bin/bash

i=1

while true; do
  sudo turbostat --interval 0.5 --quiet --Summary --show "Busy%,Bzy_MHz,PkgTmp" > "logs/log_turbostat_$i.txt" &
  TURBOSTAT_PID=$!
  ./measuretime.sh "nexuslua $(pwd)/demo5.lua"
  kill $TURBOSTAT_PID
  i=$((i + 1))
  sleep 5
done
