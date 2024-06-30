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

if [ -z "$1" ]; then
  echo "usage: run_ioctl.sh <CMD>"
  exit 1
fi

source ../env.sh

python3 ioctl.py "$1"
