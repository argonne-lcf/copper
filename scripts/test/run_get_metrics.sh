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

export SEARCH_DIR="$SCRIPTS_DIR"
export OUTPUT_DIR=$SCRIPTS_DIR/test/metrics
export CSV_VIEW_OUTPUT_PATH=$OUTPUT_DIR/view_metrics.csv
export CSV_TARGET_OUTPUT_PATH=$OUTPUT_DIR/target_metrics.csv

echo "deleting prev metrics folder"
rm -r $OUTPUT_DIR
mkdir -p $OUTPUT_DIR

# Run the Python script with the provided directory parameter
python3 $SCRIPTS_DIR/test/get_metrics.py "$SEARCH_DIR"
