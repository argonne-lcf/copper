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

rm -rf $ROOT_DIR/build/*
rm -rf $ROOT_DIR/CMakeFiles/* $ROOT_DIR/CMakeCache.txt  $ROOT_DIR/cmake_install.cmake $ROOT_DIR/compile_commands.json
