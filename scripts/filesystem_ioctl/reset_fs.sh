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

SCRIPTS_IOCTL_DIR=$(readlink -f "$SCRIPTS_DIR/filesystem_ioctl")
RUN_IOCTL="$(readlink -f "$SCRIPTS_DIR"/filesystem_ioctl/run_ioctl.sh)"

if [ ! -d "$SCRIPTS_IOCTL_DIR" ]; then
  echo "failed to find SCRIPTS_IOCTL_DIR: $SCRIPTS_IOCTL_DIR"
  exit 1
fi

echo "RUN_IOCTL: $RUN_IOCTL"

(cd "$SCRIPTS_IOCTL_DIR" && bash "$RUN_IOCTL" "$IOCTL_RESET_FS")

rm "$SPEC_DEV_DIR"/*.output || true
