#!/bin/bash

echo "formatting driver"
find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=file:"$(pwd)/.clang-format"

echo "formating mongo fuse fs"
find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=file:"$(pwd)/.clang-format"
