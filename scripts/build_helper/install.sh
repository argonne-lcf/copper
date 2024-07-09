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

cd "$ROOT_DIR"

EXTERNAL_DIR=$(readlink -f "$ROOT_DIR"/external)

function install_fuse() {
    if [ -d $EXTERNAL_DIR ]
    then
        echo "skpping extern dir creation - already exists"
        cd $EXTERNAL_DIR
    else
        echo "creating and moving int external dir"
        mkdir $EXTERNAL_DIR && cd $EXTERNAL_DIR
    fi

    echo "installing tar packages"
    FUSE3_FOLDER_NAME=fuse3
    FUSE3_FILENAME_TAR="$FUSE3_FOLDER_NAME.tar.gz"
    FUSE3_FOLDER_ABS_NAME="$EXTERNAL_DIR/$FUSE3_FOLDER_NAME/install"
    curl -L --output $FUSE3_FILENAME_TAR "https://github.com/libfuse/libfuse/releases/download/fuse-3.16.2/fuse-3.16.2.tar.gz"


    echo "removing prev fuse installation"
    sudo rm -rf $FUSE3_FOLDER_NAME || { echo "Failed to rm prev fuse3 dir"; exit 1; }

    mkdir $FUSE3_FOLDER_NAME && tar xf $FUSE3_FILENAME_TAR -C $FUSE3_FOLDER_NAME --strip-components 1 || { echo "Failed to extract and rename fuse3 tar"; exit 1; }
    cd $FUSE3_FOLDER_NAME || { echo "Failed to move into fuse3 dir"; exit 1; }
    mkdir build && cd build
    mkdir $FUSE3_FOLDER_ABS_NAME

    echo "moving build dir"
    meson ..
    echo "configuring meson"
    meson configure --prefix=$FUSE3_FOLDER_ABS_NAME || { echo "Failed to configure build"; exit 1; }
    echo "building project with ninja"
    ninja || { echo "Failed to build with ninja"; exit 1; }
    echo "installing project with ninja"
    sudo ninja install || { echo "Failed to install with ninja"; exit 1; }
}

install_fuse