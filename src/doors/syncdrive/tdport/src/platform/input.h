#pragma once
/* Keyboard/joystick input — port of TDEGA 0x5C24, 0x67DD..0x6939, 0x7864, 0x8AFD, 0x92A8, 0x95B0, 0x95D9,
 * 0xA12F (port/spec/platform.md §4.5, game_flow.md for the name editor). Keys come from the host's BIOS
 * keyboard queue (host_kbd_*); the joystick is emulated from host_joy_read(). */
#include "../mem.h"

u16  input_poll_drive(void);        /* 0x5C24 direction/fire/hotkeys for driving and "press fire" waits */
u16  getkey(void);                  /* 0x67E8 non-blocking menu key (dispatch on input_mode DS:6422) */
u16  getkey_wait(void);             /* 0x67DD loops getkey until nonzero (pumps the host) */
void kbd_flush(void);               /* 0x6939 */
void input_set_mode(u16 mode);      /* 0x8AFD */
u16  getkey_until_deadline(void);   /* 0x7864 getkey until the set_deadline() deadline, else 0 */
int  menu_key(void);                /* 0x95B0 0 -> -1, Esc -> 1, else the key */
int  toupper_c(int c);              /* 0x95D9 */
u8   joy_read(void);                /* 0xA12F direction + button bits (emulated from the gamepad) */

/* 0x92A8 high-score name line editor (cursor glyph via draw_glyph). Returns the original's result code;
 * see game_flow.md scores_enter_name and platform.md §4.8 note. */
int  text_input_line(char *buf, int maxlen, s16 x, s16 y, u16 timeout);
