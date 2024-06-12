#!/bin/bash

INIT_PWD=$(pwd)
MNT_DIR="$INIT_PWD/mnt"
MNT_MIRROR_DIR="$INIT_PWD/mnt_mirror"
FUSE_FS="$INIT_PWD/build/cu_fuse"

function unmount() {
    echo "unmounting mnt point"
    fusermount -u $MNT_DIR || true
}

unmount || { echo "mnt already unmounted"; exit 0; }
