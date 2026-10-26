#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_core/page_00_boot_anim.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D && UI_BOOT_VARIANT == UI_BOOT_VARIANT_B
#include <string.h>
#include "un260/font/boot_fonts.h"
#include "boot_welcome_locale.h"
#include "boot_customer_art.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_08_boot.h"

/* Shared with the customer native player's minimum brand presentation.
 * Preparation and errors still own completion; the video is not a startup timer. */
#define INTRO_HOLD_MS 2200U
#define INTRO_ENTER_MS 600U
static struct {
    lv_obj_t *root, *logo, *error, *advice, *diagnostics;
    lv_timer_t *timer;
    uint32_t start_tick;
    bool art, covered, prepared, first_drawn, managed, ready, failed;
} intro;
static bool failed_create;

static void stop_timer(void)
{
    lv_timer_t *timer=intro.timer;intro.timer=NULL;
    if(timer)lv_timer_del(timer);
}
static void deleted(lv_event_t *event)
{
    if(lv_event_get_target(event)!=intro.root)return;
    stop_timer();
    if(intro.covered)ui_page_08_curr_set_covered(false);
    if(intro.art)boot_customer_art_release();
    memset(&intro,0,sizeof(intro));
}
static void drawn(lv_event_t *event)
{ if(lv_event_get_target(event)==intro.root)intro.first_drawn=true; }
static void finish(void)
{
    stop_timer();
    if(ui_manager_get_current_page()==UI_PAGE_BOOT_ANIM)ui_manager_switch(UI_PAGE_BOOT);
    else ui_page_00_boot_anim_destroy();
}
static void create_failed(void)
{ ui_page_00_boot_anim_destroy();failed_create=true; }
void ui_page_00_boot_anim_poll(void)
{
    if(!failed_create)return;
    failed_create=false;
    if(ui_manager_get_current_page()==UI_PAGE_BOOT_ANIM)ui_manager_switch(UI_PAGE_BOOT);
}
static void apply_elapsed(uint32_t elapsed)
{
    if(!intro.logo)return;
    float p=elapsed>=INTRO_ENTER_MS?1.0f:(float)elapsed/INTRO_ENTER_MS;
    lv_opa_t opacity=(lv_opa_t)(255*p*p*(3-2*p)+.5f);
    if(lv_obj_get_style_img_opa(intro.logo,0)!=opacity)
        lv_obj_set_style_img_opa(intro.logo,opacity,0);
}
void ui_page_00_boot_anim_adopt_elapsed(uint32_t elapsed)
{
    if(!intro.root || intro.failed)return;
    intro.start_tick=lv_tick_get()-elapsed;apply_elapsed(elapsed);
}
void ui_page_00_boot_anim_set_startup_ready(bool ready)
{ if(intro.root && !intro.failed) {intro.managed=true;intro.ready=ready;} }
static void diagnostics(lv_event_t *event)
{
    LV_UNUSED(event);
    ui_page_00_boot_anim_destroy();ui_manager_switch(UI_PAGE_SENSOR);
}
static lv_obj_t *label(lv_obj_t *parent,const char *value,int y,const lv_font_t *font,int size)
{
    lv_obj_t *obj=lv_label_create(parent);
    if(!obj)return NULL;
    lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,320,y);lv_obj_set_width(obj,640);
    lv_obj_set_style_text_font(obj,ui_message_font_sized(font,size),0);
    lv_obj_set_style_text_color(obj,lv_color_hex(0x353734),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);lv_label_set_text(obj,value);
    return obj;
}
void ui_page_00_boot_anim_set_startup_error(bool available)
{
    if(!intro.root)return;
    if(!intro.failed) {
        intro.failed=true;intro.managed=true;stop_timer();
        if(intro.logo)lv_obj_add_flag(intro.logo,LV_OBJ_FLAG_HIDDEN);
        if(intro.advice)lv_obj_add_flag(intro.advice,LV_OBJ_FLAG_HIDDEN);
        intro.error=label(intro.root,ui_tr("STARTUP ERROR"),160,&lv_font_open_runde_medium_40,36);
        intro.advice=label(intro.root,ui_tr("RESTART DEVICE"),225,&lv_font_open_runde_medium_24,24);
    }
    if(!available || intro.diagnostics)return;
    intro.diagnostics=lv_btn_create(intro.root);
    if(!intro.diagnostics)return;
    lv_obj_remove_style_all(intro.diagnostics);
    lv_obj_set_pos(intro.diagnostics,480,288);lv_obj_set_size(intro.diagnostics,320,56);
    lv_obj_set_style_bg_color(intro.diagnostics,lv_color_white(),0);
    lv_obj_set_style_bg_opa(intro.diagnostics,LV_OPA_COVER,0);
    lv_obj_set_style_radius(intro.diagnostics,12,0);
    lv_obj_add_event_cb(intro.diagnostics,diagnostics,LV_EVENT_CLICKED,NULL);
    lv_obj_t *caption=lv_label_create(intro.diagnostics);
    if(caption) {
        lv_label_set_text(caption,ui_tr("DIAGNOSTICS"));
        lv_obj_set_style_text_font(caption,ui_message_font_sized(&lv_font_open_runde_medium_24,24),0);
        lv_obj_set_style_text_color(caption,lv_color_hex(0x353734),0);lv_obj_center(caption);
    }
}
static void tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    ui_page_t page=ui_manager_get_current_page();
    if(page!=UI_PAGE_BOOT && page!=UI_PAGE_BOOT_ANIM) {ui_page_00_boot_anim_destroy();return;}
    if(intro.failed)return;
    uint32_t elapsed=lv_tick_elaps(intro.start_tick);
    /* Build the next page in bounded steps after the short logo entrance. */
    if(intro.covered && !intro.prepared && intro.first_drawn && elapsed>=INTRO_ENTER_MS) {
        if(ui_page_08_curr_prepare_step())intro.prepared=true;
        else if(ui_page_08_curr_prepare_failed()) {
            ui_page_00_boot_anim_set_startup_error(false);return;
        }
    }
    if(elapsed>=INTRO_HOLD_MS && (!intro.managed || intro.ready) &&
       (!intro.covered || intro.prepared)) {finish();return;}
    apply_elapsed(elapsed);
}
static lv_obj_t *image(unsigned id)
{
    const lv_img_dsc_t *source=boot_customer_art_image(id);
    if(!source)return NULL;
    lv_obj_t *obj=lv_img_create(intro.root);
    if(!obj)return NULL;
    lv_coord_t x=0,y=0;boot_customer_art_position(id,&x,&y);
    lv_img_set_src(obj,source);lv_obj_set_pos(obj,x,y);return obj;
}
void ui_page_00_boot_anim_create(lv_obj_t *parent)
{
    if(intro.root)return;
    ui_page_00_boot_anim_destroy();failed_create=false;
    if(!parent)parent=lv_layer_top();
    intro.root=lv_obj_create(parent);
    if(!intro.root) {create_failed();return;}
    lv_obj_remove_style_all(intro.root);lv_obj_set_size(intro.root,1280,400);
    lv_obj_clear_flag(intro.root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(intro.root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(intro.root,lv_color_hex(0xD7D5C8),0);
    lv_obj_set_style_bg_opa(intro.root,LV_OPA_COVER,0);
    lv_obj_add_event_cb(intro.root,deleted,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(intro.root,drawn,LV_EVENT_DRAW_POST_END,NULL);
    if(ui_manager_get_current_page()==UI_PAGE_BOOT) {
        intro.covered=true;ui_page_08_curr_set_covered(true);
    }
    intro.art=boot_customer_art_acquire(boot_welcome_tags[boot_welcome_initial_index(ui_lang_get())]);
    if(intro.art) {
        image(BOOT_CUSTOMER_ART_BG);intro.logo=image(BOOT_CUSTOMER_ART_LOGO_LARGE);
        image(BOOT_CUSTOMER_ART_VERSION);image(BOOT_CUSTOMER_ART_POWERED);
    }
    if(!intro.logo)intro.advice=label(intro.root,ui_tr("Initialization"),174,&lv_font_open_runde_medium_40,36);
    apply_elapsed(0);lv_obj_update_layout(intro.root);lv_obj_move_foreground(intro.root);
    intro.start_tick=lv_tick_get();intro.timer=lv_timer_create(tick,LV_DISP_DEF_REFR_PERIOD,NULL);
    if(!intro.timer)create_failed();
}
void ui_page_00_boot_anim_destroy(void)
{
    stop_timer();if(intro.root)lv_obj_del(intro.root);
    memset(&intro,0,sizeof(intro));
}
bool ui_page_00_boot_anim_is_active(void) {return intro.root!=NULL;}
#endif
