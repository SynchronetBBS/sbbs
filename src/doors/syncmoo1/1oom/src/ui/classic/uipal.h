#ifndef INC_1OOM_UIPAL_H
#define INC_1OOM_UIPAL_H

#include "types.h"

extern void ui_palette_set_n(void);
extern void ui_palette_fade_n(uint16_t fadepercent);

extern void ui_palette_fade_wait_1s(void);

extern void ui_palette_fadeout_19_19_1(void);
extern void ui_palette_fadeout_14_14_2(void);
extern void ui_palette_fadeout_a_f_1(void);
extern void ui_palette_fadeout_4_3_1(void);
extern void ui_palette_fadeout_5_5_1(void);
extern void ui_palette_fadein_60_3_1(void);
extern void ui_palette_fadein_5f_5_1(void);
extern void ui_palette_fadein_5a_f_1(void);
extern void ui_palette_fadein_50_14_2(void);
extern void ui_palette_fadein_4b_19_1(void);

#endif
