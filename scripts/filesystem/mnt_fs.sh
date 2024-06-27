#!/bin/bash
set -o nounset

if [ ! -f ../env.sh ]
then
	echo "env.sh not found. please cp default_env.sh and set appropriate ENV vars"
	exit 1
fi

source ../env.sh

function unmount() {
    echo "unmounting"
    fusermount -u $VIEW_DIR || true
}

echo "VIEW_DIR: $VIEW_DIR"
echo "TARGET_DIR: $TARGET_DIR"

if [ -f $FUSE_FS ]
then
    echo "found cu_fuse binary"
else
    echo "fuse_distributed_cache binary not found - please compile"
    exit 1;
fi

if [ -f "$SPEC_DEV_TARGET" ]
then
  echo "found spec_dev: $SPEC_DEV_TARGET"
else
  echo "creating spec_dev: $SPEC_DEV_TARGET"
  mkdir -p "$(dirname "$SPEC_DEV_TARGET")" || true
  touch "$SPEC_DEV_TARGET"
fi

echo "mounting fuse distributed cache to view dir"
$FUSE_FS -tpath $TARGET_DIR -vpath $VIEW_DIR -f -s $VIEW_DIR

unmount || { echo "already unmounted"; exit 0; }
