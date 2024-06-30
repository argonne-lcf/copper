#!/bin/bash
set -o nounset

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh

if [ ! -f $ENV ]
then
	echo "failed to find ENV: $ENV"
	exit 1
fi

source $ENV

if [ $# -ne 1 ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

ABS_PATH=$(readlink -f $1)
python_output=$(python3 statx.py "$ABS_PATH")
echo "$python_output"
