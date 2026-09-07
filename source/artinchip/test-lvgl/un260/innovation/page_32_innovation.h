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
void page_32_innovation_handle_detach(void);
/* Arm the preview snapshot only after the page manager has committed Main.
 * Main is often constructed during boot/prewarm while another page is still
 * current, so the attach-time timer must be made ready again at the commit
 * point. */
void page_32_innovation_schedule_preload(void);

/* Called on the LVGL thread after a complete count-detail snapshot is ready. */
void page_32_innovation_notify_verification_event(
    const multi_pass_capture_event_t *event);

#endif
