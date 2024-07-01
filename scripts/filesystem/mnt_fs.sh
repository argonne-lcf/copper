#!/bin/bash
set -o nounset

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
    echo "unmounting"
    fusermount -u $VIEW_DIR || true
}

echo "unmounting possible prev mnt"
fusermount -u "$VIEW_DIR" || true

echo "removing old files"
rm $ROOT_DIR/copper_address_book.txt
rm $ROOT_DIR/*.log

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
$FUSE_FS -tpath $TARGET_DIR                \
         -vpath $VIEW_DIR                  \
         -log_level $LOG_LEVEL             \
         -log_type $LOG_TYPE               \
         -log_output_path $LOG_OUTPUT_PATH \
         -f $VIEW_DIR # add -d for debugging fuse # chance of issue with -s # to check noah


fusermount -u "$VIEW_DIR" || true
# mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view 