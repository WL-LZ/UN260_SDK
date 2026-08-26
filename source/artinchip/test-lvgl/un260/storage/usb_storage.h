#ifndef USB_STORAGE_H
#define USB_STORAGE_H

#include <stdbool.h>
#include <stddef.h>

#define USB_STORAGE_MOUNT_POINT "/mnt/usb"

typedef struct {
    bool device_present;
    bool mounted;
} usb_storage_status_t;

void usb_storage_refresh(usb_storage_status_t* status);
bool usb_storage_prepare(void);
bool usb_storage_available(void);
bool usb_storage_make_unique_file_pair(const char* base_name,
                                       const char* first_extension,
                                       char* first_path,
                                       size_t first_path_size,
                                       const char* second_extension,
                                       char* second_path,
                                       size_t second_path_size);
bool usb_storage_commit_file_pair(const char* first_temp_path,
                                  const char* first_final_path,
                                  const char* second_temp_path,
                                  const char* second_final_path);

#endif
