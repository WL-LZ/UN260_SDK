# UN260_ENGINE_TRIAL_V1: keep the complete prior transaction until UI health.
# Assembled into the package engine; all records are data, never sourced code.
TRIAL_EXPECTED_UID=0
TRIAL_BOOT_ID_PATH=/proc/sys/kernel/random/boot_id
TRIAL_HEALTH_PATH=/tmp/un260-ui.health
TRIAL_PROC=/proc

trial_safe_path()
{
    trial_walk=$1
    case "$trial_walk" in /*) ;; *) return 1 ;; esac
    [ ! -L "$trial_walk" ] || return 1
    while [ "$trial_walk" != / ]; do
        trial_walk=${trial_walk%/*}
        [ -n "$trial_walk" ] || trial_walk=/
        [ -d "$trial_walk" ] && [ ! -L "$trial_walk" ] || return 1
    done
}

trial_private_file()
{
    trial_safe_path "$1" && [ -f "$1" ] &&
        [ "$(LC_ALL=C ls -ln "$1" | awk '{print $1 ":" $3}')" = "-rw-------:$TRIAL_EXPECTED_UID" ]
}

trial_journal_valid()
{
    trial_private_file "$JOURNAL/state" && [ -d "$JOURNAL" ] &&
        [ "$(LC_ALL=C ls -ldn "$JOURNAL" | awk '{print $1 ":" $3}')" = "drwx------:$TRIAL_EXPECTED_UID" ] &&
        [ "$(wc -c < "$JOURNAL/state")" -le 3145728 ] || return 1
    for trial_entry in "$JOURNAL"/* "$JOURNAL"/.[!.]* "$JOURNAL"/..?*; do
        [ -e "$trial_entry" ] || [ -L "$trial_entry" ] || continue
        [ -f "$trial_entry" ] && [ ! -L "$trial_entry" ] || return 1
        case "${trial_entry##*/}" in
            state|committed|rolled-back|boot-publish|boot-publish.tmp|boot-gc|boot-gc.tmp) ;;
            trial|trial.rollback|trial.confirmed|trial.tmp|trial.targets|trial.targets.tmp|\
            trial.boot-active|trial.boot-active.tmp|trial.boot-active.previous|trial.boot-active.previous.tmp|\
            trial.installed.fnv64|trial.installed.fnv64.tmp|trial.unified.completed|trial.unified.completed.tmp)
                trial_private_file "$trial_entry" || return 1 ;;
            *) return 1 ;;
        esac
    done
    for trial_name in committed rolled-back; do
        [ ! -e "$JOURNAL/$trial_name" ] || [ "$(wc -c < "$JOURNAL/$trial_name")" = 0 ] || return 1
    done
    trial_state_lines=$(wc -l < "$JOURNAL/state") || return 1
    LC_ALL=C awk -F '|' -v lines="$trial_state_lines" '
        function safe(p) {return p!="" && length(p)<=255 && p!~/[^A-Za-z0-9_.\/-]/ && p!~/^\// &&
            p!~/\.\./ && p!~/\/\// && p!~/\.un260-/ && p!~/\/$/ &&
            (p~/^usr\/local\/(bin|lib|share)\// || p~/^etc\/un260\// ||
             p=="usr/bin/ui_update.sh" || p=="etc/init.d/S00lvgl") &&
            p!~/^etc\/un260\/(app-storage|boot-active)(\/|\.|$)/ &&
            p!~/^usr\/local\/lib\/un260-updater(\/|$)/ &&
            p!="usr/local/bin/un260_update_dispatch" && p!="etc/un260/updater-platform"}
        NF!=2 || $1!~/^[01]$/ || !safe($2) || seen[$2]++ {bad=1}
        END {exit (bad || NR>8192 || NR!=lines)}' "$JOURNAL/state"
}

trial_load_record()
{
    trial_private_file "$1" && [ "$(wc -c < "$1")" -le 1024 ] &&
        [ "$(wc -l < "$1")" = 11 ] || return 1
    LC_ALL=C awk -F= '
        function hex(s) {return length(s)==64 && s~/^[0-9a-f]+$/}
        NR==1 {if($0!="UN260_UPDATE_TRIAL_V1") bad=1; next}
        NR==2 {if(NF!=2 || $1!="boot_id" || length($2)!=36 ||
            $2!~/^[0-9a-f]+-[0-9a-f]+-[0-9a-f]+-[0-9a-f]+-[0-9a-f]+$/ ||
            substr($2,9,1)!="-" || substr($2,14,1)!="-" || substr($2,19,1)!="-" || substr($2,24,1)!="-") bad=1; next}
        NR==3 {if(NF!=2 || $1!="app_sha256" || !hex($2)) bad=1; next}
        NR==4 {if(NF!=2 || $1!="target_id" || !hex($2)) bad=1; next}
        NR==5 {if(NF!=2 || $1!="boot_bundle_id" || ($2!="none" && !hex($2))) bad=1; b=$2; next}
        NR==6 {if(NF!=2 || $1!="boot_manifest_sha256" || ($2!="none" && !hex($2)) || ((b=="none")!=($2=="none"))) bad=1; next}
        NR>=7 && NR<=10 {
            split("old_active_sha256 old_previous_sha256 old_installed_fnv64_sha256 old_unified_completed_sha256",k," ")
            if(NF!=2 || $1!=k[NR-6] || ($2!="none" && !hex($2))) bad=1; next}
        NR==11 {if(NF!=2 || $1!="targets_count" || $2!~/^[1-9][0-9]*$/ || length($2)>4 || $2+0>8192) bad=1; next}
        END {exit (bad || NR!=11)}' "$1" || return 1
    {
        read -r trial_header
        read -r trial_install_boot
        read -r trial_app_sha
        read -r trial_target_id
        read -r trial_boot_id
        read -r trial_boot_sha
        read -r trial_old_active
        read -r trial_old_previous
        read -r trial_old_fnv
        read -r trial_old_completed
        read -r trial_targets_count
    } < "$1" || return 1
    trial_install_boot=${trial_install_boot#boot_id=}
    trial_app_sha=${trial_app_sha#app_sha256=}
    trial_target_id=${trial_target_id#target_id=}
    trial_boot_id=${trial_boot_id#boot_bundle_id=}
    trial_boot_sha=${trial_boot_sha#boot_manifest_sha256=}
    trial_old_active=${trial_old_active#old_active_sha256=}
    trial_old_previous=${trial_old_previous#old_previous_sha256=}
    trial_old_fnv=${trial_old_fnv#old_installed_fnv64_sha256=}
    trial_old_completed=${trial_old_completed#old_unified_completed_sha256=}
    trial_targets_count=${trial_targets_count#targets_count=}
}

trial_table_valid()
{
    trial_private_file "$1" && [ "$(wc -c < "$1")" -le 3145728 ] &&
        [ "$(wc -l < "$1")" = "$trial_targets_count" ] &&
        [ "$(checked_digest "$1")" = "$trial_target_id" ] || return 1
    trial_table_format_valid "$1"
}

trial_table_format_valid()
{
    LC_ALL=C awk -F '|' '
        function safe(p) {return p!="" && length(p)<=255 && p!~/[^A-Za-z0-9_.\/-]/ && p!~/^\// &&
            p!~/\.\./ && p!~/\/\// && p!~/\.un260-/ && p!~/\/$/ &&
            (p~/^usr\/local\/(bin|lib|share)\// || p~/^etc\/un260\// ||
             p=="usr/bin/ui_update.sh" || p=="etc/init.d/S00lvgl") &&
            p!~/^etc\/un260\/(app-storage|boot-active)(\/|\.|$)/ &&
            p!~/^usr\/local\/lib\/un260-updater(\/|$)/ &&
            p!="usr/local/bin/un260_update_dispatch" && p!="etc/un260/updater-platform"}
        NF!=4 || length($1)!=64 || $1!~/^[0-9a-f]+$/ || $2!~/^0(644|755)$/ ||
            $3!~/^(0|[1-9][0-9]*)$/ || length($3)>8 || $3+0>25165824 ||
            !safe($4) || (NR>1 && previous>=$4) {bad=1}
        {previous=$4; total+=$3}
        $4=="usr/local/bin/test_lvgl" {app=$1; appmode=$2; appbytes=$3}
        END {exit (bad || NR<1 || NR>8192 || NR!=count || total>33554432 || appmode!="0755" || appbytes<4 || app!=expected)}
        ' expected="$trial_app_sha" count="$trial_targets_count" "$1"
}

trial_snapshot_valid()
{
    if [ "$2" = none ]; then [ ! -e "$JOURNAL/$1" ] && [ ! -L "$JOURNAL/$1" ]; return $?; fi
    trial_private_file "$JOURNAL/$1" && [ "$(wc -c < "$JOURNAL/$1")" -le 400 ] &&
        [ "$(checked_digest "$JOURNAL/$1")" = "$2" ] || return 1
    case "$1" in
        trial.boot-active|trial.boot-active.previous) boot_read_descriptor "$JOURNAL/$1" ;;
        trial.installed.fnv64)
            [ "$(wc -c < "$JOURNAL/$1")" = 17 ] && [ "$(wc -l < "$JOURNAL/$1")" = 1 ] &&
                LC_ALL=C grep -Eq '^[0-9a-f]{16}$' "$JOURNAL/$1" ;;
        trial.unified.completed)
            [ "$(wc -c < "$JOURNAL/$1")" = 65 ] && [ "$(wc -l < "$JOURNAL/$1")" = 1 ] &&
                boot_hex64 "$(cat "$JOURNAL/$1")" ;;
        *) return 1 ;;
    esac
}

trial_pending_valid()
{
    { [ "$trial_old_active" != none ] || [ "$trial_old_previous" = none ]; } &&
        trial_table_valid "$JOURNAL/trial.targets" &&
        trial_snapshot_valid trial.boot-active "$trial_old_active" &&
        trial_snapshot_valid trial.boot-active.previous "$trial_old_previous" &&
        trial_snapshot_valid trial.installed.fnv64 "$trial_old_fnv" &&
        trial_snapshot_valid trial.unified.completed "$trial_old_completed"
}

trial_decision()
{
    trial_kind=none
    for trial_name in trial trial.rollback trial.confirmed; do
        [ -e "$JOURNAL/$trial_name" ] || [ -L "$JOURNAL/$trial_name" ] || continue
        [ "$trial_kind" = none ] || return 1
        trial_load_record "$JOURNAL/$trial_name" || return 1
        trial_kind=$trial_name
    done
}

trial_is_committed()
{
    [ -e "$JOURNAL" ] || [ -L "$JOURNAL" ] || return 1
    trial_journal_valid && trial_decision || return 2
    case "$trial_kind" in
        none)
            if [ -e "$JOURNAL/committed" ]; then
                for trial_entry in "$JOURNAL"/trial*; do [ ! -e "$trial_entry" ] || return 2; done
            fi
            return 1 ;;
        trial.confirmed)
            [ -f "$JOURNAL/committed" ] && [ ! -e "$JOURNAL/rolled-back" ] || return 2
            return 1 ;;
        trial.rollback)
            if [ -f "$JOURNAL/rolled-back" ]; then [ ! -e "$JOURNAL/committed" ] || return 2;
            else trial_pending_valid || return 2; fi
            return 1 ;;
    esac
    trial_pending_valid || return 2
    [ ! -e "$JOURNAL/rolled-back" ] || return 2
    [ -f "$JOURNAL/committed" ] || return 1
}

trial_current_boot()
{
    trial_safe_path "$TRIAL_BOOT_ID_PATH" && [ -f "$TRIAL_BOOT_ID_PATH" ] || return 1
    trial_now_boot=$(cat "$TRIAL_BOOT_ID_PATH") || return 1
    [ "${#trial_now_boot}" = 36 ] || return 1
    case "$trial_now_boot" in *[!0-9a-f-]*) return 1 ;; esac
    [ "$(wc -c < "$TRIAL_BOOT_ID_PATH")" = 37 ] && [ "$(wc -l < "$TRIAL_BOOT_ID_PATH")" = 1 ] &&
        LC_ALL=C awk -F- 'NF!=5 || length($1)!=8 || length($2)!=4 || length($3)!=4 || length($4)!=4 || length($5)!=12 {bad=1}
            END {exit (bad || NR!=1)}' "$TRIAL_BOOT_ID_PATH"
}

trial_boot_eligible()
{
    trial_is_committed || return $?
    trial_current_boot || return 2
    [ "$trial_now_boot" != "$trial_install_boot" ]
}

trial_copy_snapshot()
{
    trial_snapshot_sha=none
    trial_safe_path "$1" || return 1
    [ -e "$1" ] || return 0
    [ -f "$1" ] && [ "$(wc -c < "$1")" -le 400 ] || return 1
    case "$2" in trial.boot-active|trial.boot-active.previous)
        [ "$(LC_ALL=C ls -ln "$1" | awk '{print $1 ":" $3}')" = "-rw-r--r--:$TRIAL_EXPECTED_UID" ] || return 1 ;;
    esac
    trial_source_sha=$(checked_digest "$1") || return 1
    [ ! -e "$JOURNAL/$2" ] && [ ! -L "$JOURNAL/$2" ] &&
        [ ! -e "$JOURNAL/$2.tmp" ] && [ ! -L "$JOURNAL/$2.tmp" ] || return 1
    (umask 077; dd if="$1" of="$JOURNAL/$2.tmp" bs=400 count=1 2>/dev/null) &&
        chmod 0600 "$JOURNAL/$2.tmp" &&
        [ "$(checked_digest "$JOURNAL/$2.tmp")" = "$trial_source_sha" ] &&
        mv "$JOURNAL/$2.tmp" "$JOURNAL/$2" || return 1
    trial_snapshot_sha=$trial_source_sha
    trial_snapshot_valid "$2" "$trial_snapshot_sha"
}

trial_prepare()
{
    [ "$TRIAL_ENABLED" = 1 ] && [ "$DISPATCH_MODE" = update ] &&
        [ "$package_schema" = 1 ] && [ "$(manifest_value package_type)" = ui ] || return 0
    trial_journal_valid && trial_decision && [ "$trial_kind" = none ] &&
        [ ! -e "$JOURNAL/committed" ] && [ ! -e "$JOURNAL/rolled-back" ] || return 1
    for trial_name in trial.tmp trial.targets trial.targets.tmp trial.boot-active trial.boot-active.tmp \
        trial.boot-active.previous trial.boot-active.previous.tmp trial.installed.fnv64 trial.installed.fnv64.tmp \
        trial.unified.completed trial.unified.completed.tmp; do
        [ ! -e "$JOURNAL/$trial_name" ] && [ ! -L "$JOURNAL/$trial_name" ] || return 1
    done
    trial_current_boot || return 1
    trial_install_boot=$trial_now_boot
    trial_target_id=$FULL_TARGET_ID
    [ -f "$STAGE_DIR/target.tsv" ] && [ ! -L "$STAGE_DIR/target.tsv" ] || return 1
    trial_targets_count=$(wc -l < "$STAGE_DIR/target.tsv") || return 1
    trial_targets_count=$(printf '%s' "$trial_targets_count" | tr -d ' ')
    trial_app_sha=$(awk -F '|' '$4=="usr/local/bin/test_lvgl" {print $1}' "$STAGE_DIR/target.tsv") || return 1
    boot_hex64 "$trial_app_sha" && boot_hex64 "$trial_target_id" &&
        [ -f "$STAGE_DIR/target.tsv" ] && [ ! -L "$STAGE_DIR/target.tsv" ] &&
        [ "$(wc -c < "$STAGE_DIR/target.tsv")" -le 3145728 ] || return 1
    (umask 077; dd if="$STAGE_DIR/target.tsv" of="$JOURNAL/trial.targets.tmp" bs=1024 count=3072 2>/dev/null) &&
        chmod 0600 "$JOURNAL/trial.targets.tmp" && trial_table_valid "$JOURNAL/trial.targets.tmp" &&
        mv "$JOURNAL/trial.targets.tmp" "$JOURNAL/trial.targets" || return 1
    trial_copy_snapshot "$ROOT_PREFIX/$BOOT_ACTIVE" trial.boot-active || return 1
    trial_old_active=$trial_snapshot_sha
    trial_copy_snapshot "$ROOT_PREFIX/$BOOT_ACTIVE.previous" trial.boot-active.previous || return 1
    trial_old_previous=$trial_snapshot_sha
    [ "$trial_old_active" != none ] || [ "$trial_old_previous" = none ] || return 1
    trial_copy_snapshot "$INSTALLED_HASH_PATH" trial.installed.fnv64 || return 1
    trial_old_fnv=$trial_snapshot_sha
    trial_copy_snapshot "$INSTALLED_STATE_DIR/unified.completed" trial.unified.completed || return 1
    trial_old_completed=$trial_snapshot_sha
    trial_boot_id=${BOOT_REQUEST_ID:-none}
    trial_boot_sha=${BOOT_REQUEST_HASH:-none}
    if [ "$trial_boot_id" = none ] && [ "$trial_old_active" != none ]; then
        boot_read_descriptor "$JOURNAL/trial.boot-active" || return 1
        trial_boot_id=$boot_current
        trial_boot_sha=$boot_current_hash
    fi
    (umask 077; printf 'UN260_UPDATE_TRIAL_V1\nboot_id=%s\napp_sha256=%s\ntarget_id=%s\nboot_bundle_id=%s\nboot_manifest_sha256=%s\nold_active_sha256=%s\nold_previous_sha256=%s\nold_installed_fnv64_sha256=%s\nold_unified_completed_sha256=%s\ntargets_count=%s\n' \
        "$trial_install_boot" "$trial_app_sha" "$trial_target_id" "$trial_boot_id" "$trial_boot_sha" \
        "$trial_old_active" "$trial_old_previous" "$trial_old_fnv" "$trial_old_completed" "$trial_targets_count" > "$JOURNAL/trial.tmp") &&
        chmod 0600 "$JOURNAL/trial.tmp" && trial_load_record "$JOURNAL/trial.tmp" &&
        trial_pending_valid && local_barrier && mv "$JOURNAL/trial.tmp" "$JOURNAL/trial" && local_barrier
}

trial_runtime_file()
{
    trial_row=$(awk -F '|' -v p="$1" '$4==p {print $1 "|" $2 "|" $3; found++} END {if(found!=1) exit 1}' "$JOURNAL/trial.targets") || return 1
    trial_hash=${trial_row%%|*}
    trial_row=${trial_row#*|}
    trial_mode=${trial_row%%|*}
    trial_bytes=${trial_row#*|}
    safe_destination "$1" || return 1
    trial_file=$(destination_for "$1") || return 1
    [ -f "$trial_file" ] && [ ! -L "$trial_file" ] &&
        [ "$(wc -c < "$trial_file")" = "$trial_bytes" ] &&
        [ "$(checked_digest "$trial_file")" = "$trial_hash" ] || return 1
    case "$trial_mode" in 0755) trial_permissions=-rwxr-xr-x ;; 0644) trial_permissions=-rw-r--r-- ;; *) return 1 ;; esac
    [ "$(LC_ALL=C ls -ln "$trial_file" | awk '{print $1 ":" $3}')" = "$trial_permissions:$TRIAL_EXPECTED_UID" ]
}

trial_runtime_valid()
{
    trial_is_committed
    trial_rc=$?
    [ "$trial_rc" != 1 ] || return 0
    [ "$trial_rc" = 0 ] || return "$trial_rc"
    for trial_runtime in usr/local/bin/test_lvgl usr/local/lib/liblvgl.so \
        usr/local/lib/un260-startup/runtime.sh etc/un260/update-core.manifest etc/un260/startup-runtime.sha256; do
        trial_runtime_file "$trial_runtime" || return 2
    done
}

trial_restore_snapshot()
{
    trial_safe_path "$2" && [ ! -L "$2.tmp" ] &&
        { [ ! -e "$2" ] || [ -f "$2" ]; } &&
        { [ ! -e "$2.tmp" ] || [ -f "$2.tmp" ]; } || return 1
    if [ "$3" = none ]; then rm -f "$2" "$2.tmp" && local_barrier; return $?; fi
    trial_snapshot_valid "$1" "$3" || return 1
    case "$1" in trial.boot-active|trial.boot-active.previous)
        boot_read_descriptor "$JOURNAL/$1" &&
            boot_validate_bundle "${ROOT_PREFIX:-/}" "$boot_current" "$boot_current_hash" || return 1
        if [ "$boot_previous" != none ]; then
            boot_validate_bundle "${ROOT_PREFIX:-/}" "$boot_previous" "$boot_previous_hash" || return 1
        fi ;;
    esac
    (umask 077; dd if="$JOURNAL/$1" of="$2.tmp" bs=400 count=1 2>/dev/null) &&
        chmod "$4" "$2.tmp" && [ "$(checked_digest "$2.tmp")" = "$3" ] &&
        local_barrier && mv -f "$2.tmp" "$2" && local_barrier
}

trial_recover_prepare()
{
    trial_journal_valid && trial_decision || return 2
    case "$trial_kind" in
        none)
            # Interrupted preparation is uncommitted. A completed decision is
            # deleted last, so a committed journal cannot have orphan snapshots.
            if [ -e "$JOURNAL/committed" ]; then
                for trial_entry in "$JOURNAL"/trial*; do
                    [ ! -e "$trial_entry" ] || return 2
                done
            fi
            return 0 ;;
        trial.confirmed)
            [ -f "$JOURNAL/committed" ] && [ ! -e "$JOURNAL/rolled-back" ] || return 2
            return 0 ;;
        trial)
            trial_pending_valid && [ ! -e "$JOURNAL/rolled-back" ] || return 2
            [ ! -e "$JOURNAL/committed" ] || return 0
            mv "$JOURNAL/trial" "$JOURNAL/trial.rollback" && local_barrier || return 2 ;;
        trial.rollback)
            if [ -f "$JOURNAL/rolled-back" ] && [ ! -e "$JOURNAL/committed" ]; then return 0; fi
            [ ! -e "$JOURNAL/rolled-back" ] && trial_pending_valid || return 2 ;;
    esac
    # Decision is durable before changing metadata. Replay is safe after a cut
    # between any metadata rename and the root/app inode rollback below.
    trial_restore_snapshot trial.boot-active "$ROOT_PREFIX/$BOOT_ACTIVE" "$trial_old_active" 0644 &&
        trial_restore_snapshot trial.boot-active.previous "$ROOT_PREFIX/$BOOT_ACTIVE.previous" "$trial_old_previous" 0644 &&
        trial_restore_snapshot trial.installed.fnv64 "$INSTALLED_HASH_PATH" "$trial_old_fnv" 0600 &&
        trial_restore_snapshot trial.unified.completed "$INSTALLED_STATE_DIR/unified.completed" "$trial_old_completed" 0600 &&
        rm -f "$JOURNAL/committed" && local_barrier || return 2
    BOOT_PUBLICATION_DONE=1
}

trial_cleanup_files()
{
    trial_journal_valid && trial_decision || return 2
    case "$trial_kind" in
        trial) [ ! -e "$JOURNAL/committed" ] || return 2 ;;
        trial.rollback) [ -f "$JOURNAL/rolled-back" ] && [ ! -e "$JOURNAL/committed" ] || return 2 ;;
        trial.confirmed) [ -f "$JOURNAL/committed" ] && [ ! -e "$JOURNAL/rolled-back" ] || return 2 ;;
    esac
    rm -f "$JOURNAL/trial.targets" "$JOURNAL/trial.targets.tmp" "$JOURNAL/trial.tmp" \
        "$JOURNAL/trial.boot-active" "$JOURNAL/trial.boot-active.tmp" \
        "$JOURNAL/trial.boot-active.previous" "$JOURNAL/trial.boot-active.previous.tmp" \
        "$JOURNAL/trial.installed.fnv64" "$JOURNAL/trial.installed.fnv64.tmp" \
        "$JOURNAL/trial.unified.completed" "$JOURNAL/trial.unified.completed.tmp" && local_barrier &&
        rm -f "$JOURNAL/trial" "$JOURNAL/trial.rollback" "$JOURNAL/trial.confirmed" && local_barrier
}

trial_confirm()
{
    trial_boot_eligible || return $?
    case "$1" in ''|0*|*[!0-9]*) return 2 ;; esac
    [ "${#1}" -le 9 ] && trial_safe_path "$TRIAL_HEALTH_PATH" &&
        [ -f "$TRIAL_HEALTH_PATH" ] && [ "$(wc -l < "$TRIAL_HEALTH_PATH")" = 1 ] &&
        [ "$(wc -c < "$TRIAL_HEALTH_PATH")" = "$((${#1}+1))" ] &&
        [ "$(cat "$TRIAL_HEALTH_PATH")" = "$1" ] && kill -0 "$1" 2>/dev/null || return 2
    trial_exe=$TRIAL_PROC/$1/exe
    [ -f "$trial_exe" ] &&
        [ "$(dd if="$trial_exe" bs=4 count=1 2>/dev/null)" = "$(printf '\177ELF')" ] &&
        [ "$(checked_digest "$trial_exe")" = "$trial_app_sha" ] && trial_runtime_valid || return 2
    if [ "$trial_boot_id" != none ]; then
        boot_safe_file "${ROOT_PREFIX:-/}" "$BOOT_ACTIVE" && boot_read_descriptor "$ROOT_PREFIX/$BOOT_ACTIVE" &&
            [ "$(LC_ALL=C ls -ln "$ROOT_PREFIX/$BOOT_ACTIVE" | awk '{print $1 ":" $3}')" = "-rw-r--r--:$TRIAL_EXPECTED_UID" ] &&
            [ "$boot_current" = "$trial_boot_id" ] && [ "$boot_current_hash" = "$trial_boot_sha" ] &&
            boot_validate_bundle "${ROOT_PREFIX:-/}" "$trial_boot_id" "$trial_boot_sha" || return 2
    fi
    kill -0 "$1" 2>/dev/null && mv "$JOURNAL/trial" "$JOURNAL/trial.confirmed" && local_barrier || return 2
    if [ "$trial_boot_id" != none ]; then boot_collect_versions || return 2; fi
    recover_transaction
}

trial_rollback()
{
    trial_boot_eligible || return $?
    mv "$JOURNAL/trial" "$JOURNAL/trial.rollback" && local_barrier || return 2
    recover_transaction
}
