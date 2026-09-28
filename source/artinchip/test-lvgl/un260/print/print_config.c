#include "print_config.h"

#include <stddef.h>
#include <string.h>

#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_send.h"

#define PRINT_CONFIG_COMMAND            0x41
#define PRINT_CONFIG_REQUEST_TIMEOUT_MS 800U

static print_config_value_t g_print_config = {
    .space_top = 0,
    .head1 = "",
    .head2 = "",
    .content = PRINT_SETTING_CONTENT_LIST,
    .space_bottom = 0,
};
static protocol_request_t g_print_request =
    PROTOCOL_REQUEST_INITIALIZER(PRINT_CONFIG_REQUEST_TIMEOUT_MS);
static uint8_t g_print_request_sub_command;
static print_config_value_t g_print_request_target;
static print_config_request_result_t g_print_last_result;
const print_config_request_result_t *print_config_last_result(void){return &g_print_last_result;}

static void print_config_copy_head(char dst[PRINT_SETTING_HEAD_MAX_LEN + 1], const char *src)
{
    size_t len = 0;

    if (src) {
        while (len < PRINT_SETTING_HEAD_MAX_LEN && src[len] != '\0') {
            dst[len] = src[len];
            len++;
        }
    }
    dst[len] = '\0';
}

void print_config_get(print_config_value_t *value)
{
    if (!value) return;
    *value = g_print_config;
}

void print_config_confirm(const print_config_value_t *value)
{
    if (!value) return;

    g_print_config.space_top = value->space_top;
    print_config_copy_head(g_print_config.head1, value->head1);
    print_config_copy_head(g_print_config.head2, value->head2);
    g_print_config.content = value->content;
    g_print_config.space_bottom = value->space_bottom;
}

bool print_config_request(uint8_t sub_command,
                          const uint8_t *payload,
                          uint16_t payload_len,
                          const print_config_value_t *target)
{
    if (payload == NULL || payload_len == 0 || target == NULL ||
        !protocol_request_begin(&g_print_request)) {
        return false;
    }

    g_print_request_sub_command = sub_command;
    g_print_request_target = *target;
    if (protocol_send(PRINT_CONFIG_COMMAND, payload, payload_len) < 0) {
        print_config_cancel_request();
        return false;
    }
    return true;
}

bool print_config_take_reply(uint8_t sub_command,
                             uint8_t status,
                             print_config_request_result_t *result)
{
    if (result == NULL || sub_command != g_print_request_sub_command ||
        !protocol_request_take_result(&g_print_request)) {
        return false;
    }

    result->sub_command = sub_command;
    result->success = status != 0x00;
    result->timeout = false;
    g_print_last_result = *result;
    if (result->success) {
        print_config_confirm(&g_print_request_target);
    }
    g_print_request_sub_command = 0;
    return true;
}

bool print_config_take_status_reply(uint8_t status,
                                    print_config_request_result_t *result)
{
    /* The controller sends FD DF 06 41 <status> <checksum>, without
     * echoing the sub-command. Never infer a transaction when idle. */
    if (status > 0x01 || !protocol_request_is_pending(&g_print_request)) return false;
    return print_config_take_reply(g_print_request_sub_command, status, result);
}

bool print_config_take_timeout(print_config_request_result_t *result)
{
    if (result == NULL || !protocol_request_take_timeout(&g_print_request)) {
        return false;
    }

    result->sub_command = g_print_request_sub_command;
    result->success = false;
    result->timeout = true;
    g_print_last_result = *result;
    g_print_request_sub_command = 0;
    return true;
}

void print_config_cancel_request(void)
{
    protocol_request_finish(&g_print_request);
    g_print_request_sub_command = 0;
}
bool print_config_pending(void){return protocol_request_is_pending(&g_print_request);}
bool print_config_request_field(print_config_field_t field,const print_config_value_t *value)
{
    if(!value)return false;
    print_config_value_t next;print_config_get(&next);
    uint8_t payload[2+PRINT_SETTING_HEAD_MAX_LEN]={0};uint16_t size;
    if(field==PRINT_CONFIG_CONTENT){
        if(value->content<PRINT_SETTING_CONTENT_LIST||value->content>PRINT_SETTING_CONTENT_LIST_SN)return false;
        next.content=value->content;payload[0]=1;payload[1]=next.content;size=2;
    }else if(field==PRINT_CONFIG_HEAD1||field==PRINT_CONFIG_HEAD2){
        const char *head=field==PRINT_CONFIG_HEAD1?value->head1:value->head2;
        size_t len=0;while(len<=PRINT_SETTING_HEAD_MAX_LEN&&head[len])len++;
        if(len>PRINT_SETTING_HEAD_MAX_LEN)return false;
        for(size_t i=0;i<len;i++)if((unsigned char)head[i]<32||(unsigned char)head[i]>126)return false;
        print_config_copy_head(field==PRINT_CONFIG_HEAD1?next.head1:next.head2,head);
        payload[0]=2;payload[1]=field==PRINT_CONFIG_HEAD1?1:2;
        memset(payload+2,' ',PRINT_SETTING_HEAD_MAX_LEN);memcpy(payload+2,head,len);size=sizeof(payload);
    }else if(field==PRINT_CONFIG_TOP||field==PRINT_CONFIG_BOTTOM){
        uint8_t lines=field==PRINT_CONFIG_TOP?value->space_top:value->space_bottom;
        if(lines>PRINT_SETTING_SPACE_MAX_LINES)return false;
        if(field==PRINT_CONFIG_TOP)next.space_top=lines;else next.space_bottom=lines;
        payload[0]=3;payload[1]=field==PRINT_CONFIG_TOP?1:2;payload[2]=lines;size=3;
    }else return false;
    return print_config_request(payload[0],payload,size,&next);
}
