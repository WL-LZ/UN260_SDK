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
    sync
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

apply_tree_permissions()
{
    perm_root="$1"
    perm_dir_list=/tmp/ui_update_perm_dirs.$$
    perm_file_list=/tmp/ui_update_perm_files.$$
    perm_failed=0

    rm -f "$perm_dir_list" "$perm_file_list"
    find "$perm_root" -type d -print > "$perm_dir_list" 2>> "$LOG" || perm_failed=1
    find "$perm_root" -type f -print > "$perm_file_list" 2>> "$LOG" || perm_failed=1

    if [ "$perm_failed" -eq 0 ]; then
        while IFS= read -r perm_path; do
            chmod 0755 "$perm_path" >> "$LOG" 2>&1 || {
                perm_failed=1
                break
            }
        done < "$perm_dir_list"
    fi

    if [ "$perm_failed" -eq 0 ]; then
        while IFS= read -r perm_path; do
            chmod 0644 "$perm_path" >> "$LOG" 2>&1 || {
                perm_failed=1
                break
            }
        done < "$perm_file_list"
    fi

    rm -f "$perm_dir_list" "$perm_file_list"
    [ "$perm_failed" -eq 0 ]
}

cleanup_install_artifacts()
{
    [ -f "$STAGE_DIR/install.tsv" ] || return 0

    while IFS='|' read -r cleanup_kind cleanup_mode cleanup_rel cleanup_extra; do
        [ -n "$cleanup_rel" ] || continue
        is_safe_relative_path "$cleanup_rel" || continue
        is_allowed_target "$cleanup_rel" || continue
        cleanup_dest=/$cleanup_rel
        rm -rf "${cleanup_dest}.un260-new.$$" "${cleanup_dest}.un260-old.$$"
    done < "$STAGE_DIR/install.tsv"
}

rollback_bundle()
{
    [ "$TRANSACTION_STARTED" -eq 1 ] || return 0
    [ "$ROLLBACK_RUNNING" -eq 0 ] || return 0
    [ -f "$BACKUP_STATE" ] || return 0

    ROLLBACK_RUNNING=1
    echo "Rollback started" >> "$LOG"

    while IFS='|' read -r present kind mode rel; do
        [ -n "$rel" ] || continue
        is_safe_relative_path "$rel" || continue
        is_allowed_target "$rel" || continue

        dest=/$rel
        backup=$BACKUP_DIR/rootfs/$rel
        rm -rf "$dest"

        if [ "$present" = "1" ] && [ -e "$backup" ]; then
            mkdir -p "$(dirname "$dest")"
            cp -R "$backup" "$dest" >> "$LOG" 2>&1 || true
            if [ "$kind" = "tree" ]; then
                apply_tree_permissions "$dest" || true
            else
                chmod "$mode" "$dest" 2>/dev/null || true
            fi
        fi
    done < "$BACKUP_STATE"

    sync
    echo "Rollback finished" >> "$LOG"
}

fail_update()
{
    msg="$1"
    trap - HUP INT TERM
    echo "ERR: $msg" >> "$LOG"
    cleanup_install_artifacts
    rollback_bundle
    cleanup_install_artifacts
    write_status 12 fail "Upgrade package verification failed" 0 "$msg"
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

validate_install_manifest()
{
    entry_count=0
    backup_required_kb=0
    root_consumed_kb=0
    root_peak_kb=0
    : > "$SEEN_TARGETS"

    while IFS='|' read -r kind mode rel extra; do
        [ -n "$kind" ] || continue
        [ -z "$extra" ] || fail_update "Invalid install manifest column count"
        case "$kind" in
            file|tree) ;;
            *) fail_update "Invalid install manifest entry type" ;;
        esac
        case "$mode" in
            [0-7][0-7][0-7][0-7]) ;;
            *) fail_update "Invalid install manifest file mode" ;;
        esac
        is_safe_relative_path "$rel" ||
            fail_update "Install manifest contains an unsafe path"
        is_allowed_target "$rel" ||
            fail_update "Install manifest target is not allowed"

        if grep -Fx "$rel" "$SEEN_TARGETS" >/dev/null 2>&1; then
            fail_update "Install manifest contains duplicate targets"
        fi
        echo "$rel" >> "$SEEN_TARGETS"

        if [ "$kind" = "file" ]; then
            [ -f "$STAGE_DIR/payload/$rel" ] ||
                fail_update "Install manifest payload file is missing"
        else
            [ -d "$STAGE_DIR/payload/$rel" ] ||
                fail_update "Install manifest payload directory is missing"
        fi

        source_kb=$(du -sk "$STAGE_DIR/payload/$rel" 2>/dev/null | awk '{print $1}')
        case "$source_kb" in
            ''|*[!0-9]*) fail_update "Unable to measure upgrade payload size" ;;
        esac

        target_kb=0
        if [ -e "/$rel" ]; then
            target_kb=$(du -sk "/$rel" 2>/dev/null | awk '{print $1}')
            case "$target_kb" in
                ''|*[!0-9]*) fail_update "Unable to measure installed target size" ;;
            esac
            backup_required_kb=$((backup_required_kb + target_kb))
        fi

        entry_peak_kb=$((root_consumed_kb + source_kb))
        [ "$entry_peak_kb" -le "$root_peak_kb" ] || root_peak_kb=$entry_peak_kb

        case "$rel" in
            usr/local/bin/test_lvgl|usr/local/lib/liblvgl.so)
                # The running UI keeps the replaced executable and shared library
                # inodes alive until reboot, so their new sizes remain consumed.
                root_consumed_kb=$((root_consumed_kb + source_kb))
                ;;
            *)
                if [ "$source_kb" -gt "$target_kb" ]; then
                    root_consumed_kb=$((root_consumed_kb + source_kb - target_kb))
                fi
                ;;
        esac

        entry_count=$((entry_count + 1))
        [ "$entry_count" -le 256 ] ||
            fail_update "Install manifest contains too many entries"
    done < "$STAGE_DIR/install.tsv"

    [ "$entry_count" -gt 0 ] || fail_update "Install manifest is empty"
    INSTALL_ENTRY_COUNT=$entry_count
    BACKUP_REQUIRED_KB=$backup_required_kb
    ROOT_PEAK_REQUIRED_KB=$root_peak_kb
}

validate_storage_space()
{
    root_free_kb=$(df -Pk / 2>/dev/null | awk 'NR == 2 {print $4}')
    usb_free_kb=$(df -Pk "$USB_MNT" 2>/dev/null | awk 'NR == 2 {print $4}')

    [ -n "$root_free_kb" ] && [ -n "$usb_free_kb" ] &&
        [ -n "$BACKUP_REQUIRED_KB" ] && [ -n "$ROOT_PEAK_REQUIRED_KB" ] ||
        fail_update "Unable to determine upgrade storage capacity"
    case "$root_free_kb:$usb_free_kb:$BACKUP_REQUIRED_KB:$ROOT_PEAK_REQUIRED_KB" in
        *[!0-9:]*) fail_update "Unable to determine upgrade storage capacity" ;;
    esac

    root_required_kb=$((ROOT_PEAK_REQUIRED_KB + 1024))
    usb_required_kb=$((BACKUP_REQUIRED_KB + 4096))
    echo "Storage preflight: root_free=${root_free_kb}KB root_peak=${ROOT_PEAK_REQUIRED_KB}KB root_required=${root_required_kb}KB usb_free=${usb_free_kb}KB backup_required=${BACKUP_REQUIRED_KB}KB" >> "$LOG"
    [ "$root_free_kb" -ge "$root_required_kb" ] ||
        fail_update "Insufficient root filesystem space for atomic upgrade"
    [ "$usb_free_kb" -ge "$usb_required_kb" ] ||
        fail_update "Insufficient USB space for upgrade rollback backup"
}

backup_target()
{
    kind="$1"
    mode="$2"
    rel="$3"
    dest=/$rel
    backup=$BACKUP_DIR/rootfs/$rel

    mkdir -p "$(dirname "$backup")"
    if [ -e "$dest" ]; then
        cp -R "$dest" "$backup" >> "$LOG" 2>&1 ||
            fail_update "Failed to back up an installed file"
        echo "1|$kind|$mode|$rel" >> "$BACKUP_STATE"
    else
        echo "0|$kind|$mode|$rel" >> "$BACKUP_STATE"
    fi
}

install_file_entry()
{
    mode="$1"
    rel="$2"
    src=$STAGE_DIR/payload/$rel
    dest=/$rel
    new=${dest}.un260-new.$$

    mkdir -p "$(dirname "$dest")"
    rm -f "$new"
    cp "$src" "$new" >> "$LOG" 2>&1 ||
        fail_update "Failed to stage an upgrade file"
    chmod "$mode" "$new" || fail_update "Failed to set upgrade file permissions"
    sync
    mv -f "$new" "$dest" >> "$LOG" 2>&1 ||
        fail_update "Failed to replace an installed file"
}

install_tree_entry()
{
    mode="$1"
    rel="$2"
    src=$STAGE_DIR/payload/$rel
    dest=/$rel
    new=${dest}.un260-new.$$
    old=${dest}.un260-old.$$

    mkdir -p "$(dirname "$dest")"
    rm -rf "$new" "$old"
    cp -R "$src" "$new" >> "$LOG" 2>&1 ||
        fail_update "Failed to stage an upgrade directory"
    chmod "$mode" "$new" || fail_update "Failed to set directory permissions"
    apply_tree_permissions "$new" ||
        fail_update "Failed to set resource directory permissions"
    sync

    if [ -e "$dest" ]; then
        mv "$dest" "$old" >> "$LOG" 2>&1 ||
            fail_update "Failed to prepare directory replacement"
    fi
    if ! mv "$new" "$dest" >> "$LOG" 2>&1; then
        [ -e "$old" ] && mv "$old" "$dest" >> "$LOG" 2>&1
        fail_update "Failed to replace an installed directory"
    fi
    rm -rf "$old"
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
    validate_archive_paths
    rm -rf "$STAGE_DIR" "$BACKUP_DIR"
    mkdir -p "$STAGE_DIR" "$BACKUP_DIR/rootfs"
    : > "$BACKUP_STATE"

    tar -xzf "$BUNDLE_PATH" -C "$STAGE_DIR" >> "$LOG" 2>&1 ||
        fail_update "Failed to extract upgrade archive"

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

    validate_payload_checksums

    validate_install_manifest
    validate_storage_space
    write_status 24 verify "Upgrade package verified" "" ""

    TRANSACTION_STARTED=1
    trap interrupt_update HUP INT TERM
    install_index=0

    while IFS='|' read -r kind mode rel extra; do
        [ -n "$kind" ] || continue
        backup_target "$kind" "$mode" "$rel"

        if [ "$kind" = "file" ]; then
            install_file_entry "$mode" "$rel"
        else
            install_tree_entry "$mode" "$rel"
        fi

        install_index=$((install_index + 1))
        progress=$((24 + (install_index * 64 / INSTALL_ENTRY_COUNT)))
        write_status "$progress" write "Writing system files" "" ""
    done < "$STAGE_DIR/install.tsv"

    record_installed_bundle
    sync
    TRANSACTION_STARTED=0
    trap - HUP INT TERM

    write_status 96 finish "Finalizing upgrade" "" ""
    rm -rf "$STAGE_DIR" "$ARCHIVE_LIST" "$ARCHIVE_TYPES" \
        "$SEEN_TARGETS" "$CHECKSUM_TARGETS" "$PAYLOAD_FILES"
    write_status 100 success "Upgrade complete" 1 "The system has been updated successfully. Restarting the device is recommended."
    write_result success "Upgrade completed successfully; reboot is required" "$package_version"
    echo "Bundle update OK: version=$package_version id=$package_id" >> "$LOG"
}

install_legacy_file()
{
    src="$1"
    dest="$2"
    mode="$3"
    new=${dest}.un260-new.$$

    mkdir -p "$(dirname "$dest")"
    cp "$src" "$new" >> "$LOG" 2>&1 || fail_update "Failed to copy legacy file"
    chmod "$mode" "$new" || fail_update "Failed to set legacy file permissions"
    sync
    mv -f "$new" "$dest" >> "$LOG" 2>&1 || fail_update "Failed to replace legacy file"
}

install_legacy_package()
{
    [ -f "$LEGACY_APP_PATH" ] || fail_update "Upgrade binary not found on USB drive"
    write_status 18 verify "Upgrade package verified" "" ""

    [ ! -f "$LEGACY_LIB_PATH" ] ||
        install_legacy_file "$LEGACY_LIB_PATH" /usr/local/lib/liblvgl.so 0755
    write_status 48 write "Writing system files" "" ""

    if [ -d "$LEGACY_DATA_PATH" ]; then
        rm -rf /usr/local/share/lvgl_data.un260-new.$$
        cp -R "$LEGACY_DATA_PATH" /usr/local/share/lvgl_data.un260-new.$$ >> "$LOG" 2>&1 ||
            fail_update "Failed to copy UI image resources"
        rm -rf /usr/local/share/lvgl_data
        mv /usr/local/share/lvgl_data.un260-new.$$ /usr/local/share/lvgl_data ||
            fail_update "Failed to replace UI image resources"
    fi
    write_status 72 write "Writing system files" "" ""

    install_legacy_file "$LEGACY_APP_PATH" /usr/local/bin/test_lvgl 0755
    sync
    write_status 100 success "Upgrade complete" 1 "The system has been updated successfully. Restarting the device is recommended."
    write_result success "Legacy UI upgrade completed; reboot is required" legacy
    echo "Legacy update OK" >> "$LOG"
}

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
write_status 5 verify "Verifying upgrade package" "" ""

if [ -f "$BUNDLE_PATH" ]; then
    install_bundle
elif [ -f "$LEGACY_APP_PATH" ]; then
    install_legacy_package
else
    fail_update "No supported UN260 upgrade package was found"
fi

exit 0
