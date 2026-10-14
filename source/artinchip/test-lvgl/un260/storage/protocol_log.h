#ifndef PROTOCOL_LOG_H
#define PROTOCOL_LOG_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define PROTOCOL_LOG_KEEP 5
/* Worker owns files and disk I/O; UART producers only copy bounded chunks. */
typedef struct {uint64_t id,bytes;char name[48];} protocol_log_file_t;
typedef enum {PLOG_IDLE,PLOG_BUSY,PLOG_ACTIVE,PLOG_ERROR,PLOG_EXPORTED} protocol_log_state_t;
typedef struct {protocol_log_state_t state;bool active,busy;unsigned count;uint64_t current,dropped;protocol_log_file_t files[PROTOCOL_LOG_KEEP];uint32_t revision;} protocol_log_snapshot_t;
bool protocol_log_init(void);
void protocol_log_shutdown(void);
void protocol_log_ports(int controller,int bridge);
void protocol_log_record(int fd,bool tx,const void *data,size_t length);
void protocol_log_snapshot(protocol_log_snapshot_t *out);
bool protocol_log_start(void);
bool protocol_log_stop(void);
bool protocol_log_export(uint64_t id);
bool protocol_log_delete(uint64_t id);
#endif
