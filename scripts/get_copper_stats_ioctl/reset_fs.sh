#!/bin/bash
set -euo pipefail
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/run_ioctl.sh" reset-fs "$@"
