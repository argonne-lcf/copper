#!/bin/bash

rm -rf build
mkdir build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Bbuild || { echo "Failed to create compile commands"; exit 1; }
cp build/compile_commands.json . || { echo "Failed to copy compile commands"; exit 1; }
