#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$(mktemp -d /tmp/un260-layout-model-XXXXXX)
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$root" "-DUI_STATE_STORE_PATH=\"$out/state.cfg\"" \
  "$root/tools/test_main_layout_model.c" "$root/un260/lv_system/ui_main_layout.c" \
  "$root/un260/lv_system/ui_state_store.c" -o "$out/test-layout-model"
"$out/test-layout-model"
