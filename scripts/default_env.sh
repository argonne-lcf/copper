#!/bin/bash
# set -o nounset # not sure -causes bash: !ref: unbound variable - can be used for debugging
# set +o nounset # fixes the above issue
# set for mnt to work
export VIEW_DIR=
# set for tests to work
export PY_PACKAGES_DIR=

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
export LOG_TYPE=stdout
# only used if LOG_TYPE=file
export LOG_OUTPUT_PATH=$(readlink -f ~/copper.log)

export ROOT_DIR=$(readlink -f "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/..")
export SCRIPTS_DIR="$ROOT_DIR"/scripts
export TARGET_DIR=/
export FUSE_FS="$ROOT_DIR/build/cu_fuse"
# the directory for which the dev directory will be created
export SPEC_DEV_PARENT_DIR="$(readlink -f ~)"
export SPEC_DEV_DIR=$SPEC_DEV_PARENT_DIR/copper-metrics
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
