#!/bin/sh
# UN260_APP_VOLUME_V1: logical app path maps to its owned UBIFS volume.
# UN260_STORAGE_SYNC_1: held-fd writeback barriers before reporting success.
# UN260_UNIFIED_1: one USB bundle, persistent opt-in, automatic boot continuation.

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
APP_VOLUME=0
APP_VOLUME_DIR=/mnt/un260-app
APP_PEAK_REQUIRED_KB=0
PLAN_FILE=$UPDATE_DIR/.un260_plan
JOURNAL=$INSTALLED_STATE_DIR/transaction
LOCK_DIR=/tmp/un260-updater.lock
WORK_OWNED=0
PAYLOAD_VERIFIED=0
package_schema=1
DELTA_ALREADY_APPLIED=0
PHASE_TIME=$(date +%s)
START_TIME=$PHASE_TIME
GUARDED=0
USB_GUARDED=0
USB_FAILED=0
APP_GUARDED=0
LOCAL_FAILED=0
SYNC_TOOL=/usr/local/bin/un260_storage_sync
UNIFIED_REQUEST=etc/un260/unified-request
UNIFIED_RUNNING=0
UNIFIED_RESUME=0
VERIFY_ONLY=0
UNIFIED_COMMIT=""
UNIFIED_BRIDGE=0

local_barrier()
{
    if [ "$GUARDED" = 0 ]; then sync; return; fi
    [ "$LOCAL_FAILED" = 0 ] || return 1
    "$SYNC_TOOL" --fd 7 "${ROOT_PREFIX:-/}" || { LOCAL_FAILED=1; return 1; }
    if [ "$APP_GUARDED" = 1 ]; then "$SYNC_TOOL" --fd 9 "$APP_VOLUME_DIR" || { LOCAL_FAILED=1; return 1; }; fi
}

usb_barrier()
{
    [ "$GUARDED" = 1 ] || { sync; return; }
    [ "$USB_FAILED" = 0 ] || return 1
    if [ "$USB_GUARDED" = 1 ] && [ -b "$USB_DEV" ] &&
        awk -v p="$USB_MNT" -v d="$USB_DEV" '$2==p && $1==d && $4 ~ /(^|,)rw(,|$)/ {ok=1} END {exit !ok}' /proc/mounts &&
        "$SYNC_TOOL" --fd 8 "$USB_MNT"; then return 0; fi
    USB_FAILED=1
    return 1
}

backup_digest()
{
    if [ "$GUARDED" = 1 ]; then "$SYNC_TOOL" --hash-direct "$1"; else sha256sum "$1" | awk '{print $1}'; fi
}

finish_success()
{
    completion_message=$1
    if [ "$UNIFIED_BRIDGE" = 1 ]; then
        completion_message='Bridge ready; restart with the SAME USB to finish the upgrade'
    fi
    write_status 96 finish finish "" "Keep USB and power connected; finalizing update"
    cleanup_usb_work || fail_update 'Installed files verified; USB cleanup failed'
    local_barrier && usb_barrier || fail_update 'Installed files verified; storage flush failed'
    # An inner phase is not completion of the user's one-package operation.
    [ "$UNIFIED_RUNNING" = 0 ] || return 0
    echo "Update verified: version=$package_version; final USB flush pending" >> "$LOG"
    write_result success "$completion_message" "$package_version" || fail_update 'Cannot write update result'
    if [ "$GUARDED" = 1 ]; then
        # LOG remains in RAM: never write USB logs after the final USB barrier.
        cp "$LOG" "$USB_MNT/ui_update.log" &&
        cp "$RESULT_FILE" "$USB_MNT/UN260_UPDATE_RESULT.txt" && usb_barrier ||
            fail_update 'Files installed; USB finalization failed. Keep power connected and inspect RAM log'
    fi
    record_installed_bundle && local_barrier || fail_update 'Files installed; completion marker could not be persisted'
    if [ -n "$UNIFIED_COMMIT" ]; then
        printf '%s\n' "$UNIFIED_COMMIT" > "$INSTALLED_STATE_DIR/unified.completed.tmp" &&
        local_barrier && mv -f "$INSTALLED_STATE_DIR/unified.completed.tmp" "$INSTALLED_STATE_DIR/unified.completed" &&
        local_barrier || fail_update 'Cannot persist unified completion; keep USB for retry'
    fi
    write_status 100 success success 1 "$completion_message" || { echo 'Cannot publish final status' >&2; exit 1; }
}

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
    if [ "$stage" != "$LAST_STAGE" ]; then
        now=$(date +%s)
        echo "UPGRADE_TIME stage=$LAST_STAGE seconds=$((now-PHASE_TIME)) elapsed=$((now-START_TIME))" >> "$LOG"
        PHASE_TIME=$now
    fi

    printf 'progress=%s\nstage=%s\nstep=%s\nsuccess=%s\nmessage=%s\n' \
        "$progress" "$stage" "$step" "$success" "$message" > "$STATUS_TEMP" || return 1
    mv -f "$STATUS_TEMP" "$STATUS_FILE" || return 1
    LAST_PROGRESS="$progress"
    LAST_STAGE="$stage"
}

write_result()
{
    result="$1"
    message="$2"
    version="$3"

    printf 'result=%s\nversion=%s\nmessage=%s\ntime=%s\n' "$result" "$version" "$message" "$(date)" > "$RESULT_FILE"
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

    # Runtime migration state is never package-owned.
    case "$rel" in etc/un260/app-storage|etc/un260/app-storage/*) return 1 ;; esac

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

destination_for()
{
    if [ "$APP_VOLUME" = 1 ] && [ "$1" = usr/local/bin/test_lvgl ]; then
        printf '%s/test_lvgl\n' "$APP_VOLUME_DIR"
    else
        printf '%s/%s\n' "$ROOT_PREFIX" "$1"
    fi
}

prepare_app_storage()
{
    storage_helper=$ROOT_PREFIX/usr/local/bin/un260_app_storage
    if [ -e "$storage_helper" ]; then
        safe_destination usr/local/bin/un260_app_storage || return 1
        sh "$storage_helper" --prepare >> "$LOG" 2>&1 || return 1
    fi
    if [ -e "$ROOT_PREFIX/etc/un260/app-storage/active" ]; then
        [ -f "$storage_helper" ] || return 1
        APP_VOLUME=1
        if [ "$GUARDED" = 1 ] && [ "$APP_GUARDED" = 0 ]; then
            exec 9< "$APP_VOLUME_DIR" || return 1
            APP_GUARDED=1
        fi
    fi
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
    check_path=$(destination_for "$1")
    [ ! -L "$check_path" ] || return 1
    [ ! -e "$check_path" ] || [ -f "$check_path" ] || return 1
    check_path=$(dirname "$check_path")
    check_root=${ROOT_PREFIX:-/}
    if [ "$APP_VOLUME" = 1 ] && [ "$1" = usr/local/bin/test_lvgl ]; then check_root=/; fi
    while [ "$check_path" != "$check_root" ]; do
        [ ! -L "$check_path" ] || return 1
        [ ! -e "$check_path" ] || [ -d "$check_path" ] || return 1
        [ "$check_path" != / ] || return 1
        check_path=$(dirname "$check_path")
    done
}

cleanup_usb_work()
{
    [ "$WORK_OWNED" -eq 1 ] || return 0
    [ "$USB_FAILED" = 0 ] || return 1
    # Only this invocation's staging files, never the user's package or logs.
    [ "$GUARDED" = 0 ] || usb_barrier || return 1
    rm -rf "$STAGE_DIR" "$BACKUP_DIR" || return 1
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
            rdest=$(destination_for "$rpath")
            for suffix in old new restore; do
                [ ! -L "$rdest.un260-$suffix" ] || return 1
            done
        done < "$JOURNAL/state"
        while IFS='|' read -r present rpath; do
            rdest=$(destination_for "$rpath")
            if [ "$present" = 1 ]; then
                if [ -f "$rdest.un260-old" ]; then
                    # A failed staging copy has not replaced the original.
                    # mv between two links to that same inode may fail; there
                    # is nothing to restore in that case. Also makes a second
                    # recovery after a power cut during rollback idempotent.
                    if [ ! "$rdest" -ef "$rdest.un260-old" ]; then
                        rm -f "$rdest.un260-restore" || return 1
                        ln "$rdest.un260-old" "$rdest.un260-restore" || return 1
                        mv -f "$rdest.un260-restore" "$rdest" || return 1
                    fi
                fi
                # No old link means interruption before ln; original untouched.
                [ -f "$rdest" ] || return 1
            else
                rm -f "$rdest" || return 1
            fi
        done < "$JOURNAL/state"
        local_barrier || return 1
        : > "$JOURNAL/rolled-back" || return 1
        local_barrier || return 1
        echo "Rollback finished" >> "$LOG"
    fi
    while IFS='|' read -r present rpath extra; do
        [ -z "$extra" ] && safe_destination "$rpath" || return 1
        rdest=$(destination_for "$rpath")
        rm -f "$rdest.un260-old" "$rdest.un260-new" "$rdest.un260-restore" || return 1
    done < "$JOURNAL/state"
    local_barrier || return 1
    rm -f "$JOURNAL/state" "$JOURNAL/committed" "$JOURNAL/rolled-back"
    rmdir "$JOURNAL" || return 1
    local_barrier || return 1
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
    if [ "$LOCAL_FAILED" = 0 ] && rollback_bundle; then
        cleanup_usb_work || echo 'USB work cleanup incomplete; preserve remaining backup' >> "$LOG"
    else
        msg="$msg; recovery incomplete, keep USB backup and do not start UI"
        echo "ERR: Recovery incomplete; journal=$JOURNAL backup=$BACKUP_DIR" >> "$LOG"
    fi
    write_status "$LAST_PROGRESS" fail "fail" 0 "$msg"
    write_result fail "$msg" ""
    # RAM status/log stay available even after USB disappears. USB reporting is
    # best effort on failure and cannot change the failed outcome.
    if [ "$USB_GUARDED" = 1 ] && usb_barrier; then
        cp "$LOG" "$USB_MNT/ui_update.log"
        cp "$RESULT_FILE" "$USB_MNT/UN260_UPDATE_RESULT.txt"
        usb_barrier || :
    fi
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
            manifest.ini|checksums.sha256|install.tsv|baseline.tsv|target.tsv|payload|payload/*) ;;
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

    [ "$checksum_count" -gt 0 ] || [ "$package_schema" = 2 ] || fail_update "Checksum manifest is empty"

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

    [ "$checksum_count" = 0 ] || (cd "$STAGE_DIR" && sha256sum -c checksums.sha256) >> "$LOG" 2>&1 ||
        fail_update "Upgrade payload checksum verification failed"
    PAYLOAD_VERIFIED=1
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
    plan_dest=$(destination_for "$plan_rel")
    for suffix in old new restore; do
        [ ! -e "$plan_dest.un260-$suffix" ] && [ ! -L "$plan_dest.un260-$suffix" ] ||
            fail_update "Unrecovered install artifact: $plan_rel"
    done
    if [ "$PAYLOAD_VERIFIED" = 1 ]; then
        plan_hash=$(awk -v p="payload/$plan_rel" '$2 == p {print $1}' "$STAGE_DIR/checksums.sha256")
    else
        plan_hash=$(sha256sum "$plan_src" | awk '{print $1}')
    fi
    case "$plan_hash" in ''|*[!0-9a-f]*) fail_update "Cannot hash payload" ;; esac
    plan_size=$(wc -c < "$plan_src" | tr -d ' ')
    case "$plan_size" in ''|*[!0-9]*) fail_update "Cannot measure payload" ;; esac
    dest_hash=-
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
    if [ "$APP_VOLUME" = 1 ] && [ "$plan_rel" = usr/local/bin/test_lvgl ]; then
        APP_PEAK_REQUIRED_KB=$((APP_PEAK_REQUIRED_KB + (plan_size + 1023) / 1024 + 4))
    else
        ROOT_PEAK_REQUIRED_KB=$((ROOT_PEAK_REQUIRED_KB + (plan_size + 1023) / 1024 + 4))
    fi
    printf 'file|%s|%s|%s|%s|%s\n' "$plan_mode" "$plan_rel" "$plan_hash" "$dest_hash" "$plan_size" >> "$PLAN_FILE"
    INSTALL_TOTAL_BYTES=$((INSTALL_TOTAL_BYTES+plan_size))
    INSTALL_ENTRY_COUNT=$((INSTALL_ENTRY_COUNT + 1))
    [ "$INSTALL_ENTRY_COUNT" -le 8192 ] || fail_update "Too many changed files"
}

validate_install_manifest()
{
    INSTALL_ENTRY_COUNT=0
    SKIPPED_COUNT=0
    BACKUP_REQUIRED_KB=0
    ROOT_PEAK_REQUIRED_KB=0
    APP_PEAK_REQUIRED_KB=0
    INSTALL_TOTAL_BYTES=0
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
            delete)
                case "$rel" in usr/local/bin/test_lvgl|usr/local/lib/liblvgl.so|usr/local/bin/un260_unpack|usr/local/bin/un260_storage_sync|usr/local/bin/un260_resource_cleanup|usr/local/bin/un260_app_storage|usr/bin/ui_update.sh|etc/init.d/S00lvgl) fail_update "Cannot delete a core upgrade component" ;; esac
                [ "$package_schema" = 2 ] || fail_update "Deletes require an incremental package"
                [ ! -e "$STAGE_DIR/payload/$rel" ] || fail_update "Delete has a payload"
                awk -F '|' -v p="$rel" '$4==p {found=1} END {exit !found}' "$STAGE_DIR/baseline.tsv" || fail_update "Delete is absent from baseline"
                if awk -F '|' -v p="$rel" '$4==p {found=1} END {exit !found}' "$STAGE_DIR/target.tsv"; then fail_update "Delete is in target"; fi
                safe_destination "$rel" || fail_update "Unsafe deletion target"
                for suffix in old new restore; do
                    [ ! -e "$ROOT_PREFIX/$rel.un260-$suffix" ] && [ ! -L "$ROOT_PREFIX/$rel.un260-$suffix" ] || fail_update "Unrecovered deletion artifact"
                done
                grep -Fx "$rel" "$SEEN_TARGETS" >/dev/null && fail_update "Duplicate deletion target"
                echo "$rel" >> "$SEEN_TARGETS"
                delete_hash=$(sha256sum "$ROOT_PREFIX/$rel" | awk '{print $1}')
                delete_bytes=$(wc -c < "$ROOT_PREFIX/$rel")
                BACKUP_REQUIRED_KB=$((BACKUP_REQUIRED_KB + (delete_bytes+1023)/1024+4))
                printf 'delete|%s|%s|-|%s|0\n' "$mode" "$rel" "$delete_hash" >> "$PLAN_FILE"
                INSTALL_ENTRY_COUNT=$((INSTALL_ENTRY_COUNT+1))
                ;;
            *) fail_update "Invalid install manifest entry type" ;;
        esac
        manifest_count=$((manifest_count + 1))
        if [ "$package_schema" = 2 ]; then manifest_limit=8192; else manifest_limit=256; fi
        [ "$manifest_count" -le "$manifest_limit" ] || fail_update "Too many manifest entries"
    done < "$STAGE_DIR/install.tsv"
    [ "$manifest_count" -gt 0 ] || [ "$package_schema" = 2 ] || fail_update "Empty install manifest"
    echo "Install plan: changed=$INSTALL_ENTRY_COUNT unchanged=$SKIPPED_COUNT bytes=$INSTALL_TOTAL_BYTES root_staged=${ROOT_PEAK_REQUIRED_KB}KB" >> "$LOG"
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
        fail_update "Insufficient root space: need ${root_required_kb}KB, free ${root_free_kb}KB; keep USB log for storage service"
    [ "$usb_free_kb" -ge "$usb_required_kb" ] ||
        fail_update "Insufficient USB space for upgrade rollback backup"
    if [ "$APP_VOLUME" = 1 ]; then
        app_free_kb=$(df -Pk "$APP_VOLUME_DIR" 2>/dev/null | awk 'NR==2 {print $4}')
        case "$app_free_kb" in ''|*[!0-9]*) fail_update 'Cannot read app volume free space' ;; esac
        app_required_kb=$((APP_PEAK_REQUIRED_KB + 2048))
        echo "App preflight: free=${app_free_kb}KB peak=${APP_PEAK_REQUIRED_KB}KB required=${app_required_kb}KB" >> "$LOG"
        [ "$app_free_kb" -ge "$app_required_kb" ] || fail_update "Insufficient app volume space: need ${app_required_kb}KB, free ${app_free_kb}KB; keep USB log"
    fi
}

install_file_entry()
{
    install_mode=$1
    install_rel=$2
    install_hash=$3
    old_hash=$4
    install_kind=$5
    install_src=$STAGE_DIR/payload/$install_rel
    install_dest=$(destination_for "$install_rel")
    safe_destination "$install_rel" || fail_update "Destination changed during upgrade"
    mkdir -p "$(dirname "$install_dest")" "$BACKUP_DIR/rootfs/$(dirname "$install_rel")" ||
        fail_update "Cannot prepare install directories"
    present=0
    if [ -f "$install_dest" ]; then
        cp "$install_dest" "$BACKUP_DIR/rootfs/$install_rel" >> "$LOG" 2>&1 ||
            fail_update "Cannot back up installed file"
        usb_barrier || fail_update 'USB backup write failed; original retained'
        backup_hash=$(backup_digest "$BACKUP_DIR/rootfs/$install_rel") || fail_update 'USB backup readback failed'
        [ -n "$old_hash" ] && [ "$old_hash" = "$backup_hash" ] ||
            fail_update "USB backup verification failed"
        present=1
    fi
    printf '%s|%s\n' "$present" "$install_rel" >> "$JOURNAL/state" ||
        fail_update "Cannot persist recovery journal"
    local_barrier || fail_update 'Recovery journal flush failed'
    if [ "$present" = 1 ]; then
        ln "$install_dest" "$install_dest.un260-old" >> "$LOG" 2>&1 ||
            fail_update "Cannot preserve original inode"
        local_barrier || fail_update 'Original inode flush failed'
    fi
    if [ "$install_kind" = delete ]; then
        rm -f "$install_dest" || fail_update "Cannot remove retired file"
        local_barrier || fail_update 'Retired file removal flush failed'
        return 0
    fi
    cp "$install_src" "$install_dest.un260-new" >> "$LOG" 2>&1 ||
        fail_update "Cannot stage replacement file"
    chmod "$install_mode" "$install_dest.un260-new" ||
        fail_update "Cannot set replacement permissions"
    copied_hash=$(sha256sum "$install_dest.un260-new" | awk '{print $1}')
    [ "$copied_hash" = "$install_hash" ] || fail_update "Replacement verification failed"
    local_barrier || fail_update 'Replacement flush failed'
    mv -f "$install_dest.un260-new" "$install_dest" >> "$LOG" 2>&1 ||
        fail_update "Cannot replace installed file"
    local_barrier || fail_update 'Replacement rename flush failed'
}

run_resource_maintenance()
{
    maintenance_rel=usr/local/bin/un260_resource_cleanup
    maintenance=$ROOT_PREFIX/$maintenance_rel
    if [ ! -e "$maintenance" ]; then
        echo 'Storage maintenance: not installed; bootstrap package needed for automatic cleanup' >> "$LOG"
        return 0
    fi
    safe_destination "$maintenance_rel" && [ -f "$maintenance" ] || fail_update 'Unsafe storage maintenance helper'
    grep -q 'UN260_STORAGE_SYNC_1' "$maintenance" || { echo 'Skip old maintenance helper until guarded bootstrap is installed' >> "$LOG"; return 0; }
    # Do not retire a file explicitly owned by this package or its delta base.
    for table in install.tsv baseline.tsv target.tsv; do
        [ -f "$STAGE_DIR/$table" ] || continue
        if grep -Eq 'usr/local/(share/ge_data(/|$)|bin/ge_)' "$STAGE_DIR/$table"; then
            echo 'Storage maintenance: skipped; package references SDK demo files' >> "$LOG"
            return 0
        fi
    done
    echo 'Storage maintenance: backup and exact-hash cleanup before space preflight' >> "$LOG"
    sh "$maintenance" --apply --updater "$$" >> "$LOG" 2>&1 || fail_update 'Storage maintenance failed; original UI unchanged, inspect USB log'
}

execute_plan()
{
    validate_install_manifest
    if [ -n "${storage_migration:-}" ]; then
        # A migration package only repeats the small bootstrap components.
        # Never mix partition initialization with an app/dependency replacement.
        [ "$package_schema" = 1 ] && [ "$(manifest_value package_type)" = storage-migration ] || fail_update 'Invalid migration package'
        while IFS= read -r migration_rel; do
            case "$migration_rel" in usr/local/bin/un260_storage_sync|usr/local/bin/un260_resource_cleanup|usr/local/bin/un260_app_storage|usr/bin/ui_update.sh|etc/init.d/S00lvgl) ;;
                *) fail_update 'Migration package must not replace application or business data' ;;
            esac
        done < "$SEEN_TARGETS"
        validate_storage_space
        [ -f "$ROOT_PREFIX/usr/local/bin/un260_app_storage" ] || fail_update 'Install storage bootstrap first'
        grep -q 'UN260_STORAGE_SYNC_1' "$ROOT_PREFIX/usr/local/bin/un260_app_storage" || fail_update 'Install guarded bootstrap before migration'
        grep -q 'UN260_MTD_STREAM_1' "$ROOT_PREFIX/usr/local/bin/un260_app_storage" || fail_update 'Install streaming-read repair package before migration'
        sh "$ROOT_PREFIX/usr/local/bin/un260_app_storage" --migrate "$storage_migration" >> "$LOG" 2>&1 || fail_update 'Storage migration paused; keep USB backup and log; no IMG required for diagnosis'
        prepare_app_storage || fail_update 'Cannot prepare migrated application volume'
    fi
    run_resource_maintenance
    if [ "$INSTALL_ENTRY_COUNT" = 0 ]; then
        finish_success 'All files already match; no replacements required'
        return 0
    fi
    validate_storage_space
    mkdir -p "$INSTALLED_STATE_DIR" || fail_update "Cannot create updater state directory"
    mkdir -m 0700 "$JOURNAL" || fail_update "Pending recovery must finish first"
    : > "$JOURNAL/state" || fail_update "Cannot create recovery journal"
    local_barrier || fail_update 'Cannot flush empty recovery journal'
    TRANSACTION_STARTED=1
    trap interrupt_update HUP INT TERM
    install_index=0
    installed_bytes=0
    while IFS='|' read -r kind mode rel expected_hash old_hash planned_bytes; do
        echo "Installing: kind=$kind path=$rel bytes=$planned_bytes" >> "$LOG"
        install_file_entry "$mode" "$rel" "$expected_hash" "$old_hash" "$kind"
        install_index=$((install_index + 1))
        installed_bytes=$((installed_bytes+planned_bytes))
        if [ "$INSTALL_TOTAL_BYTES" -gt 0 ]; then
            progress=$((40 + (installed_bytes * 50 / INSTALL_TOTAL_BYTES)))
        else
            progress=$((40 + (install_index * 50 / INSTALL_ENTRY_COUNT)))
        fi
        write_status "$progress" install "install" "" ""
    done < "$PLAN_FILE"
    write_status 92 sync "sync" "" ""
    local_barrier && usb_barrier || fail_update 'Storage flush failed before commit'
    : > "$JOURNAL/committed" || fail_update "Cannot persist transaction commit"
    local_barrier || fail_update 'Cannot flush transaction commit'
    recover_transaction || fail_update "Committed; cleanup requires recovery"
    TRANSACTION_STARTED=0
    trap - HUP INT TERM
    finish_success 'Upgrade completed successfully; reboot is required'
}

prepare_usb_work()
{
    # Older installer backups with actual entries need review, never discard.
    if [ -s "$BACKUP_STATE" ] && [ ! -f "$BACKUP_DIR/managed-v2" ]; then
        fail_update "Previous rollback backup exists; preserve and inspect it first"
    fi
    WORK_OWNED=1
    cleanup_usb_work || fail_update 'USB staging cleanup failed; preserve backup and check filesystem'
    if ! mkdir -p "$STAGE_DIR" "$BACKUP_DIR/rootfs" >> "$LOG" 2>&1; then
        USB_FAILED=1
        fail_update 'USB staging write failed; check filesystem or connection; preserve backup'
    fi
    if ! { : > "$BACKUP_DIR/managed-v2"; } >> "$LOG" 2>&1; then
        USB_FAILED=1
        fail_update 'USB staging marker write failed; check filesystem; preserve backup'
    fi
    usb_barrier || fail_update 'USB staging writeback failed; check filesystem or connection; preserve backup'
}

validate_delta_table()
{
    table=$1
    : > "$SEEN_TARGETS"
    while IFS='|' read -r dh dm ds dp extra; do
        [ -n "$dh" ] && [ -z "$extra" ] || fail_update "Malformed delta table"
        echo "$dh" | grep -Eq '^[0-9a-f]{64}$' || fail_update "Bad delta digest"
        case "$dm" in 0644|0755) ;; *) fail_update "Bad delta mode" ;; esac
        case "$ds" in ''|*[!0-9]*) fail_update "Bad delta size" ;; esac
        is_safe_relative_path "$dp" && is_allowed_target "$dp" && safe_destination "$dp" || fail_update "Unsafe delta target"
        grep -Fx "$dp" "$SEEN_TARGETS" >/dev/null && fail_update "Duplicate delta target"
        echo "$dp" >> "$SEEN_TARGETS"
    done < "$table"
}

delta_target_matches()
{
    while IFS='|' read -r mh mm ms mp; do
        match_dest=$(destination_for "$mp")
        [ -f "$match_dest" ] || return 1
        [ "$(sha256sum "$match_dest" | awk '{print $1}')" = "$mh" ] || return 1
        [ "$(wc -c < "$match_dest" | tr -d ' ')" = "$ms" ] || return 1
        case "$mm" in 0755) match_mode=-rwxr-xr-x ;; *) match_mode=-rw-r--r-- ;; esac
        [ "$(LC_ALL=C ls -ld "$match_dest" | awk '{print $1}')" = "$match_mode" ] || return 1
    done < "$STAGE_DIR/target.tsv"
    while IFS='|' read -r mh mm ms mp; do
        if ! awk -F '|' -v p="$mp" '$4==p {found=1} END {exit !found}' "$STAGE_DIR/target.tsv"; then
            [ ! -e "$ROOT_PREFIX/$mp" ] || return 1
        fi
    done < "$STAGE_DIR/baseline.tsv"
}

validate_delta_baseline()
{
    [ "$(manifest_value package_type)" = ui-delta ] || fail_update "Wrong incremental type"
    [ -f "$STAGE_DIR/baseline.tsv" ] && [ -f "$STAGE_DIR/target.tsv" ] || fail_update "Missing delta tables"
    for name in baseline target; do
        expected=$(manifest_value "${name}_id")
        actual=$(sha256sum "$STAGE_DIR/$name.tsv" | awk '{print $1}')
        [ -n "$expected" ] && [ "$expected" = "$actual" ] || fail_update "Delta table checksum mismatch"
        validate_delta_table "$STAGE_DIR/$name.tsv"
    done
    if delta_target_matches; then
        DELTA_ALREADY_APPLIED=1
        echo "Delta target already verified on disk; no replacements required" >> "$LOG"
        return 0
    fi
    # Every immutable packaged dependency is checked on disk, not merely a
    # stored version marker. Local settings outside this inventory are untouched.
    while IFS='|' read -r bh bm bs bp; do
        baseline_dest=$(destination_for "$bp")
        [ -f "$baseline_dest" ] || fail_update "Incremental baseline missing: use full package"
        current=$(sha256sum "$baseline_dest" | awk '{print $1}')
        current_size=$(wc -c < "$baseline_dest" | tr -d ' ')
        current_mode=$(LC_ALL=C ls -ld "$baseline_dest" | awk '{print $1}')
        case "$bm" in 0755) mode_text=-rwxr-xr-x ;; *) mode_text=-rw-r--r-- ;; esac
        [ "$current" = "$bh" ] && [ "$current_size" = "$bs" ] && [ "$current_mode" = "$mode_text" ] ||
            fail_update "Incremental baseline differs: use matching full package"
        if ! awk -F '|' -v p="$bp" '$4==p {found=1} END {exit !found}' "$STAGE_DIR/target.tsv"; then
            grep -Fx "delete|$bm|$bp" "$STAGE_DIR/install.tsv" >/dev/null || fail_update "Missing explicit deletion"
        fi
    done < "$STAGE_DIR/baseline.tsv"
    while IFS='|' read -r th tm ts tp; do
        if [ -f "$STAGE_DIR/payload/$tp" ]; then
            payload_digest=$(awk -v p="payload/$tp" '$2==p {print $1}' "$STAGE_DIR/checksums.sha256")
            [ "$payload_digest" = "$th" ] && [ "$(wc -c < "$STAGE_DIR/payload/$tp" | tr -d ' ')" = "$ts" ] || fail_update "Delta payload differs from target"
            grep -Fx "file|$tm|$tp" "$STAGE_DIR/install.tsv" >/dev/null || fail_update "Missing target install entry"
        else
            grep -Fx "$th|$tm|$ts|$tp" "$STAGE_DIR/baseline.tsv" >/dev/null || fail_update "Delta omits a changed dependency"
        fi
    done < "$STAGE_DIR/target.tsv"
    while read -r ph pp; do
        [ -n "$ph" ] || continue
        pr=${pp#payload/}
        awk -F '|' -v p="$pr" '$4==p {found=1} END {exit !found}' "$STAGE_DIR/target.tsv" || fail_update "Unexpected delta payload"
    done < "$STAGE_DIR/checksums.sha256"
    echo "Delta baseline verified: all dependency hashes and modes match" >> "$LOG"
}

record_installed_bundle()
{
    [ -n "$BUNDLE_FNV" ] || return 0
    echo "$BUNDLE_FNV" | grep -Eq '^[0-9a-fA-F]{16}$' || return 0

    mkdir -p "$INSTALLED_STATE_DIR" || return 1
    echo "$BUNDLE_FNV" > "$INSTALLED_HASH_PATH.tmp.$$" || return 1
    mv -f "$INSTALLED_HASH_PATH.tmp.$$" "$INSTALLED_HASH_PATH"
}

install_bundle()
{
    # Capture the original USB archive, never an inner package later moved or
    # deleted by unified continuation. Manual/boot upgrades have no UI-supplied
    # --bundle-fnv; they must publish the same installed identity after success.
    if [ "$UNIFIED_RUNNING" = 0 ]; then
        BUNDLE_FNV=$("$ROOT_PREFIX/usr/local/bin/un260_storage_sync" --hash-fnv64 "$BUNDLE_PATH" 2>> "$LOG") ||
            fail_update 'Cannot fingerprint selected USB package'
        echo "$BUNDLE_FNV" | grep -Eq '^[0-9a-f]{16}$' || fail_update 'Invalid selected package fingerprint'
    fi
    write_status 5 verify "verify_archive" "" ""
    prepare_usb_work
    unpacker=$ROOT_PREFIX/usr/local/bin/un260_unpack
    if [ -x "$unpacker" ] && [ "$("$unpacker" --probe 2>/dev/null)" = UN260_UNPACK_1 ]; then
        write_status 10 extract "extract" "" ""
        "$unpacker" "$BUNDLE_PATH" "$STAGE_DIR" >> "$LOG" 2>&1 || fail_update "Unsafe or damaged archive"
    else
        validate_archive_paths
        write_status 10 extract "extract" "" ""
        tar -xzf "$BUNDLE_PATH" -C "$STAGE_DIR" >> "$LOG" 2>&1 || fail_update "Failed to extract upgrade archive"
    fi

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
    case "$package_schema" in 1|2) ;; *) fail_update "Unsupported package schema" ;; esac
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
    if [ "$UNIFIED_RESUME" = 1 ] && [ "$UNIFIED_RUNNING" = 0 ]; then
        [ "$(manifest_value package_type)" = ui-unified ] || fail_update 'Keep the same unified USB package used before restart'
    fi
    if [ "$(manifest_value package_type)" = ui-unified ]; then
        [ "$UNIFIED_RUNNING" = 0 ] && [ "$VERIFY_ONLY" = 0 ] || fail_update 'Nested unified packages are forbidden'
        validate_unified_bridge
        if [ "$UNIFIED_RESUME" = 1 ]; then
            continue_unified
        else
            # Old and new installers use this same tiny first-pass manifest.
            # No app, nested package or partition initialization reaches root.
            UNIFIED_BRIDGE=1
            execute_plan
        fi
        return 0
    fi
    storage_migration=$(manifest_value storage_migration)
    if [ "$GUARDED" = 1 ]; then
        [ "$(manifest_value storage_guard)" = syncfs-v1 ] || fail_update 'Package predates USB write protection; use current UPK'
        for guarded_component in usr/bin/ui_update.sh usr/local/bin/un260_app_storage usr/local/bin/un260_resource_cleanup usr/local/bin/un260_storage_sync; do
            if [ -e "$STAGE_DIR/payload/$guarded_component" ]; then
                grep -q 'UN260_STORAGE_SYNC_1' "$STAGE_DIR/payload/$guarded_component" || fail_update 'Package contains an outdated storage guard component'
            fi
        done
    fi
    if [ "$VERIFY_ONLY" = 0 ] && [ -e "$ROOT_PREFIX/etc/un260/app-storage/pending" ] && [ -z "$storage_migration" ]; then
        fail_update 'Finish the pending storage migration before a normal update'
    fi
    if [ "$APP_VOLUME" = 1 ] || [ -n "$storage_migration" ]; then
        [ "$(manifest_value storage_layout)" = app-volume-v1 ] || fail_update 'Package predates app-volume support; use current full UPK'
        # Do not allow a declared capability to downgrade the actual recovery
        # components. These exact small files are required in full/migration.
        for component in usr/bin/ui_update.sh usr/local/bin/un260_app_storage etc/init.d/S00lvgl; do
            if [ "$package_schema" = 1 ] || [ -e "$STAGE_DIR/payload/$component" ]; then
                [ -f "$STAGE_DIR/payload/$component" ] || fail_update 'Storage-compatible recovery component missing'
                grep -q 'UN260_APP_VOLUME_V1' "$STAGE_DIR/payload/$component" || fail_update 'Storage recovery component too old'
            fi
        done
    fi
    if [ "$package_schema" = 2 ]; then validate_delta_baseline; fi
    if [ "$VERIFY_ONLY" = 1 ]; then
        [ "$package_schema" = 1 ] && [ -z "$storage_migration" ] || fail_update 'Unified application must be a normal full package'
        validate_install_manifest
        return 0
    fi
    if [ "$DELTA_ALREADY_APPLIED" = 1 ]; then
        finish_success 'Target files already match'
        return 0
    fi

    write_status 34 preflight "preflight" "" ""
    execute_plan
}

checked_digest()
{
    digest_output=$(sha256sum "$1" 2>> "$LOG") || return 1
    digest_value=${digest_output%% *}
    echo "$digest_value" | grep -Eq '^[0-9a-f]{64}$' || return 1
    printf '%s\n' "$digest_value"
}

validate_unified_bridge()
{
    [ "$package_schema" = 1 ] && [ "$(manifest_value storage_guard)" = syncfs-v1 ] &&
        [ "$(manifest_value storage_layout)" = app-volume-v1 ] && [ -z "$(manifest_value storage_migration)" ] || fail_update 'Invalid unified capabilities'
    validate_install_manifest
    [ "$(wc -l < "$SEEN_TARGETS" | tr -d ' ')" = 7 ] || fail_update 'Incomplete unified bridge'
    for bridge_required in usr/local/bin/un260_storage_sync usr/local/bin/un260_resource_cleanup usr/local/bin/un260_app_storage usr/local/bin/un260_upgrade_display usr/bin/ui_update.sh etc/init.d/S00lvgl etc/un260/unified-request; do
        grep -Fx "$bridge_required" "$SEEN_TARGETS" >/dev/null || fail_update 'Unexpected unified bridge file set'
    done
}

unified_pending()
{
    [ -e "$ROOT_PREFIX/$UNIFIED_REQUEST" ] || return 1
    safe_destination "$UNIFIED_REQUEST" && [ -f "$ROOT_PREFIX/$UNIFIED_REQUEST" ] || return 0
    request_id=$(checked_digest "$ROOT_PREFIX/$UNIFIED_REQUEST") || return 0
    [ -f "$INSTALLED_STATE_DIR/unified.completed" ] &&
        [ "$(cat "$INSTALLED_STATE_DIR/unified.completed")" = "$request_id" ] && return 1
    return 0
}

continue_unified()
{
    [ "$package_schema" = 1 ] && [ "$(manifest_value storage_guard)" = syncfs-v1 ] &&
        [ "$(manifest_value storage_layout)" = app-volume-v1 ] || fail_update 'Invalid unified capabilities'
    safe_destination "$UNIFIED_REQUEST" && [ -f "$ROOT_PREFIX/$UNIFIED_REQUEST" ] ||
        fail_update 'Missing or unsafe continuation request; inspect installed bridge'
    UNIFIED_COMMIT=$(checked_digest "$ROOT_PREFIX/$UNIFIED_REQUEST") || fail_update 'Cannot read continuation request'
    bridge_request_hash=$(checked_digest "$STAGE_DIR/payload/$UNIFIED_REQUEST") || fail_update 'Cannot read packaged continuation request'
    [ "$UNIFIED_COMMIT" = "$bridge_request_hash" ] || fail_update 'Keep the same USB package used before restart'
    # Check exact installed bridge bytes: never run an obsolete helper simply
    # because a marker string or a successful first-pass status was present.
    validate_install_manifest
    while IFS= read -r bridge_rel; do
        case "$bridge_rel" in usr/local/bin/un260_storage_sync|usr/local/bin/un260_resource_cleanup|usr/local/bin/un260_app_storage|usr/local/bin/un260_upgrade_display|usr/bin/ui_update.sh|etc/init.d/S00lvgl|etc/un260/unified-request) ;;
            *) fail_update 'Unified bridge may not install application data' ;; esac
        # The deployed minimal system has sha256sum but no cmp applet.
        bridge_installed_hash=$(checked_digest "$ROOT_PREFIX/$bridge_rel") || fail_update 'Cannot read installed bridge component'
        bridge_payload_hash=$(checked_digest "$STAGE_DIR/payload/$bridge_rel") || fail_update 'Cannot read packaged bridge component'
        [ "$bridge_installed_hash" = "$bridge_payload_hash" ] || fail_update 'Bridge is incomplete; run this same package then restart'
    done < "$SEEN_TARGETS"
    unified_work=$UPDATE_DIR/.un260_unified
    [ ! -L "$unified_work" ] || fail_update 'Unsafe unified staging directory'
    mkdir -p "$unified_work" || fail_update 'Cannot create unified staging'
    # Request has exactly two ordered digest lines, not executable shell input.
    [ "$(wc -l < "$ROOT_PREFIX/$UNIFIED_REQUEST" | tr -d ' ')" = 2 ] || fail_update 'Invalid continuation request'
    for inner in migration application; do
        if [ "$inner" = migration ]; then line=1; else line=2; fi
        expected=$(sed -n "${line}p" "$ROOT_PREFIX/$UNIFIED_REQUEST")
        echo "$expected" | grep -Eq '^[0-9a-f]{64}$' || fail_update 'Invalid inner package digest'
        inner_src=$STAGE_DIR/payload/usr/local/share/un260-unified/$inner.upk
        [ "$(checked_digest "$inner_src")" = "$expected" ] || fail_update 'Inner package digest mismatch'
        [ ! -L "$unified_work/$inner.upk" ] || fail_update 'Unsafe inner package destination'
        mv -f "$inner_src" "$unified_work/$inner.upk" || fail_update 'Cannot retain inner package on USB'
    done
    usb_barrier || fail_update 'Cannot persist unified staging'
    UNIFIED_RUNNING=1
    BUNDLE_PATH=$unified_work/application.upk
    VERIFY_ONLY=1
    install_bundle
    VERIFY_ONLY=0
    # Only after the entire final application package passes validation may
    # the separately journaled, fingerprint-approved migration run.
    BUNDLE_PATH=$unified_work/migration.upk
    install_bundle
    BUNDLE_PATH=$unified_work/application.upk
    install_bundle
    rm -f "$unified_work/migration.upk" "$unified_work/application.upk" && rmdir "$unified_work" || fail_update 'Cannot clean unified staging'
    UNIFIED_RUNNING=0
    mkdir -p "$INSTALLED_STATE_DIR" || fail_update 'Cannot create unified completion directory'
    finish_success 'Unified upgrade completed; application is starting'
}

install_legacy_package()
{
    [ "$APP_VOLUME" = 0 ] || fail_update 'Managed app volume requires a current UPK, not loose legacy files'
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
if [ "${1:-}" = --unified-pending ]; then unified_pending; exit $?; fi
[ -x "$SYNC_TOOL" ] && [ "$("$SYNC_TOOL" --probe)" = UN260_STORAGE_SYNC_1 ] || { echo 'Storage guard missing; install matching repair bootstrap' >&2; exit 1; }
exec 7< / || exit 1
GUARDED=1
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    echo "Updater already running or stale lock: $LOCK_DIR" >&2
    exit 1
fi
printf '%s\n' "$$" > "$LOCK_DIR/pid" || { rmdir "$LOCK_DIR" 2>/dev/null; exit 1; }
trap 'rm -f "$LOCK_DIR/pid"; rmdir "$LOCK_DIR" 2>/dev/null' EXIT
if ! prepare_app_storage; then
    echo 'Application storage unavailable; no update or recovery writes attempted' >&2
    exit 1
fi
if ! recover_transaction; then
    echo "Recovery failed; keep $JOURNAL and USB backup" >&2
    exit 1
fi
if [ "${1:-}" = "--recover" ]; then exit 0; fi
if [ "${1:-}" = --continue-unified ]; then
    unified_pending || exit 0
    UNIFIED_RESUME=1
fi

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
if [ "$UNIFIED_RESUME" = 1 ]; then
    # USB enumeration may finish after the early-display startup service.
    usb_wait=0
    while [ -z "$USB_DEV" ] && [ "$usb_wait" -lt 15 ]; do
        sleep 1
        USB_DEV="$(detect_usb_dev)"
        usb_wait=$((usb_wait + 1))
    done
fi
if [ -z "$USB_DEV" ] || [ ! -b "$USB_DEV" ]; then
    fail_update "USB block device not found"
fi
if ! grep -q " $USB_MNT " /proc/mounts 2>/dev/null; then
    mount "$USB_DEV" "$USB_MNT" >> "$LOG" 2>&1 ||
        fail_update "Failed to mount USB device"
fi

exec 8< "$USB_MNT" || fail_update 'Cannot hold USB filesystem for writeback checks'
USB_GUARDED=1
usb_barrier || fail_update 'USB is not a healthy writable mounted filesystem'
: > "$LOG"
rm -f "$RESULT_FILE" "$USB_MNT/UN260_UPDATE_RESULT.txt" || fail_update 'Cannot clear previous USB result'
usb_barrier || fail_update 'USB result reset could not be persisted'
echo "ui_update start: $(date)" >> "$LOG"
write_status 2 prepare "prepare" "" ""

# Full package wins if both exist, matching UI detection. Never accidentally
# apply a stale delta while a full repair package is present.
if [ ! -f "$BUNDLE_PATH" ] && [ -f "$UPDATE_DIR/UN260_UPDATE_DELTA.upk" ]; then
    BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE_DELTA.upk
fi
echo "Selected package: $BUNDLE_PATH" >> "$LOG"

if [ -f "$BUNDLE_PATH" ]; then
    install_bundle
elif [ -f "$LEGACY_APP_PATH" ]; then
    install_legacy_package
else
    fail_update "No supported UN260 upgrade package was found"
fi

exit 0
