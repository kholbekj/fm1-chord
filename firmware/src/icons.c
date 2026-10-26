/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Icons: 4-bit alpha cells of the Fukiai icon font (MIT), 16 and 24 px: web/fukiai.ttf -> build/gen/ui_icons.h
 * (tools/gen_aa_icons.py: the seven the chord machine draws). */
#include "ui_icons.h"

/* icon id at size 16 or 24 px (24: the header battery), fg on bg; returns its width (0: not at that size) */
static int32_t cv_icon_on(int32_t x, int32_t y, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    const uint8_t *idx = size == 24u ? AI24_IDX : AI16_IDX;
    const uint8_t *dat = size == 24u ? AI24_DATA : AI16_DATA;
    if (id >= ICON_COUNT || idx[id] == 0xFFu)
        return 0;
    size = size == 24u ? 24u : 16u;
    cv_alpha(x, y, size, size, dat + (uint32_t)idx[id] * (size * size / 2u), ramp(fg, bg));
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)size, y + cv_oy + (int32_t)size, "icon", 4u);
    return (int32_t)size;
}
/* the same, its ink centred on row cy (header icons: each glyph sits differently in its cell) */
static int32_t cv_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    const uint8_t *idx = size == 24u ? AI24_IDX : AI16_IDX;
    const uint8_t *dat = size == 24u ? AI24_DATA : AI16_DATA;
    int32_t top = -1, bot = 0, r, k;
    size = size == 24u ? 24u : 16u;
    if (id < ICON_COUNT && idx[id] != 0xFFu) {
        const uint8_t *c = dat + (uint32_t)idx[id] * (size * size / 2u);
        for (r = 0; r < (int32_t)size; r++)
            for (k = 0; k < (int32_t)size / 2; k++)
                if (c[r * (int32_t)size / 2 + k]) {
                    if (top < 0) top = r;
                    bot = r;
                    break;
                }
    }
    if (top < 0) { top = 0; bot = (int32_t)size - 1; }
    return cv_icon_on(x, cy - (top + bot + 1) / 2, size, id, fg, bg);
}
