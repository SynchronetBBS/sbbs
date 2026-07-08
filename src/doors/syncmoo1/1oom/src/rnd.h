#ifndef INC_1OOM_RND_H
#define INC_1OOM_RND_H

#include "types.h"

extern uint16_t rnd_0_nm1(uint16_t n, uint32_t *seed);
extern uint16_t rnd_1_n(uint16_t n, uint32_t *seed);
extern uint32_t rnd_get_new_seed(void);
extern uint16_t rnd_bitfiddle(uint16_t ax);

#endif
