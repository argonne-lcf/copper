#!/bin/bash
set -o nounset

if [ ! -f env.sh ]; then
	echo "env.sh not found. please cp default_env.sh and set appropriate ENV vars"
	exit 1
fi

if [ -z "$1" ]; then
  echo "usage: run_ioctl.sh <CMD>"
  exit 1
fi

source env.sh

python3 ioctl.py "$1"
