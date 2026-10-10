#!/bin/sh
# UN260_UPDATE_DISPATCH_1: stable envelope, same-PID package engine selection.
# Business, boot and storage policies belong to the selected engine.

ROOT_DIR=/
USB_MNT=/mnt/usb
MOUNTS=/proc/mounts
PLATFORM=/etc/un260/updater-platform
RESCUE=/usr/local/lib/un260-updater/rescue
LOCK_DIR=/tmp/un260-updater.lock
LOCK_FILE=/tmp/un260-updater.flock
RAM_BASE=/tmp
STATUS_FILE=/tmp/ui_update.status
LOG=/tmp/ui_update.log
EXPECTED_UID=0
CORE_LIMIT=262144
MODE=update
ACTION_ARG=none
GUI_FNV=
USB_DEV=none
STAGE=none
BUNDLE=none
BUNDLE_HASH=none
BUNDLE_FNV=none
CTX=
OWN_LOCK=0
LOCK_PID_WRITTEN=0
USB_HELD=0
START_TIME=$(date +%s) || exit 1
umask 077

hex64() { [ "${#1}" = 64 ] && case "$1" in *[!0-9a-f]*) return 1 ;; *) return 0 ;; esac; }
hex16() { [ "${#1}" = 16 ] && case "$1" in *[!0-9a-f]*) return 1 ;; *) return 0 ;; esac; }
digest()
{
    digest_line=$(sha256sum "$1" 2>> "$LOG") || return 1
    digest_value=${digest_line%% *}
    hex64 "$digest_value" || return 1
    printf '%s\n' "$digest_value"
}
safe_file()
{
    safe_base=$1
    safe_path=$2
    [ -f "$safe_path" ] && [ ! -L "$safe_path" ] || return 1
    safe_parent=${safe_path%/*}
    while [ "$safe_parent" != "$safe_base" ]; do
        [ -d "$safe_parent" ] && [ ! -L "$safe_parent" ] || return 1
        [ "$safe_parent" != / ] || return 1
        safe_parent=${safe_parent%/*}
        [ -n "$safe_parent" ] || safe_parent=/
    done
    [ -d "$safe_base" ] && [ ! -L "$safe_base" ]
}
private_dir()
{
    [ -d "$1" ] && [ ! -L "$1" ] &&
        [ "$(LC_ALL=C ls -ldn "$1" | awk '{print $1 ":" $3}')" = "drwx------:$EXPECTED_UID" ]
}
reclaim_dead_lock()
{
    "$RESCUE/sync" --lock-held "$LOCK_FILE" && private_dir "$LOCK_DIR" || return 1
    stale_empty=1
    for stale_entry in "$LOCK_DIR"/* "$LOCK_DIR"/.[!.]* "$LOCK_DIR"/..?*; do
        [ -e "$stale_entry" ] || [ -L "$stale_entry" ] || continue
        stale_empty=0
        [ "${stale_entry##*/}" = pid ] && [ -f "$stale_entry" ] && [ ! -L "$stale_entry" ] || return 1
    done
    [ "$stale_empty" != 1 ] || { rmdir "$LOCK_DIR"; return $?; }
    safe_file "$LOCK_DIR" "$LOCK_DIR/pid" &&
        [ "$(LC_ALL=C ls -ln "$LOCK_DIR/pid" | awk '{print $1 ":" $3}')" = "-rw-------:$EXPECTED_UID" ] &&
        [ "$(wc -l < "$LOCK_DIR/pid")" = 1 ] && [ "$(wc -c < "$LOCK_DIR/pid")" -le 10 ] || return 1
    stale_pid=$(cat "$LOCK_DIR/pid") || return 1
    case "$stale_pid" in ''|0*|*[!0-9]*) return 1 ;; esac
    [ "${#stale_pid}" -le 9 ] && ! kill -0 "$stale_pid" 2>/dev/null || return 1
    rm -f "$LOCK_DIR/pid" && rmdir "$LOCK_DIR"
}
unique_name()
{
    case "$1" in "$2"*) ;; *) return 1 ;; esac
    unique_suffix=${1#"$2"}
    [ "${#unique_suffix}" = 6 ] && case "$unique_suffix" in *[!A-Za-z0-9]*) return 1 ;; *) return 0 ;; esac
}
usb_device() { [ -b "$1" ]; }
usb_barrier()
{
    [ "$USB_HELD" = 1 ] && usb_device "$USB_DEV" &&
        awk -v m="$USB_MNT" -v d="$USB_DEV" '$2==m && $1==d && $4 ~ /(^|,)rw(,|$)/ {n++} END {exit n!=1}' "$MOUNTS" &&
        "$RESCUE/sync" --fd 8 "$USB_MNT"
}
cleanup()
{
    cleanup_rc=$?
    trap - 0 HUP INT TERM
    if [ -n "$CTX" ]; then
        if unique_name "$CTX" "$RAM_BASE/un260-update-engine."; then
            private_dir "$CTX" && rm -rf "$CTX" || cleanup_rc=1
        else cleanup_rc=1; fi
    fi
    if [ "$STAGE" != none ]; then
        if unique_name "$STAGE" "$USB_MNT/update/.un260_dispatch.$$."; then
            if usb_barrier && [ -d "$STAGE" ] && [ ! -L "$STAGE" ]; then
                rm -rf "$STAGE" && usb_barrier || cleanup_rc=1
            else cleanup_rc=1; fi
        else cleanup_rc=1; fi
    fi
    if [ "$OWN_LOCK" = 1 ]; then
        if private_dir "$LOCK_DIR" && { [ "$LOCK_PID_WRITTEN" = 0 ] || {
            safe_file "$LOCK_DIR" "$LOCK_DIR/pid" && [ "$(cat "$LOCK_DIR/pid")" = "$$" ]; }; }; then
            rm -f "$LOCK_DIR/pid" && rmdir "$LOCK_DIR" || cleanup_rc=1
        else cleanup_rc=1; fi
    fi
    exit "$cleanup_rc"
}
fatal()
{
    printf 'DISPATCH_FAIL %s\n' "$1" >&2
    if [ "$OWN_LOCK" = 1 ]; then
        printf 'ERR: %s\n' "$1" >> "$LOG"
        case "$MODE" in update|continue)
            { printf 'progress=0\nstage=fail\nstep=fail\nsuccess=0\nmessage=%s\nwriter_pid=%s\n' "$1" "$$" &&
                { if hex16 "$BUNDLE_FNV"; then printf 'bundle_fnv64=%s\n' "$BUNDLE_FNV"; else :; fi; }
            } > "$STATUS_FILE.tmp.$$" &&
                mv -f "$STATUS_FILE.tmp.$$" "$STATUS_FILE" || : ;;
        esac
    fi
    exit 1
}
value()
{
    awk -F= -v key="$2" '$1==key {if (NF!=2) bad=1; v=$2; n++}
        END {if (bad || n!=1) exit 1; print v}' "$1"
}
core_digest()
{
    awk -v path="$2" '$2==path {if (NF!=2 || length($1)!=64 || $1 !~ /^[0-9a-f]+$/) bad=1; v=$1; n++}
        END {if (bad || n!=1) exit 1; print v}' "$1"
}
read_core()
{
    CORE_KIND=$1
    if [ "$CORE_KIND" = installed ]; then
        CORE_ROOT=$ROOT_DIR
        core_record=${ROOT_DIR%/}/etc/un260/update-core.manifest
        ENGINE_SOURCE=${ROOT_DIR%/}/usr/bin/ui_update.sh
        SYNC_SOURCE=${ROOT_DIR%/}/usr/local/bin/un260_storage_sync
        UNPACK_SOURCE=${ROOT_DIR%/}/usr/local/bin/un260_unpack
        STORAGE_SOURCE=${ROOT_DIR%/}/usr/local/bin/un260_app_storage
        CLEANUP_SOURCE=${ROOT_DIR%/}/usr/local/bin/un260_resource_cleanup
    else
        [ "$CORE_KIND" = rescue ] || return 1
        CORE_ROOT=$RESCUE
        core_record=$RESCUE/manifest
        ENGINE_SOURCE=$RESCUE/engine.sh
        SYNC_SOURCE=$RESCUE/sync
        UNPACK_SOURCE=$RESCUE/unpack
        STORAGE_SOURCE=$RESCUE/app_storage
        CLEANUP_SOURCE=$RESCUE/resource_cleanup
    fi
    safe_file "$ROOT_DIR" "$core_record" || return 1
    core_record_bytes=$(wc -c < "$core_record") || return 1
    [ "$core_record_bytes" -le 512 ] &&
        [ "$(wc -l < "$core_record")" = 6 ] &&
        [ "$(LC_ALL=C ls -ldn "$core_record" | awk '{print $1 ":" $3}')" = "-rw-r--r--:$EXPECTED_UID" ] || return 1
    # Parse the bounded, fixed-order descriptor once with shell builtins. The
    # same complete size, permission, hash and ABI checks remain below.
    {
        IFS= read -r rescue_record && [ "$rescue_record" = UN260_UPDATE_CORE_V1 ] || return 1
        core_parsed_bytes=$((${#rescue_record}+1))
        for rescue_role in engine sync unpack app_storage resource_cleanup; do
            IFS= read -r rescue_record || return 1
            core_parsed_bytes=$((core_parsed_bytes+${#rescue_record}+1))
            case "$rescue_record" in "${rescue_role}_sha256="*) ;; *) return 1 ;; esac
            rescue_hash=${rescue_record#*=}
            hex64 "$rescue_hash" || return 1
            case "$rescue_role" in
                engine) ENGINE_HASH=$rescue_hash ;;
                sync) SYNC_HASH=$rescue_hash ;;
                unpack) UNPACK_HASH=$rescue_hash ;;
                app_storage) STORAGE_HASH=$rescue_hash ;;
                resource_cleanup) CLEANUP_HASH=$rescue_hash ;;
            esac
        done
        if IFS= read -r rescue_record || [ -n "$rescue_record" ]; then return 1; fi
    } < "$core_record" || return 1
    [ "$core_parsed_bytes" = "$core_record_bytes" ] || return 1
    for rescue_role in engine sync unpack app_storage resource_cleanup; do
        case "$rescue_role" in
            engine) rescue_path=$ENGINE_SOURCE; rescue_hash=$ENGINE_HASH ;;
            sync) rescue_path=$SYNC_SOURCE; rescue_hash=$SYNC_HASH ;;
            unpack) rescue_path=$UNPACK_SOURCE; rescue_hash=$UNPACK_HASH ;;
            app_storage) rescue_path=$STORAGE_SOURCE; rescue_hash=$STORAGE_HASH ;;
            resource_cleanup) rescue_path=$CLEANUP_SOURCE; rescue_hash=$CLEANUP_HASH ;;
        esac
        safe_file "$ROOT_DIR" "$rescue_path" || return 1
        rescue_actual=$(digest "$rescue_path") || return 1
        [ "$rescue_actual" = "$rescue_hash" ] && [ "$(wc -c < "$rescue_path")" -gt 0 ] &&
            [ "$(wc -c < "$rescue_path")" -le "$CORE_LIMIT" ] || return 1
        rescue_permissions=-rwxr-xr-x
        [ "$CORE_KIND:$rescue_role" != rescue:engine ] || rescue_permissions=-rw-------
        [ "$(LC_ALL=C ls -ldn "$rescue_path" | awk '{print $1 ":" $3}')" = "$rescue_permissions:$EXPECTED_UID" ] || return 1
    done
    grep -q '^# UN260_PACKAGE_ENGINE_V1\($\|[: ]\)' "$ENGINE_SOURCE" &&
        /bin/sh -n "$ENGINE_SOURCE" && /bin/sh -n "$STORAGE_SOURCE" && /bin/sh -n "$CLEANUP_SOURCE" &&
        [ "$("$SYNC_SOURCE" --probe)" = UN260_STORAGE_SYNC_1 ] && [ "$("$UNPACK_SOURCE" --probe)" = UN260_UNPACK_1 ]
}
mount_usb()
{
    [ -d "$USB_MNT" ] && [ ! -L "$USB_MNT" ] || return 1
    USB_DEV=$(awk -v m="$USB_MNT" '$2==m {print $1; n++} END {if(n>1) exit 1}' "$MOUNTS") || return 1
    if [ -z "$USB_DEV" ]; then
        for usb_candidate in /dev/sd[a-z][0-9]* /dev/sd[a-z]; do
            usb_device "$usb_candidate" || continue
            USB_DEV=$usb_candidate
            break
        done
        [ -n "$USB_DEV" ] && usb_device "$USB_DEV" && mount "$USB_DEV" "$USB_MNT" >> "$LOG" 2>&1 || return 1
    fi
    usb_device "$USB_DEV" || return 1
    exec 8< "$USB_MNT" || return 1
    USB_HELD=1
    usb_barrier
}
read_package_core()
{
    metadata=$STAGE/manifest.ini
    safe_file "$STAGE" "$metadata" && [ "$(wc -c < "$metadata")" -le 4096 ] &&
        safe_file "$STAGE" "$STAGE/checksums.sha256" || return 1
    [ "$(value "$metadata" format)" = UN260_UPGRADE ] &&
        [ "$(value "$metadata" product)" = UN260 ] &&
        [ "$(value "$metadata" schema)" = 1 ] &&
        [ "$(value "$metadata" package_type)" = ui ] &&
        [ "$(value "$metadata" updater_envelope)" = UN260_UPDATE_ENVELOPE_V1 ] &&
        [ "$(value "$metadata" updater_platform)" = UN260_UPDATER_PLATFORM_V1 ] &&
        [ "$(value "$metadata" updater_engine_abi)" = 1 ] || return 1
    envelope_id=$(value "$metadata" package_id) || return 1
    actual_id=$(digest "$STAGE/checksums.sha256") || return 1
    hex64 "$envelope_id" && [ "$envelope_id" = "$actual_id" ] || return 1
    ENGINE_HASH=$(core_digest "$STAGE/checksums.sha256" payload/usr/bin/ui_update.sh) &&
        SYNC_HASH=$(core_digest "$STAGE/checksums.sha256" payload/usr/local/bin/un260_storage_sync) &&
        UNPACK_HASH=$(core_digest "$STAGE/checksums.sha256" payload/usr/local/bin/un260_unpack) &&
        STORAGE_HASH=$(core_digest "$STAGE/checksums.sha256" payload/usr/local/bin/un260_app_storage) &&
        CLEANUP_HASH=$(core_digest "$STAGE/checksums.sha256" payload/usr/local/bin/un260_resource_cleanup)
}
copy_core()
{
    core_source=$1
    core_destination=$2
    core_expected=$3
    safe_file "$CORE_ROOT" "$core_source" || return 1
    core_bytes=$(wc -c < "$core_source") || return 1
    case "$core_bytes" in ''|*[!0-9]*) return 1 ;; esac
    [ "$core_bytes" -gt 0 ] && [ "$core_bytes" -le "$CORE_LIMIT" ] || return 1
    core_hash=$(digest "$core_source") || return 1
    [ "$core_hash" = "$core_expected" ] || return 1
    # Physical cap also bounds a source that grows after the size check.
    dd if="$core_source" of="$core_destination" bs=1024 count=256 2>> "$LOG" || return 1
    [ "$(wc -c < "$core_destination")" = "$core_bytes" ] || return 1
    core_hash=$(digest "$core_destination") || return 1
    [ "$core_hash" = "$core_expected" ] || return 1
    case "${core_destination##*/}" in engine.sh) core_mode=0600 ;; *) core_mode=0755 ;; esac
    chmod "$core_mode" "$core_destination"
}

case "${1:-}" in
    --probe) [ "$#" = 1 ] || exit 2; printf 'UN260_UPDATE_DISPATCH_1\n'; exit 0 ;;
    --recover) [ "$#" = 1 ] || exit 2; MODE=recover ;;
    --continue-unified) [ "$#" = 1 ] || exit 2; MODE=continue ;;
    --unified-pending) [ "$#" = 1 ] || exit 2; MODE=pending ;;
    --trial-pending) [ "$#" = 1 ] || exit 2; MODE=trial-pending ;;
    --trial-installed) [ "$#" = 1 ] || exit 2; MODE=trial-installed ;;
    --confirm-trial)
        [ "$#" = 2 ] && [ "${#2}" -le 9 ] || exit 2
        case "$2" in ''|0*|*[!0-9]*) exit 2 ;; esac
        MODE=trial-confirm; ACTION_ARG=$2 ;;
    --rollback-trial) [ "$#" = 1 ] || exit 2; MODE=trial-rollback ;;
    --can-launch) [ "$#" = 1 ] || exit 2; MODE=app-check ;;
    --bundle-fnv) [ "$#" = 2 ] && hex16 "$2" || exit 2; GUI_FNV=$2 ;;
    '') [ "$#" = 0 ] || exit 2 ;;
    *) exit 2 ;;
esac
[ "$(id -u)" = "$EXPECTED_UID" ] || fatal 'Updater requires root privileges'
safe_file "$ROOT_DIR" "$PLATFORM" && [ "$(wc -c < "$PLATFORM")" = 26 ] &&
    [ "$(cat "$PLATFORM")" = UN260_UPDATER_PLATFORM_V1 ] || fatal 'Package requires matching factory firmware'
read_core rescue || fatal 'Factory updater rescue is invalid or incomplete'
# The native FD6 flock survives exec and vanishes even after SIGKILL. Only its
# holder can retire a precisely known dead RAM pid record before taking mkdir.
if ! "$RESCUE/sync" --lock-held "$LOCK_FILE" 2>/dev/null; then
    exec "$RESCUE/sync" --lock-run "$LOCK_FILE" "$0" "$@"
fi
if ! mkdir -m 0700 "$LOCK_DIR" 2>/dev/null; then
    if ! reclaim_dead_lock || ! mkdir -m 0700 "$LOCK_DIR" 2>/dev/null; then
        case "$MODE" in pending) exit 0 ;; trial-pending) exit 2 ;; esac
        fatal 'Updater already running or unsafe lock record'
    fi
fi
OWN_LOCK=1
trap cleanup 0
trap 'fatal "Upgrade interrupted"' HUP INT TERM
private_dir "$LOCK_DIR" && printf '%s\n' "$$" > "$LOCK_DIR/pid" || fatal 'Cannot hold updater lock'
LOCK_PID_WRITTEN=1
case "$MODE" in update|continue)
    : > "$LOG" || fatal 'Cannot create updater log'
    rm -f "$STATUS_FILE" "$STATUS_FILE.tmp.$$" || fatal 'Cannot reset updater status'
    ;;
esac
exec 7< "$ROOT_DIR" || fatal 'Cannot hold root filesystem for writeback checks'
"$RESCUE/sync" --fd 7 "$ROOT_DIR" || fatal 'Root filesystem is not healthy and writable'
case "$MODE" in update|continue) ;; *) read_core installed || read_core rescue || fatal 'Factory updater rescue is invalid or incomplete' ;; esac
if [ "$MODE" = update ] || [ "$MODE" = continue ]; then
    if mount_usb; then
        BUNDLE=$USB_MNT/update/UN260_UPDATE.upk
        if [ "$MODE" = update ] || [ -f "$BUNDLE" ]; then
            safe_file "$USB_MNT" "$BUNDLE" || fatal 'Upgrade package not found or unsafe'
            BUNDLE_HASH=$(digest "$BUNDLE") || fatal 'Cannot fingerprint selected USB package'
            BUNDLE_FNV=$("$RESCUE/sync" --hash-fnv64 "$BUNDLE") && hex16 "$BUNDLE_FNV" || fatal 'Cannot fingerprint selected USB package'
            [ -z "$GUI_FNV" ] || [ "$GUI_FNV" = "$BUNDLE_FNV" ] || fatal 'Selected USB package changed'
            STAGE=$(mktemp -d "$USB_MNT/update/.un260_dispatch.$$.XXXXXX") || fatal 'Cannot create unique USB staging'
            unique_name "$STAGE" "$USB_MNT/update/.un260_dispatch.$$." || fatal 'Unsafe dispatcher staging directory'
            [ -d "$STAGE" ] && [ ! -L "$STAGE" ] || fatal 'Unsafe dispatcher staging directory'
            "$RESCUE/unpack" "$BUNDLE" "$STAGE" >> "$LOG" 2>&1 && usb_barrier || fatal 'Unsafe or damaged archive'
            after_hash=$(digest "$BUNDLE") || fatal 'Cannot fingerprint selected USB package'
            after_fnv=$("$RESCUE/sync" --hash-fnv64 "$BUNDLE") || fatal 'Cannot fingerprint selected USB package'
            [ "$after_hash" = "$BUNDLE_HASH" ] && [ "$after_fnv" = "$BUNDLE_FNV" ] || fatal 'Selected USB package changed'
            read_package_core || fatal 'Package engine envelope is invalid or unsupported'
            CORE_ROOT=$STAGE
            ENGINE_SOURCE=$STAGE/payload/usr/bin/ui_update.sh
            SYNC_SOURCE=$STAGE/payload/usr/local/bin/un260_storage_sync
            UNPACK_SOURCE=$STAGE/payload/usr/local/bin/un260_unpack
            STORAGE_SOURCE=$STAGE/payload/usr/local/bin/un260_app_storage
            CLEANUP_SOURCE=$STAGE/payload/usr/local/bin/un260_resource_cleanup
        else BUNDLE=none; fi
    else fatal 'USB is not a healthy writable mounted filesystem'; fi
fi
CTX=$(mktemp -d "$RAM_BASE/un260-update-engine.XXXXXX") || fatal 'Cannot create private package engine context'
unique_name "$CTX" "$RAM_BASE/un260-update-engine." || fatal 'Unsafe package engine context'
private_dir "$CTX" || fatal 'Unsafe package engine context'
copy_core "$ENGINE_SOURCE" "$CTX/engine.sh" "$ENGINE_HASH" &&
    copy_core "$SYNC_SOURCE" "$CTX/sync" "$SYNC_HASH" &&
    copy_core "$UNPACK_SOURCE" "$CTX/unpack" "$UNPACK_HASH" &&
    copy_core "$STORAGE_SOURCE" "$CTX/app_storage" "$STORAGE_HASH" &&
    copy_core "$CLEANUP_SOURCE" "$CTX/resource_cleanup" "$CLEANUP_HASH" || fatal 'Package engine core is invalid or incomplete'
grep -q '^# UN260_PACKAGE_ENGINE_V1\($\|[: ]\)' "$CTX/engine.sh" &&
    /bin/sh -n "$CTX/engine.sh" && /bin/sh -n "$CTX/app_storage" && /bin/sh -n "$CTX/resource_cleanup" || fatal 'Package engine ABI or syntax is invalid'
[ "$("$CTX/sync" --probe)" = UN260_STORAGE_SYNC_1 ] &&
    [ "$("$CTX/unpack" --probe)" = UN260_UNPACK_1 ] || fatal 'Package engine helper ABI is invalid'
printf 'UN260_UPDATE_CONTEXT_V1\npid=%s\nusb_dev=%s\nstage=%s\nbundle_path=%s\nbundle_sha256=%s\nbundle_fnv64=%s\nengine_sha256=%s\nsync_sha256=%s\nunpack_sha256=%s\napp_storage_sha256=%s\nresource_cleanup_sha256=%s\nstart_time=%s\nphase_time=%s\nlast_progress=0\nlast_stage=prepare\nguarded=1\nusb_guarded=%s\napp_guarded=0\nmode=%s\naction_arg=%s\n' \
    "$$" "$USB_DEV" "$STAGE" "$BUNDLE" "$BUNDLE_HASH" "$BUNDLE_FNV" "$ENGINE_HASH" "$SYNC_HASH" "$UNPACK_HASH" "$STORAGE_HASH" "$CLEANUP_HASH" \
    "$START_TIME" "$START_TIME" "$USB_HELD" "$MODE" "$ACTION_ARG" > "$CTX/context" && chmod 0600 "$CTX/context" || fatal 'Cannot prepare package engine context'
"$RESCUE/sync" --fd 7 "$ROOT_DIR" || fatal 'Root filesystem writeback failed'
[ "$USB_HELD" = 0 ] || usb_barrier || fatal 'USB staging writeback failed'
printf 'DISPATCH engine=%s pid=%s mode=%s stage=%s\n' "$ENGINE_HASH" "$$" "$MODE" "$STAGE" >> "$LOG"
# No subprocess or second GUI operation: the selected engine takes this PID,
# lock, descriptors and the sole exit/cleanup responsibility.
unset UN260_UPDATER_LIBRARY_ONLY
exec /bin/sh "$CTX/engine.sh" --dispatch-context "$CTX"
