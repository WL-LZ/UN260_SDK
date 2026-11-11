#!/bin/sh
# UN260_APP_VOLUME_V1: logical app path maps to its owned UBIFS volume.
# UN260_STORAGE_SYNC_1: held-fd writeback barriers before reporting success.
# UN260_BOOT_BUNDLE_1: immutable root player/theme; activate only after commit.
# UN260_PACKAGE_ENGINE_V1: package-matched engine with inherited lock and FDs.

USB_MNT=/mnt/usb
UPDATE_DIR=$USB_MNT/update
BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk

STATUS_FILE=/tmp/ui_update.status
STATUS_TEMP=/tmp/ui_update.status.tmp.$$
LOG=/tmp/ui_update.log
RESULT_FILE=/tmp/UN260_UPDATE_RESULT.txt

STAGE_DIR=$UPDATE_DIR/.un260_stage
BACKUP_DIR=$UPDATE_DIR/.un260_backup
BACKUP_STATE=$BACKUP_DIR/state.tsv
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
CHECKSUM_MANIFEST_HASH=
FULL_PLAN_COLLECT=0
FULL_PLAN_REQUEST_COUNT=0
FULL_INVENTORY_READY=0
FULL_INVENTORY_HASH=
FULL_INVENTORY_FAST=0
FULL_SCAN_HASH=
INVENTORY_CAPABILITY=unknown
package_schema=1
PHASE_TIME=$(date +%s)
START_TIME=$PHASE_TIME
GUARDED=0
USB_GUARDED=0
USB_FAILED=0
APP_GUARDED=0
LOCAL_FAILED=0
SYNC_TOOL=/usr/local/bin/un260_storage_sync
BOOT_BUNDLE_TREE=usr/local/lib/un260-boot/versions
BOOT_ACTIVE=etc/un260/boot-active
OWNED_STATIC_TREE=usr/local/share/lvgl_data
PRUNE_STATIC_TREE=0
BOOT_REQUEST_ID=""
BOOT_REQUEST_HASH=""
BOOT_OLD_ID=none
BOOT_OLD_HASH=none
BOOT_PUBLICATION_DONE=0
RECOVERED_TRIAL_PENDING=0
TRIAL_ENABLED=0
DISPATCH_CONTEXT=
DISPATCH_STAGE=none
DISPATCH_STAGE_READY=0
DISPATCH_MODE=
APP_STORAGE_TOOL=
RESOURCE_CLEANUP_TOOL=
UNPACK_TOOL=

app_check_trace()
{
    [ "${UN260_BOOT_TRACE:-0}" = 1 ] && [ "${DISPATCH_MODE:-}" = app-check ] || return 0
    if read -r check_uptime check_idle < /proc/uptime; then
        printf 'BOOT_TIME uptime=%s stage=app_check_%s\n' "$check_uptime" "$1" >&2 || :
    fi
}

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
    write_status 96 finish finish "" "Keep USB and power connected; finalizing update"
    cleanup_usb_work || fail_update 'Installed files verified; USB cleanup failed'
    local_barrier && usb_barrier || fail_update 'Installed files verified; storage flush failed'
    echo "Update verified: version=$package_version; final USB flush pending" >> "$LOG"
    write_result success "$completion_message" "$package_version" || fail_update 'Cannot write update result'
    if [ "$GUARDED" = 1 ]; then
        # LOG remains in RAM: never write USB logs after the final USB barrier.
        cp "$LOG" "$USB_MNT/ui_update.log" &&
        cp "$RESULT_FILE" "$USB_MNT/UN260_UPDATE_RESULT.txt" && usb_barrier ||
            fail_update 'Files installed; USB finalization failed. Keep power connected and inspect RAM log'
    fi
    record_installed_bundle && local_barrier || fail_update 'Files installed; completion marker could not be persisted'
    write_status 100 success success 1 "$completion_message" || { echo 'Cannot publish final status' >&2; exit 1; }
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

    printf 'progress=%s\nstage=%s\nstep=%s\nsuccess=%s\nmessage=%s\nwriter_pid=%s\nbundle_fnv64=%s\n' \
        "$progress" "$stage" "$step" "$success" "$message" "$$" "$BUNDLE_FNV" > "$STATUS_TEMP" || return 1
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

    # Factory storage state and earlier migration receipts stay outside package ownership.
    case "$rel" in etc/un260/app-storage|etc/un260/app-storage/*|etc/un260/boot-active|etc/un260/boot-active.*) return 1 ;; esac
    case "$rel" in usr/local/bin/un260_update_dispatch|usr/local/lib/un260-updater|usr/local/lib/un260-updater/*|etc/un260/updater-platform) return 1 ;; esac

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

scan_regular_tree()
(
    scan_root=${1%/}
    scan_output=$2
    [ -n "$scan_root" ] && [ -d "$scan_root" ] && [ ! -L "$scan_root" ] || return 1
    umask 077
    scan_tmp=$(mktemp -d /tmp/un260-tree-scan.XXXXXX) || return 1
    scan_suffix=${scan_tmp#/tmp/un260-tree-scan.}
    [ "$scan_tmp" = "/tmp/un260-tree-scan.$scan_suffix" ] && [ "${#scan_suffix}" = 6 ] || return 1
    case "$scan_suffix" in *[!A-Za-z0-9]*) return 1 ;; esac
    trap 'scan_rc=$?; trap - 0; rm -rf "$scan_tmp" || exit 1; exit "$scan_rc"' 0
    trap 'exit 1' HUP INT TERM
    [ -d "$scan_tmp" ] && [ ! -L "$scan_tmp" ] &&
        [ "$(LC_ALL=C ls -ldn "$scan_tmp" | awk '{print $1 ":" $3}')" = "drwx------:$(id -u)" ] || return 1
    find "$scan_root" -print0 > "$scan_tmp/raw" || return 1
    [ -s "$scan_tmp/raw" ] || return 1
    LC_ALL=C tr -d '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_./\000-' \
        < "$scan_tmp/raw" > "$scan_tmp/bad" || return 1
    [ ! -s "$scan_tmp/bad" ] || return 1
    tr '\000' '\n' < "$scan_tmp/raw" > "$scan_tmp/paths" || return 1
    : > "$scan_tmp/files" || return 1
    scan_root_seen=0
    while :; do
        scan_entry=
        if ! IFS= read -r scan_entry; then
            [ -z "$scan_entry" ] || return 1
            break
        fi
        [ -n "$scan_entry" ] && [ ! -L "$scan_entry" ] || return 1
        case "$scan_entry" in
            "$scan_root")
                [ "$scan_root_seen" = 0 ] && [ -d "$scan_entry" ] || return 1
                scan_root_seen=1
                continue ;;
            "$scan_root"/*) scan_rel=${scan_entry#"$scan_root"/} ;;
            *) return 1 ;;
        esac
        is_safe_relative_path "$scan_rel" || return 1
        [ ! -d "$scan_entry" ] || continue
        [ -f "$scan_entry" ] || return 1
        printf '%s\n' "$scan_entry" >> "$scan_tmp/files" || return 1
    done < "$scan_tmp/paths"
    [ "$scan_root_seen" = 1 ] || return 1
    LC_ALL=C sort "$scan_tmp/files" > "$scan_tmp/sorted" || return 1
    [ ! -L "$scan_output" ] || return 1
    cat "$scan_tmp/sorted" > "$scan_output" || return 1
)

resolve_destination_path()
{
    if [ "$APP_VOLUME" = 1 ] && [ "$1" = usr/local/bin/test_lvgl ]; then
        UPDATE_DESTINATION=$APP_VOLUME_DIR/test_lvgl
    else
        UPDATE_DESTINATION=$ROOT_PREFIX/$1
    fi
}

destination_for()
{
    resolve_destination_path "$1"
    printf '%s\n' "$UPDATE_DESTINATION"
}

prepare_app_storage()
{
    storage_helper=${APP_STORAGE_TOOL:-$ROOT_PREFIX/usr/local/bin/un260_app_storage}
    if [ -e "$storage_helper" ]; then
        if [ -n "$DISPATCH_CONTEXT" ]; then storage_prepare_mode=--prepare-storage;
        else safe_destination usr/local/bin/un260_app_storage || return 1; storage_prepare_mode=--prepare; fi
        sh "$storage_helper" "$storage_prepare_mode" >> "$LOG" 2>&1 || return 1
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
    resolve_destination_path "$1"
    check_path=$UPDATE_DESTINATION
    [ ! -L "$check_path" ] || return 1
    [ ! -e "$check_path" ] || [ -f "$check_path" ] || return 1
    check_path=${check_path%/*}
    [ -n "$check_path" ] || check_path=/
    check_root=${ROOT_PREFIX:-/}
    if [ "$APP_VOLUME" = 1 ] && [ "$1" = usr/local/bin/test_lvgl ]; then check_root=/; fi
    while [ "$check_path" != "$check_root" ]; do
        [ ! -L "$check_path" ] || return 1
        [ ! -e "$check_path" ] || [ -d "$check_path" ] || return 1
        [ "$check_path" != / ] || return 1
        check_path=${check_path%/*}
        [ -n "$check_path" ] || check_path=/
    done
}


# The bootstrap reads only boot-active and complete self-contained versions.
# Boot activation is outside the per-file plan; old active remains usable while
# ordinary UI binaries are incomplete or their rollback has not yet run.
boot_hex64()
{
    [ "${#1}" = 64 ] || return 1
    case "$1" in *[!0123456789abcdef]*) return 1 ;; esac
}

boot_manifest_value()
{
    awk -F= -v key="$2" '$1 == key { if (NF != 2) exit 1; value=$2; count++ }
        END { if (count != 1) exit 1; print value }' "$1"
}

boot_safe_file()
{
    boot_safe_root=${1%/}
    boot_safe_path=$boot_safe_root/$2
    [ -f "$boot_safe_path" ] && [ ! -L "$boot_safe_path" ] || return 1
    boot_safe_path=$(dirname "$boot_safe_path")
    while :; do
        [ -d "$boot_safe_path" ] && [ ! -L "$boot_safe_path" ] || return 1
        [ "$boot_safe_path" != "${boot_safe_root:-/}" ] || return 0
        [ "$boot_safe_path" != / ] || return 1
        boot_safe_path=$(dirname "$boot_safe_path")
    done
}

boot_safe_parent()
{
    boot_parent_root=${1%/}
    boot_parent_path=$(dirname "$boot_parent_root/$2")
    while :; do
        [ ! -L "$boot_parent_path" ] || return 1
        [ ! -e "$boot_parent_path" ] || [ -d "$boot_parent_path" ] || return 1
        [ "$boot_parent_path" != "${boot_parent_root:-/}" ] || return 0
        [ "$boot_parent_path" != / ] || return 1
        boot_parent_path=$(dirname "$boot_parent_path")
    done
}

boot_transport_install_modes()
{
    awk -F '|' -v base="$BOOT_BUNDLE_TREE/$1/" '
        BEGIN { player=base "player"; theme=base "theme.pack"; manifest=base "manifest" }
        $3==player || $3==theme || $3==manifest {
            expected=($3==player ? "0755" : "0644")
            if (NF!=3 || $1!="file" || $2!=expected) bad=1
            count[$3]++
        }
        END { exit (bad || count[player]!=1 || count[theme]!=1 || count[manifest]!=1) }
    ' "$STAGE_DIR/install.tsv"
}

boot_check_transport_bundle()
(
    # Unpacked files stay non-executable. FAT/noexec transport cannot express
    # installation modes; execute only a verified copy on private local storage.
    umask 077
    boot_check_root=$(mktemp -d /tmp/un260-boot-check.XXXXXX) || {
        echo 'Boot transport check: cannot create private verification directory' >> "$LOG"
        return 1
    }
    case "$boot_check_root" in /tmp/un260-boot-check.??????) ;; *) return 1 ;; esac
    trap 'boot_check_rc=$?; trap - 0; if ! rm -rf "$boot_check_root"; then exit 1; fi; exit "$boot_check_rc"' 0
    trap 'exit 1' HUP INT TERM
    [ -d "$boot_check_root" ] && [ ! -L "$boot_check_root" ] &&
        [ "$(LC_ALL=C ls -ld "$boot_check_root" | awk '{print $1}')" = drwx------ ] &&
        [ "$(LC_ALL=C ls -nd "$boot_check_root" | awk '{print $3}')" = "$(id -u)" ] || return 1
    boot_check_source=${1%/}/$BOOT_BUNDLE_TREE/$2
    boot_check_destination=$boot_check_root/$BOOT_BUNDLE_TREE/$2
    mkdir -p "$boot_check_destination" || return 1
    for boot_check_name in manifest player theme.pack; do
        case "$boot_check_name" in manifest) boot_check_blocks=1 ;; *) boot_check_blocks=1024 ;; esac
        dd if="$boot_check_source/$boot_check_name" of="$boot_check_destination/$boot_check_name" \
            bs=2048 count="$boot_check_blocks" 2>> "$LOG" || return 1
    done
    chmod 0755 "$boot_check_destination/player" &&
        chmod 0644 "$boot_check_destination/theme.pack" "$boot_check_destination/manifest" || return 1
    echo 'Boot transport check: verify copied bytes and run private player' >> "$LOG"
    # Reuse all content, real-mode and native theme checks on the private copy.
    boot_validate_bundle "$boot_check_root" "$2" "$3"
)

# Use directory metadata before reading bytes. The target BusyBox has ls -ldn,
# including on noexec FAT transports. An old FIFO or oversized file is never opened.
boot_member_size()
{
    LC_ALL=C ls -ldn "$1" | awk -v mode="$2" -v limit="$3" \
        -v scope="$boot_validation_scope" -v owner="$boot_member_owner" '
        NR==1 && $1~/^-/ && (scope=="transport" || $1==mode) &&
        (owner=="" || ($2==1 && $3==owner)) &&
        $5~/^[0-9]+$/ && $5+0>0 && $5+0<=limit {print $5; ok=1}
        END {exit !(NR==1 && ok)}'
}

boot_id_for_hashes()
{
    printf 'player_sha256=%s\ntheme_sha256=%s\nmode=%s\n' "$1" "$2" "$3" | sha256sum | awk '{print $1}'
}

boot_validate_bundle()
{
    boot_validation_scope=${4:-installed}
    boot_tree=${1%/}
    boot_id=$2
    boot_expected_manifest=$3
    boot_hex64 "$boot_id" || return 1
    boot_member_owner=
    case "$boot_validation_scope" in
        installed|transport) boot_hex64 "$boot_expected_manifest" || return 1 ;;
        retired|retired-partial) boot_member_owner=$(id -u) || return 1 ;;
        *) return 1 ;;
    esac
    boot_dir=$boot_tree/$BOOT_BUNDLE_TREE/$boot_id
    for boot_entry in "$boot_dir"/* "$boot_dir"/.[!.]* "$boot_dir"/..?*; do
        [ -e "$boot_entry" ] || [ -L "$boot_entry" ] || continue
        case "${boot_entry##*/}" in manifest|player|theme.pack) ;; *) return 1 ;; esac
    done
    boot_safe_file "${boot_tree:-/}" "$BOOT_BUNDLE_TREE/$boot_id/manifest" &&
        boot_member_size "$boot_dir/manifest" -rw-r--r-- 2048 >/dev/null &&
        [ "$(wc -l < "$boot_dir/manifest")" = 8 ] || return 1
    if [ -z "$boot_member_owner" ]; then
        [ "$(checked_digest "$boot_dir/manifest")" = "$boot_expected_manifest" ] || return 1
    fi
    [ "$(boot_manifest_value "$boot_dir/manifest" schema)" = un260-boot-bundle-v1 ] &&
        [ "$(boot_manifest_value "$boot_dir/manifest" id)" = "$boot_id" ] &&
        [ "$(boot_manifest_value "$boot_dir/manifest" protocol)" = 1 ] || return 1
    boot_player_hash=$(boot_manifest_value "$boot_dir/manifest" player_sha256) &&
        boot_theme_hash=$(boot_manifest_value "$boot_dir/manifest" theme_sha256) &&
        boot_player_bytes=$(boot_manifest_value "$boot_dir/manifest" player_bytes) &&
        boot_theme_bytes=$(boot_manifest_value "$boot_dir/manifest" theme_bytes) &&
        boot_mode=$(boot_manifest_value "$boot_dir/manifest" mode) || return 1
    boot_hex64 "$boot_player_hash" && boot_hex64 "$boot_theme_hash" || return 1
    case "$boot_player_bytes:$boot_theme_bytes" in ''|*[!0-9:]*) return 1 ;; esac
    case "$boot_mode" in animation|logo) ;; *) return 1 ;; esac
    [ -n "$boot_player_bytes" ] && [ -n "$boot_theme_bytes" ] &&
        [ "$boot_player_bytes" -gt 0 ] && [ "$boot_player_bytes" -le 2097152 ] &&
        [ "$boot_theme_bytes" -gt 0 ] && [ "$boot_theme_bytes" -le 2097152 ] || return 1
    [ "$(boot_id_for_hashes "$boot_player_hash" "$boot_theme_hash" "$boot_mode")" = "$boot_id" ] || return 1
    for boot_name in player theme.pack; do
        boot_member=$boot_dir/$boot_name
        if [ "$boot_validation_scope" = retired-partial ] && [ ! -e "$boot_member" ] && [ ! -L "$boot_member" ]; then
            continue
        fi
        case "$boot_name" in
            player) boot_bytes=$boot_player_bytes; boot_hash=$boot_player_hash; boot_permissions=-rwxr-xr-x ;;
            theme.pack) boot_bytes=$boot_theme_bytes; boot_hash=$boot_theme_hash; boot_permissions=-rw-r--r-- ;;
        esac
        boot_safe_file "${boot_tree:-/}" "$BOOT_BUNDLE_TREE/$boot_id/$boot_name" &&
            [ "$(boot_member_size "$boot_member" "$boot_permissions" 2097152)" = "$boot_bytes" ] &&
            [ "$(checked_digest "$boot_member")" = "$boot_hash" ] || return 1
    done
    # An inactive garbage-collection candidate has no runtime role. Its
    # content address proves the owned bytes without executing a retired binary.
    [ -z "$boot_member_owner" ] || return 0
    if [ "$boot_validation_scope" = transport ]; then
        boot_transport_install_modes "$boot_id" || {
            echo 'Boot transport check: invalid or incomplete install mode contract' >> "$LOG"
            return 1
        }
        boot_check_transport_bundle "$boot_tree" "$boot_id" "$boot_expected_manifest"
        return $?
    fi
    "$boot_dir/player" --theme "$boot_dir/theme.pack" --mode "$boot_mode" --check >> "$LOG" 2>&1
}

boot_read_descriptor()
{
    [ -f "$1" ] && [ ! -L "$1" ] && [ "$(wc -l < "$1")" = 5 ] &&
        [ "$(wc -c < "$1")" -le 400 ] || return 1
    [ "$(sed -n '1p' "$1")" = UN260_BOOT_ACTIVE_V1 ] || return 1
    boot_current=$(sed -n '2p' "$1")
    boot_current_hash=$(sed -n '3p' "$1")
    boot_previous=$(sed -n '4p' "$1")
    boot_previous_hash=$(sed -n '5p' "$1")
    boot_hex64 "$boot_current" && boot_hex64 "$boot_current_hash" || return 1
    if [ "$boot_previous" = none ]; then
        [ "$boot_previous_hash" = none ] || return 1
    else
        boot_hex64 "$boot_previous" && boot_hex64 "$boot_previous_hash" &&
            [ "$boot_previous" != "$boot_current" ] || return 1
    fi
}

boot_package_request()
{
    BOOT_REQUEST_ID=""
    BOOT_REQUEST_HASH=""
    if [ -f "$STAGE_DIR/manifest.ini" ]; then
        BOOT_REQUEST_ID=$(boot_manifest_value "$STAGE_DIR/manifest.ini" boot_bundle_id)
        BOOT_REQUEST_HASH=$(boot_manifest_value "$STAGE_DIR/manifest.ini" boot_manifest_sha256)
    fi
    if [ -z "$BOOT_REQUEST_ID" ] && [ -z "$BOOT_REQUEST_HASH" ]; then
        if grep -F "$BOOT_BUNDLE_TREE/" "$SEEN_TARGETS" >/dev/null; then return 1; fi
        return 0
    fi
    boot_hex64 "$BOOT_REQUEST_ID" && boot_hex64 "$BOOT_REQUEST_HASH" || return 1
    while IFS= read -r boot_planned_target; do
        case "$boot_planned_target" in
            "$BOOT_BUNDLE_TREE"/*)
                case "$boot_planned_target" in "$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID/manifest"|"$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID/player"|"$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID/theme.pack") ;; *) return 1 ;; esac ;;
        esac
    done < "$SEEN_TARGETS"
    if [ -f "$STAGE_DIR/payload/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID/manifest" ]; then
        boot_validate_bundle "$STAGE_DIR/payload" "$BOOT_REQUEST_ID" "$BOOT_REQUEST_HASH" transport || return 1
    else
        return 1
    fi
    ROOT_PEAK_REQUIRED_KB=$((ROOT_PEAK_REQUIRED_KB + 8))
    # A content-addressed version may be reused, never changed in place.
    if [ -e "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID" ]; then
        [ -d "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID" ] &&
            [ ! -L "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID" ] || return 1
        boot_existing_empty=1
        for boot_existing_entry in "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID"/* "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID"/.[!.]* "$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$BOOT_REQUEST_ID"/..?*; do
            [ -e "$boot_existing_entry" ] || [ -L "$boot_existing_entry" ] || continue
            boot_existing_empty=0
        done
        [ "$boot_existing_empty" = 1 ] || boot_validate_bundle "${ROOT_PREFIX:-/}" "$BOOT_REQUEST_ID" "$BOOT_REQUEST_HASH" || return 1
    fi
    if [ -e "$ROOT_PREFIX/$BOOT_ACTIVE" ] || [ -L "$ROOT_PREFIX/$BOOT_ACTIVE" ]; then
        boot_safe_file "${ROOT_PREFIX:-/}" "$BOOT_ACTIVE" &&
            boot_read_descriptor "$ROOT_PREFIX/$BOOT_ACTIVE" || return 1
        BOOT_OLD_ID=$boot_current
        BOOT_OLD_HASH=$boot_current_hash
        BOOT_OLD_PREVIOUS=$boot_previous
        BOOT_OLD_PREVIOUS_HASH=$boot_previous_hash
        boot_validate_bundle "${ROOT_PREFIX:-/}" "$BOOT_OLD_ID" "$BOOT_OLD_HASH" || return 1
    else
        [ ! -e "$ROOT_PREFIX/$BOOT_ACTIVE.previous" ] && [ ! -L "$ROOT_PREFIX/$BOOT_ACTIVE.previous" ] || return 1
        BOOT_OLD_ID=none
        BOOT_OLD_HASH=none
        BOOT_OLD_PREVIOUS=none
        BOOT_OLD_PREVIOUS_HASH=none
    fi
}

boot_prepare_publication()
{
    [ -n "$BOOT_REQUEST_ID" ] || return 0
    boot_validate_bundle "${ROOT_PREFIX:-/}" "$BOOT_REQUEST_ID" "$BOOT_REQUEST_HASH" || return 1
    [ ! -L "$JOURNAL/boot-publish" ] && [ ! -L "$JOURNAL/boot-publish.tmp" ] || return 1
    boot_publish_previous=$BOOT_OLD_ID
    boot_publish_previous_hash=$BOOT_OLD_HASH
    if [ "$BOOT_REQUEST_ID" = "$BOOT_OLD_ID" ]; then
        boot_publish_previous=$BOOT_OLD_PREVIOUS
        boot_publish_previous_hash=$BOOT_OLD_PREVIOUS_HASH
    fi
    printf 'UN260_BOOT_ACTIVE_V1\n%s\n%s\n%s\n%s\n' "$BOOT_REQUEST_ID" "$BOOT_REQUEST_HASH" \
        "$boot_publish_previous" "$boot_publish_previous_hash" > "$JOURNAL/boot-publish.tmp" &&
        local_barrier && mv -f "$JOURNAL/boot-publish.tmp" "$JOURNAL/boot-publish" && local_barrier
}


boot_gc_safe_dir()
{
    boot_hex64 "$2" && boot_safe_parent "${ROOT_PREFIX:-/}" "$BOOT_BUNDLE_TREE/$2/manifest" &&
        [ -d "$1" ] && [ ! -L "$1" ] &&
        [ "$(LC_ALL=C ls -ldn "$1" | awk '{print $3}')" = "$(id -u)" ]
}

# A new plan may retire only an empty version or a complete verified bundle.
boot_gc_candidate_valid()
{
    boot_gc_safe_dir "$1" "$2" || return 1
    for boot_gc_entry in "$1"/* "$1"/.[!.]* "$1"/..?*; do
        [ -e "$boot_gc_entry" ] || [ -L "$boot_gc_entry" ] || continue
        boot_validate_bundle "${ROOT_PREFIX:-/}" "$2" "" retired
        return $?
    done
    return 0
}

# A persisted plan can be replayed after only part of a version was removed.
# Never unlink anything in a directory that now contains an unexpected entry.
boot_gc_replay_safe()
{
    boot_gc_safe_dir "$1" "$2" || return 1
    boot_gc_remaining=0
    for boot_gc_entry in "$1"/* "$1"/.[!.]* "$1"/..?*; do
        [ -e "$boot_gc_entry" ] || [ -L "$boot_gc_entry" ] || continue
        case "${boot_gc_entry##*/}" in manifest|player|theme.pack) ;; *) return 1 ;; esac
        [ -f "$boot_gc_entry" ] && [ ! -L "$boot_gc_entry" ] || return 1
        boot_gc_remaining=$((boot_gc_remaining + 1))
    done
    [ "$boot_gc_remaining" != 0 ] || return 0
    if [ -e "$1/manifest" ]; then
        boot_validate_bundle "${ROOT_PREFIX:-/}" "$2" "" retired-partial
        return $?
    fi
    # Older engines removed the manifest first. With both payloads the
    # directory ID still binds their bytes. A lone legacy member is kept.
    [ "$boot_gc_remaining" = 2 ] || return 2
    boot_validation_scope=retired
    boot_member_owner=$(id -u) || return 1
    boot_member_size "$1/player" -rwxr-xr-x 2097152 >/dev/null &&
        boot_member_size "$1/theme.pack" -rw-r--r-- 2097152 >/dev/null || return 1
    boot_gc_player=$(checked_digest "$1/player") && boot_gc_theme=$(checked_digest "$1/theme.pack") || return 1
    [ "$(boot_id_for_hashes "$boot_gc_player" "$boot_gc_theme" animation)" = "$2" ] ||
        [ "$(boot_id_for_hashes "$boot_gc_player" "$boot_gc_theme" logo)" = "$2" ]
}

# Preserve the active/previous versions and the immutable snapshot actually
# used by this cold boot. Delete only a durably journalled, prevalidated plan.
boot_collect_versions()
{
    boot_read_descriptor "$ROOT_PREFIX/$BOOT_ACTIVE" || return 1
    boot_gc_current=$boot_current
    boot_gc_previous=$boot_previous
    boot_gc_live=none
    if [ -e "$ROOT_PREFIX/dev/.un260-boot-bundle" ] || [ -L "$ROOT_PREFIX/dev/.un260-boot-bundle" ]; then
        boot_safe_file "${ROOT_PREFIX:-/}" dev/.un260-boot-bundle &&
            boot_read_descriptor "$ROOT_PREFIX/dev/.un260-boot-bundle" || return 1
        [ "$boot_previous" = none ] && [ "$boot_previous_hash" = none ] || return 1
        boot_gc_live=$boot_current
        boot_validate_bundle "${ROOT_PREFIX:-/}" "$boot_current" "$boot_current_hash" || return 1
    fi
    [ ! -L "$JOURNAL/boot-gc" ] && [ ! -L "$JOURNAL/boot-gc.tmp" ] || return 1
    if [ ! -e "$JOURNAL/boot-gc" ]; then
        printf 'UN260_BOOT_GC_V1\n' > "$JOURNAL/boot-gc.tmp" || return 1
        for boot_gc_dir in "$ROOT_PREFIX/$BOOT_BUNDLE_TREE"/*; do
            [ -e "$boot_gc_dir" ] || [ -L "$boot_gc_dir" ] || continue
            boot_gc_id=${boot_gc_dir##*/}
            if boot_hex64 "$boot_gc_id"; then
                case "$boot_gc_id" in "$boot_gc_current"|"$boot_gc_previous"|"$boot_gc_live")
                    [ -d "$boot_gc_dir" ] && [ ! -L "$boot_gc_dir" ] || return 1
                    continue ;;
                esac
            fi
            if ! boot_gc_candidate_valid "$boot_gc_dir" "$boot_gc_id"; then
                [ "${1:-}" = confirmed ] || return 1
                echo "Keeping unverified retired boot entry: $boot_gc_id" >> "$LOG" || :
                continue
            fi
            printf '%s\n' "$boot_gc_id" >> "$JOURNAL/boot-gc.tmp" || return 1
        done
        local_barrier && mv -f "$JOURNAL/boot-gc.tmp" "$JOURNAL/boot-gc" && local_barrier || return 1
    fi
    [ -f "$JOURNAL/boot-gc" ] && [ "$(wc -c < "$JOURNAL/boot-gc")" -le 20000 ] &&
        [ "$(sed -n '1p' "$JOURNAL/boot-gc")" = UN260_BOOT_GC_V1 ] || return 1
    while IFS= read -r boot_gc_id; do
        [ "$boot_gc_id" != UN260_BOOT_GC_V1 ] || continue
        boot_hex64 "$boot_gc_id" || return 1
        case "$boot_gc_id" in "$boot_gc_current"|"$boot_gc_previous"|"$boot_gc_live") continue ;; esac
        boot_gc_dir=$ROOT_PREFIX/$BOOT_BUNDLE_TREE/$boot_gc_id
        [ -e "$boot_gc_dir" ] || [ -L "$boot_gc_dir" ] || continue
        boot_gc_replay_safe "$boot_gc_dir" "$boot_gc_id" || {
            boot_gc_rc=$?
            [ "$boot_gc_rc" = 2 ] || [ "${1:-}" = confirmed ] || return 1
            echo "Keeping changed retired boot entry: $boot_gc_id" >> "$LOG" || :
            continue
        }
        rm -f "$boot_gc_dir/player" "$boot_gc_dir/theme.pack" "$boot_gc_dir/manifest" && rmdir "$boot_gc_dir" || return 1
        local_barrier || return 1
    done < "$JOURNAL/boot-gc"
    rm -f "$JOURNAL/boot-gc" "$JOURNAL/boot-gc.tmp" && local_barrier
}

boot_publish_committed()
{
    [ -e "$JOURNAL/boot-publish" ] || return 0
    [ -f "$JOURNAL/committed" ] && [ ! -L "$JOURNAL/committed" ] &&
        boot_read_descriptor "$JOURNAL/boot-publish" || return 1
    boot_publish_id=$boot_current
    boot_publish_hash=$boot_current_hash
    boot_expected_previous=$boot_previous
    boot_expected_previous_hash=$boot_previous_hash
    boot_validate_bundle "${ROOT_PREFIX:-/}" "$boot_publish_id" "$boot_publish_hash" || return 1
    boot_publish_target=$ROOT_PREFIX/$BOOT_ACTIVE
    boot_already_published=0
    # Do not put active into install.tsv: it becomes visible only after every
    # application target and the self-contained bundle have committed durably.
    boot_safe_parent "${ROOT_PREFIX:-/}" "$BOOT_ACTIVE" && mkdir -p "$(dirname "$boot_publish_target")" || return 1
    [ ! -L "$boot_publish_target" ] && [ ! -L "$boot_publish_target.tmp" ] &&
        [ ! -L "$boot_publish_target.previous" ] && [ ! -L "$boot_publish_target.previous.tmp" ] || return 1
    if [ -e "$boot_publish_target" ]; then
        boot_safe_file "${ROOT_PREFIX:-/}" "$BOOT_ACTIVE" && boot_read_descriptor "$boot_publish_target" || return 1
        boot_backup_id=$boot_current
        boot_backup_hash=$boot_current_hash
        if [ "$boot_backup_id" != "$boot_publish_id" ] || [ "$boot_backup_hash" != "$boot_publish_hash" ]; then
            boot_validate_bundle "${ROOT_PREFIX:-/}" "$boot_backup_id" "$boot_backup_hash" || return 1
            [ "$boot_expected_previous" = "$boot_backup_id" ] && [ "$boot_expected_previous_hash" = "$boot_backup_hash" ] || return 1
            printf 'UN260_BOOT_ACTIVE_V1\n%s\n%s\nnone\nnone\n' "$boot_backup_id" "$boot_backup_hash" > "$boot_publish_target.previous.tmp" &&
                chmod 0644 "$boot_publish_target.previous.tmp" && local_barrier &&
                mv -f "$boot_publish_target.previous.tmp" "$boot_publish_target.previous" && local_barrier || return 1
        else
            # The identical content ID and manifest above already passed the player check.
            boot_record_hash=$(checked_digest "$JOURNAL/boot-publish") || return 1
            [ "$(checked_digest "$boot_publish_target")" = "$boot_record_hash" ] || return 1
            boot_active_stat=$(LC_ALL=C ls -ldn "$boot_publish_target" | awk '{print $1 ":" $2 ":" $3 ":" $4}')
            if [ ! -e "$boot_publish_target.tmp" ] &&
               [ "$boot_active_stat" = "-rw-r--r--:1:$(id -u):$(id -g)" ]; then
                boot_already_published=1
            fi
        fi
    elif [ -e "$boot_publish_target.previous" ] || [ "$boot_expected_previous" != none ] || [ "$boot_expected_previous_hash" != none ]; then
        return 1
    fi
    if [ "$boot_already_published" = 0 ]; then
        cp "$JOURNAL/boot-publish" "$boot_publish_target.tmp" && chmod 0644 "$boot_publish_target.tmp" &&
            local_barrier && mv -f "$boot_publish_target.tmp" "$boot_publish_target" || return 1
    fi
    # Even exact replay may follow a visible rename whose last writeback failed.
    local_barrier || return 1
    echo "Boot bundle published: $boot_publish_id" >> "$LOG"
    if [ "$TRIAL_ENABLED" != 1 ] ||
       { [ ! -e "$JOURNAL/trial" ] && [ ! -e "$JOURNAL/trial.confirmed" ]; }; then
        boot_collect_versions || return 1
    fi
    BOOT_PUBLICATION_DONE=1
}

cleanup_usb_work()
{
    [ "$WORK_OWNED" -eq 1 ] || return 0
    [ "$USB_FAILED" = 0 ] || return 1
    # Only this invocation's staging files, never the user's package or logs.
    [ "$GUARDED" = 0 ] || usb_barrier || return 1
    rm -rf "$STAGE_DIR" "$BACKUP_DIR" || return 1
    rm -f "$SEEN_TARGETS" \
        "$CHECKSUM_TARGETS" "$PAYLOAD_FILES" "$PLAN_FILE" "$PLAN_FILE.tree" "$PLAN_FILE.retired" "$PLAN_FILE.verify" \
        "$PLAN_FILE.requests" "$PLAN_FILE.inventory" "$PLAN_FILE.retired.pending" \
        "$PLAN_FILE.actual" "$PLAN_FILE.scan"
}

# Recover original inode links without copying a large executable into a full
# rootfs. The local journal permits recovery without the USB drive. This is a
# recoverable file transaction, not a partition-level A/B firmware design.
recover_transaction()
{
    RECOVERED_TRIAL_PENDING=0
    [ ! -L "$JOURNAL" ] || return 1
    [ -e "$JOURNAL" ] || return 0
    [ -d "$JOURNAL" ] || return 1
    if [ ! -e "$JOURNAL/state" ] && [ ! -L "$JOURNAL/state" ]; then
        # mkdir may have reached flash before the first state-file creation.
        # rmdir succeeds only for an empty directory; preserve unknown contents.
        rmdir "$JOURNAL" || return 1
        local_barrier
        return $?
    fi
    [ -f "$JOURNAL/state" ] && [ ! -L "$JOURNAL/state" ] || return 1
    for journal_control in committed rolled-back boot-publish boot-publish.tmp boot-gc boot-gc.tmp; do
        [ ! -L "$JOURNAL/$journal_control" ] || return 1
        [ ! -e "$JOURNAL/$journal_control" ] || [ -f "$JOURNAL/$journal_control" ] || return 1
    done
    if [ "$TRIAL_ENABLED" = 1 ]; then
        app_check_trace journal_begin
        trial_recover_prepare || return 1
        app_check_trace journal_complete
    fi
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
    if [ -f "$JOURNAL/committed" ] && [ "$BOOT_PUBLICATION_DONE" = 0 ]; then
        app_check_trace boot_publish_begin
        boot_publish_committed || return 1
        app_check_trace boot_publish_complete
    fi
    # trial_recover_prepare checked the journal, decision, target table and snapshots
    # under this lock. Publication does not modify a committed pending trial.
    if [ "$TRIAL_ENABLED" = 1 ] && [ "$trial_kind" = trial ] && [ -f "$JOURNAL/committed" ]; then
        RECOVERED_TRIAL_PENDING=1
        return 0
    fi
    if [ "$TRIAL_ENABLED" = 1 ] && [ "$trial_kind" = trial.confirmed ] && [ "$trial_boot_id" != none ]; then
        # Activation/confirmation is mandatory. Retirement of unrelated old versions is not.
        if [ "$BOOT_PUBLICATION_DONE" = 1 ]; then
            [ "$boot_publish_id" = "$trial_boot_id" ] && [ "$boot_publish_hash" = "$trial_boot_sha" ] || return 1
        else
            trial_active_boot_valid || return 1
        fi
        if ! boot_collect_versions confirmed; then
            echo 'Old boot version cleanup incomplete; preserving remaining versions' >> "$LOG" || :
        fi
    fi
    while IFS='|' read -r present rpath extra; do
        [ -z "$extra" ] && safe_destination "$rpath" || return 1
        rdest=$(destination_for "$rpath")
        rm -f "$rdest.un260-old" "$rdest.un260-new" "$rdest.un260-restore" || return 1
    done < "$JOURNAL/state"
    local_barrier || return 1
    rm -f "$JOURNAL/boot-publish.tmp" "$JOURNAL/boot-publish" "$JOURNAL/boot-gc.tmp" "$JOURNAL/boot-gc" || return 1
    local_barrier || return 1
    if [ "$TRIAL_ENABLED" = 1 ]; then trial_cleanup_files || return 1; fi
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

validate_payload_checksums()
{
    checksum_count=0
    CHECKSUM_MANIFEST_HASH=$(checked_digest "$STAGE_DIR/checksums.sha256") || fail_update "Invalid checksum manifest"
    : > "$CHECKSUM_TARGETS"

    while read -r checksum checksum_path extra; do
        [ -n "$checksum" ] || continue
        [ -z "$extra" ] || fail_update "Invalid checksum manifest"
        boot_hex64 "$checksum" ||
            fail_update "Checksum manifest contains an invalid checksum"
        is_safe_relative_path "$checksum_path" ||
            fail_update "Checksum manifest contains an unsafe path"
        case "$checksum_path" in
            payload/*) ;;
            *) fail_update "Checksum manifest references an invalid file" ;;
        esac
        [ -f "$STAGE_DIR/$checksum_path" ] ||
            fail_update "Checksum manifest references a missing file"
        echo "$checksum_path" >> "$CHECKSUM_TARGETS"
        checksum_count=$((checksum_count + 1))
    done < "$STAGE_DIR/checksums.sha256"

    [ "$checksum_count" -gt 0 ] || [ "$package_schema" = 2 ] || fail_update "Checksum manifest is empty"

    (cd "$STAGE_DIR" && scan_regular_tree payload "$PAYLOAD_FILES") ||
        fail_update "Unable to enumerate upgrade payload"
    while IFS= read -r payload_path; do
        [ -n "$payload_path" ] || continue
        is_safe_relative_path "$payload_path" ||
            fail_update "Upgrade payload contains an unsafe path"
    done < "$PAYLOAD_FILES"

    if LC_ALL=C awk '
        FILENAME==ARGV[1] {if(seen[$0]++) duplicate=1; n++; next}
        {if(!($0 in seen)) unchecked=1; m++}
        END {exit (duplicate ? 2 : unchecked ? 3 : n!=m ? 1 : 0)}' "$CHECKSUM_TARGETS" "$PAYLOAD_FILES"; then :;
    else
        case "$?" in
            2) fail_update "Checksum manifest contains duplicate files" ;;
            3) fail_update "Upgrade payload contains an unchecked file" ;;
            *) fail_update "Upgrade checksum manifest does not match payload" ;;
        esac
    fi

    payload_count=$(wc -l < "$PAYLOAD_FILES" | tr -d ' ')
    [ "$payload_count" = "$checksum_count" ] ||
        fail_update "Upgrade checksum manifest does not match payload"

    [ "$checksum_count" = 0 ] || (cd "$STAGE_DIR" && sha256sum -c checksums.sha256) >> "$LOG" 2>&1 ||
        fail_update "Upgrade payload checksum verification failed"
    [ "$(checked_digest "$STAGE_DIR/checksums.sha256")" = "$CHECKSUM_MANIFEST_HASH" ] ||
        fail_update "Upgrade checksum manifest does not match payload"
    PAYLOAD_VERIFIED=1
}

request_install_file()
{
    if [ "$FULL_PLAN_COLLECT" = 1 ]; then
        FULL_PLAN_REQUEST_COUNT=$((FULL_PLAN_REQUEST_COUNT+1))
        [ "$FULL_PLAN_REQUEST_COUNT" -le "$checksum_count" ] ||
            fail_update 'Full install manifest does not cover its complete payload'
        printf '%s|%s\n' "$2" "$1" >> "$PLAN_FILE.requests" || fail_update 'Full target inventory differs from payload'
    else
        plan_file "$1" "$2"
    fi
}

file_permissions()
{
    permissions_record=$(LC_ALL=C ls -ld "$1") || return 1
    printf '%s\n' "${permissions_record%% *}"
}

full_inventory_valid()
{
    [ -f "$PLAN_FILE.inventory" ] && [ ! -L "$PLAN_FILE.inventory" ] &&
        [ "$(checked_digest "$PLAN_FILE.inventory")" = "$FULL_INVENTORY_HASH" ] &&
        [ "$(checked_digest "$STAGE_DIR/target.tsv")" = "$FULL_TARGET_ID" ] || return 1
    LC_ALL=C awk -F '|' -v count="$checksum_count" '
        FILENAME==ARGV[1] {expected[$4]=$2 "|" $1 "|" $3; next}
        {if(NF!=4 || used[$2]++ || !($2 in expected) || expected[$2]!=$1 "|" $3 "|" $4) bad=1; n++}
        END {exit (bad || n!=count)}' "$STAGE_DIR/target.tsv" "$PLAN_FILE.inventory" || return 1
    [ "$(checked_digest "$STAGE_DIR/target.tsv")" = "$FULL_TARGET_ID" ] &&
        [ "$(checked_digest "$PLAN_FILE.inventory")" = "$FULL_INVENTORY_HASH" ]
}

full_target_matches_checksums()
{
    LC_ALL=C awk '
        FILENAME==ARGV[1] {hash[substr($2,9)]=$1; next}
        {n=split($0,row,"|"); if(n!=4 || !(row[4] in hash) || row[1]!=hash[row[4]]) bad=1}
        END {exit bad}' "$STAGE_DIR/checksums.sha256" "$STAGE_DIR/target.tsv"
}

inventory_available()
{
    if [ "$INVENTORY_CAPABILITY" = unknown ]; then
        INVENTORY_CAPABILITY=0
        if [ -x "$SYNC_TOOL" ] && [ "$("$SYNC_TOOL" --probe-inventory 2>/dev/null)" = UN260_STORAGE_INVENTORY_1 ]; then
            INVENTORY_CAPABILITY=1
        fi
    fi
    [ "$INVENTORY_CAPABILITY" = 1 ]
}

prepare_native_inventory()
{
    # Actual bytes are read again, using the authenticated table. No persistent
    # hash cache or optimistic trust in an installed version marker is used.
    "$SYNC_TOOL" --verify-payload "$STAGE_DIR/payload" "$STAGE_DIR/target.tsv" >> "$LOG" 2>&1 ||
        fail_update 'Upgrade payload checksum verification failed'
    inventory_app=-
    [ "$APP_VOLUME" != 1 ] || inventory_app=$APP_VOLUME_DIR
    "$SYNC_TOOL" --inspect-inventory "${ROOT_PREFIX:-/}" "$inventory_app" "$STAGE_DIR/target.tsv" > "$PLAN_FILE.actual" 2>> "$LOG" ||
        fail_update 'Cannot read installed target'
    actual_inventory_hash=$(checked_digest "$PLAN_FILE.actual") || fail_update 'Cannot read installed target'
    LC_ALL=C awk -F '|' -v count="$checksum_count" '
        FILENAME==ARGV[1] {
            if(NF!=4 || seen[$4]++ || ($1!="-" && (length($1)!=64 || $1~/[^0-9a-f]/)) ||
                length($2)!=4 || $2~/[^0-7]/ || $3!~/^(0|[1-9][0-9]*)$/ ||
                ($1=="-" && ($2!="0000" || $3!="0"))) bad=1
            hash[$4]=$1; mode[$4]=$2; bytes[$4]=$3; n++; next
        }
        {if(NF!=4 || !($2 in seen) || used[$2]++) bad=1
            print $0 "|" hash[$2] "|" mode[$2] "|" bytes[$2]; m++}
        END {exit (bad || n!=count || m!=count)}
        ' "$PLAN_FILE.actual" "$PLAN_FILE.inventory" > "$PLAN_FILE.scan" ||
        fail_update 'Full target inventory differs from payload'
    [ "$(checked_digest "$PLAN_FILE.actual")" = "$actual_inventory_hash" ] ||
        fail_update 'Full target inventory differs from payload'
    FULL_SCAN_HASH=$(checked_digest "$PLAN_FILE.scan") || fail_update 'Full target inventory differs from payload'
    FULL_INVENTORY_FAST=1
    echo "Full inventory: native batch scan; targets=$checksum_count" >> "$LOG"
}

prepare_full_inventory()
{
    [ "$PAYLOAD_VERIFIED" = 1 ] &&
        [ "$(checked_digest "$STAGE_DIR/target.tsv")" = "$FULL_TARGET_ID" ] &&
        [ "$(checked_digest "$STAGE_DIR/checksums.sha256")" = "$CHECKSUM_MANIFEST_HASH" ] ||
        fail_update 'Full target inventory differs from payload'
    full_target_matches_checksums || fail_update 'Full target inventory differs from payload'
    # Join authenticated target metadata once, retaining install.tsv order.
    # Every target must be requested exactly once; this is data, never code.
    LC_ALL=C awk -F '|' -v count="$checksum_count" '
        FILENAME==ARGV[1] {
            if(NF!=4 || target[$4]++) bad=1
            hash[$4]=$1; mode[$4]=$2; bytes[$4]=$3; n++; next
        }
        {p=$2; if(NF!=2 || !(p in target) || used[p]++ || mode[p]!=$1) bad=1
            print $1 "|" p "|" hash[p] "|" bytes[p]; m++}
        END {exit (bad || n!=m || n!=count)}
        ' "$STAGE_DIR/target.tsv" "$PLAN_FILE.requests" > "$PLAN_FILE.inventory" ||
        fail_update 'Full install manifest does not cover its complete payload'
    [ "$(checked_digest "$STAGE_DIR/target.tsv")" = "$FULL_TARGET_ID" ] &&
        [ "$(checked_digest "$STAGE_DIR/checksums.sha256")" = "$CHECKSUM_MANIFEST_HASH" ] ||
        fail_update 'Full target inventory differs from payload'
    FULL_INVENTORY_HASH=$(checked_digest "$PLAN_FILE.inventory") || fail_update 'Full target inventory differs from payload'
    FULL_PLAN_COLLECT=0
    FULL_INVENTORY_READY=1
    full_inventory_valid || fail_update 'Full target inventory differs from payload'
    inventory_input=$PLAN_FILE.inventory
    if inventory_available; then
        prepare_native_inventory
        inventory_input=$PLAN_FILE.scan
    fi
    inventory_consumed=0
    while :; do
        inventory_mode= inventory_path= inventory_hash= inventory_bytes= inventory_extra=
        inventory_old_hash= inventory_old_mode= inventory_old_bytes=
        if ! IFS='|' read -r inventory_mode inventory_path inventory_hash inventory_bytes inventory_old_hash inventory_old_mode inventory_old_bytes inventory_extra; then
            [ -z "$inventory_mode$inventory_path$inventory_hash$inventory_bytes$inventory_old_hash$inventory_old_mode$inventory_old_bytes$inventory_extra" ] ||
                fail_update 'Full target inventory differs from payload'
            break
        fi
        [ -z "$inventory_extra" ] || fail_update 'Full target inventory differs from payload'
        if [ "$FULL_INVENTORY_FAST" != 1 ]; then
            [ -z "$inventory_old_hash$inventory_old_mode$inventory_old_bytes" ] || fail_update 'Full target inventory differs from payload'
        fi
        plan_file "$inventory_path" "$inventory_mode" "$inventory_hash" "$inventory_bytes" "$inventory_old_hash" "$inventory_old_mode" "$inventory_old_bytes"
        inventory_consumed=$((inventory_consumed+1))
    done < "$inventory_input" || fail_update 'Full target inventory differs from payload'
    [ "$inventory_consumed" = "$checksum_count" ] || fail_update 'Full target inventory differs from payload'
    if [ "$FULL_INVENTORY_FAST" = 1 ]; then
        [ "$(checked_digest "$PLAN_FILE.scan")" = "$FULL_SCAN_HASH" ] || fail_update 'Full target inventory differs from payload'
    fi
    full_inventory_valid || fail_update 'Full target inventory differs from payload'
}

plan_file()
{
    plan_rel=$1
    plan_mode=$2
    safe_destination "$plan_rel" || fail_update "Unsafe install destination: $plan_rel"
    if [ "$FULL_INVENTORY_READY" != 1 ] && grep -Fx "$plan_rel" "$SEEN_TARGETS" >/dev/null 2>&1; then
        fail_update "Duplicate or overlapping install target: $plan_rel"
    fi
    echo "$plan_rel" >> "$SEEN_TARGETS"
    plan_src=$STAGE_DIR/payload/$plan_rel
    resolve_destination_path "$plan_rel"
    plan_dest=$UPDATE_DESTINATION
    for suffix in old new restore; do
        [ ! -e "$plan_dest.un260-$suffix" ] && [ ! -L "$plan_dest.un260-$suffix" ] ||
            fail_update "Unrecovered install artifact: $plan_rel"
    done
    if [ "$FULL_INVENTORY_READY" = 1 ]; then
        plan_hash=$3
    elif [ "$PAYLOAD_VERIFIED" = 1 ]; then
        plan_hash=$(awk -v p="payload/$plan_rel" '$2 == p {print $1}' "$STAGE_DIR/checksums.sha256")
    else
        plan_hash=$(checked_digest "$plan_src") || fail_update "Cannot hash payload"
    fi
    case "$plan_hash" in ''|*[!0-9a-f]*) fail_update "Cannot hash payload" ;; esac
    if [ "$FULL_INVENTORY_FAST" = 1 ]; then
        plan_size=$4
    else
        plan_size=$(wc -c < "$plan_src") || fail_update "Cannot measure payload"
    fi
    case "$plan_size" in ''|*[!0-9]*) fail_update "Cannot measure payload" ;; esac
    if [ "$package_schema" = 1 ] && [ -n "${FULL_TARGET_ID:-}" ]; then
        [ "$FULL_INVENTORY_READY" = 1 ] && [ "$plan_size" = "$4" ] ||
            fail_update 'Full target inventory differs from installation plan'
    fi
    if [ -n "$DISPATCH_CONTEXT" ] && [ "$plan_rel" = etc/init.d/S00lvgl ]; then
        entry_hash=$(cat "$ROOT_PREFIX/usr/local/lib/un260-updater/rescue/startup-entry.sha256") || fail_update 'Factory startup entry is unavailable'
        boot_hex64 "$entry_hash" && [ "$plan_hash" = "$entry_hash" ] || fail_update 'Package would change the fixed startup entry'
    fi
    dest_hash=-
    if [ "$FULL_INVENTORY_FAST" = 1 ]; then
        dest_hash=$5
        case "$dest_hash" in
            -) [ ! -e "$plan_dest" ] || fail_update 'Cannot read installed target' ;;
            *)
                [ -f "$plan_dest" ] || fail_update 'Cannot read installed target'
                if [ "$plan_hash" = "$dest_hash" ] && [ "$plan_mode" = "$6" ]; then
                    SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
                    return 0
                fi
                old_bytes=$7
                BACKUP_REQUIRED_KB=$((BACKUP_REQUIRED_KB + old_bytes / 1024 + (old_bytes % 1024 != 0) + 4))
                ;;
        esac
    elif [ -f "$plan_dest" ]; then
        dest_hash=$(checked_digest "$plan_dest") || fail_update 'Cannot read installed target'
        # This board's minimal BusyBox does not ship stat/cmp applets.
        dest_mode=$(file_permissions "$plan_dest") || fail_update 'Cannot read installed target'
        case "$plan_mode" in 0755) expected_mode=-rwxr-xr-x ;; *) expected_mode=-rw-r--r-- ;; esac
        if [ "$plan_hash" = "$dest_hash" ] && [ "$expected_mode" = "$dest_mode" ]; then
            SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
            return 0
        fi
        old_bytes=$(wc -c < "$plan_dest" | tr -d ' ')
        BACKUP_REQUIRED_KB=$((BACKUP_REQUIRED_KB + old_bytes / 1024 + (old_bytes % 1024 != 0) + 4))
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
    PRUNE_STATIC_TREE=0
    manifest_count=0
    FULL_PLAN_COLLECT=0
    FULL_PLAN_REQUEST_COUNT=0
    FULL_INVENTORY_READY=0
    FULL_INVENTORY_HASH=
    FULL_INVENTORY_FAST=0
    FULL_SCAN_HASH=
    if [ "$package_schema" = 1 ] && [ -n "${FULL_TARGET_ID:-}" ]; then
        FULL_PLAN_COLLECT=1
        : > "$PLAN_FILE.requests" || fail_update 'Full target inventory differs from payload'
    fi
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
                request_install_file "$rel" "$mode"
                ;;
            tree)
                [ -d "$STAGE_DIR/payload/$rel" ] || fail_update "Missing payload tree"
                if [ "$rel" = "$OWNED_STATIC_TREE" ]; then PRUNE_STATIC_TREE=1; fi
                scan_regular_tree "$STAGE_DIR/payload/$rel" "$PLAN_FILE.tree" ||
                    fail_update "Cannot enumerate payload tree"
                while IFS= read -r tree_file; do
                    tree_rel=${tree_file#"$STAGE_DIR/payload/"}
                    request_install_file "$tree_rel" 0644
                done < "$PLAN_FILE.tree"
                ;;
            *) fail_update "Invalid install manifest entry type" ;;
        esac
        manifest_count=$((manifest_count + 1))
        [ "$manifest_count" -le 256 ] || fail_update "Too many manifest entries"
    done < "$STAGE_DIR/install.tsv"
    [ "$manifest_count" -gt 0 ] || fail_update "Empty install manifest"
    if [ "$package_schema" = 1 ] && [ -n "${FULL_TARGET_ID:-}" ]; then
        prepare_full_inventory
        [ "$(wc -l < "$SEEN_TARGETS" | tr -d ' ')" = "$checksum_count" ] || fail_update 'Full install manifest does not cover its complete payload'
    fi
    if [ "$PRUNE_STATIC_TREE" = 1 ]; then plan_retired_static_files; fi
    echo "Install plan: changed=$INSTALL_ENTRY_COUNT unchanged=$SKIPPED_COUNT bytes=$INSTALL_TOTAL_BYTES root_staged=${ROOT_PEAK_REQUIRED_KB}KB" >> "$LOG"
}

plan_retired_static_files()
{
    static_root=$ROOT_PREFIX/$OWNED_STATIC_TREE
    [ ! -L "$static_root" ] || fail_update 'Unsafe installed static resource tree'
    [ -e "$static_root" ] || return 0
    [ -d "$static_root" ] || fail_update 'Installed static resource tree is not a directory'
    scan_regular_tree "$static_root" "$PLAN_FILE.retired" || fail_update 'Cannot enumerate installed static resources'
    LC_ALL=C awk -v prefix="$ROOT_PREFIX/" '
        FILENAME==ARGV[1] {seen[$0]=1; next}
        {if(substr($0,1,length(prefix))!=prefix) {bad=1; next}
            if(!(substr($0,length(prefix)+1) in seen)) print}
        END {exit bad}' "$SEEN_TARGETS" "$PLAN_FILE.retired" > "$PLAN_FILE.retired.pending" ||
        fail_update 'Cannot enumerate installed static resources'
    while IFS= read -r static_file; do
        static_rel=${static_file#"$ROOT_PREFIX/"}
        safe_destination "$static_rel" || fail_update 'Unsafe retired static resource'
        for suffix in old new restore; do
            [ ! -e "$static_file.un260-$suffix" ] && [ ! -L "$static_file.un260-$suffix" ] || fail_update 'Unrecovered static resource artifact'
        done
        static_hash=$(checked_digest "$static_file") || fail_update 'Cannot read retired static resource'
        static_bytes=$(wc -c < "$static_file" | tr -d ' ')
        case "$static_bytes" in ''|*[!0-9]*) fail_update 'Cannot measure retired static resource' ;; esac
        BACKUP_REQUIRED_KB=$((BACKUP_REQUIRED_KB + (static_bytes+1023)/1024+4))
        printf 'delete|0644|%s|-|%s|0\n' "$static_rel" "$static_hash" >> "$PLAN_FILE"
        INSTALL_ENTRY_COUNT=$((INSTALL_ENTRY_COUNT+1))
        [ "$INSTALL_ENTRY_COUNT" -le 8192 ] || fail_update 'Too many changed files'
    done < "$PLAN_FILE.retired.pending"
}

verify_installed_file()
{
    verified_rel=$1
    verified_mode=$2
    safe_destination "$verified_rel" || fail_update 'Installed destination changed before verification'
    verified_dest=$(destination_for "$verified_rel")
    verified_hash=$(checked_digest "$verified_dest") || fail_update 'Cannot read installed target'
    if [ "$FULL_INVENTORY_READY" = 1 ]; then
        expected_hash=$3
        [ "$(wc -c < "$verified_dest")" = "$4" ] || fail_update 'Installed target checksum differs from package'
    elif [ "$PAYLOAD_VERIFIED" = 1 ]; then
        expected_hash=$(awk -v p="payload/$verified_rel" '$2==p {print $1}' "$STAGE_DIR/checksums.sha256")
    else
        expected_hash=$(checked_digest "$STAGE_DIR/payload/$verified_rel") || fail_update 'Cannot read expected target'
    fi
    [ "$verified_hash" = "$expected_hash" ] || fail_update 'Installed target checksum differs from package'
    verified_permissions=$(file_permissions "$verified_dest") || fail_update 'Cannot read installed target'
    case "$verified_mode" in 0755) expected_permissions=-rwxr-xr-x ;; *) expected_permissions=-rw-r--r-- ;; esac
    [ "$verified_permissions" = "$expected_permissions" ] || fail_update 'Installed target permissions differ from package'
}

verify_installed_targets()
{
    if [ "$FULL_INVENTORY_READY" = 1 ]; then
        full_inventory_valid || fail_update 'Full target inventory differs from payload'
        if [ "$FULL_INVENTORY_FAST" = 1 ]; then
            verify_app=-
            [ "$APP_VOLUME" != 1 ] || verify_app=$APP_VOLUME_DIR
            "$SYNC_TOOL" --verify-inventory "${ROOT_PREFIX:-/}" "$verify_app" "$STAGE_DIR/target.tsv" >> "$LOG" 2>&1 ||
                fail_update 'Installed target checksum differs from package'
        else
        verified_consumed=0
        while :; do
            verified_mode= verified_path= verified_hash= verified_bytes= verified_extra=
            if ! IFS='|' read -r verified_mode verified_path verified_hash verified_bytes verified_extra; then
                [ -z "$verified_mode$verified_path$verified_hash$verified_bytes$verified_extra" ] ||
                    fail_update 'Full target inventory differs from payload'
                break
            fi
            [ -z "$verified_extra" ] || fail_update 'Full target inventory differs from payload'
            verify_installed_file "$verified_path" "$verified_mode" "$verified_hash" "$verified_bytes"
            verified_consumed=$((verified_consumed+1))
        done < "$PLAN_FILE.inventory" || fail_update 'Full target inventory differs from payload'
        [ "$verified_consumed" = "$checksum_count" ] || fail_update 'Full target inventory differs from payload'
        fi
        full_inventory_valid || fail_update 'Full target inventory differs from payload'
    else
    while IFS='|' read -r verified_kind verified_mode verified_path verified_extra; do
        case "$verified_kind" in
            file) verify_installed_file "$verified_path" "$verified_mode" ;;
            tree)
                scan_regular_tree "$STAGE_DIR/payload/$verified_path" "$PLAN_FILE.verify" || fail_update 'Cannot enumerate expected installed tree'
                while IFS= read -r verified_file; do
                    verify_installed_file "${verified_file#"$STAGE_DIR/payload/"}" 0644
                done < "$PLAN_FILE.verify"
                ;;
        esac
    done < "$STAGE_DIR/install.tsv"
    fi
    if [ "$PRUNE_STATIC_TREE" = 1 ] && [ -d "$ROOT_PREFIX/$OWNED_STATIC_TREE" ]; then
        [ ! -L "$ROOT_PREFIX/$OWNED_STATIC_TREE" ] || fail_update 'Installed static resource tree changed during upgrade'
        scan_regular_tree "$ROOT_PREFIX/$OWNED_STATIC_TREE" "$PLAN_FILE.verify" || fail_update 'Cannot verify installed static resources'
        LC_ALL=C awk -v prefix="$ROOT_PREFIX/" '
            FILENAME==ARGV[1] {seen[$0]=1; next}
            /\.un260-(old|new|restore)$/ {next}
            {if(substr($0,1,length(prefix))!=prefix || !(substr($0,length(prefix)+1) in seen)) bad=1}
            END {exit bad}' "$SEEN_TARGETS" "$PLAN_FILE.verify" || fail_update 'Installed static resources differ from package'
    fi

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

    trial_required_kb=0
    if [ "$TRIAL_ENABLED" = 1 ]; then
        trial_table_bytes=$(wc -c < "$STAGE_DIR/target.tsv") || fail_update 'Unable to determine upgrade storage capacity'
        case "$trial_table_bytes" in ''|*[!0-9]*) fail_update 'Unable to determine upgrade storage capacity' ;; esac
        [ "$trial_table_bytes" -le 3145728 ] || fail_update 'Unable to determine upgrade storage capacity'
        trial_required_kb=$(((trial_table_bytes+1023)/1024+32))
    fi
    root_required_kb=$((ROOT_PEAK_REQUIRED_KB + trial_required_kb + 2048))
    usb_required_kb=$((BACKUP_REQUIRED_KB + 4096))
    echo "Storage preflight: root_free=${root_free_kb}KB root_peak=${ROOT_PEAK_REQUIRED_KB}KB root_required=${root_required_kb}KB usb_free=${usb_free_kb}KB backup_required=${BACKUP_REQUIRED_KB}KB" >> "$LOG"
    [ "$usb_free_kb" -ge "$usb_required_kb" ] ||
        fail_update "Insufficient USB space for upgrade rollback backup"
    if [ "$APP_VOLUME" = 1 ]; then
        app_free_kb=$(df -Pk "$APP_VOLUME_DIR" 2>/dev/null | awk 'NR==2 {print $4}')
        case "$app_free_kb" in ''|*[!0-9]*) fail_update 'Cannot read app volume free space' ;; esac
        app_required_kb=$((APP_PEAK_REQUIRED_KB + 2048))
        echo "App preflight: free=${app_free_kb}KB peak=${APP_PEAK_REQUIRED_KB}KB required=${app_required_kb}KB" >> "$LOG"
        if [ "$TRIAL_ENABLED" = 1 ] &&
           { [ "$root_free_kb" -lt "$root_required_kb" ] || [ "$app_free_kb" -lt "$app_required_kb" ]; }; then
            if [ "$root_free_kb" -lt "$root_required_kb" ]; then
                run_resource_maintenance || echo 'SDK maintenance incomplete; checking actual capacity' >> "$LOG"
            fi
            "$SYNC_TOOL" --prepare-history-space "${ROOT_PREFIX%/}/etc/ui_state/count_history" "$APP_VOLUME_DIR" \
                "$root_required_kb" "$app_required_kb" >> "$LOG" 2>&1 ||
                echo 'History space preparation incomplete; checking actual capacity' >> "$LOG"
            root_free_kb=$(df -Pk "${ROOT_PREFIX:-/}" | awk 'NR==2 {print $4}')
            app_free_kb=$(df -Pk "$APP_VOLUME_DIR" | awk 'NR==2 {print $4}')
            case "$root_free_kb:$app_free_kb" in :*|*:|*[!0-9:]*) fail_update 'Unable to determine upgrade storage capacity' ;; esac
        fi
    fi
    [ "$root_free_kb" -ge "$root_required_kb" ] ||
        fail_update "Insufficient root space: need ${root_required_kb}KB, free ${root_free_kb}KB; keep USB log for storage service"
    if [ "$APP_VOLUME" = 1 ]; then
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
    maintenance=${RESOURCE_CLEANUP_TOOL:-$ROOT_PREFIX/$maintenance_rel}
    demo_present=0
    for demo in "$ROOT_PREFIX/usr/local/share/ge_data" "$ROOT_PREFIX"/usr/local/bin/ge_*; do
        [ ! -e "$demo" ] || { demo_present=1; break; }
    done
    [ "$demo_present" = 1 ] || return 0
    if [ ! -e "$maintenance" ]; then
        echo 'Storage maintenance: not installed; bootstrap package needed for automatic cleanup' >> "$LOG"
        return 0
    fi
    safe_destination "$maintenance_rel" && [ -f "$maintenance" ] || return 1
    grep -q 'UN260_STORAGE_SYNC_1' "$maintenance" || { echo 'Skip old maintenance helper until guarded bootstrap is installed' >> "$LOG"; return 0; }
    # Never retire a file explicitly owned by the incoming package.
    for table in install.tsv target.tsv; do
        [ -f "$STAGE_DIR/$table" ] || continue
        if grep -Eq 'usr/local/(share/ge_data(/|$)|bin/ge_)' "$STAGE_DIR/$table"; then
            echo 'Storage maintenance: skipped; package references SDK demo files' >> "$LOG"
            return 0
        fi
    done
    echo 'Storage maintenance: low root space, checking known SDK files' >> "$LOG"
    sh "$maintenance" --apply --updater "$$" >> "$LOG" 2>&1
}

execute_plan()
{
    validate_install_manifest
    boot_package_request || fail_update "Invalid or incomplete boot bundle publication"
    if [ "$INSTALL_ENTRY_COUNT" = 0 ] && { [ -z "$BOOT_REQUEST_ID" ] || { [ "$BOOT_REQUEST_ID" = "$BOOT_OLD_ID" ] && [ "$BOOT_REQUEST_HASH" = "$BOOT_OLD_HASH" ]; }; }; then
        write_status 94 verify verify_installed "" "Verifying installed files and storage layout"
        verify_installed_targets
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
    write_status 94 verify verify_installed "" "Verifying installed files and storage layout"
    verify_installed_targets
    boot_prepare_publication || fail_update "Cannot prepare complete boot bundle publication"
    if [ "$TRIAL_ENABLED" = 1 ]; then trial_prepare || fail_update 'Cannot retain the previous application for startup recovery'; fi
    : > "$JOURNAL/committed" || fail_update "Cannot persist transaction commit"
    local_barrier || fail_update 'Cannot flush transaction commit'
    boot_publish_committed || fail_update "Committed boot publication requires recovery"
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

record_installed_bundle()
{
    [ -n "$BUNDLE_FNV" ] || return 0
    echo "$BUNDLE_FNV" | grep -Eq '^[0-9a-fA-F]{16}$' || return 0

    mkdir -p "$INSTALLED_STATE_DIR" || return 1
    echo "$BUNDLE_FNV" > "$INSTALLED_HASH_PATH.tmp.$$" || return 1
    mv -f "$INSTALLED_HASH_PATH.tmp.$$" "$INSTALLED_HASH_PATH"
}

boot_guard_supported()
{
    case "$(manifest_value storage_guard)" in syncfs-v1|syncfs-boot-bundle-v1) return 0 ;; *) return 1 ;; esac
}

validate_updater_contract()
{
    FULL_TARGET_ID=
    updater_envelope=$(manifest_value updater_envelope)
    if [ -e "$ROOT_PREFIX/etc/un260/updater-platform" ] || [ -L "$ROOT_PREFIX/etc/un260/updater-platform" ] || [ -n "$updater_envelope" ]; then
        [ -f "$ROOT_PREFIX/etc/un260/updater-platform" ] && [ ! -L "$ROOT_PREFIX/etc/un260/updater-platform" ] &&
            [ "$(cat "$ROOT_PREFIX/etc/un260/updater-platform")" = UN260_UPDATER_PLATFORM_V1 ] || fail_update 'Package requires matching factory firmware'
        [ "$updater_envelope" = UN260_UPDATE_ENVELOPE_V1 ] &&
            [ "$(manifest_value updater_platform)" = UN260_UPDATER_PLATFORM_V1 ] &&
            [ "$(manifest_value updater_engine_abi)" = 1 ] && [ "$package_schema" = 1 ] &&
            [ "$(manifest_value package_type)" = ui ] && [ "$APP_VOLUME" = 1 ] &&
            [ -z "$(manifest_value storage_migration)" ] || fail_update 'Package engine envelope is invalid or unsupported'
        FULL_TARGET_ID=$(manifest_value target_id)
        boot_hex64 "$FULL_TARGET_ID" && [ -f "$STAGE_DIR/target.tsv" ] && [ ! -L "$STAGE_DIR/target.tsv" ] &&
            [ "$(checked_digest "$STAGE_DIR/target.tsv")" = "$FULL_TARGET_ID" ] || fail_update 'Full target inventory is invalid'
        [ "$(wc -c < "$STAGE_DIR/target.tsv")" -le 3145728 ] && (
            trial_targets_count=$checksum_count
            trial_app_sha=$(awk '$2=="payload/usr/local/bin/test_lvgl" {print $1}' "$STAGE_DIR/checksums.sha256")
            trial_table_format_valid "$STAGE_DIR/target.tsv"
        ) || fail_update 'Full target inventory is invalid'
        : > "$SEEN_TARGETS"
        full_consumed=0
        while IFS='|' read -r full_hash full_mode full_bytes full_path; do
            [ "$full_path" != etc/un260/unified-request ] || fail_update 'A full UN260 UI package is required'
            safe_destination "$full_path" || fail_update "Unsafe full-package target"
            echo "$full_path" >> "$SEEN_TARGETS"
            full_consumed=$((full_consumed+1))
        done < "$STAGE_DIR/target.tsv" || fail_update 'Full target inventory differs from payload'
        [ "$full_consumed" = "$checksum_count" ] || fail_update 'Full target inventory differs from payload'
        [ "$(wc -l < "$STAGE_DIR/target.tsv" | tr -d ' ')" = "$checksum_count" ] || fail_update 'Full target inventory differs from payload'
        full_target_matches_checksums || fail_update 'Full target inventory differs from payload'
        for full_required in usr/local/bin/test_lvgl usr/local/lib/liblvgl.so usr/bin/ui_update.sh \
            usr/local/bin/un260_storage_sync usr/local/bin/un260_unpack usr/local/bin/un260_app_storage \
            usr/local/bin/un260_resource_cleanup usr/local/lib/un260-startup/runtime.sh \
            etc/un260/update-core.manifest etc/un260/startup-runtime.sha256; do
            [ -f "$STAGE_DIR/payload/$full_required" ] || fail_update 'Full package is missing a core runtime component'
        done
        core_descriptor=$STAGE_DIR/payload/etc/un260/update-core.manifest
        [ "$(wc -l < "$core_descriptor")" = 6 ] && [ "$(wc -c < "$core_descriptor")" -le 512 ] &&
            [ "$(sed -n '1p' "$core_descriptor")" = UN260_UPDATE_CORE_V1 ] ||
            fail_update 'Full package runtime descriptors are invalid or incomplete'
        core_line=2
        for core_role in engine sync unpack app_storage resource_cleanup; do
            case "$core_role" in
                engine) core_path=usr/bin/ui_update.sh ;;
                sync) core_path=usr/local/bin/un260_storage_sync ;;
                unpack) core_path=usr/local/bin/un260_unpack ;;
                app_storage) core_path=usr/local/bin/un260_app_storage ;;
                resource_cleanup) core_path=usr/local/bin/un260_resource_cleanup ;;
            esac
            core_expected=$(awk -v p="payload/$core_path" '$2==p {print $1}' "$STAGE_DIR/checksums.sha256")
            [ "$(sed -n "${core_line}p" "$core_descriptor")" = "${core_role}_sha256=$core_expected" ] ||
                fail_update 'Full package runtime descriptors are invalid or incomplete'
            core_line=$((core_line+1))
        done
        runtime_descriptor=$STAGE_DIR/payload/etc/un260/startup-runtime.sha256
        runtime_payload=$STAGE_DIR/payload/usr/local/lib/un260-startup/runtime.sh
        runtime_expected=$(awk '$2=="payload/usr/local/lib/un260-startup/runtime.sh" {print $1}' "$STAGE_DIR/checksums.sha256")
        [ "$(wc -l < "$runtime_descriptor")" = 1 ] && [ "$(wc -c < "$runtime_descriptor")" = 65 ] &&
            [ "$(cat "$runtime_descriptor")" = "$runtime_expected" ] ||
            fail_update 'Full package runtime descriptors are invalid or incomplete'
        /bin/sh -n "$runtime_payload" >> "$LOG" 2>&1 || fail_update 'Full package startup runtime syntax is invalid'
    fi
}

validate_boot_compatibility()
{
    boot_package_guard=$(manifest_value storage_guard)
    boot_declared_id=$(boot_manifest_value "$STAGE_DIR/manifest.ini" boot_bundle_id) || boot_declared_id=""
    boot_declared_hash=$(boot_manifest_value "$STAGE_DIR/manifest.ini" boot_manifest_sha256) || boot_declared_hash=""
    boot_platform_required=0
    if [ -n "$boot_declared_id" ] || [ -n "$boot_declared_hash" ] ||
        grep -q '^boot_\(bundle_id\|manifest_sha256\)=' "$STAGE_DIR/manifest.ini"; then
        boot_hex64 "$boot_declared_id" && boot_hex64 "$boot_declared_hash" ||
            fail_update 'Invalid or incomplete boot bundle publication'
        [ "$boot_package_guard" = syncfs-boot-bundle-v1 ] ||
            fail_update 'Boot package requires matching factory firmware'
        boot_platform_required=1
    fi
    if [ -e "$ROOT_PREFIX/$BOOT_ACTIVE" ] || [ -L "$ROOT_PREFIX/$BOOT_ACTIVE" ] ||
        [ -e "$ROOT_PREFIX/$BOOT_ACTIVE.previous" ] || [ -L "$ROOT_PREFIX/$BOOT_ACTIVE.previous" ] ||
        [ -e "$ROOT_PREFIX/dev/.un260-boot-platform" ] || [ -L "$ROOT_PREFIX/dev/.un260-boot-platform" ]; then
        [ "$boot_package_guard" = syncfs-boot-bundle-v1 ] ||
            fail_update 'Package would downgrade boot bundle recovery'
        boot_platform_required=1
        [ -n "$boot_declared_id" ] && [ -n "$boot_declared_hash" ] ||
            fail_update 'Package would downgrade boot bundle recovery'
    fi
    if [ "$boot_package_guard" = syncfs-boot-bundle-v1 ]; then boot_platform_required=1; fi
    [ "$boot_platform_required" = 1 ] || return 0
    boot_safe_file "${ROOT_PREFIX:-/}" dev/.un260-boot-platform &&
        [ "$(wc -c < "$ROOT_PREFIX/dev/.un260-boot-platform" | tr -d ' ')" = 23 ] &&
        [ "$(cat "$ROOT_PREFIX/dev/.un260-boot-platform")" = UN260_BOOT_PLATFORM_V1 ] ||
        fail_update 'Boot package requires matching factory firmware'
    for boot_component in usr/bin/ui_update.sh etc/init.d/S00lvgl; do
        if [ -e "$STAGE_DIR/payload/$boot_component" ]; then
            grep -q UN260_BOOT_BUNDLE_1 "$STAGE_DIR/payload/$boot_component" ||
                fail_update 'Package would downgrade boot bundle recovery'
        fi
    done
    if [ "$package_schema" = 1 ]; then
        for boot_required in usr/bin/ui_update.sh etc/init.d/S00lvgl; do
            [ -f "$STAGE_DIR/payload/$boot_required" ] &&
                grep -q UN260_BOOT_BUNDLE_1 "$STAGE_DIR/payload/$boot_required" ||
                fail_update 'Package would downgrade boot bundle recovery'
        done
    fi
}

install_bundle()
{
    # The V1 dispatcher verifies the selected archive before and after extraction.
    if [ -z "$DISPATCH_CONTEXT" ]; then
        BUNDLE_FNV=$("$SYNC_TOOL" --hash-fnv64 "$BUNDLE_PATH" 2>> "$LOG") ||
            fail_update 'Cannot fingerprint selected USB package'
    fi
    write_status 5 verify "verify_archive" "" ""
    prepare_usb_work
    unpacker=${UNPACK_TOOL:-$ROOT_PREFIX/usr/local/bin/un260_unpack}
    if [ "$DISPATCH_STAGE_READY" = 1 ]; then
        [ "$DISPATCH_STAGE" != none ] && [ -d "$DISPATCH_STAGE" ] && [ ! -L "$DISPATCH_STAGE" ] || fail_update 'Package engine staging is unavailable'
        rmdir "$STAGE_DIR" || fail_update 'Cannot adopt package engine staging'
        STAGE_DIR=$DISPATCH_STAGE
        DISPATCH_STAGE_READY=0
        echo 'Reusing archive verified and extracted by the package dispatcher' >> "$LOG"
    elif [ -x "$unpacker" ] && [ "$("$unpacker" --probe 2>/dev/null)" = UN260_UNPACK_1 ]; then
        write_status 10 extract "extract" "" ""
        "$unpacker" "$BUNDLE_PATH" "$STAGE_DIR" >> "$LOG" 2>&1 || fail_update "Unsafe or damaged archive"
    else
        fail_update 'A matched package extractor is required'
    fi

    write_status 18 verify "verify_manifest" "" ""
    [ -f "$STAGE_DIR/manifest.ini" ] || fail_update "Package manifest is missing"
    [ -f "$STAGE_DIR/checksums.sha256" ] || fail_update "Package checksums are missing"
    [ -f "$STAGE_DIR/install.tsv" ] || fail_update "Install manifest is missing"
    [ -d "$STAGE_DIR/payload" ] || fail_update "Package payload is missing"

    scan_regular_tree "$STAGE_DIR/payload" "$PAYLOAD_FILES" || fail_update "Unsupported upgrade payload entry"

    package_format=$(manifest_value format)
    package_schema=$(manifest_value schema)
    package_product=$(manifest_value product)
    package_version=$(manifest_value version)
    package_id=$(manifest_value package_id)

    [ "$package_format" = "UN260_UPGRADE" ] || fail_update "Unsupported package format"
    [ "$package_schema" = 1 ] && [ "$(manifest_value package_type)" = ui ] &&
        [ -z "$(manifest_value storage_migration)" ] || fail_update 'A full UN260 UI package is required'
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
    validate_updater_contract
    validate_boot_compatibility
    if [ "$GUARDED" = 1 ]; then
        boot_guard_supported || fail_update 'Package predates USB write protection; use current UPK'
        for guarded_component in usr/bin/ui_update.sh usr/local/bin/un260_app_storage usr/local/bin/un260_resource_cleanup usr/local/bin/un260_storage_sync; do
            if [ -e "$STAGE_DIR/payload/$guarded_component" ]; then
                grep -q 'UN260_STORAGE_SYNC_1' "$STAGE_DIR/payload/$guarded_component" || fail_update 'Package contains an outdated storage guard component'
            fi
        done
    fi
    [ "$APP_VOLUME" = 1 ] && [ "$(manifest_value storage_layout)" = app-volume-v1 ] ||
        fail_update 'Package requires the factory application volume'
    for component in usr/bin/ui_update.sh usr/local/bin/un260_app_storage etc/init.d/S00lvgl; do
        [ -f "$STAGE_DIR/payload/$component" ] &&
            grep -q 'UN260_APP_VOLUME_V1' "$STAGE_DIR/payload/$component" ||
            fail_update 'Storage-compatible recovery component missing'
    done
    if [ -e "$ROOT_PREFIX/etc/un260/app-storage/pending" ] || [ -L "$ROOT_PREFIX/etc/un260/app-storage/pending" ]; then
        fail_update 'Existing storage migration requires service'
    fi
    write_status 34 preflight "preflight" "" ""
    execute_plan
}

checked_digest()
{
    digest_output=$(sha256sum "$1" 2>> "$LOG") || return 1
    digest_value=${digest_output%% *}
    boot_hex64 "$digest_value" || return 1
    printf '%s\n' "$digest_value"
}

legacy_request_pending()
{
    old_request=etc/un260/unified-request
    [ -e "$ROOT_PREFIX/$old_request" ] || [ -L "$ROOT_PREFIX/$old_request" ] || return 1
    boot_safe_file "${ROOT_PREFIX:-/}" "$old_request" || return 0
    request_id=$(checked_digest "$ROOT_PREFIX/$old_request") || return 0
    [ -f "$INSTALLED_STATE_DIR/unified.completed" ] && [ ! -L "$INSTALLED_STATE_DIR/unified.completed" ] &&
        [ "$(cat "$INSTALLED_STATE_DIR/unified.completed")" = "$request_id" ] && return 1
    return 0
}

# Called only after successful recovery in this invocation of the locked engine.
app_launch_after_recovery()
{
    [ "$APP_VOLUME" = 1 ] && [ -f "$APP_VOLUME_DIR/test_lvgl" ] &&
        [ ! -L "$APP_VOLUME_DIR/test_lvgl" ] && [ -x "$APP_VOLUME_DIR/test_lvgl" ] || return 1
    if [ "$RECOVERED_TRIAL_PENDING" = 1 ]; then
        app_check_trace runtime_files_begin
        trial_runtime_files_valid || return 1
        app_check_trace runtime_files_complete
    else
        [ ! -e "$JOURNAL" ] && [ ! -L "$JOURNAL" ] || return 1
    fi
    printf '%s/test_lvgl\n' "$APP_VOLUME_DIR"
}

# UN260_ENGINE_CONTEXT_MODULE
# UN260_ENGINE_TRIAL_MODULE

# Host tests load definitions, then redirect paths into a temporary fixture.
if [ "${UN260_UPDATER_LIBRARY_ONLY:-0}" = 1 ]; then return 0; fi
if [ "${1:-}" != --dispatch-context ]; then
    [ -x /usr/local/bin/un260_update_dispatch ] || { echo 'Factory V1 updater required' >&2; exit 1; }
    exec /usr/local/bin/un260_update_dispatch "$@"
fi
[ "$#" = 2 ] && [ "$0" = "$2/engine.sh" ] || exit 2
if ! dispatch_context_load "$2"; then
    echo 'Package engine context is invalid; no installation attempted' >&2
    if [ "$DISPATCH_OWNERSHIP_VERIFIED" = 1 ]; then trap dispatch_engine_cleanup 0; fi
    exit 1
fi
trap dispatch_engine_cleanup 0
# Old V1 factory launchers may still query continuation. Preserve unfinished
# records for service, and never run or advance a migration from this engine.
case "$DISPATCH_MODE" in
    pending) legacy_request_pending; exit $? ;;
    continue) echo 'Legacy storage migration requires service' >&2; exit 1 ;;
esac
app_check_trace storage_begin
if ! prepare_app_storage; then
    echo 'Application storage unavailable; no update or recovery writes attempted' >&2
    exit 1
fi
app_check_trace recovery_begin
if ! recover_transaction; then
    echo "Recovery failed; keep $JOURNAL and USB backup" >&2
    exit 1
fi
app_check_trace recovery_complete
case "$DISPATCH_MODE" in
    trial-installed) trial_is_committed; exit $? ;;
    trial-pending) trial_is_committed && trial_boot_eligible; exit $? ;;
    trial-confirm) trial_confirm "$DISPATCH_ACTION_ARG"; exit $? ;;
    trial-rollback) trial_rollback; exit $? ;;
    app-check) app_launch_after_recovery; exit $? ;;
    recover) exit 0 ;;
esac
if [ "$TRIAL_ENABLED" = 1 ] && trial_is_committed; then
    write_status 96 restart restart -1 'Restart to complete the previous upgrade before installing another package' || exit 1
    write_result pending 'Restart to complete the previous upgrade before installing another package' '' || exit 1
    exit 0
fi
[ "$DISPATCH_MODE" = update ] || exit 2
if legacy_request_pending; then
    fail_update 'Existing storage migration requires service'
fi
rm -f "$STATUS_FILE" "$STATUS_TEMP"
usb_barrier || fail_update 'USB is not a healthy writable mounted filesystem'
rm -f "$RESULT_FILE" "$USB_MNT/UN260_UPDATE_RESULT.txt" || fail_update 'Cannot clear previous USB result'
usb_barrier || fail_update 'USB result reset could not be persisted'
echo "ui_update start: $(date)" >> "$LOG"
write_status 2 prepare "prepare" "" ""

echo "Selected package: $BUNDLE_PATH" >> "$LOG"
[ -f "$BUNDLE_PATH" ] || fail_update "No supported UN260 upgrade package was found"
install_bundle
