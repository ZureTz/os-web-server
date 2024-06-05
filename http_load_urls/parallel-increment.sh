#!/bin/zsh

for i in {1..201..20};
do
  echo "Parallel $i:"
  http_load -parallel $i -fetches 2000 urls.txt | grep -E '(fetches/sec|bytes/sec|msecs/first-response)'
  echo ''
done
