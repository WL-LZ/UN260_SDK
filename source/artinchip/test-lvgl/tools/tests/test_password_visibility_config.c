/* Compile the production implementation unchanged, with narrow I/O fault seams. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *failure_stage;
static unsigned read_calls;
static unsigned write_calls;

static bool fail_at(const char *stage)
{
    if (failure_stage != NULL && strcmp(failure_stage, stage) == 0) {
        errno = EIO;
        return true;
    }
    return false;
}

static FILE *test_fopen(const char *path, const char *mode)
{
    if (strcmp(mode, "r") == 0) {
        assert(strcmp(path, UI_STATE_DIR "/password_visibility.cfg") == 0);
        read_calls++;
        assert(!fail_at("read"));
    } else {
        assert(strcmp(mode, "w") == 0);
        assert(strcmp(path, UI_STATE_DIR "/password_visibility.cfg.tmp") == 0);
        write_calls++;
        if (fail_at("open")) return NULL;
    }
    return fopen(path, mode);
}

static int test_mkdir(const char *path, mode_t mode)
{
    assert(strcmp(path, UI_STATE_DIR) == 0);
    write_calls++;
    return fail_at("mkdir") ? -1 : mkdir(path, mode);
}

static int test_fflush(FILE *fp)
{
    return fail_at("flush") ? EOF : fflush(fp);
}

static int test_fsync(int fd)
{
    return fail_at("sync") ? -1 : fsync(fd);
}

static int test_fclose(FILE *fp)
{
    int result = fclose(fp);
    return fail_at("close") ? EOF : result;
}

static int test_rename(const char *source, const char *destination)
{
    assert(strcmp(source, UI_STATE_DIR "/password_visibility.cfg.tmp") == 0);
    assert(strcmp(destination, UI_STATE_DIR "/password_visibility.cfg") == 0);
    return fail_at("rename") ? -1 : rename(source, destination);
}

static int test_unlink(const char *path)
{
    assert(strcmp(path, UI_STATE_DIR "/password_visibility.cfg.tmp") == 0);
    return unlink(path);
}

#define fopen test_fopen
#define mkdir test_mkdir
#define fflush test_fflush
#define fsync test_fsync
#define fclose test_fclose
#define rename test_rename
#define unlink test_unlink
#include "un260/lv_system/user_cfg.c"

static bool boolean(const char *value)
{
    assert(strcmp(value, "0") == 0 || strcmp(value, "1") == 0);
    return value[0] == '1';
}

int main(int argc, char **argv)
{
    bool initial;
    assert(argc >= 3);
    initial = boolean(argv[2]);
    if (strcmp(argv[1], "get") == 0) {
        assert(argc == 3);
        assert(user_cfg_password_visibility_enabled() == initial);
        assert(read_calls == 1 && write_calls == 0);
        failure_stage = "read";
        assert(user_cfg_password_visibility_enabled() == initial);
        assert(read_calls == 1 && write_calls == 0);
    } else if (strcmp(argv[1], "load") == 0) {
        assert(argc == 4);
        assert(user_cfg_password_visibility_load() == boolean(argv[3]));
        failure_stage = "read";
        assert(user_cfg_password_visibility_enabled() == initial);
        assert(read_calls == 1 && write_calls == 0);
    } else if (strcmp(argv[1], "save") == 0) {
        bool requested;
        assert(argc == 4);
        requested = boolean(argv[3]);
        /* Deliberately do not call load/get first: save must initialize itself. */
        assert(user_cfg_password_visibility_save(requested));
        assert(user_cfg_password_visibility_enabled() == requested);
        assert(read_calls == 1 && write_calls > 0);
    } else if (strcmp(argv[1], "same") == 0) {
        assert(argc == 3);
        /* An unchanged save must also lazy-load before deciding to skip I/O. */
        failure_stage = "mkdir";
        assert(user_cfg_password_visibility_save(initial));
        assert(user_cfg_password_visibility_enabled() == initial);
        assert(read_calls == 1 && write_calls == 0);
        failure_stage = "read";
        assert(user_cfg_password_visibility_save(initial));
        assert(read_calls == 1 && write_calls == 0);
    } else if (strcmp(argv[1], "failure") == 0) {
        bool requested;
        assert(argc == 5);
        requested = boolean(argv[3]);
        assert(initial != requested);
        assert(user_cfg_password_visibility_enabled() == initial);
        failure_stage = argv[4];
        assert(!user_cfg_password_visibility_save(requested));
        assert(user_cfg_password_visibility_enabled() == initial);
        assert(read_calls == 1 && write_calls > 0);
        failure_stage = "mkdir";
        assert(user_cfg_password_visibility_save(initial));
    } else {
        assert(!"Unknown test action");
    }
    return EXIT_SUCCESS;
}
