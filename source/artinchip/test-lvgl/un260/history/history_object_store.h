#ifndef UN260_HISTORY_OBJECT_STORE_H
#define UN260_HISTORY_OBJECT_STORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef UI_HISTORY_APP_VOLUME
#define UI_HISTORY_APP_VOLUME "/mnt/un260-app"
#endif
#define HISTORY_OBJECTS ".un260-history-objects-v1"
#define HISTORY_APP_FLOOR_KB 16384U
/* The factory keeps 4 MiB for metadata and failure recovery. Permanent ABC
 * storage has the larger floor above for the next application generation. */
#define HISTORY_INDEX_APP_RESERVE_KB 4096U
#ifndef HISTORY_UPDATER_LOCK_DIR
#define HISTORY_UPDATER_LOCK_DIR "/tmp"
#endif
#ifndef HISTORY_UPDATER_TRANSACTION
#define HISTORY_UPDATER_TRANSACTION "/var/lib/un260-updater/transaction"
#endif
#define HISTORY_ROOT_FLOOR_KB 16384U
#define HISTORY_INDEX_BACKUP ".committed-index-v1.cfg"

uint64_t history_free_kb(int fd);
bool history_owned_marker(int dir,const char *name,const char *value);
int history_objects_open(int app);
bool history_object_target(int root,const char *prefix,const char *name,char *target,size_t capacity,bool create);
int history_place_new_detail(const char *history_path,const char *name,const unsigned char *bytes,size_t size);
int history_index_upgrade_lock(void);
bool history_release_index_headroom(const char *history_path);
void history_finish_index_headroom(const char *history_path);
bool history_recovery_index_link(int root,const char *prefix);
bool history_has_index_recovery(const char *history_path);
int history_read_recovery_index(const char *history_path);
void history_remove_detail(const char *history_path,const char *name);
#endif
