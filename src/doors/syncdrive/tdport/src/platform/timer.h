#pragma once
/* Timer and PC-speaker sound — port of TDEGA 0x694A..0x6C3B, 0x7881..0x7894, 0x89F0..0x8A3E, 0x9A01/0x9A05
 * (port/spec/platform.md §4.6). State lives in DGROUP (tick_count DS:642A, snd_* DS:642C..6450,
 * note_div DS:6452..). The speaker is driven through host_speaker(). */
#include "../mem.h"

/* Port setup: installs timer_host_tick() as the host tick handler. */
void timer_init(void);

/* Host tick (100.0404 Hz): runs the installed driving ISR, or timer_isr() when none is installed. */
void timer_host_tick(void);

/* Replacement for hooking INT 8 during driving (0x4908 installs 0x3B1F, 0x1F29 restores): the ISR is
 * responsible for calling timer_isr() first, as the original far-calls the old vector. NULL removes it. */
void timer_set_driving_isr(void (*isr)(void));

void timer_install_menu(void);      /* 0x699E */
void timer_install_drive(void);     /* 0x698C */
void timer_restore(void);           /* 0x694A */
void timer_isr(void);               /* 0x6A1F body: tick_count++, song step (no BIOS chaining, no EOI) */

void delay_ticks(u16 n);            /* 0x6C3B busy wait (pumps the host) */
u16  ticks_now(void);               /* 0x9A01 */
u16  ticks_elapsed(u16 start);      /* 0x9A05 |now - start| */
void set_deadline(u16 ticks);       /* 0x7894 */
void wait_deadline(void);           /* 0x7881 (pumps the host) */

void snd_stop_oneshot(void);        /* 0x89F0 */
void snd_stop_all(void);            /* 0x8A08 */
void snd_set_loop(FarPtr stream);   /* 0x8A0E */
void snd_play_oneshot(FarPtr stream); /* 0x8A3E */
