#!/bin/bash

if [ ! -f env.sh ]
then
	echo "env.sh not found. please cp default_env.sh and set appropriate ENV vars"
	exit 1
fi

source env.sh

function unmount() {
    echo "unmounting"
    fusermount -u $VIEW_DIR || true
}

function create_folder() {
    if [ -d $1 ]
    then
        echo "$1 already exists - skipping creation"
    else
        echo "creating $1"
        mkdir $1
    fi
}

echo "VIEW_DIR: $VIEW_DIR"
echo "TARGET_DIR: $TARGET_DIR"

create_folder $VIEW_DIR
create_folder $TARGET_DIR

unmount

if [ -f $FUSE_FS ]
then
    echo "found cu_fuse binary"
else
    echo "fuse_distributed_cache binary not found - please compile"
    exit 1;
fi

echo "mounting fuse distributed cache to view dir"
$FUSE_FS -f -tpath $TARGET_DIR $VIEW_DIR

unmount || { echo "already unmounted"; exit 0; }
