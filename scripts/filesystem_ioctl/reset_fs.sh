#!/bin/bash
set -o nounset

if [ ! -f ../env.sh ]; then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../env.sh

SCRIPTS_IOCTL_DIR=$(readlink -f "$SCRIPTS_DIR/filesystem_ioctl")
RUN_IOCTL="$(readlink -f "$SCRIPTS_DIR"/filesystem_ioctl/run_ioctl.sh)"

if [ ! -d "$SCRIPTS_IOCTL_DIR" ]; then
  echo "failed to find SCRIPTS_IOCTL_DIR: $SCRIPTS_IOCTL_DIR"
  exit 1
fi

echo "RUN_IOCTL: $RUN_IOCTL"

(cd "$SCRIPTS_IOCTL_DIR" && bash "$RUN_IOCTL" "$IOCTL_RESET_FS")

rm "$SPEC_DEV_TARGET_DIR"/*.output || true
