#!/bin/bash
set -euo pipefail

SDK_ROOT=$(pwd)
BUILDER="$SDK_ROOT/tools/un260-update/build_un260_update.sh"
INTERNAL_DIR="${BINARIES_DIR}/un260_internal"
OUTPUT_PACKAGE="${INTERNAL_DIR}/UN260_APPLICATION.upk"
PUBLIC_PACKAGE="${BINARIES_DIR}/UN260_UPDATE.upk"
PUBLIC_README="${BINARIES_DIR}/UN260_UPK_README.md"
EXTRA_ROOT="${TARGET_BOARD_DIR}/update_extra_root"

[ ! -L "$INTERNAL_DIR" ] || { echo "Refusing symlinked internal package directory" >&2; exit 1; }
mkdir -p "$INTERNAL_DIR"
[ -x "$BUILDER" ] || { echo "UN260 package builder missing: $BUILDER" >&2; exit 1; }
PACKAGE_VERSION=$(python3 "$SDK_ROOT/tools/un260-update/release_version.py")
BUILD_ARGS=(--version "$PACKAGE_VERSION" --output "$OUTPUT_PACKAGE")
if [ -d "$EXTRA_ROOT" ]; then BUILD_ARGS+=(--extra-root "$EXTRA_ROOT"); fi

# The factory IMG owns the platform and pre-enrolled app volume. Routine
# updates are one full transaction; migration/delta tools are explicit only.
"$BUILDER" "${BUILD_ARGS[@]}"
python3 "$SDK_ROOT/tools/un260-update/verify_direct_release.py" \
    --package "$OUTPUT_PACKAGE" --target "$TARGET_DIR" --report "$INTERNAL_DIR/UN260_RELEASE.json"
BASE_VERSION=$(python3 "$SDK_ROOT/tools/un260-update/release_version.py" --base)
FACTORY_ARGS=(
    --image "${BINARIES_DIR}/d211_d213_devkitf_page_2k_block_128k_v${BASE_VERSION}.img" \
    --application "$OUTPUT_PACKAGE" --target "$TARGET_DIR")
if [ -n "${UN260_FACTORY_DEEP_QA_PYTHON:-}" ]; then
    [ -x "$UN260_FACTORY_DEEP_QA_PYTHON" ] || { echo "Deep factory QA interpreter is not executable" >&2; exit 1; }
    "$UN260_FACTORY_DEEP_QA_PYTHON" "$SDK_ROOT/tools/un260-update/verify_factory_release.py" "${FACTORY_ARGS[@]}" --deep
else
    python3 "$SDK_ROOT/tools/un260-update/verify_factory_release.py" "${FACTORY_ARGS[@]}"
    echo "Factory component CRCs, UBIFS image bytes, release contract and TARGET checked; deep UBIFS inode readback not performed (set UN260_FACTORY_DEEP_QA_PYTHON for release QA)."
fi

# Preserve only known historical operator packages away from the current
# release slot. Unknown files and user archives are never enumerated/deleted.
ARCHIVE_DIR="$INTERNAL_DIR/archived-public"
archive_public_file()
{
    local source="$BINARIES_DIR/$1" destination="$ARCHIVE_DIR/$1"
    if [ ! -e "$source" ] && [ ! -L "$source" ]; then return; fi
    [ -f "$source" ] && [ ! -L "$source" ] || { echo "Unsafe historical output: $source" >&2; exit 1; }
    [ ! -L "$ARCHIVE_DIR" ] || { echo "Unsafe archive directory" >&2; exit 1; }
    mkdir -p "$ARCHIVE_DIR"
    if [ -e "$destination" ] || [ -L "$destination" ]; then
        echo "Historical output already archived; preserve both and review: $source" >&2
        exit 1
    fi
    if [ -e "$source.sha256" ] || [ -L "$source.sha256" ]; then
        [ -f "$source.sha256" ] && [ ! -L "$source.sha256" ] && [ ! -e "$destination.sha256" ] && [ ! -L "$destination.sha256" ] || exit 1
    fi
    mv "$source" "$destination"
    if [ -e "$source.sha256" ]; then mv "$source.sha256" "$destination.sha256"; fi
}
for HISTORICAL in UN260_APPLICATION.upk UN260_UPDATE_BASE.upk UN260_UPDATE_DELTA.upk \
    UN260_UNIFIED_UPDATE.upk UN260_FIRST_MIGRATION.upk UN260_STORAGE_BOOTSTRAP.upk \
    UN260_STORAGE_EXPANSION_R7_20260929.upk UN260_STORAGE_EXPANSION_R8_20260930.upk; do
    archive_public_file "$HISTORICAL"
done

for PUBLISH_PATH in "$PUBLIC_PACKAGE" "$PUBLIC_PACKAGE.tmp" "$PUBLIC_PACKAGE.sha256" "$PUBLIC_PACKAGE.sha256.tmp" "$PUBLIC_PACKAGE.release.json" "$PUBLIC_PACKAGE.release.json.tmp" "$PUBLIC_README" "$PUBLIC_README.tmp"; do
    [ ! -L "$PUBLISH_PATH" ] || { echo "Refusing symlinked release output: $PUBLISH_PATH" >&2; exit 1; }
done
cat > "$PUBLIC_README.tmp" <<'EOF'
# UN260 正式升级包

本轮先烧录配套的正式 IMG，建立 V1 升级基座。此后普通完整 make 只向客户交付一个升级包：`UN260_UPDATE.upk`，以及配套的校验文件。

将升级包放在 U 盘的 `update/UN260_UPDATE.upk`，在机器界面发起一次升级。升级在一次事务中完成；按界面提示重启后，系统进行健康确认。

`un260_internal/` 中的包、验证报告和历史归档仅供内部使用，不交付客户。
EOF
chmod 0644 "$PUBLIC_README.tmp"
cp "$OUTPUT_PACKAGE" "$PUBLIC_PACKAGE.tmp"
cmp -s "$OUTPUT_PACKAGE" "$PUBLIC_PACKAGE.tmp"
mv -f "$PUBLIC_PACKAGE.tmp" "$PUBLIC_PACKAGE"
(cd "$BINARIES_DIR" && sha256sum UN260_UPDATE.upk) > "$PUBLIC_PACKAGE.sha256.tmp"
mv -f "$PUBLIC_PACKAGE.sha256.tmp" "$PUBLIC_PACKAGE.sha256"
cp "$INTERNAL_DIR/UN260_RELEASE.json" "$PUBLIC_PACKAGE.release.json.tmp"
mv -f "$PUBLIC_PACKAGE.release.json.tmp" "$PUBLIC_PACKAGE.release.json"
mv -f "$PUBLIC_README.tmp" "$PUBLIC_README"
echo "UN260 direct U-disk update ready: $PUBLIC_PACKAGE"
echo "Verified internal application: $OUTPUT_PACKAGE"
