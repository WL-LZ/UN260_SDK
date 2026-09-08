#ifndef UI_LIST_WINDOW_H
#define UI_LIST_WINDOW_H
#include <stdbool.h>
#include <stdint.h>

/* Fixed-height, read-only list navigation. Logical offsets deliberately use
 * 32 bits: 10,000 rows must not overflow LVGL 8's 16-bit object coordinates. */
typedef struct {
    uint32_t count;
    uint32_t first;
    uint16_t rows;
    uint16_t row_height;
    float offset;
    bool paged;
} ui_list_window_t;
void ui_list_window_init(ui_list_window_t *w, uint16_t rows, uint16_t height);
void ui_list_window_update(ui_list_window_t *w, uint32_t count, bool reset);
void ui_list_window_mode(ui_list_window_t *w, bool paged);
void ui_list_window_move(ui_list_window_t *w, float offset);
void ui_list_window_page(ui_list_window_t *w, int step);
uint32_t ui_list_window_pages(const ui_list_window_t *w);
uint32_t ui_list_window_page_number(const ui_list_window_t *w);
uint32_t ui_list_window_last(const ui_list_window_t *w);
float ui_list_window_limit(const ui_list_window_t *w);
#endif
