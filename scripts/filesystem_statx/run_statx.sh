#!/bin/bash
set -o nounset

if [ $# -ne 1 ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

ABS_PATH=$(readlink -f $1)
python_output=$(python3 statx.py "$ABS_PATH")
echo "$python_output"
