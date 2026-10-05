#include "un260/app_service/touch_calibration_service.h"
#include "un260/storage/touch_calibration_store.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t now;
static touch_calibration_source_t source = {1280,400,0,1279,0,399,0x12345678};
static touch_calibration_matrix_t identity = {{TOUCH_CALIBRATION_SCALE,0,0,0,TOUCH_CALIBRATION_SCALE,0}};
static int io_fault;
static bool descriptor_directory[1024];
static atomic_bool write_blocked, write_release;
static atomic_int injected;
int __real_open(const char *,int,...);
int __real_fsync(int);
int __real_rename(const char *,const char *);
ssize_t __real_write(int,const void *,size_t);
int __wrap_open(const char *path,int flags,...)
{
    mode_t mode=0;
    if(flags&O_CREAT){va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
    if(io_fault==3 && (flags&O_DIRECTORY)){errno=EIO;atomic_fetch_add(&injected,1);return -1;}
    int fd=flags&O_CREAT?__real_open(path,flags,mode):__real_open(path,flags);
    if(fd>=0 && fd<1024)descriptor_directory[fd]=(flags&O_DIRECTORY)!=0;
    return fd;
}
int __wrap_fsync(int fd)
{
    if((io_fault==1 && !descriptor_directory[fd]) || (io_fault==2 && descriptor_directory[fd])) {
        errno=EIO;atomic_fetch_add(&injected,1);return -1;
    }
    return __real_fsync(fd);
}
int __wrap_rename(const char *from,const char *to)
{
    if(io_fault==4){errno=EIO;atomic_fetch_add(&injected,1);return -1;}
    return __real_rename(from,to);
}
ssize_t __wrap_write(int fd,const void *data,size_t bytes)
{
    if(io_fault==5){errno=ENOSPC;atomic_fetch_add(&injected,1);return -1;}
    if(io_fault==6){atomic_store(&write_blocked,true);while(!atomic_load(&write_release))usleep(1000);}
    return __real_write(fd,data,bytes);
}
static touch_calibration_snapshot_t snapshot(void)
{
    touch_calibration_snapshot_t s;touch_calibration_get_snapshot(&s);return s;
}
static void raw_tap(int x,int y,int screen_y,uint32_t duration)
{
    now+=100;
    assert(touch_calibration_input_feed(x,y,screen_y,1,false,now));
    now+=duration;
    assert(touch_calibration_input_feed(x,y,screen_y,0,false,now));
}
static void ready(void)
{
    assert(touch_calibration_begin(1280,400,now));
    for(unsigned i=0;i<5;i++) {
        touch_calibration_snapshot_t s=snapshot();
        assert(s.phase==TOUCH_CAL_COLLECT && s.target_index==i);
        raw_tap(s.target_x+10,s.target_y+4,s.target_y+4,150);
    }
    assert(snapshot().phase==TOUCH_CAL_VALIDATE && !touch_calibration_enabled());
    for(unsigned i=0;i<3;i++) {
        touch_calibration_snapshot_t s=snapshot();
        assert(s.phase==TOUCH_CAL_VALIDATE && s.target_index==i);
        raw_tap(s.target_x+10,s.target_y+4,s.target_y+4,150);
    }
    assert(snapshot().phase==TOUCH_CAL_READY && snapshot().progress==8);
    assert(touch_calibration_can_save());
    assert(touch_calibration_busy() && !touch_calibration_storage_busy());
    now+=100;assert(!touch_calibration_input_feed(1000,20,20,1,false,now));
    now+=100;assert(!touch_calibration_input_feed(1000,20,20,0,false,now));
}
static void wait_save(void)
{
    for(unsigned i=0;i<10000;i++) {
        touch_calibration_poll(now);
        if(!touch_calibration_storage_busy())return;
        usleep(1000);
    }
    assert(!"calibration writer timed out");
}
static void math_tests(void)
{
    touch_calibration_point_t targets[5]={{64,96},{1215,96},{1215,303},{64,303},{640,200}};
    touch_calibration_point_t raw[5];touch_calibration_matrix_t matrix;
    for(unsigned i=0;i<5;i++)raw[i]=(touch_calibration_point_t){targets[i].x+10,targets[i].y+4};
    assert(touch_calibration_fit(&source,raw,targets,&matrix));
    int x,y;touch_calibration_map(&matrix,410,204,&x,&y);assert(x==400&&y==200);
    for(unsigned i=0;i<5;i++)raw[i]=(touch_calibration_point_t){100,100};
    assert(!touch_calibration_fit(&source,raw,targets,&matrix));
    for(unsigned i=0;i<5;i++)raw[i]=(touch_calibration_point_t){targets[i].x+10,targets[i].y+4};
    raw[4].x+=100;assert(!touch_calibration_fit(&source,raw,targets,&matrix));
    touch_calibration_source_t scaled={1280,400,0,4095,0,4095,7};
    for(unsigned i=0;i<5;i++)raw[i]=(touch_calibration_point_t){3*targets[i].x+100,9*targets[i].y+50};
    assert(touch_calibration_fit(&scaled,raw,targets,&matrix));
    touch_calibration_map(&matrix,1300,1850,&x,&y);assert(x==400&&y==200);
    for(unsigned i=0;i<5;i++)raw[i]=(touch_calibration_point_t){1279-targets[i].x,399-targets[i].y};
    assert(touch_calibration_fit(&source,raw,targets,&matrix));
    assert(!touch_calibration_matrix_valid(&source,&(touch_calibration_matrix_t){{0}}));
    matrix=(touch_calibration_matrix_t){{INT32_MAX,INT32_MAX,INT32_MAX,INT32_MAX,INT32_MAX,INT32_MAX}};
    assert(!touch_calibration_point_matches(&matrix,&(touch_calibration_point_t){INT32_MAX,INT32_MAX},&targets[0],14));
}
int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"math")){math_tests();puts("PASS affine fit, singular/outlier/overflow/scaled/inverted geometry");return 0;}
    if(!strcmp(argv[1],"boot")) {
        touch_calibration_input_init(&source);
        assert(touch_calibration_calibrated() && touch_calibration_enabled());
        int x,y;assert(touch_calibration_transform(410,204,&x,&y)&&x==400&&y==200);
        puts("PASS power-cycle reload of confirmed matrix and enabled flag");return 0;
    }
    if(!strcmp(argv[1],"disabled-boot")) {
        touch_calibration_input_init(&source);
        assert(touch_calibration_calibrated() && !touch_calibration_enabled());
        puts("PASS disabling keeps matrix across power-cycle");return 0;
    }
    if(!strcmp(argv[1],"bad-boot")) {
        touch_calibration_input_init(&source);
        assert(!touch_calibration_calibrated()&&!touch_calibration_enabled());
        assert(snapshot().load_error!=TOUCH_CAL_ERROR_NONE);
        puts("PASS invalid configuration preserves uncalibrated fallback");return 0;
    }
    touch_calibration_input_init(&source);
    assert(snapshot().available&&!snapshot().calibrated&&!snapshot().enabled);
    assert(!touch_calibration_set_enabled(true));
    if(!strcmp(argv[1],"session")) {
        now=UINT32_MAX-1000;
        assert(touch_calibration_begin(1280,400,now));
        touch_calibration_snapshot_t s=snapshot();
        raw_tap(s.target_x,s.target_y,s.target_y,20);
        assert(snapshot().progress==0 && snapshot().error==TOUCH_CAL_ERROR_TOO_SHORT);
        now+=100;assert(touch_calibration_input_feed(64,96,96,1,false,now));
        now+=100;assert(touch_calibration_input_feed(100,96,96,1,false,now));
        now+=100;assert(touch_calibration_input_feed(100,96,96,0,false,now));
        assert(snapshot().progress==0 && snapshot().error==TOUCH_CAL_ERROR_UNSTABLE);
        now+=100;assert(touch_calibration_input_feed(64,96,96,2,false,now));
        now+=100;assert(touch_calibration_input_feed(64,96,96,1,false,now));
        now+=100;assert(touch_calibration_input_feed(64,96,96,0,false,now));
        assert(snapshot().progress==0 && snapshot().error==TOUCH_CAL_ERROR_MULTITOUCH);
        raw_tap(64,96,96,150);assert(snapshot().progress==1);
        /* A captured finger may enter the control strip, but cannot click. */
        now+=100;assert(touch_calibration_input_feed(1215,96,96,1,false,now));
        now+=100;assert(touch_calibration_input_feed(1215,350,350,1,false,now));
        now+=100;assert(touch_calibration_input_feed(1215,350,350,0,false,now));
        assert(snapshot().progress==1);
        now+=100;assert(!touch_calibration_input_feed(1000,370,370,1,false,now));
        now+=100;assert(!touch_calibration_input_feed(1000,370,370,0,false,now));
        assert(snapshot().progress==1);
        assert(touch_calibration_cancel());assert(!touch_calibration_busy());
        /* The Start button's still-held physical finger is discarded. */
        now+=100;touch_calibration_input_feed(100,370,370,1,false,now);
        assert(touch_calibration_begin(1280,400,now));
        now+=100;assert(touch_calibration_input_feed(64,96,96,1,false,now));
        now+=100;assert(touch_calibration_input_feed(64,96,96,0,false,now));
        assert(snapshot().progress==0);assert(touch_calibration_cancel());
        ready();assert(touch_calibration_cancel());assert(!touch_calibration_calibrated());
        ready();assert(touch_calibration_save());wait_save();assert(snapshot().phase==TOUCH_CAL_SAVED);
        puts("PASS hold/jitter/multitouch/control-strip ownership/start lift/wrap/cancel/save");return 0;
    }
    if(!strcmp(argv[1],"validation")) {
        assert(touch_calibration_begin(1280,400,now));
        for(unsigned i=0;i<5;i++){touch_calibration_snapshot_t s=snapshot();raw_tap(s.target_x+10,s.target_y+4,s.target_y+4,150);}
        touch_calibration_snapshot_t s=snapshot();raw_tap(s.target_x+80,s.target_y+4,s.target_y+4,150);
        assert(snapshot().phase==TOUCH_CAL_VALIDATE&&snapshot().error==TOUCH_CAL_ERROR_VALIDATION);
        assert(!touch_calibration_save() && !touch_calibration_calibrated());
        touch_calibration_poll(now+30001);assert(snapshot().phase==TOUCH_CAL_FAILED&&snapshot().error==TOUCH_CAL_ERROR_TIMEOUT);
        assert(touch_calibration_cancel());
        assert(touch_calibration_begin(1280,400,now));
        now+=100;touch_calibration_input_feed(64,96,96,1,false,now);
        now+=100;touch_calibration_input_feed(64,96,96,0,true,now);
        assert(snapshot().error==TOUCH_CAL_ERROR_INPUT_LOST&&snapshot().progress==0);
        puts("PASS independent validation blocks bad candidates; timeout/input-loss fallback");return 0;
    }
    ready();
    if(!strcmp(argv[1],"fifo-temp")) {
        assert(touch_calibration_save());wait_save();
        assert(snapshot().phase==TOUCH_CAL_FAILED&&snapshot().error==TOUCH_CAL_ERROR_STORE);
        assert(!touch_calibration_enabled()&&!touch_calibration_storage_busy());
        puts("PASS nonregular temporary calibration file cannot block writer or report saved");return 0;
    }
    if(!strcmp(argv[1],"fault")) {
        io_fault=1;assert(touch_calibration_save());wait_save();
        assert(atomic_load(&injected)>0 && snapshot().phase==TOUCH_CAL_FAILED && snapshot().error==TOUCH_CAL_ERROR_STORE);
        assert(!touch_calibration_enabled() && !touch_calibration_calibrated());
        io_fault=0;assert(touch_calibration_save());wait_save();assert(touch_calibration_enabled());
        io_fault=1;assert(touch_calibration_set_enabled(false));wait_save();
        assert(touch_calibration_enabled() && snapshot().error==TOUCH_CAL_ERROR_STORE);
        io_fault=0;assert(touch_calibration_save());wait_save();assert(!touch_calibration_enabled()&&touch_calibration_calibrated());
        puts("PASS failed save/enable flag rolls runtime back; retry succeeds");return 0;
    }
    if(!strcmp(argv[1],"busy")) {
        io_fault=6;assert(touch_calibration_save());
        for(unsigned i=0;i<10000&&!atomic_load(&write_blocked);i++)usleep(1000);
        assert(atomic_load(&write_blocked));
        assert(touch_calibration_busy() && touch_calibration_storage_busy());
        assert(!touch_calibration_cancel()&&!touch_calibration_begin(1280,400,now)&&!touch_calibration_set_enabled(false));
        assert(!touch_calibration_enabled());
        atomic_store(&write_release,true);wait_save();assert(touch_calibration_enabled());
        puts("PASS accepted writer ownership and factory gate; completion survives hidden page");return 0;
    }
    if(!strcmp(argv[1],"store")) {
        assert(touch_calibration_save());wait_save();assert(touch_calibration_enabled());
        touch_calibration_config_t cfg={source,identity,true}, loaded;
        for(int f=1;f<=5;f++) {
            atomic_store(&injected,0);io_fault=f;
            assert(!touch_calibration_store_save(&cfg));assert(atomic_load(&injected)>0);
            io_fault=0;
        }
        assert(touch_calibration_store_load(&source,&loaded)==TOUCH_CAL_STORE_LOADED);
        touch_calibration_source_t changed=source;changed.device_signature++;
        assert(touch_calibration_store_load(&changed,&loaded)==TOUCH_CAL_STORE_INVALID);
        changed=source;changed.height++;
        assert(touch_calibration_store_load(&changed,&loaded)==TOUCH_CAL_STORE_INVALID);
        puts("PASS file/dir sync, directory open, rename, write faults and device/geometry mismatch");return 0;
    }
    assert(!"unknown case");
}
