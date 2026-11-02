#!/bin/bash
set -euo pipefail
exec python3 "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/auxiliary_shutdown.py" --scheduler aurora "$@"
