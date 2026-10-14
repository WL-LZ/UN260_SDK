#!/usr/bin/env python3
"""Run production evdev report dispatch against pipe-backed Linux input frames."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
driver = (root / 'lv_port_indev.c').read_text()
def function(name):
    match = re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{', driver, re.M)
    assert match, name
    if name == 'evdev_read':
        # Its mutually exclusive preprocessor branches each open a frame block.
        return driver[match.start():driver.index('\nint map(', match.start())]
    start = driver.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', driver[start:], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return driver[match.start():start+token.end()]
    raise AssertionError(name)
code = r'''
#include "un260/app_service/touch_calibration_service.h"
#include "un260/gesture/touch_frame.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#define USE_TSLIB 0
#define USE_XKB 0
#define EVDEV_SWAP_AXES 0
#define EVDEV_CALIBRATE 0
#define LV_UNUSED(x) (void)(x)
enum {LV_INDEV_STATE_REL,LV_INDEV_STATE_PR};
enum {LV_INDEV_TYPE_POINTER,LV_INDEV_TYPE_KEYPAD};
enum {LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED};
enum {LV_KEY_BACKSPACE,LV_KEY_ENTER,LV_KEY_PREV,LV_KEY_NEXT,LV_KEY_UP,LV_KEY_LEFT,LV_KEY_RIGHT,LV_KEY_DOWN};
typedef int lv_event_code_t;
typedef struct {int16_t x,y;} lv_point_t;
typedef struct {int hor_res,ver_res;} display_driver_t;
typedef struct {display_driver_t *driver;} display_t;
typedef struct {int type;display_t *disp;} lv_indev_drv_t;
typedef struct {lv_indev_drv_t *driver;} lv_indev_t;
typedef struct {lv_point_t point;int state,key;bool continue_reading;} lv_indev_data_t;
typedef bool (*lv_port_pointer_observer_t)(lv_indev_t*,int,const lv_point_t*,uint8_t,void*);
static int evdev_fd,evdev_root_x,evdev_root_y,evdev_key_val,evdev_button;
static bool evdev_press_cancelled,g_raw_down,g_contact_captured;
static void (*g_contact_observer)(bool);
static void *evdev_pressed_obj,*g_pointer_observer_data;
static uint8_t g_touch_count;
static touch_frame_t g_mt_frame;
static lv_indev_t *g_pointer_indev;
static lv_port_pointer_observer_t g_pointer_observer;
static uint32_t tick;
uint32_t lv_tick_get(void){return tick;}
static void evdev_seed_slot_positions(void){}
static unsigned observer_calls;
static lv_point_t observed;
static bool observe(lv_indev_t *indev,int event,const lv_point_t *point,uint8_t count,void *data)
{(void)indev;(void)event;(void)count;(void)data;observer_calls++;observed=*point;return false;}
void lv_port_indev_capture_pointer(lv_indev_t *indev){(void)indev;g_contact_captured=true;}
'''
code += '\n'.join(function(name) for name in ('evdev_map_point','lv_port_indev_touch_points','evdev_read'))
code += r'''
static int writer_fd;
static lv_indev_drv_t drv;
static lv_indev_data_t data;
static void event(int type,int code,int value){struct input_event e={.type=type,.code=code,.value=value};assert(write(writer_fd,&e,sizeof(e))==sizeof(e));}
static void report(void){event(EV_SYN,SYN_REPORT,0);data=(lv_indev_data_t){0};evdev_read(&drv,&data);}
static touch_calibration_snapshot_t snapshot(void){touch_calibration_snapshot_t s;touch_calibration_get_snapshot(&s);return s;}
static void press(int x,int y,int id){event(EV_ABS,ABS_MT_SLOT,0);event(EV_ABS,ABS_MT_TRACKING_ID,id);event(EV_ABS,ABS_MT_POSITION_X,x);event(EV_ABS,ABS_MT_POSITION_Y,y);report();}
static void release(void){event(EV_ABS,ABS_MT_SLOT,0);event(EV_ABS,ABS_MT_TRACKING_ID,-1);report();}
int main(void){
 int pipefd[2];assert(pipe(pipefd)==0);evdev_fd=pipefd[0];writer_fd=pipefd[1];assert(fcntl(evdev_fd,F_SETFL,O_NONBLOCK)==0);
 display_driver_t display_driver={1280,400};display_t display={&display_driver};drv=(lv_indev_drv_t){LV_INDEV_TYPE_POINTER,&display};lv_indev_t indev={&drv};g_pointer_indev=&indev;g_pointer_observer=observe;
 touch_frame_init(&g_mt_frame,true);touch_calibration_source_t source={1280,400,0,1279,0,399,55};touch_calibration_input_init(&source);
 assert(touch_calibration_begin(1280,400,tick));
 tick+=100;press(64,96,1);assert(data.state==LV_INDEV_STATE_REL);
 tick+=100;event(EV_SYN,SYN_DROPPED,0);report();
 assert(snapshot().progress==0&&snapshot().error==TOUCH_CAL_ERROR_INPUT_LOST);
 event(EV_KEY,BTN_TOUCH,0);report();assert(!g_mt_frame.recovery);
 for(unsigned i=0;i<8;i++){
  touch_calibration_snapshot_t s=snapshot();unsigned before=s.progress;
  tick+=100;press(s.target_x+10,s.target_y+4,i+1);assert(data.state==LV_INDEV_STATE_REL&&data.continue_reading);
  tick+=50;report();assert(snapshot().progress==before); /* duplicate held report */
  tick+=100;release();assert(data.state==LV_INDEV_STATE_REL&&snapshot().progress==before+1);
  report();assert(snapshot().progress==before+1); /* duplicate released report */
 }
 assert(snapshot().phase==TOUCH_CAL_READY && !g_contact_captured);
 unsigned before=observer_calls;
 tick+=100;press(1000,20,20);assert(data.state==LV_INDEV_STATE_PR&&observer_calls==before+1); /* Top Save uses confirmed input */
 tick+=100;release();assert(data.state==LV_INDEV_STATE_REL);
 assert(touch_calibration_save());
 for(unsigned i=0;i<10000&&touch_calibration_storage_busy();i++){evdev_read(&drv,&data);usleep(1000);}
 assert(touch_calibration_enabled()&&!touch_calibration_storage_busy()); /* No new event needed to publish */
 tick+=100;press(410,204,30);assert(data.point.x==400&&data.point.y==200&&observed.x==400&&observed.y==200);
 event(EV_ABS,ABS_MT_SLOT,1);event(EV_ABS,ABS_MT_TRACKING_ID,31);event(EV_ABS,ABS_MT_POSITION_X,610);event(EV_ABS,ABS_MT_POSITION_Y,204);report();
 assert(data.point.x==500&&data.point.y==200);
 lv_point_t points[2];int32_t ids[2];assert(lv_port_indev_touch_points(points,ids,2)==2);assert(points[0].x==400&&points[1].x==600&&points[0].y==200&&points[1].y==200);
 puts("PASS production evdev SYN_DROPPED rejection, SYN_REPORT capture/lift, duplicate frames, READY Save click, hidden writer poll, centroid/slot mapping");
 close(writer_fd);close(evdev_fd);
}
'''
with tempfile.TemporaryDirectory(prefix='un260-touch-driver-') as temp:
    work=Path(temp); path=work/'driver.c'; path.write_text(code)
    exe=work/'test'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=undefined','-I',str(root),
                    '-DTOUCH_CALIBRATION_STORE_DIR="%s"'%(work/'state'), str(path),
                    str(root/'un260/input/touch_calibration_math.c'),
                    str(root/'un260/gesture/touch_frame.c'),
                    str(root/'un260/storage/touch_calibration_store.c'),
                    str(root/'un260/app_service/touch_calibration_service.c'),
                    '-pthread','-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
