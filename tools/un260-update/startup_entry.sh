#!/bin/sh
# UN260_APP_VOLUME_V1
# UN260_STORAGE_SYNC_1
# UN260_STORAGE_POLICY_V2
# UN260_STORAGE_POLICY_V3
# UN260_BOOT_BUNDLE_1
# UN260_STARTUP_ENTRY_V1: immutable selector; startup policy lives in runtime.
RESCUE=/usr/local/lib/un260-updater/rescue
RUNTIME=/usr/local/lib/un260-startup/runtime.sh
WATCH_PID=/tmp/un260-startup-watch.pid
WATCH_STOP=/tmp/un260-startup-watch.stop

runtime_safe_file()
{
    runtime_walk=$1
    [ -f "$runtime_walk" ] && [ ! -L "$runtime_walk" ] || return 1
    while [ "$runtime_walk" != / ]; do
        [ ! -L "$runtime_walk" ] || return 1
        runtime_parent=${runtime_walk%/*}
        [ -n "$runtime_parent" ] || runtime_parent=/
        [ "$runtime_parent" != "$runtime_walk" ] || return 1
        runtime_walk=$runtime_parent
    done
}
runtime_valid()
{
    runtime_safe_file "$1" && runtime_safe_file "$2" || return 1
    [ "$(wc -c < "$2")" = 65 ] && [ "$(wc -l < "$2")" = 1 ] || return 1
    runtime_expected=$(cat "$2") || return 1
    [ "${#runtime_expected}" = 64 ] || return 1
    case "$runtime_expected" in *[!0-9a-f]*) return 1 ;; esac
    runtime_actual=$("$RESCUE/sync" --hash-file "$1") || return 1
    [ "$runtime_actual" = "$runtime_expected" ] && /bin/sh -n "$1"
}

select_runtime()
{
    if runtime_valid "$RUNTIME" /etc/un260/startup-runtime.sha256; then
        SELECTED_RUNTIME=$RUNTIME
    elif runtime_valid "$RESCUE/startup.sh" "$RESCUE/startup.sha256"; then
        SELECTED_RUNTIME=$RESCUE/startup.sh
    else
        printf '%s\n' 'UN260_STARTUP: current and factory startup runtimes are invalid; no runtime started' >&2
        return 1
    fi
}

stop_watch()
{
    : > "$WATCH_STOP" || return 1
    if [ -f "$WATCH_PID" ] && [ ! -L "$WATCH_PID" ]; then
        watch_pid=$(cat "$WATCH_PID") || return 1
        case "$watch_pid" in ''|0*|*[!0-9]*) return 1 ;; esac
        [ "${#watch_pid}" -le 9 ] || return 1
        if [ -r "/proc/$watch_pid/cmdline" ] &&
            tr '\000' '\n' < "/proc/$watch_pid/cmdline" | grep -Fx "$RESCUE/watch.sh" >/dev/null; then
            kill "$watch_pid" 2>/dev/null || :
        fi
    fi
}

case "${1:-}" in
    prepare-devices)
        [ "$#" -eq 1 ] || exit 2
        select_runtime || exit 1
        exec /bin/sh "$SELECTED_RUNTIME" prepare-devices
        ;;
    start)
        rm -f "$WATCH_STOP"
        if runtime_valid "$RESCUE/watch.sh" "$RESCUE/watch.sha256"; then
            /bin/sh "$RESCUE/watch.sh" >> /tmp/un260-startup-watch.log 2>&1 &
        else
            printf '%s\n' 'UN260_STARTUP: factory UI watcher validation failed' >&2
        fi
        if select_runtime; then
            /bin/sh "$SELECTED_RUNTIME" start >> /tmp/un260-startup-runtime.log 2>&1 &
        else
            exit 1
        fi
        ;;
    stop)
        stop_watch || :
        if select_runtime; then /bin/sh "$SELECTED_RUNTIME" stop; fi
        ;;
    restart|reload)
        "$0" stop
        "$0" start
        ;;
    *) exit 2 ;;
esac
