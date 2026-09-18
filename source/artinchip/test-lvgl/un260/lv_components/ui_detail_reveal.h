#ifndef UI_DETAIL_REVEAL_H
#define UI_DETAIL_REVEAL_H
#include "lvgl/lvgl.h"
typedef struct {
    lv_obj_t *content, *cover, *orbit;
    uint32_t started, spinning;
    bool active, forced;
} ui_detail_reveal_t;
bool ui_detail_reveal_init(ui_detail_reveal_t *s,lv_obj_t *content);
void ui_detail_reveal_begin(ui_detail_reveal_t *s,bool refresh);
/* Owner calls while visible. Paint completed content before passing ready=true. */
void ui_detail_reveal_update(ui_detail_reveal_t *s,bool ready);
void ui_detail_reveal_cancel(ui_detail_reveal_t *s);
#endif
