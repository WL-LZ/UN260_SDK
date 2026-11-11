#ifndef UN260_HISTORY_WORK_FILE_H
#define UN260_HISTORY_WORK_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Reuse only an interrupted workspace file from this writer. Never open an
 * existing link, FIFO or foreign inode for truncation. */
static inline int history_open_work_file(char *path,size_t capacity,const char *signature)
{
    int fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0644);
    if (fd>=0 || errno!=EEXIST) return fd;
    struct stat named;
    size_t n=strlen(signature);
    if (!lstat(path,&named) && S_ISREG(named.st_mode) && named.st_nlink==1 &&
        named.st_uid==geteuid() && named.st_size>=(off_t)n && n<=40) {
        fd=open(path,O_RDWR|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
        if (fd>=0) {
            struct stat opened;
            char prefix[40];
            bool ours=!fstat(fd,&opened) && opened.st_dev==named.st_dev && opened.st_ino==named.st_ino &&
                opened.st_nlink==1 && pread(fd,prefix,n,0)==(ssize_t)n && !memcmp(prefix,signature,n);
            if (ours && !ftruncate(fd,0)) return fd;
            close(fd);
        }
    }
    /* A short interrupted prefix, an unrecognized file or a link is not
     * ours to truncate. Publish from a separately and exclusively created
     * same-directory file; the original name and its contents survive. */
    static const char suffix[]=".wXXXXXX";
    size_t len=strlen(path);
    if (len+sizeof(suffix)>capacity) { errno=ENAMETOOLONG; return -1; }
    memcpy(path+len,suffix,sizeof(suffix));
    fd=mkstemp(path);
    if (fd>=0) (void)fcntl(fd,F_SETFD,FD_CLOEXEC);
    return fd;
}

#endif
