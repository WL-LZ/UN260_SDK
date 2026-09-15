#!/usr/bin/env bash
set -euo pipefail
SDK_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$SDK_ROOT/tools/un260-update/build_un260_update.sh"
FIXTURE=$(mktemp -d /tmp/un260-package-budget.XXXXXX)
truncate -s $((17 * 1024 * 1024)) "$FIXTURE/oversize-app"
if "$BUILD" --version budget-test --output "$FIXTURE/invalid.upk" --app "$FIXTURE/oversize-app" > "$FIXTURE/app.log" 2>&1; then
    echo 'FAIL: oversized app accepted'; exit 1
fi
grep -q 'exceeds 16 MiB' "$FIXTURE/app.log"
mkdir -p "$FIXTURE/data"
truncate -s $((25 * 1024 * 1024)) "$FIXTURE/data/large.bin"
if "$BUILD" --version budget-test --output "$FIXTURE/invalid.upk" --lvgl-data "$FIXTURE/data" > "$FIXTURE/payload.log" 2>&1; then
    echo 'FAIL: oversized payload accepted'; exit 1
fi
grep -q 'exceeds 24 MiB' "$FIXTURE/payload.log"
[ ! -e "$FIXTURE/invalid.upk" ]
echo "PASS package app/payload budgets; fixtures=$FIXTURE"
mkdir -p "$FIXTURE/fixture-data/boot_theme_d"
printf oracle > "$FIXTURE/fixture-data/boot_theme_d/boot-light.bin"
if "$BUILD" --version budget-test --output "$FIXTURE/invalid.upk" --lvgl-data "$FIXTURE/fixture-data" > "$FIXTURE/fixture.log" 2>&1; then
    echo 'FAIL: host-only fixture accepted'; exit 1
fi
grep -q 'Host-only boot-light.bin' "$FIXTURE/fixture.log"
[ ! -e "$FIXTURE/invalid.upk" ]
# A individually valid 15 MiB app still exceeds the transaction qualification
# once the unchanged original and 2 MiB reserve must be retained.
mkdir -p "$FIXTURE/empty-data"
printf baseline > "$FIXTURE/small-app"
"$BUILD" --version budget-test --output "$FIXTURE/qualified.upk" --app "$FIXTURE/small-app" --lvgl-data "$FIXTURE/empty-data" > "$FIXTURE/base.log" 2>&1
before=$(sha256sum "$FIXTURE/qualified.upk")
truncate -s $((15 * 1024 * 1024)) "$FIXTURE/staged-app"
if "$BUILD" --version budget-test --output "$FIXTURE/qualified.upk" --app "$FIXTURE/staged-app" --lvgl-data "$FIXTURE/empty-data" > "$FIXTURE/staging.log" 2>&1; then
    echo 'FAIL: oversized transaction accepted'; exit 1
fi
grep -q 'Release staging exceeds' "$FIXTURE/staging.log"
[ "$before" = "$(sha256sum "$FIXTURE/qualified.upk")" ]
echo 'PASS host-fixture rejection, staging qualification, previous output preserved'
