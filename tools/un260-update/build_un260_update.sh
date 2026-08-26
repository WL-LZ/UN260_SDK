#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SDK_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
TARGET_ROOT=${TARGET_DIR:-"$SDK_ROOT/output/d211_d213_devkitf/target"}

APP_PATH="$TARGET_ROOT/usr/local/bin/test_lvgl"
LVGL_LIB_PATH="$TARGET_ROOT/usr/local/lib/liblvgl.so"
LVGL_DATA_PATH="$TARGET_ROOT/usr/local/share/lvgl_data"
UPDATER_PATH="$SDK_ROOT/target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh"
STARTUP_PATH="$SDK_ROOT/package/artinchip/test-lvgl/S20test_lvgl"

VERSION=""
OUTPUT_PATH=""
EXTRA_ROOT=""

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
        usr/bin/ui_update.sh|etc/init.d/S00lvgl|etc/un260/package-version)
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
        *)
            die "Unknown option: $1"
            ;;
    esac
done

[[ "$VERSION" =~ ^[A-Za-z0-9._-]{1,48}$ ]] ||
    die "A valid --version is required"
[[ -n "$OUTPUT_PATH" ]] || die "--output is required"
[[ -f "$APP_PATH" ]] || die "test_lvgl not found: $APP_PATH"
[[ -f "$LVGL_LIB_PATH" ]] || die "liblvgl.so not found: $LVGL_LIB_PATH"
[[ -d "$LVGL_DATA_PATH" ]] || die "lvgl_data not found: $LVGL_DATA_PATH"
[[ -f "$UPDATER_PATH" ]] || die "ui_update.sh not found: $UPDATER_PATH"
[[ -f "$STARTUP_PATH" ]] || die "S00lvgl source not found: $STARTUP_PATH"

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
copy_payload_file "$UPDATER_PATH" "usr/bin/ui_update.sh" 0755
copy_payload_file "$STARTUP_PATH" "etc/init.d/S00lvgl" 0755
copy_payload_file "$APP_PATH" "usr/local/bin/test_lvgl" 0755

mkdir -p "$PKG_ROOT/payload/etc/un260"
printf '%s\n' "$VERSION" > "$PKG_ROOT/payload/etc/un260/package-version"
chmod 0644 "$PKG_ROOT/payload/etc/un260/package-version"

INSTALL_MANIFEST="$PKG_ROOT/install.tsv"
: > "$INSTALL_MANIFEST"
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

printf 'file|0755|usr/local/lib/liblvgl.so\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/bin/ui_update.sh\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|etc/init.d/S00lvgl\n' >> "$INSTALL_MANIFEST"
printf 'file|0644|etc/un260/package-version\n' >> "$INSTALL_MANIFEST"
printf 'file|0755|usr/local/bin/test_lvgl\n' >> "$INSTALL_MANIFEST"

(
    cd "$PKG_ROOT"
    while IFS= read -r -d '' payload_file; do
        sha256sum "$payload_file"
    done < <(find payload -type f -print0 | sort -z)
) > "$PKG_ROOT/checksums.sha256"

PACKAGE_ID=$(sha256sum "$PKG_ROOT/checksums.sha256" | awk '{print $1}')
CREATED_UTC=$(date -u +%Y-%m-%dT%H:%M:%SZ)
cat > "$PKG_ROOT/manifest.ini" <<EOF
format=UN260_UPGRADE
schema=1
product=UN260
package_type=ui
version=$VERSION
package_id=$PACKAGE_ID
requires_reboot=1
created_utc=$CREATED_UTC
EOF

mkdir -p "$(dirname "$OUTPUT_PATH")"
OUTPUT_PATH=$(cd "$(dirname "$OUTPUT_PATH")" && pwd)/$(basename "$OUTPUT_PATH")
rm -f "$OUTPUT_PATH" "$OUTPUT_PATH.sha256"

tar -czf "$OUTPUT_PATH" -C "$PKG_ROOT" \
    manifest.ini checksums.sha256 install.tsv payload
(
    cd "$(dirname "$OUTPUT_PATH")"
    sha256sum "$(basename "$OUTPUT_PATH")"
) > "$OUTPUT_PATH.sha256"

echo "UN260 update package created"
echo "  version:    $VERSION"
echo "  package_id: $PACKAGE_ID"
echo "  output:     $OUTPUT_PATH"
echo "  size:       $(du -h "$OUTPUT_PATH" | awk '{print $1}')"
echo "  entries:    $(wc -l < "$INSTALL_MANIFEST" | tr -d ' ')"
echo
cat "$OUTPUT_PATH.sha256"
