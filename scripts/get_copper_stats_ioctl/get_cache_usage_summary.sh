#!/bin/bash

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh

if [ ! -f "$ENV" ]
then
    echo "failed to find ENV: $ENV"
    exit 1
fi

source "$ENV"

if [ -z "$1" ]; then
    echo "usage: get_cache_usage_summary.sh <output_dir>"
    exit 1
fi

OUTPUT_DIR=$(readlink -f "$1")
mkdir -p "$OUTPUT_DIR"

GET_ALL_METRICS="$(readlink -f "$SCRIPTPATH/get_all_metrics.sh")"
SUMMARIZE="$(readlink -f "$SCRIPTPATH/summarize_cache_usage.py")"

if [ ! -f "$GET_ALL_METRICS" ]; then
    echo "failed to find GET_ALL_METRICS: $GET_ALL_METRICS"
    exit 1
fi

if [ ! -f "$SUMMARIZE" ]; then
    echo "failed to find SUMMARIZE: $SUMMARIZE"
    exit 1
fi

bash "$GET_ALL_METRICS" "$OUTPUT_DIR"
python3 "$SUMMARIZE" "$OUTPUT_DIR" --csv "$OUTPUT_DIR/cache_usage_summary.csv" | tee "$OUTPUT_DIR/cache_usage_summary.txt"
