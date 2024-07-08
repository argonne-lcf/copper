#!/bin/bash

ITERATIONS=1

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh

if [ ! -f $ENV ]
then
	echo "failed to find ENV: $ENV"
	exit 1
fi

source $ENV

export JOB_ID=$(shuf -i 1-9999999 -n 1)
mkdir "$(pwd)/job_${JOB_ID}"
export JOB_OUTPUT_DIR="$(pwd)/job_${JOB_ID}"

python3 driver.py $ITERATIONS | tee "$JOB_OUTPUT_DIR/driver_output.stdout"


