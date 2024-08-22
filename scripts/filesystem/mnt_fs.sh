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

mkdir -vp $VIEW_DIR

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


if [[ $SINGLE_THREADED_FUSE == "ON" ]]; then
  echo "running fuse in single-threaded mode"
  export ST="-s"
else
  echo "running fuse in multi-threaded mode"
fi

NODE_FILE_CMD=""
if [[ -n $NODE_FILE ]]; then
  echo "address book found using -nf $NODE_FILE"
  NODE_FILE_CMD="-nf $NODE_FILE"
else 
  echo "address book not found"
fi

echo "mounting fuse distributed cache to view dir"
$FUSE_FS -f -tpath $TARGET_DIR                          \
         -vpath $VIEW_DIR                            \
         -log_level $LOG_LEVEL                       \
         -log_type $LOG_TYPE                         \
         -log_output_dir $LOG_OUTPUT_DIR             \
         -addr_write_sync_time $ADDR_WRITE_SYNC_TIME \
         -net_type $NET_TYPE                         \
         -trees 1                                    \
         $NODE_FILE_CMD                              \
         $ST $VIEW_DIR
