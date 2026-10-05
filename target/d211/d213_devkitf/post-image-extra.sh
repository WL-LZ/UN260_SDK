#!/bin/bash

set -euo pipefail

SDK_ROOT=$(pwd)
BUILDER="$SDK_ROOT/tools/un260-update/build_un260_update.sh"
INTERNAL_DIR="${BINARIES_DIR}/un260_internal"
OUTPUT_PACKAGE="${INTERNAL_DIR}/UN260_APPLICATION.upk"
PUBLIC_PACKAGE="${BINARIES_DIR}/UN260_UPDATE.upk"
MIGRATION_PACKAGE="${INTERNAL_DIR}/UN260_FIRST_MIGRATION.upk"
EXTRA_ROOT="${TARGET_BOARD_DIR}/update_extra_root"

# Public files stay at images/; intermediates and the pinned delta baseline
# are not operator packages. Never generate/overwrite archived expansion UPKs.
if [ -L "$INTERNAL_DIR" ]; then
    echo "Refusing symlinked internal package directory: $INTERNAL_DIR" >&2
    exit 1
fi
mkdir -p "$INTERNAL_DIR"

if [ ! -x "$BUILDER" ]; then
	echo "UN260 package builder is missing or not executable: $BUILDER" >&2
	exit 1
fi

PACKAGE_VERSION=$(python3 "$SDK_ROOT/tools/un260-update/release_version.py")

echo "Generate UN260 U-disk package: version=$PACKAGE_VERSION"
BUILD_ARGS=(
	--version "$PACKAGE_VERSION"
	--output "$OUTPUT_PACKAGE"
)
if [ -d "$EXTRA_ROOT" ]; then
	echo "Include UN260 incremental root: $EXTRA_ROOT"
	BUILD_ARGS+=(--extra-root "$EXTRA_ROOT")
fi

BASELINE=${UN260_UPDATE_BASELINE:-"${INTERNAL_DIR}/UN260_UPDATE_BASE.upk"}
# Adopt a pre-organization baseline without silently replacing its identity.
# This is a host build hook (GNU cmp), not a BusyBox device script.
LEGACY_BASELINE="${BINARIES_DIR}/UN260_UPDATE_BASE.upk"
if [ -z "${UN260_UPDATE_BASELINE:-}" ] && [ -e "$LEGACY_BASELINE" ]; then
    [ -f "$LEGACY_BASELINE" ] && [ ! -L "$LEGACY_BASELINE" ] && [ ! -L "$BASELINE" ] || exit 1
    if [ -e "$BASELINE" ]; then
        cmp -s "$LEGACY_BASELINE" "$BASELINE" || {
            echo "Conflicting pinned baselines; review before building" >&2
            exit 1
        }
    else
        cp -p "$LEGACY_BASELINE" "$BASELINE"
    fi
fi
for GENERATED in "$OUTPUT_PACKAGE" "${BINARIES_DIR}/UN260_UPDATE.upk" \
    "$MIGRATION_PACKAGE" "${INTERNAL_DIR}/UN260_STORAGE_BOOTSTRAP.upk" "${INTERNAL_DIR}/UN260_UPDATE_DELTA.upk"; do
    if [ "$(readlink -m "$BASELINE")" = "$(readlink -m "$GENERATED")" ]; then
        echo "Baseline must be a separate pinned file, not a build output" >&2
        exit 1
    fi
done
"$BUILDER" "${BUILD_ARGS[@]}"
python3 "$SDK_ROOT/tools/un260-update/build_storage_bootstrap.py" --full "$OUTPUT_PACKAGE" \
    --output "${INTERNAL_DIR}/UN260_STORAGE_BOOTSTRAP.upk"
# One public package handles both legacy enrollment and managed-volume updates.
# Its tiny bridge remains compatible with installed R7/R8 updaters; the current
# updater routes managed devices directly to the same final application.
MIGRATION_POLICY=${UN260_MIGRATION_POLICY:-${UN260_MIGRATION_APPROVED:-un260-backed-up-legacy-v3}}
python3 "$SDK_ROOT/tools/un260-update/build_unified.py" --full "$OUTPUT_PACKAGE" \
    --output "$MIGRATION_PACKAGE" \
    --migration-policy "$MIGRATION_POLICY"

# Pin the first full package made by this upgraded builder. Do not silently
# move this baseline on later development builds. First deployment uses FULL.
if [ ! -f "$BASELINE" ]; then
    if [ -n "${UN260_UPDATE_BASELINE:-}" ]; then echo "Specified baseline missing: $BASELINE" >&2; exit 1; fi
    cp "$OUTPUT_PACKAGE" "$BASELINE"
    echo "Pinned initial baseline; install full UN260_UPDATE.upk before future deltas."
fi
python3 "$SDK_ROOT/tools/un260-update/build_delta.py" --base "$BASELINE" --full "$OUTPUT_PACKAGE" \
    --output "${INTERNAL_DIR}/UN260_UPDATE_DELTA.upk"

python3 "$SDK_ROOT/tools/un260-update/verify_unified_release.py" \
    --unified "$MIGRATION_PACKAGE" --full "$OUTPUT_PACKAGE" --target "$TARGET_DIR" \
    --migration-policy "$MIGRATION_POLICY"
python3 "$SDK_ROOT/tools/un260-update/verify_direct_release.py" \
    --package "$OUTPUT_PACKAGE" --target "$TARGET_DIR"
BASE_VERSION=$(python3 "$SDK_ROOT/tools/un260-update/release_version.py" --base)
python3 "$SDK_ROOT/tools/un260-update/verify_factory_release.py" \
    --image "${BINARIES_DIR}/d211_d213_devkitf_page_2k_block_128k_v${BASE_VERSION}.img" \
    --application "$OUTPUT_PACKAGE" --target "$TARGET_DIR"
# Publish the verified unified package and its matching public checksum.
for PUBLISH_PATH in "$PUBLIC_PACKAGE" "$PUBLIC_PACKAGE.tmp" "$PUBLIC_PACKAGE.sha256" "$PUBLIC_PACKAGE.sha256.tmp"; do
    [ ! -L "$PUBLISH_PATH" ] || { echo "Refusing symlinked release output: $PUBLISH_PATH" >&2; exit 1; }
done
cp "$MIGRATION_PACKAGE" "$PUBLIC_PACKAGE.tmp"
cmp -s "$MIGRATION_PACKAGE" "$PUBLIC_PACKAGE.tmp"
mv -f "$PUBLIC_PACKAGE.tmp" "$PUBLIC_PACKAGE"
(cd "$BINARIES_DIR" && sha256sum UN260_UPDATE.upk) > "$PUBLIC_PACKAGE.sha256.tmp"
mv -f "$PUBLIC_PACKAGE.sha256.tmp" "$PUBLIC_PACKAGE.sha256"
echo "UN260 unified U-disk update is ready: $PUBLIC_PACKAGE"
echo "Verified internal application: $OUTPUT_PACKAGE"
