#!/bin/bash
set -euo pipefail
# Outputs are written by Copper itself; never copy unverified stale *.output files.
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/run_ioctl.sh" all-metrics "$@"
