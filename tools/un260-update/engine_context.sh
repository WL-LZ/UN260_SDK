# UN260_ENGINE_CONTEXT_1: verified same-process dispatcher handoff.
# This module is assembled into the engine; context data is never shell code.
DISPATCH_CONTEXT=
DISPATCH_STAGE=none
DISPATCH_STAGE_READY=0
DISPATCH_MODE=
DISPATCH_ACTION_ARG=none
DISPATCH_EXPECTED_UID=0
DISPATCH_OWNERSHIP_VERIFIED=0
TRIAL_ENABLED=0
APP_STORAGE_TOOL=
RESOURCE_CLEANUP_TOOL=
UNPACK_TOOL=

dispatch_context_value()
{
    case "$1" in
        pid) printf '%s\n' "$dispatch_cached_pid" ;;
        usb_dev) printf '%s\n' "$dispatch_cached_usb_dev" ;;
        stage) printf '%s\n' "$dispatch_cached_stage" ;;
        bundle_path) printf '%s\n' "$dispatch_cached_bundle_path" ;;
        bundle_sha256) printf '%s\n' "$dispatch_cached_bundle_sha256" ;;
        bundle_fnv64) printf '%s\n' "$dispatch_cached_bundle_fnv64" ;;
        engine_sha256) printf '%s\n' "$dispatch_cached_engine_sha256" ;;
        sync_sha256) printf '%s\n' "$dispatch_cached_sync_sha256" ;;
        unpack_sha256) printf '%s\n' "$dispatch_cached_unpack_sha256" ;;
        app_storage_sha256) printf '%s\n' "$dispatch_cached_app_storage_sha256" ;;
        resource_cleanup_sha256) printf '%s\n' "$dispatch_cached_resource_cleanup_sha256" ;;
        start_time) printf '%s\n' "$dispatch_cached_start_time" ;;
        phase_time) printf '%s\n' "$dispatch_cached_phase_time" ;;
        last_progress) printf '%s\n' "$dispatch_cached_last_progress" ;;
        last_stage) printf '%s\n' "$dispatch_cached_last_stage" ;;
        guarded) printf '%s\n' "$dispatch_cached_guarded" ;;
        usb_guarded) printf '%s\n' "$dispatch_cached_usb_guarded" ;;
        app_guarded) printf '%s\n' "$dispatch_cached_app_guarded" ;;
        mode) printf '%s\n' "$dispatch_cached_mode" ;;
        action_arg) printf '%s\n' "$dispatch_cached_action_arg" ;;
        *) return 1 ;;
    esac
}

dispatch_context_load()
{
    dispatch_candidate=$1
    dispatch_suffix=${dispatch_candidate#/tmp/un260-update-engine.}
    [ "$dispatch_candidate" = "/tmp/un260-update-engine.$dispatch_suffix" ] &&
        [ "${#dispatch_suffix}" = 6 ] || return 1
    case "$dispatch_suffix" in *[!A-Za-z0-9]*) return 1 ;; esac
    [ "$(id -u)" = "$DISPATCH_EXPECTED_UID" ] &&
        [ -d /tmp ] && [ ! -L /tmp ] && [ -d "$dispatch_candidate" ] &&
        [ ! -L "$dispatch_candidate" ] &&
        [ "$(LC_ALL=C ls -ldn "$dispatch_candidate" | awk '{print $1 ":" $3}')" = "drwx------:$DISPATCH_EXPECTED_UID" ] || return 1
    [ -f "$dispatch_candidate/context" ] && [ ! -L "$dispatch_candidate/context" ] &&
        [ "$(LC_ALL=C ls -ldn "$dispatch_candidate/context" | awk '{print $1 ":" $3}')" = "-rw-------:$DISPATCH_EXPECTED_UID" ] || return 1
    dispatch_record_bytes=$(wc -c < "$dispatch_candidate/context") || return 1
    [ "$dispatch_record_bytes" -le 4096 ] &&
        [ "$(wc -l < "$dispatch_candidate/context")" = 21 ] || return 1
    DISPATCH_CONTEXT=$dispatch_candidate
    # This record is data, never shell code. Validate each unique fixed-order
    # key and cache its value once rather than spawning sed/awk per lookup.
    {
        IFS= read -r dispatch_record && [ "$dispatch_record" = UN260_UPDATE_CONTEXT_V1 ] || return 1
        dispatch_parsed_bytes=$((${#dispatch_record}+1))
        for dispatch_key in pid usb_dev stage bundle_path bundle_sha256 bundle_fnv64 \
            engine_sha256 sync_sha256 unpack_sha256 app_storage_sha256 resource_cleanup_sha256 \
            start_time phase_time last_progress last_stage guarded usb_guarded app_guarded mode action_arg; do
            IFS= read -r dispatch_record || return 1
            dispatch_parsed_bytes=$((dispatch_parsed_bytes+${#dispatch_record}+1))
            case "$dispatch_record" in "$dispatch_key="*) ;; *) return 1 ;; esac
            dispatch_value=${dispatch_record#*=}
            case "$dispatch_value" in *'='*) return 1 ;; esac
            case "$dispatch_key" in
                pid) dispatch_cached_pid=$dispatch_value ;;
                usb_dev) dispatch_cached_usb_dev=$dispatch_value ;;
                stage) dispatch_cached_stage=$dispatch_value ;;
                bundle_path) dispatch_cached_bundle_path=$dispatch_value ;;
                bundle_sha256) dispatch_cached_bundle_sha256=$dispatch_value ;;
                bundle_fnv64) dispatch_cached_bundle_fnv64=$dispatch_value ;;
                engine_sha256) dispatch_cached_engine_sha256=$dispatch_value ;;
                sync_sha256) dispatch_cached_sync_sha256=$dispatch_value ;;
                unpack_sha256) dispatch_cached_unpack_sha256=$dispatch_value ;;
                app_storage_sha256) dispatch_cached_app_storage_sha256=$dispatch_value ;;
                resource_cleanup_sha256) dispatch_cached_resource_cleanup_sha256=$dispatch_value ;;
                start_time) dispatch_cached_start_time=$dispatch_value ;;
                phase_time) dispatch_cached_phase_time=$dispatch_value ;;
                last_progress) dispatch_cached_last_progress=$dispatch_value ;;
                last_stage) dispatch_cached_last_stage=$dispatch_value ;;
                guarded) dispatch_cached_guarded=$dispatch_value ;;
                usb_guarded) dispatch_cached_usb_guarded=$dispatch_value ;;
                app_guarded) dispatch_cached_app_guarded=$dispatch_value ;;
                mode) dispatch_cached_mode=$dispatch_value ;;
                action_arg) dispatch_cached_action_arg=$dispatch_value ;;
            esac
        done
        if IFS= read -r dispatch_record || [ -n "$dispatch_record" ]; then return 1; fi
    } < "$DISPATCH_CONTEXT/context" || return 1
    # All legitimate fields are ASCII. read may drop embedded NUL bytes, so
    # compare the complete original byte count before trusting cached data.
    [ "$dispatch_parsed_bytes" = "$dispatch_record_bytes" ] || return 1
    [ "$(dispatch_context_value pid)" = "$$" ] &&
        [ -d "$LOCK_DIR" ] && [ ! -L "$LOCK_DIR" ] &&
        [ "$(LC_ALL=C ls -ldn "$LOCK_DIR" | awk '{print $1 ":" $3}')" = "drwx------:$DISPATCH_EXPECTED_UID" ] &&
        [ -f "$LOCK_DIR/pid" ] && [ ! -L "$LOCK_DIR/pid" ] &&
        [ "$(cat "$LOCK_DIR/pid")" = "$$" ] || return 1
    DISPATCH_OWNERSHIP_VERIFIED=1
    for dispatch_role in engine sync unpack app_storage resource_cleanup; do
        dispatch_hash=$(dispatch_context_value "${dispatch_role}_sha256") || return 1
        boot_hex64 "$dispatch_hash" || return 1
        if [ "$dispatch_role" = engine ]; then dispatch_file=$DISPATCH_CONTEXT/engine.sh; dispatch_permissions=-rw-------;
        else dispatch_file=$DISPATCH_CONTEXT/$dispatch_role; dispatch_permissions=-rwxr-xr-x; fi
        [ -f "$dispatch_file" ] && [ ! -L "$dispatch_file" ] &&
            [ "$(LC_ALL=C ls -ldn "$dispatch_file" | awk '{print $1 ":" $3}')" = "$dispatch_permissions:$DISPATCH_EXPECTED_UID" ] &&
            [ "$(wc -c < "$dispatch_file")" -gt 0 ] && [ "$(wc -c < "$dispatch_file")" -le 262144 ] &&
            [ "$(checked_digest "$dispatch_file")" = "$dispatch_hash" ] || return 1
    done
    SYNC_TOOL=$DISPATCH_CONTEXT/sync
    UNPACK_TOOL=$DISPATCH_CONTEXT/unpack
    APP_STORAGE_TOOL=$DISPATCH_CONTEXT/app_storage
    RESOURCE_CLEANUP_TOOL=$DISPATCH_CONTEXT/resource_cleanup
    [ "$("$SYNC_TOOL" --probe)" = UN260_STORAGE_SYNC_1 ] &&
        [ "$("$UNPACK_TOOL" --probe)" = UN260_UNPACK_1 ] &&
        "$SYNC_TOOL" --fd 7 / || return 1
    GUARDED=$(dispatch_context_value guarded) || return 1
    USB_GUARDED=$(dispatch_context_value usb_guarded) || return 1
    [ "$GUARDED" = 1 ] && [ "$(dispatch_context_value app_guarded)" = 0 ] || return 1
    USB_DEV=$(dispatch_context_value usb_dev) || return 1
    DISPATCH_STAGE=$(dispatch_context_value stage) || return 1
    BUNDLE_PATH=$(dispatch_context_value bundle_path) || return 1
    DISPATCH_BUNDLE_HASH=$(dispatch_context_value bundle_sha256) || return 1
    BUNDLE_FNV=$(dispatch_context_value bundle_fnv64) || return 1
    DISPATCH_MODE=$(dispatch_context_value mode) || return 1
    DISPATCH_ACTION_ARG=$(dispatch_context_value action_arg) || return 1
    START_TIME=$(dispatch_context_value start_time) || return 1
    PHASE_TIME=$(dispatch_context_value phase_time) || return 1
    case "$START_TIME:$PHASE_TIME" in ''|*[!0-9:]*) return 1 ;; esac
    [ -n "$START_TIME" ] && [ -n "$PHASE_TIME" ] &&
        [ "$(dispatch_context_value last_progress)" = 0 ] &&
        [ "$(dispatch_context_value last_stage)" = prepare ] || return 1
    case "$DISPATCH_MODE" in update|recover|continue|pending|trial-installed|trial-pending|trial-confirm|trial-rollback|app-check) ;; *) return 1 ;; esac
    if [ "$DISPATCH_MODE" = trial-confirm ]; then
        case "$DISPATCH_ACTION_ARG" in ''|0*|*[!0-9]*) return 1 ;; esac
        [ "${#DISPATCH_ACTION_ARG}" -le 9 ] || return 1
    else [ "$DISPATCH_ACTION_ARG" = none ] || return 1; fi
    if [ "$USB_GUARDED" = 1 ]; then
        case "$USB_DEV" in /dev/sd[a-z]|/dev/sd[a-z][0-9]*) ;; *) return 1 ;; esac
        [ -b "$USB_DEV" ] && usb_barrier || return 1
    elif [ "$USB_GUARDED" = 0 ]; then
        [ "$USB_DEV" = none ] && [ "$DISPATCH_STAGE" = none ] && [ "$BUNDLE_PATH" = none ] &&
            [ "$DISPATCH_BUNDLE_HASH" = none ] && [ "$BUNDLE_FNV" = none ] || return 1
    else return 1; fi
    if [ "$DISPATCH_STAGE" != none ]; then
        dispatch_stage_suffix=${DISPATCH_STAGE#"$UPDATE_DIR/.un260_dispatch.$$."}
        [ "$DISPATCH_STAGE" = "$UPDATE_DIR/.un260_dispatch.$$.$dispatch_stage_suffix" ] &&
            [ "${#dispatch_stage_suffix}" = 6 ] || return 1
        case "$dispatch_stage_suffix" in *[!A-Za-z0-9]*) return 1 ;; esac
        [ -d "$DISPATCH_STAGE" ] && [ ! -L "$DISPATCH_STAGE" ] &&
            [ "$BUNDLE_PATH" = "$UPDATE_DIR/UN260_UPDATE.upk" ] &&
            boot_hex64 "$DISPATCH_BUNDLE_HASH" && [ "${#BUNDLE_FNV}" = 16 ] || return 1
        case "$BUNDLE_FNV" in *[!0-9a-f]*) return 1 ;; esac
        [ -f "$BUNDLE_PATH" ] && [ ! -L "$BUNDLE_PATH" ] &&
            [ "$(checked_digest "$BUNDLE_PATH")" = "$DISPATCH_BUNDLE_HASH" ] &&
            [ "$("$SYNC_TOOL" --hash-fnv64 "$BUNDLE_PATH")" = "$BUNDLE_FNV" ] || return 1
        case "$DISPATCH_MODE" in update|continue) ;; *) return 1 ;; esac
        DISPATCH_STAGE_READY=1
    elif [ "$DISPATCH_MODE" = update ]; then return 1; fi
    UN260_ENGINE_SYNC=$SYNC_TOOL
    export UN260_ENGINE_SYNC
    TRIAL_ENABLED=1
    return 0
}

dispatch_engine_cleanup()
{
    dispatch_cleanup_rc=$?
    trap - 0 HUP INT TERM
    if [ "$DISPATCH_STAGE" != none ] && [ -d "$DISPATCH_STAGE" ]; then
        dispatch_cleanup_suffix=${DISPATCH_STAGE#"$UPDATE_DIR/.un260_dispatch.$$."}
        case "$dispatch_cleanup_suffix" in ''|*[!A-Za-z0-9]*) dispatch_cleanup_safe=0 ;; *) dispatch_cleanup_safe=1 ;; esac
        if [ "$DISPATCH_STAGE" = "$UPDATE_DIR/.un260_dispatch.$$.$dispatch_cleanup_suffix" ] &&
            [ "${#dispatch_cleanup_suffix}" = 6 ] && [ "$dispatch_cleanup_safe" = 1 ] &&
            usb_barrier && [ ! -L "$DISPATCH_STAGE" ]; then
            rm -rf "$DISPATCH_STAGE" && usb_barrier || dispatch_cleanup_rc=1
        else dispatch_cleanup_rc=1; fi
    fi
    if [ -n "$DISPATCH_CONTEXT" ]; then
        [ -d "$DISPATCH_CONTEXT" ] && [ ! -L "$DISPATCH_CONTEXT" ] &&
            rm -rf "$DISPATCH_CONTEXT" || dispatch_cleanup_rc=1
    fi
    if [ -f "$LOCK_DIR/pid" ] && [ ! -L "$LOCK_DIR/pid" ] && [ "$(cat "$LOCK_DIR/pid")" = "$$" ]; then
        rm -f "$LOCK_DIR/pid" && rmdir "$LOCK_DIR" || dispatch_cleanup_rc=1
    else dispatch_cleanup_rc=1; fi
    exit "$dispatch_cleanup_rc"
}
