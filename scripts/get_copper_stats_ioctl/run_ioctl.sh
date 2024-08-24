#!/bin/bash

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

if [ -f "$SPEC_DEV_TARGET" ]
then
  echo "found spec_dev: $SPEC_DEV_TARGET"
else
  echo "creating spec_dev: $SPEC_DEV_TARGET"
  mkdir -p "$(dirname "$SPEC_DEV_TARGET")"
  touch "$SPEC_DEV_TARGET"
fi

python3 ioctl.py "$1"
