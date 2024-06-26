#!/bin/bash
set -o nounset

if [ ! -f ../env.sh ]
then
	echo "env.sh not found. please cp default_env.sh and set appropriate ENV vars"
	exit 1
fi

source ../env.sh

function unmount() {
    echo "unmounting view dir"
    fusermount -u "$VIEW_DIR" || true
}

unmount || { echo "mnt already unmounted"; exit 0; }
