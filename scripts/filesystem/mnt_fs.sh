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
    echo "unmounting"
    fusermount -u $VIEW_DIR || true
}

echo "unmounting possible prev mnt"
fusermount -u "$VIEW_DIR" || true

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
  mkdir -p "$(dirname "$SPEC_DEV_TARGET")"
  touch "$SPEC_DEV_TARGET"
fi

if [ -f "$LOG_OUTPUT_DIR" ]
then
  echo "found LOG_OUTPUT_DIR: $LOG_OUTPUT_DIR"
else
  echo "creating LOG_OUTPUT_DIR: $LOG_OUTPUT_DIR"
  mkdir -p "$LOG_OUTPUT_DIR"
fi

echo "removing old logs"
rm "$LOG_OUTPUT_DIR"/*

if [[ $SINGLE_THREADED_FUSE == "ON" ]]; then
  echo "running fuse in single-threaded mode"
  export ST="-s"
else
  echo "running fuse in multi-threaded mode"
fi

echo "mounting fuse distributed cache to view dir"
$FUSE_FS -tpath $TARGET_DIR                \
         -vpath $VIEW_DIR                  \
         -log_level $LOG_LEVEL             \
         -log_type $LOG_TYPE               \
         -log_output_dir $LOG_OUTPUT_DIR \
         -f $ST $VIEW_DIR # add -d for debugging fuse # chance of issue with -s # to check noah


fusermount -u "$VIEW_DIR" || true
# mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view 
