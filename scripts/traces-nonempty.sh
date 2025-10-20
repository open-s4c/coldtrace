#!/bin/sh

if [ -z "$1" ]; then
	echo "usage: $0 <DIR>"
	exit 1
fi

DIR=$1

if ! find $DIR -name '*.bin' | grep '^' > /dev/null; then
	echo "error: trace dir '$DIR' is empty"
	exit 1
fi
