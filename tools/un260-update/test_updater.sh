#!/bin/sh
# Host-only fault injection. All destination paths live in a fresh /tmp fixture.
set -eu
SCRIPT=${1:?Pass device ui_update.sh}
SCRIPT=$(readlink -f "$SCRIPT")
TEST_BASE=$(mktemp -d /tmp/un260-updater-test.XXXXXX)
echo "Fixtures: $TEST_BASE"

run_case() (
    case_name=$1
    UN260_UPDATER_LIBRARY_ONLY=1
    . "$SCRIPT"
    ROOT_PREFIX=$TEST_BASE/$case_name/root
    USB_MNT=$TEST_BASE/$case_name/usb
    UPDATE_DIR=$USB_MNT/update
    STAGE_DIR=$UPDATE_DIR/.un260_stage
    BACKUP_DIR=$UPDATE_DIR/.un260_backup
    BACKUP_STATE=$BACKUP_DIR/state.tsv
    PLAN_FILE=$UPDATE_DIR/.un260_plan
    SEEN_TARGETS=$UPDATE_DIR/.un260_seen_targets
    ARCHIVE_LIST=$UPDATE_DIR/.un260_archive.list
    ARCHIVE_TYPES=$UPDATE_DIR/.un260_archive.types
    CHECKSUM_TARGETS=$UPDATE_DIR/.un260_checksum_targets
    PAYLOAD_FILES=$UPDATE_DIR/.un260_payload_files
    INSTALLED_STATE_DIR=$ROOT_PREFIX/var/lib/un260-updater
    INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64
    JOURNAL=$INSTALLED_STATE_DIR/transaction
    STATUS_FILE=$USB_MNT/status
    STATUS_TEMP=$USB_MNT/status.tmp
    LOG=$USB_MNT/ui_update.log
    RESULT_FILE=$USB_MNT/result
    WORK_OWNED=1
    package_version=test
    mkdir -p "$ROOT_PREFIX/usr/local/bin" "$ROOT_PREFIX/usr/local/lib" \
        "$ROOT_PREFIX/usr/local/share/lvgl_data" "$STAGE_DIR/payload" "$BACKUP_DIR/rootfs"
    printf original > "$ROOT_PREFIX/usr/local/bin/test_lvgl"
    chmod 0755 "$ROOT_PREFIX/usr/local/bin/test_lvgl"
    printf library > "$ROOT_PREFIX/usr/local/lib/liblvgl.so"
    chmod 0755 "$ROOT_PREFIX/usr/local/lib/liblvgl.so"
    printf picture > "$ROOT_PREFIX/usr/local/share/lvgl_data/a.png"
    printf user-owned > "$ROOT_PREFIX/usr/local/share/lvgl_data/extra.png"
    cp -R "$ROOT_PREFIX/usr" "$STAGE_DIR/payload/usr"
    rm "$STAGE_DIR/payload/usr/local/share/lvgl_data/extra.png"
    printf replacement > "$STAGE_DIR/payload/usr/local/bin/test_lvgl"
    printf new-image > "$STAGE_DIR/payload/usr/local/share/lvgl_data/new.png"
    printf 'file|0755|usr/local/bin/test_lvgl\nfile|0755|usr/local/lib/liblvgl.so\ntree|0755|usr/local/share/lvgl_data\n' > "$STAGE_DIR/install.tsv"
    : > "$LOG"
    # Tests simulate ordered crash points, not real flash power-loss durability.
    sync() { :; }
    df() { printf 'Filesystem 1K-blocks Used Available Use%% Mounted\nfixture 200000 1000 %s 1%% /\n' "${TEST_FREE:-100000}"; }

    case "$case_name" in
        bundle|badchecksum|badarchive)
            (cd "$STAGE_DIR" && find payload -type f | sort | while IFS= read -r f; do sha256sum "$f"; done) > "$STAGE_DIR/checksums.sha256"
            pkg_id=$(sha256sum "$STAGE_DIR/checksums.sha256" | awk '{print $1}')
            printf 'format=UN260_UPGRADE\nschema=1\nproduct=UN260\nversion=test\npackage_id=%s\n' "$pkg_id" > "$STAGE_DIR/manifest.ini"
            if [ "$case_name" = badchecksum ]; then
                printf corrupted > "$STAGE_DIR/payload/usr/local/bin/test_lvgl"
            elif [ "$case_name" = badarchive ]; then
                ln -s /outside "$STAGE_DIR/payload/usr/local/bin/link"
            fi
            BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk
            tar -czf "$BUNDLE_PATH" -C "$STAGE_DIR" manifest.ini checksums.sha256 install.tsv payload
            if [ "$case_name" = bundle ]; then
                install_bundle
                [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = replacement ]
            else
                if (install_bundle); then exit 1; fi
                [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            fi
            [ -f "$BUNDLE_PATH" ] && [ ! -d "$STAGE_DIR" ]
            ;;
        plan)
            validate_install_manifest
            [ "$INSTALL_ENTRY_COUNT" = 2 ] && [ "$SKIPPED_COUNT" = 2 ]
            ;;
        success|nochange)
            if [ "$case_name" = nochange ]; then
                cp "$ROOT_PREFIX/usr/local/bin/test_lvgl" "$STAGE_DIR/payload/usr/local/bin/test_lvgl"
                rm "$STAGE_DIR/payload/usr/local/share/lvgl_data/new.png"
            fi
            lib_inode=$(stat -c %i "$ROOT_PREFIX/usr/local/lib/liblvgl.so")
            execute_plan
            [ "$lib_inode" = "$(stat -c %i "$ROOT_PREFIX/usr/local/lib/liblvgl.so")" ]
            [ ! -d "$JOURNAL" ] && [ ! -d "$STAGE_DIR" ]
            [ -f "$ROOT_PREFIX/usr/local/share/lvgl_data/extra.png" ]
            grep -q 'result=success' "$RESULT_FILE"
            ;;
        lowspace)
            TEST_FREE=1
            if (execute_plan); then exit 1; fi
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            [ ! -d "$JOURNAL" ] && [ ! -d "$STAGE_DIR" ]
            grep -q 'Insufficient root space' "$LOG"
            ;;
        copyfail)
            cp() {
                case "$2" in */new.png.un260-new) return 1 ;; esac
                command cp "$@"
            }
            if (execute_plan); then exit 1; fi
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            [ ! -e "$ROOT_PREFIX/usr/local/share/lvgl_data/new.png" ]
            [ ! -d "$JOURNAL" ]
            grep -q 'Rollback finished' "$LOG"
            ;;
        recoveryfail)
            cp() {
                case "$2" in */new.png.un260-new) return 1 ;; esac
                command cp "$@"
            }
            ln() {
                case "$2" in *.un260-restore) return 1 ;; esac
                command ln "$@"
            }
            if (execute_plan); then exit 1; fi
            [ -f "$JOURNAL/state" ]
            [ -f "$ROOT_PREFIX/usr/local/bin/test_lvgl.un260-old" ]
            [ -f "$BACKUP_DIR/rootfs/usr/local/bin/test_lvgl" ]
            grep -q 'Recovery incomplete' "$LOG"
            unset -f ln
            recover_transaction
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            ;;
        crash|commitcrash)
            if [ "$case_name" = crash ]; then
                mv() {
                    command mv "$@" || return 1
                    case "$*" in *test_lvgl.un260-new*) exit 88 ;; esac
                }
                if (execute_plan); then exit 1; fi
                unset -f mv
            else
                # Stop exactly after durable commit, before cleanup.
                if (recover_transaction() { exit 88; }; execute_plan); then exit 1; fi
            fi
            [ -f "$JOURNAL/state" ]
            recover_transaction
            if [ "$case_name" = crash ]; then expected=original; else expected=replacement; fi
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = "$expected" ]
            [ ! -d "$JOURNAL" ]
            ;;
        symlink)
            rm "$ROOT_PREFIX/usr/local/lib/liblvgl.so"
            ln -s "$TEST_BASE/do-not-touch" "$ROOT_PREFIX/usr/local/lib/liblvgl.so"
            if (execute_plan); then exit 1; fi
            [ ! -e "$TEST_BASE/do-not-touch" ]
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            ;;
        traversal)
            printf 'file|0755|usr/local/bin/../../outside\n' > "$STAGE_DIR/install.tsv"
            if (execute_plan); then exit 1; fi
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = original ]
            [ ! -d "$JOURNAL" ]
            ;;
        oldbackup)
            printf '1|file|0755|usr/local/bin/test_lvgl\n' > "$BACKUP_STATE"
            WORK_OWNED=0
            if (prepare_usb_work); then exit 1; fi
            [ -s "$BACKUP_STATE" ]
            ;;
        legacy)
            LEGACY_APP_PATH=$UPDATE_DIR/test_lvgl
            LEGACY_LIB_PATH=$UPDATE_DIR/liblvgl.so
            LEGACY_DATA_PATH=$UPDATE_DIR/lvgl_data
            cp "$STAGE_DIR/payload/usr/local/bin/test_lvgl" "$LEGACY_APP_PATH"
            install_legacy_package
            [ "$(cat "$ROOT_PREFIX/usr/local/bin/test_lvgl")" = replacement ]
            grep -q 'result=success' "$RESULT_FILE"
            ;;
    esac
    echo "PASS $case_name"
)
for name in bundle badchecksum badarchive plan success nochange lowspace copyfail recoveryfail crash commitcrash symlink traversal oldbackup legacy; do
    run_case "$name"
done
echo 'PASS all updater host tests (hardware power-cut validation still required)'
