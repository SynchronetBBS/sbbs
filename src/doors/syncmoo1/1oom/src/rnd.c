#include "config.h"

#include "rnd.h"
#include "hw.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

uint16_t rnd_0_nm1(uint16_t n, uint32_t *seed)
{   /* NOTE: This code is simplified and contains hacks.
       Assembler code in this function would be more appropriate. */
    uint16_t r = 0;
    uint32_t di_si = *seed;

    for (int i = 9; i > 0; --i) {
        uint16_t ax = di_si;
        uint16_t dx;
        {
            uint32_t dx_bx = di_si;
            dx_bx >>= 1;
            ax ^= dx_bx;
            dx_bx >>= 1;
            ax ^= dx_bx;
            dx_bx >>= 2;
            ax ^= dx_bx;
            dx_bx >>= 2;
            ax ^= dx_bx;
            dx = dx_bx >> 25;
        }
        {
            uint8_t al = ax & 0xff;
            uint8_t ah = ax >> 8;
            al ^= dx;
            ax = ((uint16_t)ah << 8) + al;
            dx = ax;
        }
        r <<= 1;
        if (dx & 1) {
            ++r;
        }
        di_si >>= 1;
        if (ax & 1) {
            di_si += 0x80000000;
        }
    }
    if (di_si == 0) {
        di_si = 0x30be;
    }
    *seed = di_si;
    return r % n;
}

uint16_t rnd_1_n(uint16_t n, uint32_t *seed)
{
    return 1 + rnd_0_nm1(n, seed);
}

uint32_t rnd_get_new_seed(void)
{
    uint32_t seed;
    seed = hw_get_time_us();
    if (seed == 0) {
        seed = 123;
    }
    return seed;
}

uint16_t rnd_bitfiddle(uint16_t ax)
{
    if (ax == 0) {
        return 0x35c8;
    }
    int loops = 8;
    uint16_t bx, dx;
    do {
        dx = ax;
        bx = ax;
        bx >>= 1;
        ax ^= bx;
        bx >>= 1;
        ax ^= bx;
        bx >>= 2;
        ax ^= bx;
        bx >>= 2;
        ax ^= bx;
        bx >>= 1;
        ax ^= bx;
        dx >>= 1;
        if (ax & 1) { dx |= 0x8000; }
        ax = dx;
    } while (--loops);
    return ax;
}
