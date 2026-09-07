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
