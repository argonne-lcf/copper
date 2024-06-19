#!/bin/bash

if [ ! -f ../scripts/env.sh ]
then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../scripts/env.sh

python3 driver.py
