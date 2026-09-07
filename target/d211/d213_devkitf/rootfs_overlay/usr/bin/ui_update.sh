#!/bin/sh

USB_MNT=/mnt/usb
UPDATE_DIR=$USB_MNT/update
BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk
LEGACY_APP_PATH=$UPDATE_DIR/test_lvgl
LEGACY_LIB_PATH=$UPDATE_DIR/liblvgl.so
LEGACY_DATA_PATH=$UPDATE_DIR/lvgl_data

STATUS_FILE=/tmp/ui_update.status
STATUS_TEMP=/tmp/ui_update.status.tmp.$$
LOG=/tmp/ui_update.log
RESULT_FILE=/tmp/UN260_UPDATE_RESULT.txt

STAGE_DIR=$UPDATE_DIR/.un260_stage
BACKUP_DIR=$UPDATE_DIR/.un260_backup
BACKUP_STATE=$BACKUP_DIR/state.tsv
ARCHIVE_LIST=$UPDATE_DIR/.un260_archive.list
ARCHIVE_TYPES=$UPDATE_DIR/.un260_archive.types
SEEN_TARGETS=$UPDATE_DIR/.un260_seen_targets
CHECKSUM_TARGETS=$UPDATE_DIR/.un260_checksum_targets
PAYLOAD_FILES=$UPDATE_DIR/.un260_payload_files

INSTALLED_STATE_DIR=/var/lib/un260-updater
INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64

USB_DEV=""
BUNDLE_FNV=""
TRANSACTION_STARTED=0
ROLLBACK_RUNNING=0
LAST_PROGRESS=0
LAST_STAGE=prepare
ROOT_PREFIX=""
PLAN_FILE=$UPDATE_DIR/.un260_plan
JOURNAL=$INSTALLED_STATE_DIR/transaction
LOCK_DIR=/tmp/un260-updater.lock
WORK_OWNED=0

detect_usb_dev()
{
    for dev in /dev/sd[a-z][0-9]* /dev/sd[a-z]; do
        [ -b "$dev" ] || continue
        echo "$dev"
        return 0
    done
    return 1
}

write_status()
{
    progress="$1"
    stage="$2"
    step="$3"
    success="$4"
    message="$5"

    {
        echo "progress=$progress"
        echo "stage=$stage"
        echo "step=$step"
        [ -n "$success" ] && echo "success=$success"
        [ -n "$message" ] && echo "message=$message"
    } > "$STATUS_TEMP"
    mv -f "$STATUS_TEMP" "$STATUS_FILE"
    LAST_PROGRESS="$progress"
    LAST_STAGE="$stage"
}

write_result()
{
    result="$1"
    message="$2"
    version="$3"

    {
        echo "result=$result"
        echo "version=$version"
        echo "message=$message"
        echo "time=$(date)"
    } > "$RESULT_FILE"
    sync
}

is_safe_relative_path()
{
    rel="$1"

    case "$rel" in
        ""|/*|*..*|*//*|*\\*|*[!A-Za-z0-9_./-]*) return 1 ;;
    esac
    return 0
}

is_allowed_target()
{
    rel="$1"

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

manifest_value()
{
    key="$1"
    awk -F= -v wanted="$key" '
        $1 == wanted {
            sub(/^[^=]*=/, "")
            print
            exit
        }
    ' "$STAGE_DIR/manifest.ini"
}

safe_destination()
{
    is_safe_relative_path "$1" && is_allowed_target "$1" || return 1
    case "$1" in *".un260-"*) return 1 ;; esac
    check_path=$ROOT_PREFIX/$1
    [ ! -L "$check_path" ] || return 1
    [ ! -e "$check_path" ] || [ -f "$check_path" ] || return 1
    check_path=$(dirname "$check_path")
    while [ "$check_path" != "${ROOT_PREFIX:-/}" ]; do
        [ ! -L "$check_path" ] || return 1
        [ ! -e "$check_path" ] || [ -d "$check_path" ] || return 1
        [ "$check_path" != / ] || return 1
        check_path=$(dirname "$check_path")
    done
}

cleanup_usb_work()
{
    [ "$WORK_OWNED" -eq 1 ] || return 0
    # Only this invocation's staging files, never the user's package or logs.
    rm -rf "$STAGE_DIR" "$BACKUP_DIR"
    rm -f "$ARCHIVE_LIST" "$ARCHIVE_TYPES" "$SEEN_TARGETS" \
        "$CHECKSUM_TARGETS" "$PAYLOAD_FILES" "$PLAN_FILE" "$PLAN_FILE.tree"
}

# Recover original inode links without copying a large executable into a full
# rootfs. The local journal permits recovery without the USB drive. This is a
# recoverable file transaction, not a partition-level A/B firmware design.
recover_transaction()
{
    [ -d "$JOURNAL" ] || return 0
    [ ! -L "$JOURNAL" ] && [ -f "$JOURNAL/state" ] || return 1
    if [ ! -f "$JOURNAL/committed" ] && [ ! -f "$JOURNAL/rolled-back" ]; then
        echo "Rollback started (local original inodes)" >> "$LOG"
        while IFS='|' read -r present rpath extra; do
            [ -z "$extra" ] || return 1
            case "$present" in 0|1) ;; *) return 1 ;; esac
            safe_destination "$rpath" || return 1
            for suffix in old new restore; do
                [ ! -L "$ROOT_PREFIX/$rpath.un260-$suffix" ] || return 1
            done
        done < "$JOURNAL/state"
        while IFS='|' read -r present rpath; do
            rdest=$ROOT_PREFIX/$rpath
            if [ "$present" = 1 ]; then
                if [ -f "$rdest.un260-old" ]; then
                    rm -f "$rdest.un260-restore" || return 1
                    ln "$rdest.un260-old" "$rdest.un260-restore" || return 1
                    mv -f "$rdest.un260-restore" "$rdest" || return 1
                fi
                # No old link means interruption before ln; original untouched.
                [ -f "$rdest" ] || return 1
            else
                rm -f "$rdest" || return 1
            fi
        done < "$JOURNAL/state"
        sync
        : > "$JOURNAL/rolled-back" || return 1
        sync
        echo "Rollback finished" >> "$LOG"
    fi
    while IFS='|' read -r present rpath extra; do
        [ -z "$extra" ] && safe_destination "$rpath" || return 1
        rm -f "$ROOT_PREFIX/$rpath.un260-old" "$ROOT_PREFIX/$rpath.un260-new" \
            "$ROOT_PREFIX/$rpath.un260-restore" || return 1
    done < "$JOURNAL/state"
    sync
    rm -f "$JOURNAL/state" "$JOURNAL/committed" "$JOURNAL/rolled-back"
    rmdir "$JOURNAL" || return 1
    sync
}

rollback_bundle()
{
    [ "$TRANSACTION_STARTED" -eq 1 ] || return 0
    recover_transaction
}

fail_update()
{
    msg="$1"
    trap - HUP INT TERM
    echo "ERR: $msg" >> "$LOG"
    if rollback_bundle; then
        cleanup_usb_work
    else
        msg="$msg; recovery incomplete, keep USB backup and do not start UI"
        echo "ERR: Recovery incomplete; journal=$JOURNAL backup=$BACKUP_DIR" >> "$LOG"
    fi
    write_status "$LAST_PROGRESS" fail "fail" 0 "$msg"
    write_result fail "$msg" ""
    exit 1
}

interrupt_update()
{
    fail_update "Upgrade interrupted"
}

validate_archive_paths()
{
    if ! tar -tzf "$BUNDLE_PATH" > "$ARCHIVE_LIST" 2>> "$LOG"; then
        fail_update "Upgrade archive is damaged or unsupported"
    fi

    while IFS= read -r entry; do
        case "$entry" in
            manifest.ini|checksums.sha256|install.tsv|payload|payload/*) ;;
            *) fail_update "Upgrade archive contains an invalid path" ;;
        esac
        case "$entry" in
            /*|*..*|*\\*) fail_update "Upgrade archive contains an unsafe path" ;;
        esac
    done < "$ARCHIVE_LIST"

    if ! tar -tvzf "$BUNDLE_PATH" > "$ARCHIVE_TYPES" 2>> "$LOG"; then
        fail_update "Upgrade archive entry types cannot be inspected"
    fi
    while IFS= read -r detail; do
        case "$detail" in
            -*|d*) ;;
            *) fail_update "Upgrade archive contains unsupported entry types" ;;
        esac
    done < "$ARCHIVE_TYPES"
}

validate_payload_checksums()
{
    checksum_count=0
    : > "$CHECKSUM_TARGETS"

    while read -r checksum checksum_path extra; do
        [ -n "$checksum" ] || continue
        [ -z "$extra" ] || fail_update "Invalid checksum manifest"
        echo "$checksum" | grep -Eq '^[0-9a-f]{64}$' ||
            fail_update "Checksum manifest contains an invalid checksum"
        is_safe_relative_path "$checksum_path" ||
            fail_update "Checksum manifest contains an unsafe path"
        case "$checksum_path" in
            payload/*) ;;
            *) fail_update "Checksum manifest references an invalid file" ;;
        esac
        [ -f "$STAGE_DIR/$checksum_path" ] ||
            fail_update "Checksum manifest references a missing file"
        if grep -Fx "$checksum_path" "$CHECKSUM_TARGETS" >/dev/null 2>&1; then
            fail_update "Checksum manifest contains duplicate files"
        fi
        echo "$checksum_path" >> "$CHECKSUM_TARGETS"
        checksum_count=$((checksum_count + 1))
    done < "$STAGE_DIR/checksums.sha256"

    [ "$checksum_count" -gt 0 ] || fail_update "Checksum manifest is empty"

    (cd "$STAGE_DIR" && find payload -type f | sort) > "$PAYLOAD_FILES" ||
        fail_update "Unable to enumerate upgrade payload"
    while IFS= read -r payload_path; do
        [ -n "$payload_path" ] || continue
        is_safe_relative_path "$payload_path" ||
            fail_update "Upgrade payload contains an unsafe path"
        grep -Fx "$payload_path" "$CHECKSUM_TARGETS" >/dev/null 2>&1 ||
            fail_update "Upgrade payload contains an unchecked file"
    done < "$PAYLOAD_FILES"

    payload_count=$(wc -l < "$PAYLOAD_FILES" | tr -d ' ')
    [ "$payload_count" = "$checksum_count" ] ||
        fail_update "Upgrade checksum manifest does not match payload"

    (cd "$STAGE_DIR" && sha256sum -c checksums.sha256) >> "$LOG" 2>&1 ||
        fail_update "Upgrade payload checksum verification failed"
}

plan_file()
{
    plan_rel=$1
    plan_mode=$2
    safe_destination "$plan_rel" || fail_update "Unsafe install destination: $plan_rel"
    if grep -Fx "$plan_rel" "$SEEN_TARGETS" >/dev/null 2>&1; then
        fail_update "Duplicate or overlapping install target: $plan_rel"
    fi
    echo "$plan_rel" >> "$SEEN_TARGETS"
    plan_src=$STAGE_DIR/payload/$plan_rel
    plan_dest=$ROOT_PREFIX/$plan_rel
    for suffix in old new restore; do
        [ ! -e "$plan_dest.un260-$suffix" ] && [ ! -L "$plan_dest.un260-$suffix" ] ||
            fail_update "Unrecovered install artifact: $plan_rel"
    done
    plan_hash=$(sha256sum "$plan_src" | awk '{print $1}')
    case "$plan_hash" in ''|*[!0-9a-f]*) fail_update "Cannot hash payload" ;; esac
    plan_size=$(wc -c < "$plan_src" | tr -d ' ')
    case "$plan_size" in ''|*[!0-9]*) fail_update "Cannot measure payload" ;; esac
    if [ -f "$plan_dest" ]; then
        dest_hash=$(sha256sum "$plan_dest" | awk '{print $1}')
        # This board's minimal BusyBox does not ship stat/cmp applets.
        dest_mode=$(LC_ALL=C ls -ld "$plan_dest" | awk '{print $1}')
        case "$plan_mode" in 0755) expected_mode=-rwxr-xr-x ;; *) expected_mode=-rw-r--r-- ;; esac
        if [ "$plan_hash" = "$dest_hash" ] && [ "$expected_mode" = "$dest_mode" ]; then
            SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
            return 0
        fi
        old_bytes=$(wc -c < "$plan_dest" | tr -d ' ')
        BACKUP_REQUIRED_KB=$((BACKUP_REQUIRED_KB + (old_bytes + 1023) / 1024 + 4))
    fi
    # All originals remain linked until commit, even when UI maps old binaries.
    # No optimistic assumption about UBIFS compression in the space check.
    ROOT_PEAK_REQUIRED_KB=$((ROOT_PEAK_REQUIRED_KB + (plan_size + 1023) / 1024 + 4))
    printf 'file|%s|%s|%s\n' "$plan_mode" "$plan_rel" "$plan_hash" >> "$PLAN_FILE"
    INSTALL_ENTRY_COUNT=$((INSTALL_ENTRY_COUNT + 1))
    [ "$INSTALL_ENTRY_COUNT" -le 8192 ] || fail_update "Too many changed files"
}

validate_install_manifest()
{
    INSTALL_ENTRY_COUNT=0
    SKIPPED_COUNT=0
    BACKUP_REQUIRED_KB=0
    ROOT_PEAK_REQUIRED_KB=0
    manifest_count=0
    : > "$SEEN_TARGETS"
    : > "$PLAN_FILE"
    while IFS='|' read -r kind mode rel extra; do
        [ -n "$kind" ] || continue
        [ -z "$extra" ] || fail_update "Invalid install manifest column count"
        case "$mode" in 0644|0755) ;; *) fail_update "Unsupported install permissions" ;; esac
        is_safe_relative_path "$rel" && is_allowed_target "$rel" ||
            fail_update "Unsafe install manifest target"
        case "$kind" in
            file)
                [ -f "$STAGE_DIR/payload/$rel" ] || fail_update "Missing payload file"
                plan_file "$rel" "$mode"
                ;;
            tree)
                [ -d "$STAGE_DIR/payload/$rel" ] || fail_update "Missing payload tree"
                find "$STAGE_DIR/payload/$rel" -type f | sort > "$PLAN_FILE.tree" ||
                    fail_update "Cannot enumerate payload tree"
                while IFS= read -r tree_file; do
                    tree_rel=${tree_file#"$STAGE_DIR/payload/"}
                    plan_file "$tree_rel" 0644
                done < "$PLAN_FILE.tree"
                ;;
            *) fail_update "Invalid install manifest entry type" ;;
        esac
        manifest_count=$((manifest_count + 1))
        [ "$manifest_count" -le 256 ] || fail_update "Too many manifest entries"
    done < "$STAGE_DIR/install.tsv"
    [ "$manifest_count" -gt 0 ] || fail_update "Empty install manifest"
    echo "Install plan: changed=$INSTALL_ENTRY_COUNT unchanged=$SKIPPED_COUNT root_staged=${ROOT_PEAK_REQUIRED_KB}KB" >> "$LOG"
}

validate_storage_space()
{
    root_free_kb=$(df -Pk "${ROOT_PREFIX:-/}" 2>/dev/null | awk 'NR == 2 {print $4}')
    usb_free_kb=$(df -Pk "$USB_MNT" 2>/dev/null | awk 'NR == 2 {print $4}')

    [ -n "$root_free_kb" ] && [ -n "$usb_free_kb" ] &&
        [ -n "$BACKUP_REQUIRED_KB" ] && [ -n "$ROOT_PEAK_REQUIRED_KB" ] ||
        fail_update "Unable to determine upgrade storage capacity"
    case "$root_free_kb:$usb_free_kb:$BACKUP_REQUIRED_KB:$ROOT_PEAK_REQUIRED_KB" in
        *[!0-9:]*) fail_update "Unable to determine upgrade storage capacity" ;;
    esac

    root_required_kb=$((ROOT_PEAK_REQUIRED_KB + 2048))
    usb_required_kb=$((BACKUP_REQUIRED_KB + 4096))
    echo "Storage preflight: root_free=${root_free_kb}KB root_peak=${ROOT_PEAK_REQUIRED_KB}KB root_required=${root_required_kb}KB usb_free=${usb_free_kb}KB backup_required=${BACKUP_REQUIRED_KB}KB" >> "$LOG"
    [ "$root_free_kb" -ge "$root_required_kb" ] ||
        fail_update "Insufficient root space: need ${root_required_kb}KB, free ${root_free_kb}KB; use matching full firmware"
    [ "$usb_free_kb" -ge "$usb_required_kb" ] ||
        fail_update "Insufficient USB space for upgrade rollback backup"
}

install_file_entry()
{
    install_mode=$1
    install_rel=$2
    install_hash=$3
    install_src=$STAGE_DIR/payload/$install_rel
    install_dest=$ROOT_PREFIX/$install_rel
    safe_destination "$install_rel" || fail_update "Destination changed during upgrade"
    mkdir -p "$(dirname "$install_dest")" "$BACKUP_DIR/rootfs/$(dirname "$install_rel")" ||
        fail_update "Cannot prepare install directories"
    present=0
    if [ -f "$install_dest" ]; then
        cp "$install_dest" "$BACKUP_DIR/rootfs/$install_rel" >> "$LOG" 2>&1 ||
            fail_update "Cannot back up installed file"
        original_hash=$(sha256sum "$install_dest" | awk '{print $1}')
        backup_hash=$(sha256sum "$BACKUP_DIR/rootfs/$install_rel" | awk '{print $1}')
        [ -n "$original_hash" ] && [ "$original_hash" = "$backup_hash" ] ||
            fail_update "USB backup verification failed"
        present=1
    fi
    printf '%s|%s\n' "$present" "$install_rel" >> "$JOURNAL/state" ||
        fail_update "Cannot persist recovery journal"
    sync
    if [ "$present" = 1 ]; then
        ln "$install_dest" "$install_dest.un260-old" >> "$LOG" 2>&1 ||
            fail_update "Cannot preserve original inode"
        sync
    fi
    cp "$install_src" "$install_dest.un260-new" >> "$LOG" 2>&1 ||
        fail_update "Cannot stage replacement file"
    chmod "$install_mode" "$install_dest.un260-new" ||
        fail_update "Cannot set replacement permissions"
    copied_hash=$(sha256sum "$install_dest.un260-new" | awk '{print $1}')
    [ "$copied_hash" = "$install_hash" ] || fail_update "Replacement verification failed"
    sync
    mv -f "$install_dest.un260-new" "$install_dest" >> "$LOG" 2>&1 ||
        fail_update "Cannot replace installed file"
    sync
}

execute_plan()
{
    validate_install_manifest
    validate_storage_space
    mkdir -p "$INSTALLED_STATE_DIR" || fail_update "Cannot create updater state directory"
    mkdir -m 0700 "$JOURNAL" || fail_update "Pending recovery must finish first"
    : > "$JOURNAL/state" || fail_update "Cannot create recovery journal"
    sync
    TRANSACTION_STARTED=1
    trap interrupt_update HUP INT TERM
    install_index=0
    while IFS='|' read -r kind mode rel expected_hash; do
        install_file_entry "$mode" "$rel" "$expected_hash"
        install_index=$((install_index + 1))
        progress=$((40 + (install_index * 50 / INSTALL_ENTRY_COUNT)))
        write_status "$progress" install "install" "" ""
    done < "$PLAN_FILE"
    write_status 92 sync "sync" "" ""
    sync
    : > "$JOURNAL/committed" || fail_update "Cannot persist transaction commit"
    sync
    recover_transaction || fail_update "Committed; cleanup requires recovery"
    TRANSACTION_STARTED=0
    trap - HUP INT TERM
    record_installed_bundle
    sync
    write_status 96 finish "finish" "" ""
    cleanup_usb_work
    write_status 100 success "success" 1 "The system has been updated successfully. Restarting the device is recommended."
    write_result success "Upgrade completed successfully; reboot is required" "$package_version"
    echo "Update OK: version=$package_version changed=$INSTALL_ENTRY_COUNT unchanged=$SKIPPED_COUNT" >> "$LOG"
}

prepare_usb_work()
{
    # Older installer backups with actual entries need review, never discard.
    if [ -s "$BACKUP_STATE" ] && [ ! -f "$BACKUP_DIR/managed-v2" ]; then
        fail_update "Previous rollback backup exists; preserve and inspect it first"
    fi
    WORK_OWNED=1
    cleanup_usb_work
    mkdir -p "$STAGE_DIR" "$BACKUP_DIR/rootfs" || fail_update "Cannot create USB staging"
    : > "$BACKUP_DIR/managed-v2"
}

record_installed_bundle()
{
    [ -n "$BUNDLE_FNV" ] || return 0
    echo "$BUNDLE_FNV" | grep -Eq '^[0-9a-fA-F]{16}$' || return 0

    mkdir -p "$INSTALLED_STATE_DIR"
    echo "$BUNDLE_FNV" > "$INSTALLED_HASH_PATH.tmp.$$"
    mv -f "$INSTALLED_HASH_PATH.tmp.$$" "$INSTALLED_HASH_PATH"
}

install_bundle()
{
    write_status 5 verify "verify_archive" "" ""
    prepare_usb_work
    validate_archive_paths

    write_status 10 extract "extract" "" ""
    tar -xzf "$BUNDLE_PATH" -C "$STAGE_DIR" >> "$LOG" 2>&1 ||
        fail_update "Failed to extract upgrade archive"

    write_status 18 verify "verify_manifest" "" ""
    [ -f "$STAGE_DIR/manifest.ini" ] || fail_update "Package manifest is missing"
    [ -f "$STAGE_DIR/checksums.sha256" ] || fail_update "Package checksums are missing"
    [ -f "$STAGE_DIR/install.tsv" ] || fail_update "Install manifest is missing"
    [ -d "$STAGE_DIR/payload" ] || fail_update "Package payload is missing"

    if find "$STAGE_DIR/payload" -type l | grep -q .; then
        fail_update "Symbolic links are not allowed in upgrade payloads"
    fi

    package_format=$(manifest_value format)
    package_schema=$(manifest_value schema)
    package_product=$(manifest_value product)
    package_version=$(manifest_value version)
    package_id=$(manifest_value package_id)

    [ "$package_format" = "UN260_UPGRADE" ] || fail_update "Unsupported package format"
    [ "$package_schema" = "1" ] || fail_update "Unsupported package schema"
    [ "$package_product" = "UN260" ] || fail_update "Package is for another product"
    [ -n "$package_version" ] || fail_update "Package version is missing"
    echo "$package_version" | grep -Eq '^[A-Za-z0-9._-]{1,48}$' ||
        fail_update "Package version is invalid"
    echo "$package_id" | grep -Eq '^[0-9a-f]{64}$' ||
        fail_update "Package identifier is invalid"

    actual_package_id=$(sha256sum "$STAGE_DIR/checksums.sha256" | awk '{print $1}')
    [ "$actual_package_id" = "$package_id" ] ||
        fail_update "Package identifier does not match its payload"

    write_status 24 verify "verify_checksum" "" ""
    validate_payload_checksums

    write_status 34 preflight "preflight" "" ""
    execute_plan
}

install_legacy_package()
{
    [ -f "$LEGACY_APP_PATH" ] || fail_update "Upgrade binary not found on USB drive"
    prepare_usb_work
    mkdir -p "$STAGE_DIR/payload/usr/local/bin" "$STAGE_DIR/payload/usr/local/lib" \
        "$STAGE_DIR/payload/usr/local/share"
    cp "$LEGACY_APP_PATH" "$STAGE_DIR/payload/usr/local/bin/test_lvgl" ||
        fail_update "Cannot stage legacy app"
    printf 'file|0755|usr/local/bin/test_lvgl\n' > "$STAGE_DIR/install.tsv"
    if [ -f "$LEGACY_LIB_PATH" ]; then
        cp "$LEGACY_LIB_PATH" "$STAGE_DIR/payload/usr/local/lib/liblvgl.so" ||
            fail_update "Cannot stage legacy library"
        printf 'file|0755|usr/local/lib/liblvgl.so\n' >> "$STAGE_DIR/install.tsv"
    fi
    if [ -d "$LEGACY_DATA_PATH" ]; then
        cp -R "$LEGACY_DATA_PATH" "$STAGE_DIR/payload/usr/local/share/lvgl_data" ||
            fail_update "Cannot stage legacy resources"
        printf 'tree|0755|usr/local/share/lvgl_data\n' >> "$STAGE_DIR/install.tsv"
    fi
    if find "$STAGE_DIR/payload" ! -type f ! -type d | grep -q .; then
        fail_update "Unsupported legacy payload types"
    fi
    package_version=legacy
    execute_plan
}

# Host tests load definitions, then redirect paths into a temporary fixture.
if [ "${UN260_UPDATER_LIBRARY_ONLY:-0}" = 1 ]; then return 0; fi
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    echo "Updater already running or stale lock: $LOCK_DIR" >&2
    exit 1
fi
trap 'rmdir "$LOCK_DIR" 2>/dev/null' EXIT
if ! recover_transaction; then
    echo "Recovery failed; keep $JOURNAL and USB backup" >&2
    exit 1
fi
if [ "${1:-}" = "--recover" ]; then exit 0; fi

if [ "${1:-}" = "--bundle-fnv" ]; then
    BUNDLE_FNV="${2:-}"
fi

mkdir -p "$USB_MNT"
rm -f "$STATUS_FILE" "$STATUS_TEMP"

if grep -q " $USB_MNT " /proc/mounts 2>/dev/null; then
    USB_DEV="$(awk -v m="$USB_MNT" '$2 == m {print $1; exit}' /proc/mounts)"
fi
if [ -z "$USB_DEV" ]; then
    USB_DEV="$(detect_usb_dev)"
fi
if [ -z "$USB_DEV" ] || [ ! -b "$USB_DEV" ]; then
    fail_update "USB block device not found"
fi
if ! grep -q " $USB_MNT " /proc/mounts 2>/dev/null; then
    mount "$USB_DEV" "$USB_MNT" >> "$LOG" 2>&1 ||
        fail_update "Failed to mount USB device"
fi

LOG=$USB_MNT/ui_update.log
RESULT_FILE=$USB_MNT/UN260_UPDATE_RESULT.txt
: > "$LOG"
rm -f "$RESULT_FILE"
echo "ui_update start: $(date)" >> "$LOG"
write_status 2 prepare "prepare" "" ""

if [ -f "$BUNDLE_PATH" ]; then
    install_bundle
elif [ -f "$LEGACY_APP_PATH" ]; then
    install_legacy_package
else
    fail_update "No supported UN260 upgrade package was found"
fi

exit 0
