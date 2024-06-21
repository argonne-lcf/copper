#!/bin/bash

# You need to set VIEW_DIR and TARGET_DIR

export INIT_PWD=$(pwd)/..
export VIEW_DIR=
export TARGET_DIR=
export FUSE_FS="$INIT_PWD/build/cu_fuse"
export SPEC_DEV_VIEW_DIR="$VIEW_DIR/dev"
export SPEC_DEV_TARGET_DIR="$TARGET_DIR/dev"
export SPEC_DEV_VIEW="$SPEC_DEV_VIEW_DIR/.spec_dev"
export SPEC_DEV_TARGET="$SPEC_DEV_TARGET_DIR/.spec_dev"
