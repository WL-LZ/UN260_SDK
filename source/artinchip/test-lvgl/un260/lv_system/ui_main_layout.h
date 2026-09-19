#ifndef UI_MAIN_LAYOUT_H
#define UI_MAIN_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>

/* Stable item identities in each ordered slot; no LVGL or business state. */
typedef struct {
    uint8_t left[4];
    uint8_t right[3];
    uint8_t mirrored;
    uint8_t footer_swapped;
} ui_main_layout_t;
void ui_main_layout_default(ui_main_layout_t *layout);
void ui_main_layout_normalize(ui_main_layout_t *layout);
bool ui_main_layout_swap(ui_main_layout_t *layout, unsigned group,
                         unsigned source, unsigned target);
bool ui_main_layout_equal(const ui_main_layout_t *a, const ui_main_layout_t *b);
#endif
