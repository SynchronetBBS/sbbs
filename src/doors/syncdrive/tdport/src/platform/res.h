#pragma once
/* Memory blocks, archives and misc runtime — port of TDEGA 0x6762, 0x78A5, 0x8A5B, 0x8BAF, 0x8CC0, 0x8D8C,
 * 0x94AB, 0x94C7, 0x9A86..0xA111, 0xA280..0xA37E and the CRT rand/srand (port/spec/platform.md §4.7, §2.4).
 * The DOS memory block spans segments HEAP_BOTTOM..HEAP_TOP of mem[]; cache/buffer slot tables stay in
 * DGROUP (DS:7D38, DS:7F1E) exactly as in the original. Files are opened through host_game_path(). */
#include "../mem.h"

void   mem_init(void);                                         /* 0x8CC0 */
u16    cache_find(const char *fname);                          /* 0xA280 */
u16    cache_alloc(const char *fname, u16 paras, u16 reserve); /* 0xA2BE */
u16    buf_alloc(u16 paras);                                   /* 0xA340 */
void   buf_free(u16 seg);                                      /* 0xA37E */

FarPtr load_archive(const char *fname, u16 reserve);           /* 0x8A5B packed .PES (TDCGA: RLE .CMP), relocated */
#if TD_CGA
FarPtr load_packed_near(const char *fname, u16 ds_buf, u16 *len); /* TDCGA 0x73EA .CMP into DGROUP:ds_buf */
#else
FarPtr load_packed_archive(const char *fname, u16 reserve);    /* 0x9A86 */
#endif
FarPtr load_raw_archive(const char *fname, u16 reserve);       /* 0x78A5 tdsnd.snd */
FarPtr res_find(FarPtr arc, char *name4);                      /* 0x6762 (space-pads name4 in place) */
void   res_find_list(FarPtr arc, const char *names, u16 out_seg, u16 out_off); /* 0x94C7 far ptrs to out */

u8     crc8_b8(const u8 *buf, int len, u8 seed);               /* 0x8D8C SCORES checksum */
u8     rand8(void);                                            /* 0x8BAF table PRNG DS:6520/6522 */
int    crt_rand(void);                                         /* 0xA90E MSC rand, bit-exact */
void   crt_srand(u16 seed);                                    /* 0xA8FD */

_Noreturn void fatal(const char *fmt, ...);                    /* 0x94AB (timer_restore + host_fatal) */
