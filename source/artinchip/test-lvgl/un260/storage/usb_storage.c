#include "usb_storage.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool usb_storage_find_device(char* path, size_t path_size)
{
    static const char* patterns[] = {
        "/dev/sd[a-z][0-9]*",
        "/dev/sd[a-z]"
    };
    size_t i;

    if (path == NULL || path_size == 0U) return false;
    path[0] = '\0';

    for (i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        glob_t matches;
        size_t j;

        memset(&matches, 0, sizeof(matches));
        if (glob(patterns[i], 0, NULL, &matches) != 0) {
            globfree(&matches);
            continue;
        }

        for (j = 0; j < matches.gl_pathc; j++) {
            const char* device = matches.gl_pathv[j];
            struct stat st;

            if (device == NULL || stat(device, &st) != 0 ||
                !S_ISBLK(st.st_mode)) {
                continue;
            }

            snprintf(path, path_size, "%s", device);
            globfree(&matches);
            return true;
        }

        globfree(&matches);
    }

    return false;
}

static bool usb_storage_get_mounted_device(char* path, size_t path_size)
{
    FILE* fp;
    char device[128];
    char mount_path[128];
    char fs_type[64];
    bool found = false;

    if (path == NULL || path_size == 0U) return false;
    path[0] = '\0';

    fp = fopen("/proc/mounts", "r");
    if (fp == NULL) return false;

    while (fscanf(fp, "%127s %127s %63s %*s %*d %*d\n",
                  device, mount_path, fs_type) == 3) {
        if (strcmp(mount_path, USB_STORAGE_MOUNT_POINT) == 0) {
            snprintf(path, path_size, "%s", device);
            found = true;
            break;
        }
    }

    fclose(fp);
    return found;
}

static bool usb_storage_is_mounted(void)
{
    char device[128];

    return usb_storage_get_mounted_device(device, sizeof(device));
}

static bool usb_storage_prepare_mount_point(void)
{
    struct stat st;

    if (stat(USB_STORAGE_MOUNT_POINT, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (errno != ENOENT) return false;
    return mkdir(USB_STORAGE_MOUNT_POINT, 0755) == 0;
}

static bool usb_storage_unmount(void)
{
    if (!usb_storage_is_mounted()) return true;
    if (system("umount " USB_STORAGE_MOUNT_POINT " 2>/dev/null") != 0) {
        return false;
    }
    return !usb_storage_is_mounted();
}

static bool usb_storage_mount_device(const char* device)
{
    char mounted_device[128];
    char command[192];

    if (device == NULL || device[0] == '\0') return false;

    if (usb_storage_get_mounted_device(mounted_device,
                                       sizeof(mounted_device))) {
        if (access(mounted_device, F_OK) == 0) return true;
        if (!usb_storage_unmount()) return false;
    }
    if (!usb_storage_prepare_mount_point()) return false;

    snprintf(command, sizeof(command), "mount '%s' %s 2>/dev/null",
             device, USB_STORAGE_MOUNT_POINT);
    if (system(command) != 0) return false;
    return usb_storage_is_mounted();
}

void usb_storage_refresh(usb_storage_status_t* status)
{
    char device[128];
    bool present;
    bool mounted;

    if (status == NULL) return;

    present = usb_storage_find_device(device, sizeof(device));
    if (present) {
        mounted = usb_storage_mount_device(device);
    } else {
        (void)usb_storage_unmount();
        mounted = usb_storage_is_mounted();
    }

    status->device_present = present;
    status->mounted = mounted;
}

bool usb_storage_prepare(void)
{
    char device[128];

    if (!usb_storage_find_device(device, sizeof(device))) return false;
    return usb_storage_mount_device(device);
}

bool usb_storage_available(void)
{
    char mounted_device[128];
    char device[128];

    if (usb_storage_get_mounted_device(mounted_device,
                                       sizeof(mounted_device)) &&
        access(mounted_device, F_OK) == 0) {
        return true;
    }
    return usb_storage_find_device(device, sizeof(device));
}

static int usb_storage_path_exists(const char* path)
{
    struct stat st;

    if (lstat(path, &st) == 0) return 1;
    return errno == ENOENT ? 0 : -1;
}

bool usb_storage_make_unique_file_pair(const char* base_name,
                                       const char* first_extension,
                                       char* first_path,
                                       size_t first_path_size,
                                       const char* second_extension,
                                       char* second_path,
                                       size_t second_path_size)
{
    int suffix;

    if (base_name == NULL || base_name[0] == '\0' ||
        first_extension == NULL || first_extension[0] == '\0' ||
        second_extension == NULL || second_extension[0] == '\0' ||
        first_path == NULL || first_path_size == 0U ||
        second_path == NULL || second_path_size == 0U) {
        return false;
    }

    first_path[0] = '\0';
    second_path[0] = '\0';

    for (suffix = 0; suffix <= 99; suffix++) {
        int first_written;
        int second_written;
        int first_exists;
        int second_exists;

        if (suffix == 0) {
            first_written = snprintf(first_path, first_path_size, "%s/%s%s",
                                     USB_STORAGE_MOUNT_POINT, base_name,
                                     first_extension);
            second_written = snprintf(second_path, second_path_size, "%s/%s%s",
                                      USB_STORAGE_MOUNT_POINT, base_name,
                                      second_extension);
        } else {
            first_written = snprintf(first_path, first_path_size,
                                     "%s/%s_%02d%s",
                                     USB_STORAGE_MOUNT_POINT, base_name, suffix,
                                     first_extension);
            second_written = snprintf(second_path, second_path_size,
                                      "%s/%s_%02d%s",
                                      USB_STORAGE_MOUNT_POINT, base_name, suffix,
                                      second_extension);
        }

        if (first_written < 0 || (size_t)first_written >= first_path_size ||
            second_written < 0 || (size_t)second_written >= second_path_size) {
            first_path[0] = '\0';
            second_path[0] = '\0';
            return false;
        }

        first_exists = usb_storage_path_exists(first_path);
        second_exists = usb_storage_path_exists(second_path);
        if (first_exists < 0 || second_exists < 0) {
            first_path[0] = '\0';
            second_path[0] = '\0';
            return false;
        }
        if (first_exists == 0 && second_exists == 0) return true;
    }

    first_path[0] = '\0';
    second_path[0] = '\0';
    return false;
}

bool usb_storage_commit_file_pair(const char* first_temp_path,
                                  const char* first_final_path,
                                  const char* second_temp_path,
                                  const char* second_final_path)
{
    if (first_temp_path == NULL || first_temp_path[0] == '\0' ||
        first_final_path == NULL || first_final_path[0] == '\0' ||
        second_temp_path == NULL || second_temp_path[0] == '\0' ||
        second_final_path == NULL || second_final_path[0] == '\0') {
        return false;
    }

    if (rename(first_temp_path, first_final_path) != 0) return false;
    if (rename(second_temp_path, second_final_path) == 0) return true;

    (void)unlink(first_final_path);
    return false;
}
