#ifndef UN260_STORAGE_IO_H
#define UN260_STORAGE_IO_H
#include <stddef.h>
#include <sys/stat.h>
int storage_write_all(int fd,const void *data,size_t len);
int storage_is_lower_hex(const char *s,size_t n);
int storage_open_absolute(const char *path,int directory);
int storage_same_regular_file(const struct stat *before,const struct stat *current);
#endif
