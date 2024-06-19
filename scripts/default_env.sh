#!/bin/bash

# You need to set VIEW_DIR and TARGET_DIR

export INIT_PWD=$(pwd)/..
export VIEW_DIR=
export TARGET_DIR=
export FUSE_FS="$INIT_PWD/build/cu_fuse"
export SPEC_DEV_VIEW="$VIEW_DIR/.spec_dev"
export SPEC_DEV_TARGET="$TARGET_DIR/.spec_dev"

