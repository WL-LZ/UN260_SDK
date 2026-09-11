#ifndef UN260_PAGE_32_INNOVATION_H
#define UN260_PAGE_32_INNOVATION_H

#include "lvgl/lvgl.h"

#include "multi_pass_verification.h"

void ui_page_32_innovation_create(lv_obj_t *parent);
bool ui_page_32_innovation_resume(void);
void ui_page_32_innovation_suspend(void);
void ui_page_32_innovation_destroy(void);

/* The handle belongs to the main page and only recognizes a downward drag. */
void page_32_innovation_handle_attach(lv_obj_t *main_page);
/* The invisible drag surface forwards stationary taps to its owner's controls. */
void page_32_innovation_handle_set_tap_handler(void (*handler)(const lv_point_t *point));
void page_32_innovation_handle_detach(void);
/* Arm the preview snapshot only after the page manager has committed Main.
 * Main is often constructed during boot/prewarm while another page is still
 * current, so the attach-time timer must be made ready again at the commit
 * point. */
void page_32_innovation_schedule_preload(void);

/* UI-thread navigation gate shared by ESC/BACK and the edge-back gesture.
 * Returns true when Innovation owns the request: start/retain its exit, or
 * cancel its uncommitted Main-page pull-down. Other pages return false. */
bool page_32_innovation_request_back(void);

/* Called on the LVGL thread after a complete count-detail snapshot is ready. */
void page_32_innovation_notify_verification_event(
    const multi_pass_capture_event_t *event);

#endif
