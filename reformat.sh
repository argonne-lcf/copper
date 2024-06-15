#!/bin/bash

echo "formating cu_fuse"
find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=file:"$(pwd)/.clang-format"
