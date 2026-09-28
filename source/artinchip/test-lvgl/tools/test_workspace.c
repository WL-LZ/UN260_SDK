#include "un260/workspace/workspace_model.h"
#include "un260/storage/workspace_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
static bool fail_directory_sync;
int __real_fsync(int fd);
int __wrap_fsync(int fd)
{
    struct stat st;
    if(fail_directory_sync&&!fstat(fd,&st)&&S_ISDIR(st.st_mode)){errno=EIO;return -1;}
    return __real_fsync(fd);
}
static bool usb_present=true;
bool usb_storage_prepare(void){return usb_present;}
static void wait_job(void)
{for(unsigned i=0;i<10000&&workspace_store_busy();i++){usleep(1000);workspace_store_poll();}assert(!workspace_store_busy());}
int main(int argc,char **argv)
{
    workspace_model_t *m=malloc(sizeof(*m));assert(m);workspace_defaults(m);assert(workspace_model_valid(m));
    assert(!workspace_delete_profile(m,1,1));uint32_t id;
    assert(workspace_add_user(m,"Alice",&id)&&id==2);assert(m->active_id==1);
    assert(!workspace_add_user(m,"alice",NULL));assert(!workspace_add_user(m," Bob",NULL));
    workspace_profile_t p=m->users[0].profiles[0];strcpy(p.name,"Bundle");p.batch_enabled=1;p.batch=100;
    assert(workspace_add_profile(m,1,&p));assert(workspace_delete_profile(m,1,1));assert(!workspace_delete_profile(m,1,2));
    assert(m->users[0].profiles[0].id==2);assert(workspace_next_batch(&m->users[0],50)==100);
    assert(workspace_next_batch(&m->users[0],150)==0);m->users[0].batch_count=1;assert(!workspace_model_valid(m));m->users[0].batch_count=5;
    m->users[0].batches[4]=200;assert(workspace_next_batch(&m->users[0],100)==0);m->users[0].batches[4]=150;
    m->users[0].batches[2]=10;assert(!workspace_model_valid(m));m->users[0].batches[2]=50;
    strcpy(m->users[0].employee_id,"A-001");strcpy(m->users[0].team,"Branch One");assert(workspace_model_valid(m));
    strcpy(m->users[1].employee_id,"a-001");assert(!workspace_model_valid(m));strcpy(m->users[1].employee_id,"A-002");assert(workspace_model_valid(m));
    memset(m->users[1].team,'X',sizeof(m->users[1].team));assert(!workspace_model_valid(m));memset(m->users[1].team,0,sizeof(m->users[1].team));
    workspace_store_init();wait_job();
    if(argc>1&&!strcmp(argv[1],"v2")){
        assert(workspace_store_ready());const workspace_model_t *old=workspace_store_get();assert(old->version==3&&old->user_count==2);
        assert(old->users[0].profiles[0].mode==1&&old->users[0].qr_after_count&&!strcmp(old->users[1].name,"Alice"));assert(!*old->users[0].employee_id&&!*old->users[0].team);
        assert(workspace_store_save(old));wait_job();assert(workspace_store_last_success());free(m);return 0;
    }
    if(argc>1&&!strcmp(argv[1],"legacy")){
        assert(workspace_store_ready());const workspace_model_t *old=workspace_store_get();
        assert(old->version==3&&old->user_count==2&&old->users[1].id==2&&!strcmp(old->users[1].name,"Alice"));
        assert(old->users[0].profiles[0].mode==0&&!old->users[0].qr_after_count);
        assert(workspace_store_save(old));wait_job();assert(workspace_store_last_success());
        free(m);puts("PASS v1 workspace migrated, IDs/preferences retained, old profiles keep current mode");return 0;
    }
    if(argc>1&&!strcmp(argv[1],"corrupt")){assert(!workspace_store_ready());assert(!workspace_store_save(m));puts("PASS corrupt file preserved; writes disabled");free(m);return 0;}
    if(argc>1&&!strcmp(argv[1],"reload-uncertain")){
        assert(workspace_store_ready());assert(!strcmp(workspace_store_get()->users[1].name,"Replacement"));free(m);return 0;
    }
    if(argc>1&&!strcmp(argv[1],"directory-sync")){
        fail_directory_sync=true;strcpy(m->users[1].name,"Replacement");assert(workspace_store_save(m));wait_job();
        assert(!workspace_store_ready()&&!workspace_store_last_success()&&!workspace_store_save(m));
        assert(!strcmp(workspace_store_get()->users[1].name,"Alice"));
        free(m);puts("PASS uncertain rename durability blocks further writes until reload");return 0;
    }
    if(argc>1&&!strcmp(argv[1],"save-failure")){
        assert(workspace_store_ready());assert(!strcmp(workspace_store_get()->users[1].name,"Alice"));
        strcpy(m->users[1].name,"Replacement");assert(workspace_store_save(m));wait_job();
        assert(!workspace_store_last_success());assert(!strcmp(workspace_store_get()->users[1].name,"Alice"));
        free(m);puts("PASS failed save keeps published operator unchanged");return 0;
    }
    assert(workspace_store_ready());assert(workspace_store_save(m));assert(!workspace_store_save(m));wait_job();
    assert(workspace_store_get()->users[1].id==2&&workspace_store_revision()>=2);assert(!strcmp(workspace_store_get()->users[0].employee_id,"A-001")&&!strcmp(workspace_store_get()->users[0].team,"Branch One"));
    assert(workspace_store_export_support("UN260 SUPPORT\nNo personal data\n"));wait_job();assert(workspace_store_last_success());
    fail_directory_sync=true;
    assert(workspace_store_export_support("UN260 SUPPORT FAILED\n"));wait_job();assert(!workspace_store_last_success());
    fail_directory_sync=false;
    assert(workspace_store_scan_usb());wait_job();
    assert(workspace_store_usb_images()&1);assert(!(workspace_store_usb_images()&(1U<<2)));
    assert(workspace_store_import_avatar(0));wait_job();assert(workspace_store_avatar()->present);
    assert(workspace_store_avatar()->pixels[3]==255);
    assert(workspace_store_import_avatar(3));wait_job();assert(!workspace_store_last_success()&&!workspace_store_avatar()->present);
    assert(workspace_store_import_avatar(4));wait_job();assert(!workspace_store_last_success()&&!workspace_store_avatar()->present);
    usb_present=false;assert(workspace_store_scan_usb());wait_job();assert(!workspace_store_usb_images());
    assert(workspace_store_export_support("UN260 SUPPORT MISSING USB\n"));wait_job();assert(!workspace_store_last_success());
    assert(workspace_store_get()->users[1].id==2);free(m);puts("PASS workspace model, minimum-one deletion, durable store, bounded PNG import, USB removal");return 0;
}
