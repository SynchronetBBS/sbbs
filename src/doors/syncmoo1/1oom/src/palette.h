#ifndef INC_1OOM_PALETTE_H
#define INC_1OOM_PALETTE_H

#include <string.h>

#include "types.h"

extern uint8_t ui_palette[256*3]; /* 6bit palette from the game */

static inline void ui_palette_set_color(int i, uint8_t r, uint8_t g, uint8_t b)
{
    int j = i * 3;
    ui_palette[j] = r & 0x3f;
    ui_palette[j+1] = g & 0x3f;
    ui_palette[j+2] = b & 0x3f;
}

static inline void ui_palette_set(const uint8_t *pal, int first, int num)
{
    memcpy(&ui_palette[first * 3], pal, num * 3);
}

static inline uint8_t palette_6bit_to_8bit(uint8_t six_bit)
{
    return (six_bit << 2) | ((six_bit >> 4) & 3);
}

#endif
