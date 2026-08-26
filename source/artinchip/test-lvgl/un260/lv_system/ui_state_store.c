#include "un260/lv_system/ui_state_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef UI_STATE_STORE_PATH
#define UI_STATE_STORE_PATH "/etc/ui_state/ui_state.cfg"
#endif

static bool ui_state_store_build_dir_path(char* dir_path, size_t dir_path_size)
{
    const char* slash = strrchr(UI_STATE_STORE_PATH, '/');
    size_t dir_len;

    if (!dir_path || dir_path_size == 0 || !slash) return false;
    dir_len = (size_t)(slash - UI_STATE_STORE_PATH);
    if (dir_len == 0 || dir_len >= dir_path_size) return false;
    memcpy(dir_path, UI_STATE_STORE_PATH, dir_len);
    dir_path[dir_len] = '\0';
    return true;
}

static bool ui_state_store_ensure_dir(void)
{
    char dir_path[128];
    struct stat info;

    if (!ui_state_store_build_dir_path(dir_path, sizeof(dir_path))) return false;
    if (stat(dir_path, &info) == 0) return S_ISDIR(info.st_mode);
    if (mkdir(dir_path, 0775) == 0 || errno == EEXIST) return true;

    printf("[ui_state] mkdir failed: %s err=%s\n", dir_path, strerror(errno));
    return false;
}

static void ui_state_store_parse_line(ui_persist_state_t* state, const char* line)
{
    if (strncmp(line, "magic=", 6) == 0) {
        state->magic = (unsigned int)strtoul(line + 6, NULL, 0);
    } else if (strncmp(line, "version=", 8) == 0) {
        state->version = (unsigned int)strtoul(line + 8, NULL, 0);
    } else if (strncmp(line, "p01_detail_section=", 19) == 0) {
        state->page01.detail_section = atoi(line + 19);
    } else if (strncmp(line, "p07_view_mode=", 14) == 0) {
        state->page07.view_mode = atoi(line + 14);
    } else if (strncmp(line, "p07_fav_only=", 13) == 0) {
        state->page07.fav_only = atoi(line + 13);
    } else if (strncmp(line, "p07_selected_abs_idx=", 21) == 0) {
        state->page07.selected_abs_idx = atoi(line + 21);
    } else if (strncmp(line, "p07_fav_count=", 14) == 0) {
        state->page07.fav_count = atoi(line + 14);
        if (state->page07.fav_count < 0) state->page07.fav_count = 0;
        if (state->page07.fav_count > MAX_CURRENCIES) {
            state->page07.fav_count = MAX_CURRENCIES;
        }
    } else if (strncmp(line, "p07_fav", 7) == 0) {
        int index = -1;
        char code[4] = { 0 };

        if (sscanf(line, "p07_fav%d=%3[A-Z]", &index, code) == 2 &&
            index >= 0 && index < MAX_CURRENCIES) {
            memcpy(state->page07.fav_codes[index], code, sizeof(code));
        }
    } else if (strncmp(line, "p05_reserved=", 13) == 0) {
        state->page05.reserved05_enable = atoi(line + 13);
    } else if (strncmp(line, "p06_reserved=", 13) == 0) {
        state->page06.reserved06_enable = atoi(line + 13);
    } else if (strncmp(line, "p18_reserved=", 13) == 0) {
        state->page18.reserved18_enable = atoi(line + 13);
    }
}

bool ui_state_store_load(ui_persist_state_t* state)
{
    ui_persist_state_t loaded_state;
    FILE* fp;
    char line[128];

    if (!state) return false;
    loaded_state = *state;
    loaded_state.magic = 0;
    loaded_state.version = 0;
    fp = fopen(UI_STATE_STORE_PATH, "r");
    if (!fp) {
#if LV_DEBUG
        printf("[ui_state] load skipped: %s\n", strerror(errno));
#endif
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        ui_state_store_parse_line(&loaded_state, line);
    }
    if (ferror(fp)) {
        printf("[ui_state] read failed: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }
    if (fclose(fp) != 0) {
        printf("[ui_state] close after read failed: %s\n", strerror(errno));
        return false;
    }

    if (loaded_state.magic != UI_STATE_STORE_MAGIC ||
        loaded_state.version != UI_STATE_STORE_VERSION) {
#if LV_DEBUG
        printf("[ui_state] incompatible state file, using defaults\n");
#endif
        return false;
    }

    *state = loaded_state;
    return true;
}

static bool ui_state_store_flush_file(FILE* fp, int fd, const char* tmp_path)
{
    if (fflush(fp) != 0 || fsync(fd) != 0) {
        printf("[ui_state] flush failed: %s\n", strerror(errno));
        fclose(fp);
        unlink(tmp_path);
        return false;
    }
    if (fclose(fp) != 0) {
        printf("[ui_state] close after write failed: %s\n", strerror(errno));
        unlink(tmp_path);
        return false;
    }
    return true;
}

static void ui_state_store_sync_dir(void)
{
    char dir_path[128];
    int dir_fd;

    if (!ui_state_store_build_dir_path(dir_path, sizeof(dir_path))) return;
    dir_fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return;
    (void)fsync(dir_fd);
    close(dir_fd);
}

bool ui_state_store_save(const ui_persist_state_t* state)
{
    char tmp_path[128];
    FILE* fp;
    int fd;
    int fav_count;

    if (!state) return false;
    if (!ui_state_store_ensure_dir()) {
        printf("[ui_state] save aborted, store dir not ready\n");
        return false;
    }
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", UI_STATE_STORE_PATH) >=
        (int)sizeof(tmp_path)) {
        printf("[ui_state] temporary path is too long\n");
        return false;
    }

    fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("[ui_state] open tmp file failed: %s\n", strerror(errno));
        return false;
    }
    fp = fdopen(fd, "w");
    if (!fp) {
        printf("[ui_state] fdopen failed: %s\n", strerror(errno));
        close(fd);
        unlink(tmp_path);
        return false;
    }

    fav_count = state->page07.fav_count;
    if (fav_count < 0) fav_count = 0;
    if (fav_count > MAX_CURRENCIES) fav_count = MAX_CURRENCIES;

    fprintf(fp, "magic=%u\n", UI_STATE_STORE_MAGIC);
    fprintf(fp, "version=%u\n", UI_STATE_STORE_VERSION);
    fprintf(fp, "p01_detail_section=%d\n", state->page01.detail_section);
    fprintf(fp, "p07_view_mode=%d\n", state->page07.view_mode);
    fprintf(fp, "p07_fav_only=%d\n", state->page07.fav_only);
    fprintf(fp, "p07_selected_abs_idx=%d\n", state->page07.selected_abs_idx);
    fprintf(fp, "p07_fav_count=%d\n", fav_count);
    for (int i = 0; i < fav_count; i++) {
        fprintf(fp, "p07_fav%d=%.3s\n", i, state->page07.fav_codes[i]);
    }
    fprintf(fp, "p05_reserved=%d\n", state->page05.reserved05_enable);
    fprintf(fp, "p06_reserved=%d\n", state->page06.reserved06_enable);
    fprintf(fp, "p18_reserved=%d\n", state->page18.reserved18_enable);

    if (ferror(fp)) {
        printf("[ui_state] write failed: %s\n", strerror(errno));
        fclose(fp);
        unlink(tmp_path);
        return false;
    }
    if (!ui_state_store_flush_file(fp, fd, tmp_path)) {
        return false;
    }
    if (rename(tmp_path, UI_STATE_STORE_PATH) != 0) {
        printf("[ui_state] rename failed: %s\n", strerror(errno));
        unlink(tmp_path);
        return false;
    }
    ui_state_store_sync_dir();
    return true;
}
