/**
 * @file lv_port_indev.c
 *
 */

#include "lv_port_indev.h"

#include "lvgl/lvgl.h"
#include "un260/app_service/touch_calibration_service.h"
#if USE_EVDEV != 0 || USE_BSD_EVDEV
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <glob.h>
#if USE_BSD_EVDEV
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#include "un260/gesture/touch_frame.h"
#endif

#if USE_XKB
#include "xkb.h"
#endif /* USE_XKB */

#if USE_TSLIB
#include "tslib.h"
struct tsdev *ts;
#endif /* USE_TSLIB */


bool evdev_set_file(const char* dev_name);
int map(int x, int in_min, int in_max, int out_min, int out_max);

/**********************
 *  STATIC VARIABLES
 **********************/
int evdev_fd = -1;
int evdev_root_x;
int evdev_root_y;
int evdev_button;

int evdev_key_val;
static bool evdev_press_cancelled;
static lv_obj_t *evdev_pressed_obj;
static lv_point_t evdev_press_point;
static bool evdev_visual_cancelled;
static lv_port_pointer_observer_t g_pointer_observer;
static void *g_pointer_observer_data;
static uint8_t g_touch_count;

static touch_frame_t g_mt_frame;
static lv_indev_t *g_pointer_indev;
static bool g_raw_down, g_contact_captured;

static void evdev_map_point(int raw_x, int raw_y, int width, int height, int *x, int *y)
{
    if(touch_calibration_transform(raw_x, raw_y, x, y)) return;
#if EVDEV_SWAP_AXES
    *x = raw_y; *y = raw_x;
#else
    *x = raw_x; *y = raw_y;
#endif
#if EVDEV_CALIBRATE
    *x = map(*x, EVDEV_HOR_MIN, EVDEV_HOR_MAX, 0, width);
    *y = map(*y, EVDEV_VER_MIN, EVDEV_VER_MAX, 0, height);
#else
    LV_UNUSED(width); LV_UNUSED(height);
#endif
}

static void evdev_calibration_init(void)
{
    touch_calibration_source_t source = {0};
#if USE_TSLIB == 0
    struct input_absinfo x, y;
    struct input_id id = {0};
    char name[80] = {0};
    bool has_x = ioctl(evdev_fd, EVIOCGABS(ABS_MT_POSITION_X), &x) == 0 && x.maximum > x.minimum;
    if(!has_x) has_x = ioctl(evdev_fd, EVIOCGABS(ABS_X), &x) == 0 && x.maximum > x.minimum;
    bool has_y = ioctl(evdev_fd, EVIOCGABS(ABS_MT_POSITION_Y), &y) == 0 && y.maximum > y.minimum;
    if(!has_y) has_y = ioctl(evdev_fd, EVIOCGABS(ABS_Y), &y) == 0 && y.maximum > y.minimum;
    if(has_x && has_y && g_pointer_indev && g_pointer_indev->driver->disp &&
       ioctl(evdev_fd, EVIOCGID, &id) == 0 && ioctl(evdev_fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
        source.width = g_pointer_indev->driver->disp->driver->hor_res;
        source.height = g_pointer_indev->driver->disp->driver->ver_res;
        source.x_min = x.minimum; source.x_max = x.maximum;
        source.y_min = y.minimum; source.y_max = y.maximum;
        uint32_t signature = 2166136261U;
        const unsigned char *bytes = (const unsigned char *)&id;
        for(size_t i = 0; i < sizeof(id); ++i) signature = (signature ^ bytes[i]) * 16777619U;
        for(size_t i = 0; i < sizeof(name) && name[i]; ++i)
            signature = (signature ^ (unsigned char)name[i]) * 16777619U;
        signature = (signature ^ EVDEV_SWAP_AXES) * 16777619U;
        signature = (signature ^ EVDEV_CALIBRATE) * 16777619U;
        source.device_signature = signature ? signature : 1;
    }
#endif
    /* RTP-tslib already supplies its own linear filter. Keep that fallback
     * usable without applying or sampling a second calibration on top. */
    touch_calibration_input_init(&source);
}

static void evdev_seed_slot_positions(void)
{
    if(!g_mt_frame.type_b) return;
    struct input_absinfo slot;
    if(ioctl(evdev_fd, EVIOCGABS(ABS_MT_SLOT), &slot) == 0)
        g_mt_frame.slot = slot.value >= 0 && slot.value < TOUCH_SLOTS ? slot.value : -1;
    int x[TOUCH_SLOTS + 1] = {ABS_MT_POSITION_X};
    int y[TOUCH_SLOTS + 1] = {ABS_MT_POSITION_Y};
    bool have_x = ioctl(evdev_fd, EVIOCGMTSLOTS(sizeof(x)), x) == 0;
    bool have_y = ioctl(evdev_fd, EVIOCGMTSLOTS(sizeof(y)), y) == 0;
    for(int i = 0; i < TOUCH_SLOTS; ++i) {
        if(have_x) { g_mt_frame.slots[i].x = x[i + 1]; g_mt_frame.slots[i].has_x = true; }
        if(have_y) { g_mt_frame.slots[i].y = y[i + 1]; g_mt_frame.slots[i].has_y = true; }
    }
}

static bool evdev_is_touch_device(const char *dev_name)
{
    struct input_absinfo abs_info;
    char input_name[64] = {0};
    int fd;
    bool has_x;
    bool has_y;

    fd = open(dev_name, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if(fd < 0)
        return false;

    (void)ioctl(fd, EVIOCGNAME(sizeof(input_name)), input_name);
    has_x = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_info) == 0 ||
            ioctl(fd, EVIOCGABS(ABS_X), &abs_info) == 0;
    has_y = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_info) == 0 ||
            ioctl(fd, EVIOCGABS(ABS_Y), &abs_info) == 0;
    close(fd);

    if(strstr(input_name, "goodix") != NULL || strstr(input_name, "touch") != NULL)
        return true;

    return has_x && has_y;
}

static bool evdev_open_runtime_device(void)
{
    const char *runtime_name = getenv("LVGL_EVDEV_DEVICE");

    if(runtime_name == NULL || runtime_name[0] == '\0')
        runtime_name = getenv("TSLIB_TSDEVICE");

    if(runtime_name != NULL && runtime_name[0] != '\0' &&
       evdev_set_file(runtime_name)) {
        fprintf(stderr, "evdev input: %s (runtime)\n", runtime_name);
        return true;
    }

    if((runtime_name == NULL || strcmp(runtime_name, EVDEV_NAME) != 0) &&
       evdev_is_touch_device(EVDEV_NAME) && evdev_set_file(EVDEV_NAME)) {
        fprintf(stderr, "evdev input: %s (configured fallback)\n", EVDEV_NAME);
        return true;
    }

    glob_t devices = {0};
    if(glob("/dev/input/event*", 0, NULL, &devices) == 0) {
      for(size_t index = 0; index < devices.gl_pathc; index++) {
        const char *candidate = devices.gl_pathv[index];
        if(!evdev_is_touch_device(candidate))
            continue;
        if(evdev_set_file(candidate)) {
            fprintf(stderr, "evdev input: %s (auto detected)\n", candidate);
            globfree(&devices);
            return true;
        }
      }
    }
    globfree(&devices);

    fprintf(stderr, "evdev input: no usable touchscreen device found\n");
    return false;
}

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

static void evdev_feedback(lv_indev_drv_t *drv, uint8_t event_code)
{
    lv_indev_t *indev = lv_indev_get_act();

    if(indev == NULL || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER)
        return;

    if(event_code == LV_EVENT_PRESSED) {
        evdev_pressed_obj = lv_indev_get_obj_act();
        lv_indev_get_point(indev,&evdev_press_point);
        evdev_visual_cancelled=false;
        if(evdev_pressed_obj != NULL &&
           !lv_obj_has_flag(evdev_pressed_obj, LV_OBJ_FLAG_USER_4))
            lv_obj_clear_flag(evdev_pressed_obj, LV_OBJ_FLAG_PRESS_LOCK);
    } else if((event_code == LV_EVENT_SCROLL_BEGIN || event_code == LV_EVENT_PRESSING) &&
              evdev_pressed_obj != NULL && !evdev_visual_cancelled) {
        /* Cancel only the visual press. PRESS_LOST resets input ownership and
         * would interrupt the very scroll that has just started. CANCEL leaves
         * the native scroll/roller/slider contact and release path intact. */
        lv_obj_t *pressed = evdev_pressed_obj;
        if(lv_obj_is_valid(pressed)) {
            bool dragging=event_code==LV_EVENT_SCROLL_BEGIN;
            if(!dragging){
                lv_point_t point;lv_indev_get_point(indev,&point);
                int threshold=LV_MAX(8,drv?drv->scroll_limit:10);
                bool moved=LV_ABS(point.x-evdev_press_point.x)>threshold||
                           LV_ABS(point.y-evdev_press_point.y)>threshold;
                bool drag_owner=lv_obj_has_flag(pressed,LV_OBJ_FLAG_USER_4);
                for(lv_obj_t *parent=pressed;moved&&parent&&!drag_owner;parent=lv_obj_get_parent(parent)){
                    drag_owner=lv_obj_has_flag(parent,LV_OBJ_FLAG_SCROLLABLE)&&
                        (lv_obj_get_scroll_top(parent)>0||lv_obj_get_scroll_bottom(parent)>0||
                         lv_obj_get_scroll_left(parent)>0||lv_obj_get_scroll_right(parent)>0);
                }
                dragging=moved&&drag_owner;
            }
            if(dragging){
                evdev_visual_cancelled=true;
                lv_obj_clear_state(pressed, LV_STATE_PRESSED);
                lv_event_send(pressed, LV_EVENT_CANCEL, indev);
            }
        }
    } else if(event_code == LV_EVENT_PRESS_LOST && evdev_pressed_obj != NULL) {
        // 滑出后取消本次按压，松手前不转移到其他对象
        evdev_press_cancelled = true;
        lv_indev_reset(indev, evdev_pressed_obj);
        evdev_pressed_obj = NULL;
    } else if(event_code == LV_EVENT_RELEASED) {
        evdev_pressed_obj = NULL;
    }

    /* Navigation observes SYN_REPORT frames in evdev_read, not widget events.
     * PRESS_LOST on an ordinary button must not end a multi-finger gesture. */
}

void lv_port_indev_set_pointer_observer(lv_port_pointer_observer_t observer,
                                        void *user_data)
{
    g_pointer_observer = observer;
    g_pointer_observer_data = user_data;
}

void lv_port_indev_capture_pointer(lv_indev_t *indev)
{
    if (indev == NULL) indev = g_pointer_indev;
    if (indev == NULL || indev != g_pointer_indev) return;
    g_contact_captured = true;
    /* lv_indev_get_obj_act() is NULL between LVGL input reads. The driver
     * retains the actual target so stationary long-press timers can cancel it. */
    lv_obj_t *cancelled = evdev_pressed_obj;
    evdev_pressed_obj = NULL;
    if (cancelled && lv_obj_is_valid(cancelled)) {
        lv_event_send(cancelled, LV_EVENT_PRESS_LOST, indev);
        if (lv_obj_is_valid(cancelled)) lv_obj_clear_state(cancelled, LV_STATE_PRESSED);
    }
    lv_indev_reset(indev, NULL);
    lv_indev_wait_release(indev);
}

uint8_t lv_port_indev_touch_count(void)
{
    return g_touch_count;
}

uint8_t lv_port_indev_touch_points(lv_point_t *points, int32_t *ids, uint8_t capacity)
{
    if(!points || !ids || !g_pointer_indev || !g_pointer_indev->driver->disp) return 0;
    int width = g_pointer_indev->driver->disp->driver->hor_res;
    int height = g_pointer_indev->driver->disp->driver->ver_res;
    uint8_t count = g_mt_frame.count < capacity ? g_mt_frame.count : capacity;
    for(uint8_t i = 0; i < count; ++i) {
        int x, y;
        evdev_map_point(g_mt_frame.contacts[i].x, g_mt_frame.contacts[i].y,
                        width, height, &x, &y);
        points[i].x = x < 0 ? 0 : x >= width ? width - 1 : x;
        points[i].y = y < 0 ? 0 : y >= height ? height - 1 : y;
        ids[i] = g_mt_frame.contacts[i].id;
    }
    return count;
}

void lv_port_indev_set_drag_obj(lv_obj_t *obj, bool enable)
{
    if(obj == NULL)
        return;

    /* Persistent drags own both the driver exemption and LVGL's press lock.
     * Keeping these flags together prevents small drag handles from losing
     * their contact when the finger moves outside the original hit box.
     * Ordinary buttons remain unregistered and retain slide-out cancellation. */
    if(enable)
        lv_obj_add_flag(obj, LV_OBJ_FLAG_USER_4 | LV_OBJ_FLAG_PRESS_LOCK);
    else
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_USER_4 | LV_OBJ_FLAG_PRESS_LOCK);
}

/**
 * Initialize the evdev interface
 */
void evdev_init(void)
{
    if (!evdev_open_runtime_device()) {
        return;
    }

#if USE_XKB
    xkb_init();
#endif
}
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(const char* dev_name)
{
     if(evdev_fd != -1) {
        close(evdev_fd);
     }
#if USE_TSLIB == 0
#if USE_BSD_EVDEV
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY);
#else
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

     if(evdev_fd == -1) {
        perror("unable to open evdev interface:");
        return false;
     }

#if USE_BSD_EVDEV
     fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
     fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif
#else
     ts = ts_setup(NULL, 1);
     if(!ts){
        perror("ts_setup");
        return false;
     }
#endif
     evdev_root_x = 0;
     evdev_root_y = 0;
     evdev_key_val = 0;
     evdev_button = LV_INDEV_STATE_REL;
     evdev_press_cancelled = false;
     evdev_pressed_obj = NULL;
     g_touch_count = 0;
     struct input_absinfo slot_info;
     touch_frame_init(&g_mt_frame, ioctl(evdev_fd, EVIOCGABS(ABS_MT_SLOT), &slot_info) == 0);
     evdev_seed_slot_positions();
     fprintf(stderr, "TOUCH_INPUT protocol=%s slots=%d observer=raw-frame\n",
             g_mt_frame.type_b ? "B" : "A/single", TOUCH_SLOTS);
     g_raw_down = g_contact_captured = false;

     return true;
}
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 */
void evdev_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    /* This read runs even without a new physical report. A hidden calibration
     * page cannot orphan an accepted writer or postpone confirmed state. */
    touch_calibration_poll(lv_tick_get());
#if USE_TSLIB == 0
    struct input_event in;
    bool frame_ready = false;

    while(read(evdev_fd, &in, sizeof(struct input_event)) == sizeof(struct input_event)) {
        bool recovering = g_mt_frame.recovery;
        frame_ready = touch_frame_feed(&g_mt_frame, &in);
        if(recovering && !g_mt_frame.recovery) evdev_seed_slot_positions();
        if(in.type == EV_SYN) {
            if(frame_ready) break;
            continue;
        }
        if(g_mt_frame.dropped || g_mt_frame.recovery) continue;
        if(in.type == EV_REL) {
            if(in.code == REL_X)
				#if EVDEV_SWAP_AXES
					evdev_root_y += in.value;
				#else
					evdev_root_x += in.value;
				#endif
            else if(in.code == REL_Y)
				#if EVDEV_SWAP_AXES
					evdev_root_x += in.value;
				#else
					evdev_root_y += in.value;
				#endif
        } else if(in.type == EV_ABS) {
            if(in.code == ABS_X)
				#if EVDEV_SWAP_AXES
					evdev_root_y = in.value;
				#else
					evdev_root_x = in.value;
				#endif
            else if(in.code == ABS_Y)
				#if EVDEV_SWAP_AXES
					evdev_root_x = in.value;
				#else
					evdev_root_y = in.value;
				#endif
        } else if(in.type == EV_KEY) {
            if(in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
                if(in.value == 0) {
                    evdev_button = LV_INDEV_STATE_REL;
                    g_touch_count = 0;
                    evdev_press_cancelled = false;
                    evdev_pressed_obj = NULL;
                } else if(in.value == 1) {
                    evdev_button = LV_INDEV_STATE_PR;
                    if(g_touch_count == 0) g_touch_count = 1;
                }
            } else if(drv->type == LV_INDEV_TYPE_KEYPAD) {
#if USE_XKB
                data->key = xkb_process_key(in.code, in.value != 0);
#else
                switch(in.code) {
                    case KEY_BACKSPACE:
                        data->key = LV_KEY_BACKSPACE;
                        break;
                    case KEY_ENTER:
                        data->key = LV_KEY_ENTER;
                        break;
                    case KEY_PREVIOUS:
                        data->key = LV_KEY_PREV;
                        break;
                    case KEY_NEXT:
                        data->key = LV_KEY_NEXT;
                        break;
                    case KEY_UP:
                        data->key = LV_KEY_UP;
                        break;
                    case KEY_LEFT:
                        data->key = LV_KEY_LEFT;
                        break;
                    case KEY_RIGHT:
                        data->key = LV_KEY_RIGHT;
                        break;
                    case KEY_DOWN:
                        data->key = LV_KEY_DOWN;
                        break;
                    case KEY_TAB:
                        data->key = LV_KEY_NEXT;
                        break;
                    default:
                        data->key = 0;
                        break;
                }
#endif /* USE_XKB */
                if (data->key != 0) {
                    /* Only record button state when actual output is produced to prevent widgets from refreshing */
                    data->state = (in.value) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                }
                evdev_key_val = data->key;
                evdev_button = data->state;
                return;
            }
        }
    }

    if(frame_ready && (g_mt_frame.mt || g_mt_frame.recovery)) {
        g_touch_count = g_mt_frame.count;
        if(g_touch_count > 0) {
#if EVDEV_SWAP_AXES
            evdev_root_x = g_mt_frame.y;
            evdev_root_y = g_mt_frame.x;
#else
            evdev_root_x = g_mt_frame.x;
            evdev_root_y = g_mt_frame.y;
#endif
            evdev_button = LV_INDEV_STATE_PR;
        } else {
            evdev_button = LV_INDEV_STATE_REL;
            evdev_press_cancelled = false;
            evdev_pressed_obj = NULL;
        }
    }

    if(drv->type == LV_INDEV_TYPE_KEYPAD) {
        /* No data retrieved */
        data->key = evdev_key_val;
        data->state = evdev_button;
        return;
    }
    if(drv->type != LV_INDEV_TYPE_POINTER)
        return ;
#else
    struct ts_sample samp;
    while(ts_read(ts, &samp, 1) == 1) {
        #if EVDEV_SWAP_AXES
            evdev_root_x = samp.y;
            evdev_root_y = samp.x;
        #else
            evdev_root_x = samp.x;
            evdev_root_y = samp.y;
        #endif

        if(samp.pressure == 0) {
            evdev_button = LV_INDEV_STATE_REL;
            evdev_press_cancelled = false;
            evdev_pressed_obj = NULL;
            g_touch_count = 0;
        } else {
            evdev_button = LV_INDEV_STATE_PR;
            g_touch_count = 1;
        }
    }

#endif
    /*Store the collected data*/

#if EVDEV_SWAP_AXES
    int raw_x = evdev_root_y, raw_y = evdev_root_x;
#else
    int raw_x = evdev_root_x, raw_y = evdev_root_y;
#endif
    int mapped_x, mapped_y;
    evdev_map_point(raw_x, raw_y, drv->disp->driver->hor_res,
                    drv->disp->driver->ver_res, &mapped_x, &mapped_y);
    data->point.x = mapped_x; data->point.y = mapped_y;

    if(evdev_press_cancelled) {
        data->state = LV_INDEV_STATE_REL;
        if(evdev_button == LV_INDEV_STATE_REL)
            evdev_press_cancelled = false;
    } else {
        data->state = evdev_button;
    }

    if(data->point.x < 0)
      data->point.x = 0;
    if(data->point.y < 0)
      data->point.y = 0;
    if(data->point.x >= drv->disp->driver->hor_res)
      data->point.x = drv->disp->driver->hor_res - 1;
    if(data->point.y >= drv->disp->driver->ver_res)
      data->point.y = drv->disp->driver->ver_res - 1;

    /* A complete frame is delivered even on empty space or while LVGL waits
     * for release. Capture once, cancel the previous target, and do not let
     * a remaining finger click after a multi-finger action. */
#if USE_TSLIB == 0
    if(frame_ready) {
#else
    {
#endif
        bool down = evdev_button == LV_INDEV_STATE_PR;
        lv_event_code_t event = down ? (g_raw_down ? LV_EVENT_PRESSING : LV_EVENT_PRESSED)
                                    : LV_EVENT_RELEASED;
        bool capture = false;
        /* The service samples the pre-clipped hardware coordinates. Every
         * contact/centroid uses the same confirmed transform above. */
        capture = touch_calibration_input_feed(raw_x, raw_y, mapped_y, g_touch_count,
                                               g_mt_frame.recovery, lv_tick_get());
        if(!capture && g_pointer_observer)
            capture = g_pointer_observer(g_pointer_indev, event, &data->point,
                                         g_touch_count, g_pointer_observer_data);
        if(capture && !g_contact_captured) {
            lv_port_indev_capture_pointer(g_pointer_indev);
        }
        g_raw_down = down;
        if(!down) g_contact_captured = false;
    }
    if(g_contact_captured) data->state = LV_INDEV_STATE_REL;
#if USE_TSLIB == 0
    /* Let LVGL consume subsequent complete reports, including the final
     * release, instead of merging an entire swipe into one centroid. */
    data->continue_reading = frame_ready;
#endif

    return ;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    evdev_init();

    /* Basic initialization */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    indev_drv.feedback_cb = evdev_feedback;

    /* Register the driver in LVGL and save the created input device object */
    g_pointer_indev = lv_indev_drv_register(&indev_drv);
    evdev_calibration_init();
}

#endif
