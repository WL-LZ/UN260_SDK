#!/bin/sh
set -eu
SDK_ROOT=$(pwd)
exec python3 "$SDK_ROOT/tools/un260-update/release_resources.py" --target "$1" \
    --cleanup-script "$BINARIES_DIR/UN260_RESOURCE_CLEANUP.sh"
