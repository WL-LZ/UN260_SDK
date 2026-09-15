#ifndef PAGE_07_CURR_INTERNAL_H
#define PAGE_07_CURR_INTERNAL_H

#include "un260/lv_core/page_07_curr.h"
#include "un260/lv_core/page_07_curr/page_07_curr_carousel.h"
#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>
#include <stdint.h>

#define PAGE07_CURR_MAX_ITEMS MAX_CURRENCIES

typedef struct lv_dma_snapshot lv_dma_snapshot_t;

typedef enum {
    PAGE07_CURR_VIEW_CARD = 0,
    PAGE07_CURR_VIEW_GRID,
} page07_curr_view_mode_t;

typedef struct {
    lv_obj_t *render_root;
    lv_obj_t *composite;
    lv_obj_t *card;
    lv_obj_t *img;
    lv_obj_t *name;
    lv_obj_t *no;
    lv_obj_t *focus_mark;
    lv_obj_t *selected_label;
    lv_obj_t *fav_btn;
    lv_obj_t *fav_icon;
    /* Two appearances, one geometry. Only the nearby working set is pinned. */
    lv_dma_snapshot_t *surface_cache[2];
    int base_x;
    int base_y;
    int abs_idx;
    int drawn_y;
    bool focused;
    bool render_initialized;
    bool render_focused;
    bool using_cache;
    /* True only when a transformed flag is inside render_root (legacy
     * fallback). Split faces keep the flag in the live input-layer tree. */
    bool has_scaled_flag;
    bool favorite_initialized;
    bool favorite_value;
} page07_curr_card_t;

typedef struct {
    lv_obj_t *item;
    lv_obj_t *img;
    lv_obj_t *selected_mark;
    lv_obj_t *name;
    lv_obj_t *fav_btn;
    lv_obj_t *fav_icon;
    int abs_idx;
} page07_curr_grid_item_t;

typedef struct {
    char favorite_codes[PAGE07_CURR_MAX_ITEMS][4];
    int favorite_count;
    int visible_indices[PAGE07_CURR_MAX_ITEMS];
    int visible_count;
    /* Controller-confirmed selection, never a request preview. Carousel motion
     * owns the independent visual focus. */
    int selected_abs_idx;
    int selected_visible_idx;
    page07_curr_view_mode_t view_mode;
    bool favorite_only;
} page07_curr_model_state_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *left_panel;
    lv_obj_t *left_img;
    lv_obj_t *left_code;
    lv_obj_t *left_code_decor;
    lv_obj_t *left_no;
    lv_obj_t *btn_view;
    lv_obj_t *btn_view_label;
    lv_obj_t *btn_favorite;
    lv_obj_t *btn_favorite_label;
    lv_obj_t *btn_back;
    lv_obj_t *btn_back_label;
    lv_obj_t *right_area;
    lv_obj_t *card_layer;
    lv_obj_t *grid_layer;
    lv_obj_t *list;
    lv_obj_t *track;
    lv_obj_t *arrow_prev;
    lv_obj_t *arrow_next;
    lv_obj_t *thumb;
    lv_obj_t *grid_scroll;
    lv_obj_t *empty_label;
} page07_curr_object_refs_t;

typedef struct {
    page07_curr_card_t cards[PAGE07_CURR_MAX_ITEMS];
    page07_curr_grid_item_t grid_items[PAGE07_CURR_MAX_ITEMS];
    page07_curr_model_state_t model;
    page07_curr_carousel_t carousel;
    page07_curr_object_refs_t objects;
} page07_curr_context_t;

extern page07_curr_context_t g_page07_curr;

void page07_curr_model_load(void);
void page07_curr_model_save(void);
bool page07_curr_model_code_equal(const char *left, const char *right);
int page07_curr_model_find_abs_idx(const char *code);
bool page07_curr_model_is_favorite(int abs_idx);
bool page07_curr_model_is_fixed(int abs_idx);
void page07_curr_model_toggle_favorite(int abs_idx);
void page07_curr_model_refresh_visible(void);
int page07_curr_model_find_visible_pos(int abs_idx);

#endif /* PAGE_07_CURR_INTERNAL_H */
