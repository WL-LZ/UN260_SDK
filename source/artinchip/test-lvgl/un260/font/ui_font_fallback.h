#ifndef UN260_UI_FONT_FALLBACK_H
#define UN260_UI_FONT_FALLBACK_H
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/* Compose once per font role, never per frame. A fallback supplies bitmaps,
 * but LVGL positions them against the primary font's baseline and line height.
 * Use the subset's actual glyph extents, not the source TTF's generous leading.
 * The caller owns the stable descriptor; neither source font is modified. */
static inline void ui_font_compose_fallback(lv_font_t *out,
    const lv_font_t *base, const lv_font_t *fallback)
{
    *out = *base;
    out->fallback = fallback;
    if (!fallback) return;
    int32_t above = (int32_t)base->line_height - base->base_line;
    int32_t below = base->base_line;
    if (fallback->get_glyph_dsc == lv_font_get_glyph_dsc_fmt_txt && fallback->dsc) {
        const lv_font_fmt_txt_dsc_t *data = fallback->dsc;
        for (uint16_t i = 0; i < data->cmap_num; ++i) {
            const lv_font_fmt_txt_cmap_t *map = &data->cmaps[i];
            bool sparse = map->type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY ||
                          map->type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL;
            uint32_t count = sparse ? map->list_length : map->range_length;
            for (uint32_t j = 0; j < count; ++j) {
                uint32_t codepoint = map->range_start + (sparse ? map->unicode_list[j] : j);
                lv_font_glyph_dsc_t glyph = {0};
                if (!fallback->get_glyph_dsc(fallback, &glyph, codepoint, 0) ||
                    glyph.is_placeholder || !glyph.box_h) continue;
                int32_t ascent = (int32_t)glyph.box_h + glyph.ofs_y;
                int32_t descent = -(int32_t)glyph.ofs_y;
                if (ascent > above) above = ascent;
                if (descent > below) below = descent;
            }
        }
    } else {
        int32_t ascent = (int32_t)fallback->line_height - fallback->base_line;
        if (ascent > above) above = ascent;
        if (fallback->base_line > below) below = fallback->base_line;
    }
    out->base_line = below;
    out->line_height = above + below;
}
#endif
