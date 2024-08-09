#!/bin/bash

# set for mnt to work
export VIEW_DIR=
# set for tests to work
export PY_PACKAGES_DIR=
# set to location of fuse library directory required to build
export FUSE3_LIB=
# set to location of fuse header directory required to build
export FUSE3_INCLUDE=

# log settings
# 0-6 from most amount of information to least
# LOG_LEVEL=0 is  TRACE data means prints from [0-6]
# LOG_LEVEL=1 is  DEBUG means prints from [1-6]
# LOG_LEVEL=2 is  INFO  means prints from [2-6]
# LOG_LEVEL=3 is  NOTICE means prints from [3-6]
# LOG_LEVEL=4 is  WARN  means prints from [4-6]
# LOG_LEVEL=5 is  ERROR means prints from [5-6]
# LOG_LEVEL=6 is  FATAL ERROR means prints from [6-6]
export LOG_LEVEL=1
# file/stdout/file_and_stdout
export LOG_TYPE=file_and_stdout
# only used if LOG_TYPE=file/file_and_stdout
export LOG_OUTPUT_DIR=$(readlink -f ~/copper_logs)
# set to either ON/OFF to enable/disable blocking redundant rpcs
export BLOCK_REDUNDANT_RPCS=ON
# set to either ON/OFF to enable/disable fuse single-threaded/multi-threaded
export SINGLE_THREADED_FUSE=OFF

# cxi/tcp/na+sm
export NET_TYPE=cxi
# only used if -nf not passed
export ADDR_WRITE_SYNC_TIME=5

export ROOT_DIR=$(readlink -f "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/..")
export SCRIPTS_DIR="$ROOT_DIR"/scripts
export TARGET_DIR=/
export FUSE_FS="$ROOT_DIR/build/cu_fuse"
export RPC_SHUTDOWN="$ROOT_DIR/build/cu_fuse_shutdown"
# the directory for which the dev directory will be created
export SPEC_DEV_PARENT_DIR=$LOG_OUTPUT_DIR
export SPEC_DEV_DIR="$SPEC_DEV_PARENT_DIR/metrics"
export SPEC_DEV_VIEW_DIR="$(readlink -f $VIEW_DIR/"$SPEC_DEV_DIR")"
export SPEC_DEV_VIEW="$SPEC_DEV_VIEW_DIR/.spec_ioctl_file"
export SPEC_DEV_TARGET="$SPEC_DEV_DIR/.spec_ioctl_file"

export IOCTL_CLEAR_CACHE_TABLES=100
export IOCTL_CLEAR_OPERATION=700
export IOCTL_CLEAR_OPERATION_CACHE_HIT=800
export IOCTL_CLEAR_OPERATION_CACHE_MISS=900
export IOCTL_CLEAR_OPERATION_TIME=1000
export IOCTL_CLEAR_DATA_CACHE_EVENT=1400
export IOCTL_CLEAR_TREE_CACHE_EVENT=1500
export IOCTL_CLEAR_MD_CACHE_EVENT=1600
export IOCTL_CLEAR_OPERATION_CACHE_NEG=1800

export IOCTL_LOG_CACHE_TABLES=200
export IOCTL_LOG_OPERATION=300
export IOCTL_LOG_OPERATION_TIME=400
export IOCTL_LOG_OPERATION_CACHE_HIT=500
export IOCTL_LOG_OPERATION_CACHE_MISS=600
export IOCTL_LOG_DATA_CACHE_EVENT=1100
export IOCTL_LOG_TREE_CACHE_EVENT=1200
export IOCTL_LOG_MD_CACHE_EVENT=1300
export IOCTL_LOG_OPERATION_CACHE_NEG=1700

export IOCTL_RESET_FS=1900
export IOCTL_LOG_ALL_METRICS=2000

export IOCTL_CLEAR_IOCTL_EVENT=2100
export IOCTL_LOG_IOCTL_EVENT=2200

export IOCTL_GET_DATA_CACHE_SIZE=2300
export IOCTL_GET_TREE_CACHE_SIZE=2400
export IOCTL_GET_MD_CACHE_SIZE=2500
