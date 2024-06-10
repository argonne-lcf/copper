#!/bin/bash

INIT_PWD=$(pwd)
MNT_DIR="$INIT_PWD/mnt"
MNT_MIRROR_DIR="$INIT_PWD/mnt_mirror"
FUSE_FS="$INIT_PWD/build/fuse_distributed_cache"

function unmount() {
    echo "unmounting mnt point"
    fusermount -u $MNT_DIR || true
}

function create_folder() {
    if [ -d $dir_path ]
    then
        echo "$1 already exists - skipping creation"
    else
        echo "creating $1"
        mkdir $1
    fi
}

create_folder $MNT_DIR
create_folder $MNT_MIRROR_DIR

unmount

if [ -f $FUSE_FS ]
then
    echo "found mnt binary"
else
    echo "fuse_distributed_cache binary not found - please compile"
    exit 1;
fi

echo "mounting fuse distributed cache to mnt"
$FUSE_FS -f $MNT_DIR

unmount || { echo "mnt already unmounted"; exit 0; }
