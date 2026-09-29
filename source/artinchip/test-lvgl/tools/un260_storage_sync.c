#define _GNU_SOURCE
/* Retain the directory fd from BEFORE writes: Linux 5.10 syncfs reports the
 * superblock's writeback errors since that open file description was sampled.
 * Opening a fresh fd only at the end can miss an earlier asynchronous error.
 * USB readback uses O_DIRECT, never silently falls back to the page cache.
 * MTD hashing uses bounded read(), not a checksum utility's mmap file path. */
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

/* UI compatibility fingerprint only, NOT authenticity/integrity protection.
 * Keep the existing UI's historical seed, not the standard FNV offset basis. */
static int hash_fnv64(const char *path)
{
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 1;
    struct stat before, after, current;
    if (fstat(fd, &before) || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        before.st_size > 64 * 1024 * 1024) { close(fd); errno = EINVAL; return 1; }
    uint64_t hash = UINT64_C(1469598103934665603);
    unsigned char buf[65536];
    off_t total = 0;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { close(fd); return 1; }
        if (!n) break;
        total += n;
        if (total > before.st_size) { close(fd); errno = ESTALE; return 1; }
        for (ssize_t i = 0; i < n; ++i) { hash ^= buf[i]; hash *= UINT64_C(1099511628211); }
    }
    int bad = fstat(fd, &after) || lstat(path, &current) || total != before.st_size ||
        !S_ISREG(current.st_mode) || current.st_dev != before.st_dev || current.st_ino != before.st_ino ||
        before.st_size != after.st_size || before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
        before.st_mtim.tv_nsec != after.st_mtim.tv_nsec || before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
        before.st_ctim.tv_nsec != after.st_ctim.tv_nsec;
    if (close(fd)) bad = 1;
    if (bad) { errno = ESTALE; return 1; }
    return printf("%016" PRIx64 "\n", hash) < 0 ? 1 : 0;
}

static int barrier(const char *number, const char *path)
{
    char *end;
    errno = 0;
    long n = strtol(number, &end, 10);
    if (errno || !*number || *end || n < 3 || n > INT_MAX) return 2;
    int fd = (int)n;
    struct stat held, current;
    struct statvfs fs;
    if (fstat(fd, &held) || lstat(path, &current) || fstatvfs(fd, &fs)) return 1;
    if (!S_ISDIR(held.st_mode) || !S_ISDIR(current.st_mode) ||
        held.st_dev != current.st_dev || held.st_ino != current.st_ino ||
        (fs.f_flag & ST_RDONLY)) {
        errno = ESTALE;
        return 1;
    }
    int rc;
    do { rc = syncfs(fd); } while (rc && errno == EINTR);
    return rc ? 1 : 0;
}

static int write_all(int fd, const void *data, size_t len)
{
    const char *p = data;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int hash_file(const char *path, off_t stream_bytes)
{
    int direct = stream_bytes < 0;
    int fd = open(path, O_RDONLY | (direct ? O_DIRECT : 0) | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 1;
    struct stat before, after;
    if (fstat(fd, &before)) { close(fd); return 1; }
    if (direct ? (!S_ISREG(before.st_mode) || before.st_size < 0 || before.st_size > 64 * 1024 * 1024) :
        (!(S_ISREG(before.st_mode) || S_ISCHR(before.st_mode)) ||
         (S_ISREG(before.st_mode) && before.st_size != stream_bytes))) {
        close(fd); errno = EINVAL; return 1;
    }
    off_t expected = direct ? before.st_size : stream_bytes;
    void *buffer = NULL;
    int err = posix_memalign(&buffer, 4096, 65536);
    if (err) { close(fd); errno = err; return 1; }
    int input[2], output[2];
    if (pipe2(input, O_CLOEXEC)) { free(buffer); close(fd); return 1; }
    if (pipe2(output, O_CLOEXEC)) {
        close(input[0]); close(input[1]); free(buffer); close(fd); return 1;
    }
    pid_t child = fork();
    if (child == 0) {
        if (dup2(input[0], STDIN_FILENO) < 0 || dup2(output[1], STDOUT_FILENO) < 0) _exit(126);
        close(input[0]); close(input[1]); close(output[0]); close(output[1]);
        execl("/usr/bin/sha256sum", "sha256sum", (char *)NULL);
        _exit(127);
    }
    close(input[0]); close(output[1]);
    if (child < 0) {
        close(input[1]); close(output[0]); free(buffer); close(fd); return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    int failed = 0;
    off_t total = 0;
    while (total < expected) {
        size_t request = !direct && expected - total < 65536 ? (size_t)(expected - total) : 65536;
        ssize_t n = read(fd, buffer, request);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0 || n > expected - total || write_all(input[1], buffer, (size_t)n)) {
            failed = 1; break;
        }
        total += n;
        /* A short final direct read need not leave an aligned offset; do not
         * issue another unaligned read after the known end of the file. */
        if (direct && total < expected && n % 4096) { failed = 1; break; }
    }
    if (!direct && !failed) {
        ssize_t extra;
        do { extra = read(fd, buffer, 1); } while (extra < 0 && errno == EINTR);
        if (extra != 0) failed = 1; /* Exact size AND successful EOF required. */
    }
    if (fstat(fd, &after) || before.st_size != after.st_size ||
        before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
        before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
        before.st_ctim.tv_nsec != after.st_ctim.tv_nsec) failed = 1;
    if (close(fd)) failed = 1;
    if (close(input[1])) failed = 1;
    free(buffer);
    char digest[128]; size_t used = 0;
    for (;;) {
        ssize_t n = read(output[0], digest + used, sizeof(digest) - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { failed = 1; break; }
        if (!n) break;
        used += (size_t)n;
        if (used == sizeof(digest) - 1) { failed = 1; break; }
    }
    close(output[0]);
    int status = 0; pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) failed = 1;
    digest[used] = 0;
    if (used < 65 || digest[64] != ' ') failed = 1;
    for (size_t i = 0; i < 64 && i < used; ++i)
        if (!((digest[i] >= '0' && digest[i] <= '9') || (digest[i] >= 'a' && digest[i] <= 'f'))) failed = 1;
    if (failed) { errno = EIO; return 1; }
    digest[64] = 0;
    return printf("%s\n", digest) < 0 ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--probe")) { puts("UN260_STORAGE_SYNC_1"); return 0; }
    if (argc == 2 && !strcmp(argv[1], "--probe-stream")) { puts("UN260_STORAGE_STREAM_1"); return 0; }
    int rc = 2;
    if (argc == 4 && !strcmp(argv[1], "--fd")) rc = barrier(argv[2], argv[3]);
    else if (argc == 3 && !strcmp(argv[1], "--hash-direct")) rc = hash_file(argv[2], -1);
    else if (argc == 3 && !strcmp(argv[1], "--hash-fnv64")) rc = hash_fnv64(argv[2]);
    else if (argc == 4 && !strcmp(argv[1], "--hash-stream")) {
        char *end;
        errno = 0;
        long long bytes = strtoll(argv[3], &end, 10);
        if (!errno && *argv[3] && !*end && bytes >= 0 && bytes <= 64 * 1024 * 1024)
            rc = hash_file(argv[2], (off_t)bytes);
    } else fputs("Usage: un260_storage_sync --fd OPEN_DIRECTORY_FD PATH | --hash-direct FILE | --hash-stream FILE EXACT_BYTES | --hash-fnv64 FILE\n", stderr);
    if (rc == 1) perror("STORAGE_IO_FAILED");
    return rc;
}
