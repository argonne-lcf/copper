#!/bin/bash

unmount() {
    echo "preparing mnt point"
    fusermount -u mnt || true
}

if [ -d mnt ]
then
    echo "mnt already exists - skipping creation"
else
    echo "creating mnt dir"
    mkdir mnt
fi

unmount

if [ -f ./build/fuse_distributed_cache ]
then
    echo "found mnt binary"
else
    echo "fuse_distributed_cache binary not found - please compile"
    exit 1;
fi

echo "mounting fuse distributed cache to mnt"
./build/fuse_distributed_cache -f mnt

umount
