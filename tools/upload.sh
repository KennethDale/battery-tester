#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
pio run --target uploadfs
pio run --target upload
