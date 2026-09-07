#!/bin/sh
set -eu
APP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_ROOT=$(mktemp -d /tmp/un260-services-XXXXXX)
# Keep the small diagnostic directory for inspection; no recursive cleanup.
mkdir -p "$TEST_ROOT/backlight/pwm" "$TEST_ROOT/state"
printf '10\n' > "$TEST_ROOT/backlight/pwm/max_brightness"
printf '5\n' > "$TEST_ROOT/backlight/pwm/brightness"
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=undefined -I"$APP_ROOT" \
    "-DUN260_BACKLIGHT_ROOT=\"$TEST_ROOT/backlight\"" \
    "-DUN260_BACKLIGHT_STATE=\"$TEST_ROOT/state\"" \
    "$APP_ROOT/tools/test_backlight.c" "$APP_ROOT/un260/lv_system/backlight_service.c" \
    -o "$TEST_ROOT/test_backlight"
"$TEST_ROOT/test_backlight"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -I"$APP_ROOT" \
    "$APP_ROOT/tools/test_touch_frame.c" "$APP_ROOT/un260/gesture/touch_frame.c" \
    -o "$TEST_ROOT/test_touch"
"$TEST_ROOT/test_touch"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
    -I"$APP_ROOT/tools/tests/stubs" -I"$APP_ROOT" \
    "$APP_ROOT/tools/test_gesture.c" "$APP_ROOT/un260/gesture/gesture_service.c" \
    -o "$TEST_ROOT/test_gesture"
"$TEST_ROOT/test_gesture"
printf 'Test artifacts: %s\n' "$TEST_ROOT"
