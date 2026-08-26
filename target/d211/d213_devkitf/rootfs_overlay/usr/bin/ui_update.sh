#!/bin/sh

USB_MNT=/mnt/usb
PKG_DIR=$USB_MNT/update
STATUS_FILE=/tmp/ui_update.status
LOG=$USB_MNT/ui_update.log

APP_NAME=test_lvgl
APP_SRC=$PKG_DIR/$APP_NAME
APP_DST=/usr/local/bin/$APP_NAME
APP_NEW=/usr/local/bin/${APP_NAME}_new

IMG_SRC=$PKG_DIR/lvgl_data
IMG_DST=/usr/local/share/lvgl_data

USB_DEV=""

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
    } > "$STATUS_FILE"

    sync
}

fail_update()
{
    msg="$1"
    echo "ERR: $msg" >> "$LOG"
    write_status 12 fail "Upgrade package verification failed" 0 "$msg"
    exit 1
}

mkdir -p "$USB_MNT"
: > "$LOG"
rm -f "$STATUS_FILE"

if grep -q " $USB_MNT " /proc/mounts 2>/dev/null; then
    USB_DEV="$(awk -v m="$USB_MNT" '$2==m {print $1; exit}' /proc/mounts)"
fi

if [ -z "$USB_DEV" ]; then
    USB_DEV="$(detect_usb_dev)"
fi

if [ -z "$USB_DEV" ] || [ ! -b "$USB_DEV" ]; then
    fail_update "USB block device not found"
fi

if ! grep -q " $USB_MNT " /proc/mounts 2>/dev/null; then
    mount "$USB_DEV" "$USB_MNT" >> "$LOG" 2>&1
    if [ $? -ne 0 ]; then
        fail_update "Failed to mount USB device"
    fi
fi

echo "ui_update start: $(date)" >> "$LOG"
write_status 5 verify "Verifying upgrade package" "" ""

if [ ! -f "$APP_SRC" ]; then
    fail_update "Upgrade binary not found on USB drive"
fi

write_status 18 verify "Upgrade package verified" "" ""

cp "$APP_SRC" "$APP_NEW" >> "$LOG" 2>&1
if [ $? -ne 0 ]; then
    fail_update "Failed to copy application binary"
fi

chmod 755 "$APP_NEW"
sync
write_status 48 write "Writing system files" "" ""

mv "$APP_NEW" "$APP_DST" >> "$LOG" 2>&1
if [ $? -ne 0 ]; then
    fail_update "Failed to replace application binary"
fi

echo "APP update OK" >> "$LOG"
write_status 72 write "Writing system files" "" ""

if [ -d "$IMG_SRC" ]; then
    echo "Updating images..." >> "$LOG"
    mkdir -p "$IMG_DST"

    cp -r "$IMG_SRC"/* "$IMG_DST"/ >> "$LOG" 2>&1
    if [ $? -ne 0 ]; then
        fail_update "Failed to copy UI image resources"
    fi

    sync
    echo "IMG update OK" >> "$LOG"
fi

write_status 92 finish "Finalizing upgrade" "" ""
sync
write_status 100 success "Upgrade complete" 1 "The system has been updated successfully. Restarting the device is recommended."
echo "Update OK" >> "$LOG"
exit 0
