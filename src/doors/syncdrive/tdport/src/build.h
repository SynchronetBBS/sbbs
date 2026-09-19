#pragma once
/* Build variant, set by CMake:
 *   default      port of TDEGA.EXE (EGA, 16 colours)
 *   TD_CGA=1     port of TDCGA.EXE (CGA, 4 colours)
 *   TD_HERC=1    TDCGA.EXE started as "tdcga herc" (Hercules monochrome); implies TD_CGA
 * The game code is the same C source in both executables; where TDCGA differs, code uses
 * EGA_CGA(ega, cga) for values and #if TD_CGA for statements. */
#ifndef TD_HERC
#define TD_HERC 0
#endif
#ifndef TD_CGA
#define TD_CGA TD_HERC
#endif
#if TD_HERC && !TD_CGA
#error "TD_HERC requires TD_CGA"
#endif

#define EGA_CGA(ega, cga) (TD_CGA ? (cga) : (ega))

#if TD_HERC
#define TD_VARIANT_NAME "Hercules"
#elif TD_CGA
#define TD_VARIANT_NAME "CGA"
#else
#define TD_VARIANT_NAME "EGA"
#endif
