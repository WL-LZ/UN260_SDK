#!/bin/sh
# UN260_STARTUP_WATCH_V1: independent UI liveness and USB recovery access.
RESCUE=/usr/local/lib/un260-updater/rescue
HEALTH=/tmp/un260-ui.health
HEARTBEAT=/tmp/un260-recovery.heartbeat
LOCK=/tmp/un260-startup-watch.lock
PID_FILE=/tmp/un260-startup-watch.pid
STOP=/tmp/un260-startup-watch.stop
BACKEND_LOCK=/tmp/un260-updater.lock
STARTUP_SECONDS=45
LIVENESS_SECONDS=4

monotonic_seconds()
{
    awk '{split($1,a,"."); print a[1]; exit}' /proc/uptime
}

health_alive()
{
    [ -f "$HEALTH" ] && [ ! -L "$HEALTH" ] && [ "$(wc -c < "$HEALTH")" -le 16 ] || return 1
    healthy_pid=$(cat "$HEALTH") || return 1
    case "$healthy_pid" in ''|0*|*[!0-9]*) return 1 ;; esac
    [ "${#healthy_pid}" -le 9 ] && kill -0 "$healthy_pid" 2>/dev/null || return 1
    healthy_exe=$(readlink "/proc/$healthy_pid/exe") || return 1
    case "$healthy_exe" in /mnt/un260-app/test_lvgl|'/mnt/un260-app/test_lvgl (deleted)') return 0 ;; *) return 1 ;; esac
}

recovery_alive()
{
    [ -f "$HEARTBEAT" ] && [ ! -L "$HEARTBEAT" ] && [ "$(wc -c < "$HEARTBEAT")" -le 16 ] || return 1
    recovery_tick=$(cat "$HEARTBEAT") || return 1
    case "$recovery_tick" in ''|*[!0-9]*) return 1 ;; esac
    [ "${#recovery_tick}" -le 12 ] || return 1
    [ "$NOW" -ge "$recovery_tick" ] && [ "$((NOW-recovery_tick))" -le 6 ]
}

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
rescue_runtime_valid()
{
    runtime_safe_file "$RESCUE/startup.sh" && runtime_safe_file "$RESCUE/startup.sha256" || return 1
    [ "$(wc -c < "$RESCUE/startup.sha256")" = 65 ] && [ "$(wc -l < "$RESCUE/startup.sha256")" = 1 ] || return 1
    rescue_expected=$(cat "$RESCUE/startup.sha256") || return 1
    [ "${#rescue_expected}" = 64 ] || return 1
    case "$rescue_expected" in *[!0-9a-f]*) return 1 ;; esac
    [ "$("$RESCUE/sync" --hash-file "$RESCUE/startup.sh")" = "$rescue_expected" ] &&
        /bin/sh -n "$RESCUE/startup.sh"
}

updater_active()
{
    [ -e "$BACKEND_LOCK" ] || [ -L "$BACKEND_LOCK" ] || return 1
    [ -d "$BACKEND_LOCK" ] && [ ! -L "$BACKEND_LOCK" ] &&
        [ "$(LC_ALL=C ls -ldn "$BACKEND_LOCK" | awk '{print $1 ":" $3}')" = drwx------:0 ] || return 0
    for backend_entry in "$BACKEND_LOCK"/* "$BACKEND_LOCK"/.[!.]* "$BACKEND_LOCK"/..?*; do
        [ -e "$backend_entry" ] || [ -L "$backend_entry" ] || continue
        [ "$backend_entry" = "$BACKEND_LOCK/pid" ] || return 0
    done
    [ -e "$BACKEND_LOCK/pid" ] || [ -L "$BACKEND_LOCK/pid" ] || return 1
    [ -f "$BACKEND_LOCK/pid" ] && [ ! -L "$BACKEND_LOCK/pid" ] &&
        [ "$(LC_ALL=C ls -ldn "$BACKEND_LOCK/pid" | awk '{print $1 ":" $3}')" = -rw-------:0 ] &&
        [ "$(wc -c < "$BACKEND_LOCK/pid")" -le 16 ] || return 0
    backend_pid=$(cat "$BACKEND_LOCK/pid") || return 0
    case "$backend_pid" in ''|0*|*[!0-9]*) return 0 ;; esac
    [ "${#backend_pid}" -le 9 ] || return 0
    kill -0 "$backend_pid" 2>/dev/null
}
watch_cleanup()
{
    watch_rc=$?
    trap - 0 HUP INT TERM
    if [ -f "$PID_FILE" ] && [ ! -L "$PID_FILE" ] && [ "$(cat "$PID_FILE")" = "$$" ]; then
        rm -f "$PID_FILE"
        rmdir "$LOCK" 2>/dev/null || :
    fi
    exit "$watch_rc"
}

if [ "${UN260_STARTUP_WATCH_LIBRARY_ONLY:-0}" = 1 ]; then return 0; fi
umask 077
if ! mkdir -m 0700 "$LOCK" 2>/dev/null; then exit 0; fi
[ ! -L "$LOCK" ] && printf '%s\n' "$$" > "$PID_FILE" || exit 1
trap watch_cleanup 0
trap 'exit 0' HUP INT TERM
START=$(monotonic_seconds) || exit 1
MISSING_SINCE=$START
SEEN_HEALTH=0
LAST_RESCUE=0
while [ ! -e "$STOP" ]; do
    NOW=$(monotonic_seconds) || exit 1
    case "$NOW" in ''|*[!0-9]*) exit 1 ;; esac
    if health_alive; then
        SEEN_HEALTH=1
        MISSING_SINCE=$NOW
    elif ! updater_active && ! recovery_alive; then
        if [ "$SEEN_HEALTH" = 0 ]; then WATCH_TIMEOUT=$STARTUP_SECONDS; else WATCH_TIMEOUT=$LIVENESS_SECONDS; fi
        if [ "$((NOW-MISSING_SINCE))" -ge "$WATCH_TIMEOUT" ] && [ "$((NOW-LAST_RESCUE))" -ge 10 ]; then
            rescue_runtime_valid || exit 1
            /bin/sh "$RESCUE/startup.sh" --rescue-only >> /tmp/un260-startup-rescue.log 2>&1 &
            LAST_RESCUE=$NOW
            MISSING_SINCE=$NOW
        fi
    fi
    sleep 2
done
