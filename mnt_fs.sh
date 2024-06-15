#!/bin/bash

INIT_PWD=$(pwd)
VIEW_DIR="$INIT_PWD/view"
TARGET_DIR="$INIT_PWD/target"
FUSE_FS="$INIT_PWD/build/cu_fuse"

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
$FUSE_FS -s -f -tpath $TARGET_DIR $VIEW_DIR

unmount || { echo "already unmounted"; exit 0; }
