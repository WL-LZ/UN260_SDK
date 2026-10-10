#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SDK_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
TARGET_ROOT=${TARGET_DIR:-"$SDK_ROOT/output/d211_d213_devkitf/target"}

APP_PATH="$TARGET_ROOT/usr/local/bin/test_lvgl"
LVGL_LIB_PATH="$TARGET_ROOT/usr/local/lib/liblvgl.so"
LVGL_DATA_PATH="$TARGET_ROOT/usr/local/share/lvgl_data"
UPDATER_PATH="$SDK_ROOT/target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh"
APP_STORAGE_PATH="$SDK_ROOT/target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage"
STARTUP_PATH="$SDK_ROOT/package/artinchip/test-lvgl/S20test_lvgl"

VERSION=""
OUTPUT_PATH=""
EXTRA_ROOT=""
STORAGE_ROUTE=factory-app-volume
LEGACY_TEST=0

usage()
{
    cat <<'EOF'
Usage:
  build_un260_update.sh --version VERSION --output FILE [options]

Required:
  --version VERSION       Package version, using letters, numbers, '.', '_' or '-'
  --output FILE           Output package path, normally UN260_UPDATE.upk

Optional:
  --extra-root DIR        Add rootfs-relative files from DIR
  --app FILE              Override built test_lvgl path
  --lvgl-lib FILE         Override built liblvgl.so path
  --lvgl-data DIR         Override built lvgl_data directory
  --legacy-test          Application-only test for old factory firmware; excludes native boot files
  --unified-route        Qualify the internal application for the verified unified migration route
  --legacy-root          Explicitly qualify an old root-only layout
  --help                  Show this help

The fixed core payload is always included. Extra files are restricted to the
same safe install allowlist used by the device-side installer.
EOF
}

die()
{
    echo "ERROR: $*" >&2
    exit 1
}

is_safe_relative_path()
{
    local rel=$1

    [[ -n "$rel" ]] || return 1
    [[ "$rel" != /* ]] || return 1
    [[ "$rel" != *..* ]] || return 1
    [[ "$rel" != *//* ]] || return 1
    [[ "$rel" != *\\* ]] || return 1
    [[ "$rel" =~ ^[A-Za-z0-9_./-]+$ ]] || return 1
}

is_allowed_target()
{
    local rel=$1

    case "$rel" in
        usr/local/bin/*|usr/local/lib/*|usr/local/share/*|etc/un260/*)
            return 0
            ;;
        usr/bin/ui_update.sh|etc/init.d/S00lvgl)
            return 0
            ;;
    esac
    return 1
}

is_fixed_target()
{
    local rel=$1

    case "$rel" in
        usr/local/bin/test_lvgl|usr/local/lib/liblvgl.so|usr/local/share/lvgl_data/*|\
        usr/bin/ui_update.sh|etc/init.d/S00lvgl|etc/un260/package-version|etc/un260/update-core.manifest|etc/un260/startup-runtime.sha256|usr/local/lib/un260-boot/*|etc/un260/boot-active|etc/un260/boot-active.*|usr/local/lib/un260-startup/runtime.sh|usr/local/bin/un260_unpack|usr/local/bin/un260_storage_sync|usr/local/bin/un260_upgrade_display|usr/local/bin/un260_runtime_recover|usr/local/bin/un260_resource_cleanup|usr/local/bin/un260_app_storage)
            return 0
            ;;
    esac
    return 1
}

copy_payload_file()
{
    local src=$1
    local rel=$2
    local mode=$3
    local dest="$PKG_ROOT/payload/$rel"

    [[ -f "$src" ]] || die "Required file is missing: $src"
    mkdir -p "$(dirname "$dest")"
    cp -f "$src" "$dest"
    chmod "$mode" "$dest"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || die "--version requires a value"
            VERSION=$2
            shift 2
            ;;
        --output)
            [[ $# -ge 2 ]] || die "--output requires a value"
            OUTPUT_PATH=$2
            shift 2
            ;;
        --extra-root)
            [[ $# -ge 2 ]] || die "--extra-root requires a value"
            EXTRA_ROOT=$2
            shift 2
            ;;
        --app)
            [[ $# -ge 2 ]] || die "--app requires a value"
            APP_PATH=$2
            shift 2
            ;;
        --lvgl-lib)
            [[ $# -ge 2 ]] || die "--lvgl-lib requires a value"
            LVGL_LIB_PATH=$2
            shift 2
            ;;
        --lvgl-data)
            [[ $# -ge 2 ]] || die "--lvgl-data requires a value"
            LVGL_DATA_PATH=$2
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --legacy-test)
            LEGACY_TEST=1
            shift
            ;;
        --unified-route)
            STORAGE_ROUTE=unified-app-volume
            shift
            ;;
        --legacy-root)
            STORAGE_ROUTE=legacy-root
            shift
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
done

if [[ "$LEGACY_TEST" = 0 ]]; then
    STARTUP_PATH="$TARGET_ROOT/etc/init.d/S00lvgl"
    UPDATER_PATH="$TARGET_ROOT/usr/bin/ui_update.sh"
    APP_STORAGE_PATH="$TARGET_ROOT/usr/local/bin/un260_app_storage"
fi
[[ "$VERSION" =~ ^[A-Za-z0-9._-]{1,48}$ ]] ||
    die "A valid --version is required"
[[ -n "$OUTPUT_PATH" ]] || die "--output is required"
[[ -f "$APP_PATH" ]] || die "test_lvgl not found: $APP_PATH"
[[ -f "$LVGL_LIB_PATH" ]] || die "liblvgl.so not found: $LVGL_LIB_PATH"
[[ -d "$LVGL_DATA_PATH" ]] || die "lvgl_data not found: $LVGL_DATA_PATH"
[[ -f "$UPDATER_PATH" ]] || die "ui_update.sh not found: $UPDATER_PATH"
[[ -f "$STARTUP_PATH" ]] || die "S00lvgl source not found: $STARTUP_PATH"

# Shipping budget, deliberately based on uncompressed bytes. Compression of the
# UPK is not a promise that a board can stage its replacement executable.
PAYLOAD_BUDGET_BYTES=$((24 * 1024 * 1024))
PAYLOAD_BUDGET_MIB=24
if [[ "$LEGACY_TEST" = 0 ]]; then
    PAYLOAD_BUDGET_BYTES=$((32 * 1024 * 1024))
    PAYLOAD_BUDGET_MIB=32
fi
app_bytes=$(stat -c %s "$APP_PATH")
APP_BUDGET_ARGS=()
if [[ "$LEGACY_TEST" = 1 ]]; then APP_BUDGET_ARGS+=(--legacy-test); fi
APP_BUDGET_BYTES=$(python3 "$SCRIPT_DIR/factory_layout.py" --check-app-size "$app_bytes" "${APP_BUDGET_ARGS[@]}") ||
    die "test_lvgl cannot retain both application generations safely; review embedded assets, do not bypass device preflight"

if [[ -n "$EXTRA_ROOT" ]]; then
    [[ -d "$EXTRA_ROOT" ]] || die "Extra root directory not found: $EXTRA_ROOT"
fi

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/un260-update.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT
PKG_ROOT="$WORK_DIR/package"
mkdir -p "$PKG_ROOT/payload"

if [[ -n "$EXTRA_ROOT" ]]; then
    if find "$EXTRA_ROOT" -type l -print -quit | grep -q .; then
        die "Symbolic links are not allowed in --extra-root"
    fi

    while IFS= read -r -d '' extra_file; do
        rel=${extra_file#"$EXTRA_ROOT"/}
        is_safe_relative_path "$rel" || die "Unsafe extra path: $rel"
        is_allowed_target "$rel" || die "Extra path is not allowed: $rel"
        is_fixed_target "$rel" && die "Extra path conflicts with fixed core payload: $rel"
        mkdir -p "$PKG_ROOT/payload/$(dirname "$rel")"
        cp -f "$extra_file" "$PKG_ROOT/payload/$rel"
    done < <(find "$EXTRA_ROOT" -type f -print0)
fi

rm -rf "$PKG_ROOT/payload/usr/local/share/lvgl_data"
mkdir -p "$PKG_ROOT/payload/usr/local/share"
cp -a "$LVGL_DATA_PATH" "$PKG_ROOT/payload/usr/local/share/lvgl_data"
find "$PKG_ROOT/payload/usr/local/share/lvgl_data" -type d -exec chmod 0755 {} +
find "$PKG_ROOT/payload/usr/local/share/lvgl_data" -type f -exec chmod 0644 {} +

copy_payload_file "$LVGL_LIB_PATH" "usr/local/lib/liblvgl.so" 0755
copy_payload_file "$TARGET_ROOT/usr/local/bin/un260_storage_sync" "usr/local/bin/un260_storage_sync" 0755
copy_payload_file "$TARGET_ROOT/usr/local/bin/un260_upgrade_display" "usr/local/bin/un260_upgrade_display" 0755
copy_payload_file "$TARGET_ROOT/usr/local/bin/un260_runtime_recover" "usr/local/bin/un260_runtime_recover" 0755
copy_payload_file "$UPDATER_PATH" "usr/bin/ui_update.sh" 0755
copy_payload_file "$STARTUP_PATH" "etc/init.d/S00lvgl" 0755
copy_payload_file "$APP_PATH" "usr/local/bin/test_lvgl" 0755
copy_payload_file "$TARGET_ROOT/usr/local/bin/un260_resource_cleanup" "usr/local/bin/un260_resource_cleanup" 0755
copy_payload_file "$APP_STORAGE_PATH" "usr/local/bin/un260_app_storage" 0755
UNPACKER_PATH="$TARGET_ROOT/usr/local/bin/un260_unpack"
if [[ -f "$UNPACKER_PATH" ]]; then
    copy_payload_file "$UNPACKER_PATH" "usr/local/bin/un260_unpack" 0755
fi
if [[ "$LEGACY_TEST" = 0 ]]; then
    copy_payload_file "$TARGET_ROOT/usr/local/lib/un260-startup/runtime.sh" "usr/local/lib/un260-startup/runtime.sh" 0755
    copy_payload_file "$TARGET_ROOT/etc/un260/update-core.manifest" "etc/un260/update-core.manifest" 0644
    copy_payload_file "$TARGET_ROOT/etc/un260/startup-runtime.sha256" "etc/un260/startup-runtime.sha256" 0644
fi

# The explicit old-base test uses the same compiled app/resources without
# changing the factory target or the device installer's platform protection.
BOOT_PACKAGE_ID=""
BOOT_MANIFEST_HASH=""
if [[ "$LEGACY_TEST" = 1 ]]; then
    python3 "$SCRIPT_DIR/release_profile.py" --payload "$PKG_ROOT/payload"
else
    # The factory descriptor is metadata, not an ordinary replaceable payload.
    python3 "$SCRIPT_DIR/boot_bundle.py" metadata --target "$TARGET_ROOT" \
        --copy-to "$PKG_ROOT/payload" > "$WORK_DIR/boot-metadata"
    BOOT_PACKAGE_ID=$(sed -n '1p' "$WORK_DIR/boot-metadata")
    BOOT_MANIFEST_HASH=$(sed -n '2p' "$WORK_DIR/boot-metadata")
fi

mkdir -p "$PKG_ROOT/payload/etc/un260"
printf '%s\n' "$VERSION" > "$PKG_ROOT/payload/etc/un260/package-version"
chmod 0644 "$PKG_ROOT/payload/etc/un260/package-version"

INSTALL_MANIFEST="$PKG_ROOT/install.tsv"
: > "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/un260_storage_sync\n' >> "$INSTALL_MANIFEST"
printf 'tree|0755|usr/local/share/lvgl_data\n' >> "$INSTALL_MANIFEST"

if [[ -n "$EXTRA_ROOT" ]]; then
    while IFS= read -r -d '' payload_file; do
        rel=${payload_file#"$PKG_ROOT/payload"/}
        is_fixed_target "$rel" && continue
        if [[ -x "$payload_file" ]]; then
            mode=0755
        else
            mode=0644
        fi
        printf 'file|%s|%s\n' "$mode" "$rel"
    done < <(find "$PKG_ROOT/payload" -type f -print0 | sort -z) >> "$INSTALL_MANIFEST"
fi

if [[ -n "$BOOT_PACKAGE_ID" ]]; then
    printf 'file|0755|usr/local/lib/un260-boot/versions/%s/player\n' "$BOOT_PACKAGE_ID" >> "$INSTALL_MANIFEST"
    printf 'file|0644|usr/local/lib/un260-boot/versions/%s/theme.pack\n' "$BOOT_PACKAGE_ID" >> "$INSTALL_MANIFEST"
    printf 'file|0644|usr/local/lib/un260-boot/versions/%s/manifest\n' "$BOOT_PACKAGE_ID" >> "$INSTALL_MANIFEST"
fi

printf 'file|0755|usr/local/lib/liblvgl.so\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/un260_upgrade_display\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/un260_runtime_recover\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/bin/ui_update.sh\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/un260_resource_cleanup\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/un260_app_storage\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|etc/init.d/S00lvgl\n' >> "$INSTALL_MANIFEST"
printf 'file|0644|etc/un260/package-version\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/test_lvgl\n' >> "$INSTALL_MANIFEST"
if [[ -f "$PKG_ROOT/payload/usr/local/bin/un260_unpack" ]]; then
    printf 'file|0755|usr/local/bin/un260_unpack\n' >> "$INSTALL_MANIFEST"
fi
if [[ "$LEGACY_TEST" = 0 ]]; then
    printf 'file|0755|usr/local/lib/un260-startup/runtime.sh\n' >> "$INSTALL_MANIFEST"
    printf 'file|0644|etc/un260/update-core.manifest\nfile|0644|etc/un260/startup-runtime.sha256\n' >> "$INSTALL_MANIFEST"
fi

(
    cd "$PKG_ROOT"
    while IFS= read -r -d '' payload_file; do
        sha256sum "$payload_file"
    done < <(find payload -type f -print0 | sort -z)
) > "$PKG_ROOT/checksums.sha256"

PACKAGE_ID=$(sha256sum "$PKG_ROOT/checksums.sha256" | awk '{print $1}')
payload_bytes=$(find "$PKG_ROOT/payload" -type f -printf '%s\n' | awk '{s += $1} END {printf "%.0f", s}')
[[ "$payload_bytes" -le "$PAYLOAD_BUDGET_BYTES" ]] ||
    die "Uncompressed payload exceeds $PAYLOAD_BUDGET_MIB MiB update budget ($payload_bytes bytes); review firmware layout and device free-space qualification"
if find "$PKG_ROOT/payload" -name boot-light.bin -print -quit | grep -q .; then
    die "Host-only boot-light.bin fixture in release payload; rebuild target installation"
fi
# Check before publishing: UPK compression does not reduce replacement staging.
# The explicit/pinned baseline and previous output cover supported transitions.
BASELINE=${UN260_UPDATE_BASELINE:-""}
if [[ -n "${UN260_UPDATE_BASELINE:-}" && ! -f "$BASELINE" ]]; then
    die "Specified baseline missing: $BASELINE"
fi
if [[ -n "$BASELINE" && "$(readlink -m "$BASELINE")" = "$(readlink -m "$OUTPUT_PATH")" ]]; then
    die "Baseline must be a separate read-only input, not the package output"
fi
for reference in "$BASELINE" "$OUTPUT_PATH"; do
    if [[ -f "$reference" ]]; then
        python3 "$SCRIPT_DIR/check_storage_budget.py" --base "$reference" \
            --payload "$PKG_ROOT/payload" --route "$STORAGE_ROUTE"
    fi
done
PACKAGE_STORAGE_GUARD=syncfs-v1
if [[ -n "$BOOT_PACKAGE_ID" ]]; then PACKAGE_STORAGE_GUARD=syncfs-boot-bundle-v1; fi
CONTRACT_ARGS=()
if [[ "$LEGACY_TEST" = 0 ]]; then CONTRACT_ARGS+=(--require-updater-contract); fi
TARGET_ID=$(python3 "$SCRIPT_DIR/release_manifest.py" --payload "$PKG_ROOT/payload" --table "$PKG_ROOT/target.tsv" "${CONTRACT_ARGS[@]}")
cat > "$PKG_ROOT/manifest.ini" <<EOF
format=UN260_UPGRADE
schema=1
product=UN260
package_type=ui
storage_layout=app-volume-v1
storage_guard=$PACKAGE_STORAGE_GUARD
qualification_route=$STORAGE_ROUTE
version=$VERSION
package_id=$PACKAGE_ID
target_id=$TARGET_ID
updater_envelope=UN260_UPDATE_ENVELOPE_V1
updater_platform=UN260_UPDATER_PLATFORM_V1
updater_engine_abi=1
requires_reboot=1
EOF
if [[ -n "$BOOT_PACKAGE_ID" ]]; then
    printf 'boot_bundle_id=%s\nboot_manifest_sha256=%s\n' "$BOOT_PACKAGE_ID" "$BOOT_MANIFEST_HASH" >> "$PKG_ROOT/manifest.ini"
fi
if [[ "$LEGACY_TEST" = 1 ]]; then
    printf 'release_profile=legacy-test\n' >> "$PKG_ROOT/manifest.ini"
fi

mkdir -p "$(dirname "$OUTPUT_PATH")"
OUTPUT_PATH=$(cd "$(dirname "$OUTPUT_PATH")" && pwd)/$(basename "$OUTPUT_PATH")
for candidate in "$OUTPUT_PATH" "$OUTPUT_PATH.tmp" "$OUTPUT_PATH.sha256" "$OUTPUT_PATH.sha256.tmp"; do
    [[ ! -L "$candidate" ]] || die "Refusing symlinked package output: $candidate"
done
chmod 0644 "$PKG_ROOT/manifest.ini" "$PKG_ROOT/checksums.sha256" "$PKG_ROOT/install.tsv" "$PKG_ROOT/target.tsv"
find "$PKG_ROOT" -type d -exec chmod 0755 {} +
tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -cf - -C "$PKG_ROOT" manifest.ini checksums.sha256 install.tsv target.tsv payload | gzip -n > "$OUTPUT_PATH.tmp"
chmod 0644 "$OUTPUT_PATH.tmp"
mv -f "$OUTPUT_PATH.tmp" "$OUTPUT_PATH"
(
    cd "$(dirname "$OUTPUT_PATH")"
    sha256sum "$(basename "$OUTPUT_PATH")"
) > "$OUTPUT_PATH.sha256.tmp"
chmod 0644 "$OUTPUT_PATH.sha256.tmp"
mv -f "$OUTPUT_PATH.sha256.tmp" "$OUTPUT_PATH.sha256"

echo "UN260 update package created"
echo "  version:    $VERSION"
echo "  package_id: $PACKAGE_ID"
echo "  target_id:  $TARGET_ID"
echo "  output:     $OUTPUT_PATH"
echo "  size:       $(du -h "$OUTPUT_PATH" | awk '{print $1}')"
echo "  entries:    $(wc -l < "$INSTALL_MANIFEST" | tr -d ' ')"
echo "  app bytes:  $app_bytes / $APP_BUDGET_BYTES"
echo "  raw bytes:  $payload_bytes / $PAYLOAD_BUDGET_BYTES"
echo
cat "$OUTPUT_PATH.sha256"
