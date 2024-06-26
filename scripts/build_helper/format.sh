#!/bin/bash
set -o nounset

if [ ! -f ../env.sh ]
then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../env.sh

cd "$ROOT_DIR"

echo "formating cu_fuse"
find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=file:"$(pwd)/.clang-format"