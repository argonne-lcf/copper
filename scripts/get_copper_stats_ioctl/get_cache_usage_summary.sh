#!/bin/bash
set -euo pipefail
# Deliberately offline: request a new snapshot separately, then summarize its committed bundle.
exec python3 "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/summarize_cache_usage.py" "$@"
