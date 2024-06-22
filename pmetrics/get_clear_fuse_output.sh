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

RUN_IOCTL="$(readlink -f "$INIT_PWD"/scripts/run_ioctl.sh)"

(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_CACHE")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_TIME")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_CACHE_HIT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_OPERATION_CACHE_MISS")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_DATA_CACHE_EVENT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_DIR_CACHE_EVENT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_LOG_MD_CACHE_EVENT")

cp "$SPEC_DEV_TARGET_DIR"/*.output "$1"

(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_CACHE")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_TIME")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_CACHE_HIT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_OPERATION_CACHE_MISS")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")
(cd "$INIT_PWD"/scripts && bash "$RUN_IOCTL" "$IOCTL_CLEAR_DATA_CACHE_EVENT")

rm "$SPEC_DEV_TARGET_DIR"/*.output
