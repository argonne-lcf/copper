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

if [ -f "$SPEC_DEV_TARGET" ]
then
  echo "found spec_dev: $SPEC_DEV_TARGET"
else
  echo "creating spec_dev: $SPEC_DEV_TARGET"
  mkdir -p "$(dirname "$SPEC_DEV_TARGET")"
  touch "$SPEC_DEV_TARGET"
fi

if [ -z "$1" ]; then
  echo "usage: get_all_metrics.sh <output_dir>"
  exit 1
fi

SCRIPTS_IOCTL_DIR=$(readlink -f "$SCRIPTS_DIR/get_copper_stats_ioctl")
RUN_IOCTL="$(readlink -f "$SCRIPTS_DIR"/get_copper_stats_ioctl/run_ioctl.sh)"

if [ ! -d "$SCRIPTS_IOCTL_DIR" ]; then
  echo "failed to find SCRIPTS_IOCTL_DIR: $SCRIPTS_IOCTL_DIR"
  exit 1
fi

echo "RUN_IOCTL: $RUN_IOCTL"

(cd "$SCRIPTS_IOCTL_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_ALL_METRICS")

cp -f "$SPEC_DEV_DIR"/*.output "$1"
