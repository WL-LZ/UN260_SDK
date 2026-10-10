#define _GNU_SOURCE
/* Retain the directory fd from BEFORE writes: Linux 5.10 syncfs reports the
 * superblock's writeback errors since that open file description was sampled.
 * Opening a fresh fd only at the end can miss an earlier asynchronous error.
 * USB readback uses O_DIRECT, never silently falls back to the page cache.
 * Local regular files and MTD use bounded read(), not mmap. Their explicit
 * APIs must not silently weaken USB's uncached readback requirement. */
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/file.h>
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

enum hash_mode { HASH_DIRECT, HASH_STREAM, HASH_REGULAR };

static int hash_file(const char *path, enum hash_mode mode, off_t stream_bytes)
{
    int direct = mode == HASH_DIRECT;
    /* Reject FIFOs without blocking in open(), before checking file shape. */
    int fd = open(path, O_RDONLY | (direct ? O_DIRECT : 0) |
                  (mode == HASH_REGULAR ? O_NONBLOCK : 0) | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 1;
    struct stat before, after;
    if (fstat(fd, &before)) { close(fd); return 1; }
    if (mode != HASH_STREAM ? (!S_ISREG(before.st_mode) || before.st_size < 0 || before.st_size > 64 * 1024 * 1024) :
        (!(S_ISREG(before.st_mode) || S_ISCHR(before.st_mode)) ||
         (S_ISREG(before.st_mode) && before.st_size != stream_bytes))) {
        close(fd); errno = EINVAL; return 1;
    }
    off_t expected = mode == HASH_STREAM ? stream_bytes : before.st_size;
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
    if (mode == HASH_REGULAR) {
        struct stat current;
        if (lstat(path, &current) || !S_ISREG(current.st_mode) ||
            current.st_dev != before.st_dev || current.st_ino != before.st_ino)
            failed = 1;
    }
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

/* The RAM path is a namespace anchor; the open file description owns the
 * gate across exec and automatically releases it after the last process dies. */
static int open_lock_path(const char *path, int create)
{
    if(!path || path[0]!='/' || strnlen(path,PATH_MAX)>=PATH_MAX) {
        errno=EINVAL; return -1;
    }
    char *copy=strdup(path);
    if(!copy)return -1;
    int directory=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(directory<0){free(copy);return -1;}
    char *component=copy+1,*slash;
    int fd=-1;
    for(;;) {
        slash=strchr(component,'/');
        if(slash)*slash=0;
        if(!*component || !strcmp(component,".") || !strcmp(component,"..")) {
            errno=EINVAL;break;
        }
        if(!slash) {
            mode_t saved=umask(077);
            fd=openat(directory,component,O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK|
                      (create?O_CREAT:0),0600);
            int error=errno;
            umask(saved);errno=error;
            break;
        }
        int next=openat(directory,component,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(next<0)break;
        close(directory);directory=next;component=slash+1;
    }
    int error=errno;
    close(directory);free(copy);
    if(fd>=0) {
        struct stat st;
        if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() ||
           (st.st_mode&07777)!=0600 || st.st_nlink!=1) {
            close(fd);errno=EINVAL;return -1;
        }
    } else errno=error;
    return fd;
}

static int lock_held(const char *path)
{
    struct stat held,current;
    if(fstat(6,&held) || !S_ISREG(held.st_mode) || held.st_uid!=geteuid() ||
       (held.st_mode&07777)!=0600 || held.st_nlink!=1)return 1;
    int fd=open_lock_path(path,0);
    if(fd<0)return 1;
    int valid=fstat(fd,&current)==0 && held.st_dev==current.st_dev && held.st_ino==current.st_ino;
    if(valid) {
        /* A new description must be blocked by an existing owner. Do not
         * certify an unlocked FD6 by acquiring a new lock with it. */
        if(flock(fd,LOCK_EX|LOCK_NB)==0) {
            flock(fd,LOCK_UN);valid=0;
        } else if(errno!=EWOULDBLOCK && errno!=EAGAIN)valid=0;
        if(valid && flock(6,LOCK_EX|LOCK_NB)<0)valid=0;
    }
    close(fd);
    return valid?0:1;
}

static int lock_run(int argc,char **argv)
{
    if(argv[3][0]!='/' || (size_t)argc>SIZE_MAX/sizeof(char *))return 2;
    char **shell=calloc((size_t)argc-1,sizeof(*shell));
    if(!shell)return 1;
    shell[0]="/bin/sh";
    for(int i=3;i<argc;i++)shell[i-2]=argv[i];
    int fd=open_lock_path(argv[2],1);
    if(fd<0){free(shell);return 1;}
    if(flock(fd,LOCK_EX|LOCK_NB)<0) {
        int error=errno;close(fd);free(shell);
        return error==EWOULDBLOCK || error==EAGAIN?75:1;
    }
    if(fd!=6) {
        if(dup2(fd,6)<0){close(fd);free(shell);return 1;}
        close(fd);
    }
    if(fcntl(6,F_SETFD,0)<0){close(6);free(shell);return 1;}
    execv("/bin/sh",shell);
    int error=errno;
    close(6);free(shell);errno=error;
    return 1;
}


/* Inventory integrity uses the platform sha256sum implementation. File fds
 * stay pinned for one bounded batch; proc paths are generated internally,
 * never accepted from a manifest. No digest is cached between operations.
 * Target tables obey the release budget; inspect also reads larger old files
 * so a full update can safely replace or shrink a mixed installed version. */
#define INV_ROWS_MAX 8192U
#define INV_TABLE_MAX (3U * 1024U * 1024U)
#define INV_MEMBER_MAX (24U * 1024U * 1024U)
#define INV_PAYLOAD_MAX (32U * 1024U * 1024U)
#define INV_BATCH 64U

enum inventory_mode { INV_INSPECT, INV_VERIFY, INV_PAYLOAD };
struct inventory_row {
    char *path;
    char expected[65], actual[65];
    uint64_t bytes;
    mode_t mode;
    int missing;
    struct stat before;
};
struct inventory_anchor {
    const char *path;
    int fd;
    struct stat before;
};

static int inventory_hex(const char *s, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return 0;
    return 1;
}

static int inventory_relative(const char *path)
{
    size_t n = strlen(path);
    if (!n || n > 255 || path[0] == '/' || path[n - 1] == '/' ||
        strstr(path, "..") || strstr(path, "//") || strstr(path, ".un260-"))
        return 0;
    const char *component = path;
    for (const char *p = path; ; ++p) {
        unsigned char c = (unsigned char)*p;
        if (!c || c == '/') {
            size_t len = (size_t)(p - component);
            if (!len || len > NAME_MAX || (len == 1 && component[0] == '.'))
                return 0;
            if (!c) return 1;
            component = p + 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) {
            return 0;
        }
    }
}

static int inventory_decimal(const char *s, uint64_t *value)
{
    if (!*s || (s[0] == '0' && s[1])) return -1;
    uint64_t n = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9' || n > (INV_MEMBER_MAX - (unsigned)(*s - '0')) / 10)
            return -1;
        n = n * 10 + (unsigned)(*s - '0');
    }
    *value = n;
    return 0;
}

/* All names are traversed relative to pinned directory descriptions.
 * O_NONBLOCK makes FIFOs fail shape validation without waiting for a writer. */
static int inventory_openat(int anchor, const char *relative, int directory)
{
    char *copy = strdup(relative);
    if (!copy) return -1;
    int current = fcntl(anchor, F_DUPFD_CLOEXEC, 3);
    if (current < 0) { free(copy); return -1; }
    char *component = copy;
    int result = -1;
    for (;;) {
        char *slash = strchr(component, '/');
        if (slash) *slash = 0;
        if (!*component || !strcmp(component, ".") || !strcmp(component, "..") ||
            strlen(component) > NAME_MAX) { errno = EINVAL; break; }
        int next = openat(current, component, O_RDONLY | O_NOFOLLOW | O_NONBLOCK |
                          O_CLOEXEC | ((slash || directory) ? O_DIRECTORY : 0));
        if (next < 0) break;
        if (!slash) { result = next; break; }
        close(current);
        current = next;
        component = slash + 1;
    }
    int error = errno;
    close(current);
    free(copy);
    errno = error;
    return result;
}

static int inventory_absolute(const char *path, int directory)
{
    if (!path || path[0] != '/' || strnlen(path, PATH_MAX) >= PATH_MAX) {
        errno = EINVAL; return -1;
    }
    int root = open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (root < 0) return -1;
    if (!strcmp(path, "/")) {
        if (directory) return root;
        close(root); errno = EINVAL; return -1;
    }
    int fd = inventory_openat(root, path + 1, directory);
    int error = errno;
    close(root);
    errno = error;
    return fd;
}

static int inventory_same(const struct stat *a, const struct stat *b)
{
    return S_ISREG(b->st_mode) && a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
        a->st_size == b->st_size && a->st_mode == b->st_mode && a->st_uid == b->st_uid &&
        a->st_gid == b->st_gid && a->st_nlink == b->st_nlink &&
        a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static int inventory_anchor_open(struct inventory_anchor *anchor, const char *path)
{
    anchor->path = path;
    anchor->fd = inventory_absolute(path, 1);
    if (anchor->fd < 0) return -1;
    if (fstat(anchor->fd, &anchor->before) || !S_ISDIR(anchor->before.st_mode)) {
        close(anchor->fd); anchor->fd = -1; return -1;
    }
    return 0;
}

static int inventory_anchor_valid(const struct inventory_anchor *anchor)
{
    struct stat held, current;
    int fd = inventory_absolute(anchor->path, 1);
    if (fd < 0) return 0;
    int valid = !fstat(anchor->fd, &held) && !fstat(fd, &current) &&
        S_ISDIR(current.st_mode) && S_ISDIR(held.st_mode) &&
        held.st_dev == anchor->before.st_dev && held.st_ino == anchor->before.st_ino &&
        current.st_dev == held.st_dev && current.st_ino == held.st_ino &&
        held.st_mode == anchor->before.st_mode && held.st_uid == anchor->before.st_uid &&
        current.st_mode == held.st_mode && current.st_uid == held.st_uid;
    if (close(fd)) valid = 0;
    return valid;
}

static int inventory_member_open(const struct inventory_anchor *root,
                                 const struct inventory_anchor *app, const char *path)
{
    if (app->fd >= 0 && !strcmp(path, "usr/local/bin/test_lvgl"))
        return inventory_openat(app->fd, "test_lvgl", 0);
    return inventory_openat(root->fd, path, 0);
}

static int inventory_row_valid(const struct inventory_anchor *root,
                               const struct inventory_anchor *app, const struct inventory_row *row)
{
    int fd = inventory_member_open(root, app, row->path);
    if (fd < 0) return row->missing && errno == ENOENT;
    struct stat current;
    int valid = !row->missing && !fstat(fd, &current) && inventory_same(&row->before, &current);
    if (close(fd)) valid = 0;
    return valid;
}

static int inventory_hash_batch(int *fds, struct inventory_row **rows, size_t count)
{
    int output[2];
    if (pipe2(output, O_CLOEXEC)) return -1;
    char names[INV_BATCH][48];
    char *arguments[INV_BATCH + 2];
    arguments[0] = "sha256sum";
    for (size_t i = 0; i < count; ++i) {
        int n = snprintf(names[i], sizeof(names[i]), "/proc/self/fd/%d", fds[i]);
        if (n <= 0 || (size_t)n >= sizeof(names[i])) {
            close(output[0]); close(output[1]); return -1;
        }
        arguments[i + 1] = names[i];
    }
    arguments[count + 1] = NULL;
    pid_t child = fork();
    if (!child) {
        if (dup2(output[1], STDOUT_FILENO) < 0) _exit(126);
        close(output[0]); close(output[1]);
        for (size_t i = 0; i < count; ++i)
            if (fcntl(fds[i], F_SETFD, 0) < 0) _exit(126);
        execv("/usr/bin/sha256sum", arguments);
        _exit(127);
    }
    close(output[1]);
    if (child < 0) { close(output[0]); return -1; }
    char data[INV_BATCH * 128 + 1];
    size_t used = 0;
    int failed = 0;
    for (;;) {
        ssize_t n = read(output[0], data + used, sizeof(data) - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { failed = 1; break; }
        if (!n) break;
        used += (size_t)n;
        if (used == sizeof(data)) { failed = 1; break; }
    }
    if (close(output[0])) failed = 1;
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status)) failed = 1;
    size_t offset = 0;
    for (size_t i = 0; !failed && i < count; ++i) {
        size_t len = 64 + 2 + strlen(names[i]) + 1;
        if (used - offset < len || !inventory_hex(data + offset, 64) ||
            memcmp(data + offset + 64, "  ", 2) ||
            memcmp(data + offset + 66, names[i], strlen(names[i])) ||
            data[offset + len - 1] != '\n') { failed = 1; break; }
        memcpy(rows[i]->actual, data + offset, 64);
        rows[i]->actual[64] = 0;
        offset += len;
    }
    if (offset != used) failed = 1;
    if (failed) { errno = EIO; return -1; }
    return 0;
}

static int inventory_batch_check(const struct inventory_anchor *root,
                                 const struct inventory_anchor *app, int *fds,
                                 struct inventory_row **rows, size_t count,
                                 enum inventory_mode mode)
{
    int failed = inventory_hash_batch(fds, rows, count);
    for (size_t i = 0; i < count; ++i) {
        struct stat after;
        if (fstat(fds[i], &after) || !inventory_same(&rows[i]->before, &after) ||
            !inventory_row_valid(root, app, rows[i])) failed = -1;
        if (mode != INV_INSPECT &&
            (strcmp(rows[i]->actual, rows[i]->expected) ||
             rows[i]->before.st_size != (off_t)rows[i]->bytes ||
             (mode == INV_VERIFY && (rows[i]->before.st_mode & 07777) != rows[i]->mode)))
            failed = -1;
        if (close(fds[i])) failed = -1;
        fds[i] = -1;
    }
    if (failed) errno = EIO;
    return failed;
}

static int inventory_run(const char *root_path, const char *app_path,
                         const char *table_path, enum inventory_mode mode)
{
    struct inventory_anchor root = { .fd = -1 }, app = { .fd = -1 };
    struct stat table_before, table_after;
    struct inventory_row *rows = NULL;
    char *table = NULL, *table_absolute = NULL;
    int table_fd = -1, rc = 1, fds[INV_BATCH];
    struct inventory_row *batch[INV_BATCH];
    size_t count = 0, held = 0;
    for (size_t i = 0; i < INV_BATCH; ++i) fds[i] = -1;
    if (inventory_anchor_open(&root, root_path) ||
        (strcmp(app_path, "-") && inventory_anchor_open(&app, app_path))) goto done;
    if (table_path[0] == '/') {
        table_absolute = strdup(table_path);
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) goto done;
        size_t n = strlen(cwd) + 1 + strlen(table_path) + 1;
        if (n > PATH_MAX) { errno = EINVAL; goto done; }
        table_absolute = malloc(n);
        if (table_absolute) {
            size_t cwd_length = strlen(cwd);
            memcpy(table_absolute, cwd, cwd_length);
            table_absolute[cwd_length] = '/';
            memcpy(table_absolute + cwd_length + 1, table_path, strlen(table_path) + 1);
        }
    }
    if (!table_absolute) goto done;
    table_fd = inventory_absolute(table_absolute, 0);
    if (table_fd < 0 || fstat(table_fd, &table_before) || !S_ISREG(table_before.st_mode) ||
        table_before.st_size <= 0 || table_before.st_size > INV_TABLE_MAX) goto done;
    size_t size = (size_t)table_before.st_size, used = 0;
    table = malloc(size + 1);
    rows = calloc(INV_ROWS_MAX, sizeof(*rows));
    if (!table || !rows) goto done;
    while (used < size) {
        ssize_t n = read(table_fd, table + used, size - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) goto done;
        used += (size_t)n;
    }
    char extra;
    ssize_t n;
    do { n = read(table_fd, &extra, 1); } while (n < 0 && errno == EINTR);
    if (n || memchr(table, 0, size) || table[size - 1] != '\n') goto done;
    table[size] = 0;
    char *line = table;
    uint64_t expected_total = 0;
    while (*line) {
        if (count == INV_ROWS_MAX) goto done;
        char *newline = strchr(line, '\n');
        if (!newline) goto done;
        *newline = 0;
        char *mode_text = strchr(line, '|');
        if (!mode_text || mode_text - line != 64 || !inventory_hex(line, 64)) goto done;
        *mode_text++ = 0;
        char *bytes_text = strchr(mode_text, '|');
        if (!bytes_text) goto done;
        *bytes_text++ = 0;
        char *path = strchr(bytes_text, '|');
        if (!path) goto done;
        *path++ = 0;
        if ((strcmp(mode_text, "0644") && strcmp(mode_text, "0755")) ||
            inventory_decimal(bytes_text, &rows[count].bytes) ||
            !inventory_relative(path) || (count && strcmp(rows[count - 1].path, path) >= 0))
            goto done;
        expected_total += rows[count].bytes;
        if (expected_total > INV_PAYLOAD_MAX) goto done;
        rows[count].path = path;
        rows[count].mode = !strcmp(mode_text, "0644") ? 0644 : 0755;
        memcpy(rows[count].expected, line, 65);
        ++count;
        line = newline + 1;
    }
    if (!count || fstat(table_fd, &table_after) || !inventory_same(&table_before, &table_after))
        goto done;
    for (size_t i = 0; i < count; ++i) {
        int fd = inventory_member_open(&root, &app, rows[i].path);
        if (fd < 0) {
            if (mode == INV_INSPECT && errno == ENOENT) { rows[i].missing = 1; continue; }
            goto done;
        }
        fds[held] = fd;
        batch[held] = &rows[i];
        ++held;
        if (fstat(fd, &rows[i].before) || !S_ISREG(rows[i].before.st_mode) ||
            rows[i].before.st_size < 0 ||
            (mode != INV_INSPECT && rows[i].before.st_size != (off_t)rows[i].bytes)) goto done;
        if (held == INV_BATCH) {
            if (inventory_batch_check(&root, &app, fds, batch, held, mode)) {
                held = 0; goto done;
            }
            held = 0;
        }
    }
    if (held) {
        int result = inventory_batch_check(&root, &app, fds, batch, held, mode);
        held = 0;
        if (result) goto done;
    }
    /* Re-resolve earlier batches too: later hashing must not hide an earlier
     * inode replacement, metadata/content change or missing-path creation. */
    for (size_t i = 0; i < count; ++i)
        if (!inventory_row_valid(&root, &app, &rows[i])) { errno = ESTALE; goto done; }
    int current_table = inventory_absolute(table_absolute, 0);
    if (current_table < 0) goto done;
    int table_valid = !fstat(current_table, &table_after) &&
        inventory_same(&table_before, &table_after);
    if (close(current_table)) table_valid = 0;
    if (!table_valid || fstat(table_fd, &table_after) ||
        !inventory_same(&table_before, &table_after) ||
        !inventory_anchor_valid(&root) || (app.fd >= 0 && !inventory_anchor_valid(&app))) {
        errno = ESTALE; goto done;
    }
    if (mode == INV_INSPECT) {
        for (size_t i = 0; i < count; ++i) {
            if (rows[i].missing) {
                if (printf("-|0000|0|%s\n", rows[i].path) < 0) goto done;
            } else if (printf("%s|%04o|%" PRIu64 "|%s\n", rows[i].actual,
                              (unsigned)(rows[i].before.st_mode & 07777),
                              (uint64_t)rows[i].before.st_size, rows[i].path) < 0) goto done;
        }
        if (fflush(stdout)) goto done;
    }
    rc = 0;
done:
    {
        int error = errno;
        for (size_t i = 0; i < held; ++i) if (fds[i] >= 0) close(fds[i]);
        if (table_fd >= 0) close(table_fd);
        if (root.fd >= 0) close(root.fd);
        if (app.fd >= 0) close(app.fd);
        free(rows); free(table); free(table_absolute);
        if (rc) errno = error ? error : EINVAL;
    }
    return rc;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--probe")) { puts("UN260_STORAGE_SYNC_1"); return 0; }
    if (argc == 2 && !strcmp(argv[1], "--probe-stream")) { puts("UN260_STORAGE_STREAM_1"); return 0; }
    if (argc == 2 && !strcmp(argv[1], "--probe-file")) { puts("UN260_STORAGE_FILE_1"); return 0; }
    if (argc == 2 && !strcmp(argv[1], "--probe-inventory")) { puts("UN260_STORAGE_INVENTORY_1"); return 0; }
    int rc = 2;
    if (argc == 5 && !strcmp(argv[1], "--inspect-inventory")) rc = inventory_run(argv[2],argv[3],argv[4],INV_INSPECT);
    else if (argc == 5 && !strcmp(argv[1], "--verify-inventory")) rc = inventory_run(argv[2],argv[3],argv[4],INV_VERIFY);
    else if (argc == 4 && !strcmp(argv[1], "--verify-payload")) rc = inventory_run(argv[2],"-",argv[3],INV_PAYLOAD);
    else if (argc >= 4 && !strcmp(argv[1], "--lock-run")) rc = lock_run(argc,argv);
    else if (argc == 3 && !strcmp(argv[1], "--lock-held")) rc = lock_held(argv[2]);
    else if (argc == 4 && !strcmp(argv[1], "--fd")) rc = barrier(argv[2], argv[3]);
    else if (argc == 3 && !strcmp(argv[1], "--hash-direct")) rc = hash_file(argv[2], HASH_DIRECT, 0);
    else if (argc == 3 && !strcmp(argv[1], "--hash-file")) rc = hash_file(argv[2], HASH_REGULAR, 0);
    else if (argc == 3 && !strcmp(argv[1], "--hash-fnv64")) rc = hash_fnv64(argv[2]);
    else if (argc == 4 && !strcmp(argv[1], "--hash-stream")) {
        char *end;
        errno = 0;
        long long bytes = strtoll(argv[3], &end, 10);
        if (!errno && *argv[3] && !*end && bytes >= 0 && bytes <= 64 * 1024 * 1024)
            rc = hash_file(argv[2], HASH_STREAM, (off_t)bytes);
    } else fputs("Usage: un260_storage_sync --inspect-inventory ROOT APP_OR_DASH TABLE | --verify-inventory ROOT APP_OR_DASH TABLE | --verify-payload ROOT TABLE | --fd OPEN_DIRECTORY_FD PATH | --hash-direct FILE | --hash-file FILE | --hash-stream FILE EXACT_BYTES | --hash-fnv64 FILE | --lock-run LOCKFILE ABSOLUTE_SCRIPT [ARGS...] | --lock-held LOCKFILE\n", stderr);
    if (rc == 1) perror("STORAGE_IO_FAILED");
    return rc;
}
