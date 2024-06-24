#!/bin/bash
set -o nounset

# You need to set VIEW_DIR and TARGET_DIR

export ROOT_DIR=$(pwd)/..
export SCRIPTS_DIR="$ROOT_DIR"/scripts
export VIEW_DIR=
export TARGET_DIR=
export FUSE_FS="$INIT_PWD/build/cu_fuse"
export SPEC_DEV_VIEW_DIR="$VIEW_DIR/dev"
export SPEC_DEV_TARGET_DIR="$TARGET_DIR/dev"
export SPEC_DEV_VIEW="$SPEC_DEV_VIEW_DIR/.spec_dev"
export SPEC_DEV_TARGET="$SPEC_DEV_TARGET_DIR/.spec_dev"

export IOCTL_CLEAR_CACHE=100
export IOCTL_CLEAR_OPERATION=700
export IOCTL_CLEAR_OPERATION_CACHE_HIT=800
export IOCTL_CLEAR_OPERATION_CACHE_MISS=900
export IOCTL_CLEAR_OPERATION_TIME=1000
export IOCTL_CLEAR_DATA_CACHE_EVENT=1400
export IOCTL_CLEAR_DIR_CACHE_EVENT=1500
export IOCTL_CLEAR_MD_CACHE_EVENT=1600

export IOCTL_LOG_CACHE=200
export IOCTL_LOG_OPERATION=300
export IOCTL_LOG_OPERATION_TIME=400
export IOCTL_LOG_OPERATION_CACHE_HIT=500
export IOCTL_LOG_OPERATION_CACHE_MISS=600
export IOCTL_LOG_DATA_CACHE_EVENT=1100
export IOCTL_LOG_DIR_CACHE_EVENT=1200
export IOCTL_LOG_MD_CACHE_EVENT=1300
