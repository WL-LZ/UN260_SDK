#include "ui_list_window.h"
#include <math.h>
#include <limits.h>
#include <string.h>

uint32_t ui_list_window_pages(const ui_list_window_t *w)
{
    return w->count ? (w->count - 1) / w->rows + 1 : 1;
}
uint32_t ui_list_window_page_number(const ui_list_window_t *w)
{
    return w->first / w->rows + 1;
}
float ui_list_window_limit(const ui_list_window_t *w)
{
    return w->count > w->rows ? (float)(w->count - w->rows) * w->row_height : 0;
}
uint32_t ui_list_window_last(const ui_list_window_t *w)
{
    uint32_t n = w->first + w->rows;
    if (!w->paged && (uint32_t)w->offset > w->first * w->row_height) ++n;
    return n < w->count ? n : w->count;
}
void ui_list_window_init(ui_list_window_t *w, uint16_t rows, uint16_t height)
{
    memset(w, 0, sizeof(*w));
    w->rows = rows ? rows : 1;
    w->row_height = height ? height : 1;
}
void ui_list_window_move(ui_list_window_t *w, float offset)
{
    if (!isfinite(offset)) return;
    float limit = ui_list_window_limit(w);
    w->offset = offset < 0 ? 0 : (offset > limit ? limit : offset);
    w->first = (uint32_t)(w->offset / w->row_height);
}
void ui_list_window_update(ui_list_window_t *w, uint32_t count, bool reset)
{
    /* Leave room for the extra recycled edge row and float pixel rounding. */
    uint64_t reserve = (uint64_t)(w->rows + 1U) * w->row_height;
    uint32_t safe_count = reserve < INT32_MAX ?
        (uint32_t)((INT32_MAX - reserve) / w->row_height) : 0;
    w->count = count < safe_count ? count : safe_count;
    if (reset) w->first = 0;
    if (w->paged) {
        uint32_t page = w->first / w->rows;
        uint32_t last = ui_list_window_pages(w) - 1;
        w->first = (page < last ? page : last) * w->rows;
        w->offset = (float)w->first * w->row_height;
    } else {
        ui_list_window_move(w, reset ? 0 : w->offset);
    }
}
void ui_list_window_mode(ui_list_window_t *w, bool paged)
{
    w->paged = paged;
    ui_list_window_update(w, w->count, false);
}
void ui_list_window_page(ui_list_window_t *w, int step)
{
    if (!w->paged) return;
    int64_t page = (int64_t)(w->first / w->rows) + step;
    uint32_t last = ui_list_window_pages(w) - 1;
    if (page < 0) page = 0;
    if (page > last) page = last;
    w->first = (uint32_t)page * w->rows;
    w->offset = (float)w->first * w->row_height;
}
