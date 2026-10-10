#!/bin/sh
set -eu
SDK_ROOT=$(pwd)
python3 "$SDK_ROOT/tools/un260-update/release_resources.py" --target "$1" \
    --cleanup-script "$BINARIES_DIR/UN260_RESOURCE_CLEANUP.sh"

python3 "$SDK_ROOT/tools/un260-update/boot_bundle.py" factory --target "$1"

python3 "$SDK_ROOT/tools/un260-update/prepare_update_platform.py" --target "$1"
