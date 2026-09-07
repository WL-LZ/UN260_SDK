#!/bin/sh
# Optional explicit one-command asset conversion + SDK rebuild + UPK/IMG.
set -eu
APP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK_ROOT=$(CDPATH= cd -- "$APP_ROOT/../../.." && pwd)
test -f "$SDK_ROOT/Makefile"
test -d "$SDK_ROOT/package/artinchip/test-lvgl"
python3 "$APP_ROOT/tools/convert_lvgl_assets.py"
make -C "$SDK_ROOT" test-lvgl-rebuild
make -C "$SDK_ROOT"
