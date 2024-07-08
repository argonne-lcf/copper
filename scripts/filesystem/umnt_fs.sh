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

function unmount() {
    echo "unmounting view dir"
    fusermount -u "$VIEW_DIR" || true
}

unmount || { echo "mnt already unmounted"; exit 0; }
