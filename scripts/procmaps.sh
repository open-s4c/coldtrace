#!/bin/bash

# Check if application is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <application>"
  exit 1
fi

# Start gdb with the application and set a breakpoint on main
gdb -q "$1" -ex "break main" -ex "run" -ex "shell cat /proc/\$(pgrep -n -x $(basename "$1"))/maps" -ex "continue" -ex "quit"
