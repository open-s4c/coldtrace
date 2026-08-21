#!/bin/sh

start=$(date +%s%N)
sh -c "$1"
status=$?
end=$(date +%s%N)
echo "time_ms $(( (end - start) / 1000000 ))" >&2
exit "$status"
