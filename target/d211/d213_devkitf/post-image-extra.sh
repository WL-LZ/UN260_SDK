#!/bin/bash

set -euo pipefail

SDK_ROOT=$(pwd)
BUILDER="$SDK_ROOT/tools/un260-update/build_un260_update.sh"
BOARD_CONFIG="${TARGET_BOARD_DIR}/image_cfg.json"
OUTPUT_PACKAGE="${BINARIES_DIR}/UN260_UPDATE.upk"
EXTRA_ROOT="${TARGET_BOARD_DIR}/update_extra_root"

if [ ! -x "$BUILDER" ]; then
	echo "UN260 package builder is missing or not executable: $BUILDER" >&2
	exit 1
fi

PACKAGE_VERSION=${UN260_UPDATE_VERSION:-}
if [ -z "$PACKAGE_VERSION" ]; then
	IMAGE_VERSION=$(sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
		"$BOARD_CONFIG" | head -n 1)
	[ -n "$IMAGE_VERSION" ] || IMAGE_VERSION=0.0.0

	if GIT_REV=$(git -C "$SDK_ROOT" rev-parse --short=12 HEAD 2>/dev/null); then
		GIT_SUFFIX="-$GIT_REV"
		if [ -n "$(git -C "$SDK_ROOT" status --porcelain --untracked-files=normal 2>/dev/null)" ]; then
			GIT_SUFFIX="$GIT_SUFFIX-dirty"
		fi
	else
		GIT_SUFFIX=-local
	fi
	PACKAGE_VERSION="${IMAGE_VERSION}${GIT_SUFFIX}"
fi

echo "Generate UN260 U-disk package: version=$PACKAGE_VERSION"
BUILD_ARGS=(
	--version "$PACKAGE_VERSION"
	--output "$OUTPUT_PACKAGE"
)
if [ -d "$EXTRA_ROOT" ]; then
	echo "Include UN260 incremental root: $EXTRA_ROOT"
	BUILD_ARGS+=(--extra-root "$EXTRA_ROOT")
fi

"$BUILDER" "${BUILD_ARGS[@]}"

echo "UN260 U-disk package is ready: $OUTPUT_PACKAGE"
