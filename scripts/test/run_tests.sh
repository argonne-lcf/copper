#!/bin/bash

# SET TO WHAT SCRIPTS YOU WANT TO TEST
#   Options:
#     1. view
#     2. target
#     3. view_and_target
export WHAT_TO_TEST=view

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh

if [ ! -f $ENV ]
then
	echo "failed to find ENV: $ENV"
	exit 1
fi

source $ENV

export TEST_ID=$(shuf -i 1-9999999 -n 1)
export HOSTNAME=$(hostname)

export TEST_TOP_DIR=$SCRIPTS_DIR/test/test_output
export TEST_ID_TOP_DIR=$TEST_TOP_DIR
export TEST_OUTPUT_DIR="$TEST_ID_TOP_DIR/$(hostname)/${TEST_ID}"
export TEST_OUTPUT_VIEW_DIR=$TEST_OUTPUT_DIR/view
export TEST_OUTPUT_TARGET_DIR=$TEST_OUTPUT_DIR/target

export TEST_TIME_FILENAME=output_time.csv
export TEST_TIME_VIEW_PATH=$TEST_OUTPUT_VIEW_DIR/$TEST_TIME_FILENAME
export TEST_TIME_TARGET_PATH=$TEST_OUTPUT_TARGET_DIR/$TEST_TIME_FILENAME

mkdir -p $TEST_OUTPUT_DIR
mkdir -p $TEST_OUTPUT_VIEW_DIR
mkdir -p $TEST_OUTPUT_TARGET_DIR

export VIEW_SCRIPT_PATH=$SCRIPTS_DIR/test/view_test.py
export TARGET_SCRIPT_PATH=$SCRIPTS_DIR/test/target_test.py

# module use /soft/preview-modulefiles/24.086.0
# module load frameworks/2024.04.15.002

python3 driver.py | tee "$TEST_OUTPUT_DIR/driver_output.stdout"

