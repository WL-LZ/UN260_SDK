#!/bin/sh
# Read-only boot inventory. No environment writes, mounts, raw NAND reads or reboot.
set -u
printf 'UN260 boot inventory v2\n'
printf '\n[KERNEL]\n'
uname -a
printf '\n[COMMAND LINE]\n'
cat /proc/cmdline
printf '\n[MTD LAYOUT]\n'
cat /proc/mtd
printf '\n[MTD GEOMETRY]\n'
for node in /sys/class/mtd/mtd[0-9]*; do
    [ -d "$node" ] || continue
    printf '%s\n' "$node"
    for field in name size erasesize writesize bad_blocks ecc_failures corrected_bits; do
        [ -r "$node/$field" ] || continue
        printf '%s=' "$field"
        cat "$node/$field"
    done
done
printf '\n[MOUNTS]\n'
cat /proc/mounts
printf '\n[SPACE]\n'
df -k
printf '\n[BOOT COMPONENTS]\n'
for file in /etc/init.d/S00lvgl /usr/local/bin/un260_boot_launch /usr/local/bin/un260_app_storage /usr/bin/ui_update.sh; do
    [ -f "$file" ] || continue
    sha256sum "$file"
done
printf '\n[BOOT VERSION RECORDS]\n'
for file in /etc/un260/boot-active /etc/un260/boot-active.previous /dev/.un260-boot-bundle /dev/.un260-boot-platform; do
    [ -f "$file" ] || continue
    printf '%s\n' "$file"
    head -n 5 "$file"
done
printf '\n[BOOT VERSION FILES]\n'
for directory in /usr/local/lib/un260-boot/versions/*; do
    [ -d "$directory" ] && [ ! -L "$directory" ] || continue
    for name in manifest player theme.pack; do
        [ -f "$directory/$name" ] || continue
        sha256sum "$directory/$name"
    done
done
printf '\n[BOOT MARKERS]\n'
for file in /dev/.un260-boot-light.ready /dev/.un260-boot-unified /etc/un260/boot-light.disabled /etc/un260/early-display.disabled /etc/pointercal /etc/un260/app-storage/active /var/lib/un260-updater/transaction; do
    [ ! -e "$file" ] || printf 'present %s\n' "$file"
done
printf '\n[EARLY MILESTONES]\n'
[ ! -r /dev/.un260-early.log ] || cat /dev/.un260-early.log
printf '\n[UI MILESTONES]\n'
[ ! -r /tmp/test_lvgl.startup.log ] || grep -E 'BOOT_TIME|BOOT_FRAME|BOOT_VISUAL|EARLY_INIT|UPDATER' /tmp/test_lvgl.startup.log || :
printf '\n[BOOT ENVIRONMENT - SELECTED KEYS ONLY]\n'
if command -v fw_printenv >/dev/null 2>&1; then
    for key in boot_partition boot_os bootcmd bootcount bootlimit altbootcmd swu_status swu_entry; do
        fw_printenv "$key" 2>&1 || :
    done
else
    printf 'fw_printenv unavailable; bootloader capability is unverified\n'
fi
printf '\n[KERNEL MILESTONES]\n'
dmesg 2>/dev/null | grep -Ei 'Linux version|initramfs|Run /init|UBI:|ubi[0-9]|UBIFS|framebuffer|artinchip.*(fb|display)|panel|backlight' || :
printf '\nComplete. No device configuration or flash was modified.\n'
