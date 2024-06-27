#!/bin/bash
set -o nounset

ITERATIONS=1

if [ ! -f ../env.sh ]
then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../env.sh

export JOB_ID=$(shuf -i 1-9999999 -n 1)
mkdir "$(pwd)/job_${JOB_ID}"
export JOB_OUTPUT_DIR="$(pwd)/job_${JOB_ID}"

python3 driver.py $ITERATIONS | tee "$JOB_OUTPUT_DIR/driver_output.stdout"


