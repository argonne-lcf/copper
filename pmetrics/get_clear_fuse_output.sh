#!/bin/bash

if [ ! -f ../scripts/env.sh ]; then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../scripts/env.sh

if [ -z "$1" ]; then
  echo "arg 1 missing, output_dir not found!"
  exit 1
fi

(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_log_cache.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_log_operation.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_log_operation_time.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_log_operation_cache_hit.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_log_operation_cache_miss.sh)")

cp "$SPEC_DEV_TARGET_DIR"/*.output "$1"

# (cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_clear_cache.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_clear_operation.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_clear_operation_time.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_clear_operation_cache_hit.sh)")
(cd $INIT_PWD/scripts && bash "$(readlink -f $INIT_PWD/scripts/ioctl_clear_operation_cache_miss.sh)")

rm "$SPEC_DEV_TARGET_DIR"/*.output
