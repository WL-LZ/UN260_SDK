#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/boot/boot_reply.h"
int main(int argc, char **argv)
{
    assert(argc==2);
    uint8_t handshake[]={0xfd,0xdf,6,1,1,0};
    uint8_t reply[]={0xfd,0xdf,7,0x37,4,1,0};
    boot_service_start(100);
    boot_service_request_handshake(100);
    if (!strcmp(argv[1],"timeout")) {
        assert(boot_service_reply_window_open(60099));
        assert(!boot_service_reply_window_open(60100));
        assert(boot_service_poll(60100)==BOOT_SERVICE_ACTION_HANDSHAKE_TIMEOUT);
        assert(boot_reply_dispatch(1,handshake,6).kind==BOOT_REPLY_IGNORED);
        assert(boot_service_poll(61000)==BOOT_SERVICE_ACTION_NONE);
    } else if (!strcmp(argv[1],"cancel")) {
        boot_service_cancel();
        assert(!boot_service_reply_window_open(200));
        assert(boot_reply_dispatch(1,handshake,6).kind==BOOT_REPLY_IGNORED);
        assert(boot_service_poll(200)==BOOT_SERVICE_ACTION_NONE);
    } else {
        assert(boot_reply_dispatch(1,handshake,5).kind==BOOT_REPLY_INVALID);
        assert(boot_reply_dispatch(1,handshake,6).kind==BOOT_REPLY_HANDSHAKE_ACCEPTED);
        assert(boot_reply_dispatch(1,handshake,6).kind==BOOT_REPLY_IGNORED);
        const uint8_t order[]={4,1,2,3,5};
        for(unsigned i=0;i<5;i++) {
            uint8_t next;
            assert(boot_service_next_self_test_protocol_step(&next));
            assert(next==order[i]);
            reply[4]=0xff;
            assert(boot_reply_dispatch(0x37,reply,7).kind==BOOT_REPLY_IGNORED);
            reply[4]=next;
            reply[5]=!strcmp(argv[1],"failure") && (i==1||i==3) ? 0xfe : 1;
            assert(boot_reply_dispatch(0x37,reply,6).kind==BOOT_REPLY_INVALID);
            boot_reply_result_t result=boot_reply_dispatch(0x37,reply,7);
            assert(result.kind==BOOT_REPLY_SELF_TEST_RECORDED);
            assert(result.self_test_index==i);
            assert(boot_reply_dispatch(0x37,reply,7).kind==BOOT_REPLY_IGNORED);
            if(i==4) assert(result.self_test_event==(!strcmp(argv[1],"failure") ?
                BOOT_SELF_TEST_EVENT_FAILURE : BOOT_SELF_TEST_EVENT_SUCCESS));
        }
        boot_snapshot_t state;boot_service_snapshot(&state);
        assert(state.completed_count==5);
        assert(!boot_service_reply_window_open(500));
        if(!strcmp(argv[1],"failure")) {
            assert(state.items[1].result==0xfe && state.items[3].result==0xfe);
            assert(state.stage==BOOT_STAGE_FAIL);
        } else assert(state.stage==BOOT_STAGE_DONE);
    }
    puts("PASS boot protocol");return 0;
}
