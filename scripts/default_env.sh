#!/bin/bash
set -o nounset

# set for mnt to work
export VIEW_DIR=
# set for tests to work
export PY_PACKAGES_DIR=

# log settings
# 0-6 from most amount of information to least
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
export SPEC_DEV_DIR=$SPEC_DEV_PARENT_DIR/dev
export SPEC_DEV_VIEW_DIR="$(readlink -f $VIEW_DIR/"$SPEC_DEV_DIR")"
export SPEC_DEV_VIEW="$SPEC_DEV_VIEW_DIR/.spec_dev"
export SPEC_DEV_TARGET="$SPEC_DEV_DIR/.spec_dev"

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
