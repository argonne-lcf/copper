#!/bin/bash
set -o nounset

if [ ! -f ../scripts/env.sh ]; then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../scripts/env.sh

if [ -z "$1" ]; then
  echo "usage: get_clear_fuse_output.sh <output_dir>"
  exit 1
fi

RUN_IOCTL="$(readlink -f "$SCRIPTS_DIR"/run_ioctl.sh)"

(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_CACHE")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_TIME")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_CACHE_HIT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_CACHE_MISS")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_DATA_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_DIR_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_MD_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_NEG")

cp -f "$SPEC_DEV_TARGET_DIR"/*.output "$1"

(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_CACHE")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_TIME")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_CACHE_HIT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_CACHE_MISS")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")
(cd "$SCRIPTS_DIR" && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_NEG")

rm "$SPEC_DEV_TARGET_DIR"/*.output
