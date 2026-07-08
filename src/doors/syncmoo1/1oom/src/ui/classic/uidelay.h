#ifndef INC_1OOM_UIDELAY_H
#define INC_1OOM_UIDELAY_H

#include "types.h"

/* The ticks used by MOO1 are from 0040:005C, which is incremented by INT 8
   at 18.2 Hz. (FIXME ... unless the sound driver messes with it.)
   Hence the ratio 10000000 / 182 == */
#define MOO_TICKS_TO_US(_t_) (((_t_) * 5000000) / 91)

extern bool ui_delay_enabled;

extern void ui_delay_prepare(void);
/* returns true if exit by click */
extern bool ui_delay_ticks_or_click(int ticks);
extern bool ui_delay_us_or_click(uint32_t us);

extern void ui_delay_1(void);
extern void ui_delay_1e(void);

#endif
