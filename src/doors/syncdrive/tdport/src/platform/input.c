/* Keyboard/joystick input — port of TDEGA 0x5C24, 0x67DD..0x6939, 0x7864, 0x8AFD, 0x92A8, 0x95B0, 0x95D9,
 * 0x95F4 (reduced), 0xA12F (port/spec/platform.md §4.5, §5; game_flow.md scores_enter_name).
 *
 * Register note: after a Ctrl hotkey or a pause, 0x5C24 returns 0xFF00 | DH where DX is whatever the
 * called routines left in it (the drained key, the last key read by the pause's getkey_wait, or 0x201
 * from the joystick reader). The internal helpers below thread that DX value explicitly. */
#include "input.h"

#include "../host.h"
#include "../symbols.h"
#include "gfx.h"
#include "timer.h"

static void joy_calibrate_screen(void);

/* INT 16h AH=00h repeated while AH=01h reports a key: returns the LAST buffered key. */
static u16 kbd_drain_last(void)
{
    u16 k = 0;
    do {
        host_kbd_read(&k);
    } while (host_kbd_peek(NULL));
    return k;
}

/* 0xA12F joy_read with its DX side effect (DX = 201h when the reader runs). */
static u8 joy_read_dx(u16 *dx)
{
    if (DSB(DS_joy_enabled) == 0) return 0;
    *dx = 0x201;
    return joy_read();
}

/* 0xA12F joy_read */
u8 joy_read(void)
{
    if (DSB(DS_joy_enabled) == 0) return 0;
    /* PORT: port 201h one-shot timing and the adaptive min/max calibration (DS:6A3E..6A5A) are replaced by
     * fixed thresholds on the host gamepad (platform.md §5). The returned bit format is kept:
     * 1 up, 2 down, 4 right, 8 left, 0x10 button A, 0x20 button B. No gamepad -> no bits. */
    u8 res = 0;
    s16 x, y;
    u8 buttons;
    if (host_joy_read(&x, &y, &buttons)) {
        if (x < -16384) res |= 8;
        else if (x >= 16384) res |= 4;
        if (y < -16384) res |= 1;
        else if (y >= 16384) res |= 2;
        if (buttons & 1) res |= 0x10;
        if (buttons & 2) res |= 0x20;
    }
    DSB(DS_joy_result) = res;
    return res;
}

/* 0x6846 getkey_kbd_ctrl: drain the buffer, keep the last key, handle Ctrl hotkeys. */
static u16 getkey_kbd_ctrl(u16 *dx);
static u16 getkey_wait_dx(u16 *dx);

static u16 getkey_kbd_ctrl(u16 *dx)
{
    u16 k = kbd_drain_last();
    *dx = k;
    u8 al = (u8)k;
    if ((s8)al >= 0x20) return al;              /* ASCII, AH = 0 */
    if (al == 0) return k;                      /* extended: scan << 8 */
    /* PORT: AL >= 0x80 indexes past the CS:686B jump table in the original (crash); ignore the key. */
    if (al >= 0x80) return 0;
    switch (al) {                               /* CS:686B */
    case 0x0A:                                  /* Ctrl-J: joystick on (no modal_pause here) */
        DSB(DS_joy_enabled) = 1;
        if (DSB(DS_joy_calibrated) == 0) joy_calibrate_screen();
        return 0;
    case 0x0B:                                  /* Ctrl-K: keyboard only */
        DSB(DS_joy_enabled) = 0;
        return 0;
    case 0x10:                                  /* Ctrl-P: pause */
        DSB(DS_modal_pause) = 1;
        getkey_wait_dx(dx);
        DSB(DS_modal_pause) = 0;
        return 0;
    case 0x11:                                  /* Ctrl-Q: sound off */
        DSB(DS_snd_flags) &= 3;
        return 0;
    case 0x13:                                  /* Ctrl-S: sound on, resume loop at the stale pointer */
        DSB(DS_snd_flags) |= 4;
        if (DSB(DS_snd_flags) & 2) DSB(DS_snd_playing) |= 2;
        return 0;
    default:                                    /* Enter 0x0D, BS 0x08, Esc 0x1B, ... */
        return al;
    }
}

/* 0x67F8 getkey_kbd (mode 0) */
static u16 getkey_kbd(void)
{
    u16 k;
    if (!host_kbd_peek(NULL)) return 0;
    host_kbd_read(&k);
    if ((u8)k != 0) k &= 0x00FF;
    return k;
}

/* 0x680E getkey_kbd_joy_level (mode 2, unused) */
static u16 getkey_kbd_joy_level(u16 *dx)
{
    DSB(DS_kbd_shift_btn) = host_kbd_shift_flags() & 3;
    if (host_kbd_peek(NULL)) return getkey_kbd_ctrl(dx);
    u16 j = joy_read_dx(dx);
    if (j == 0) return 0;
    u8 hi = (u8)(j >> 4);
    DSB(DS_kbd_shift_btn) |= hi;
    if (hi) return 0x000D;
    return DSW((u16)(DS_joy_menu_scan + (j & 0x0F) * 2));
}

/* 0x68F4 getkey_kbd_joy_edge (mode 4, the one main selects) */
static u16 getkey_kbd_joy_edge(u16 *dx)
{
    DSB(DS_kbd_shift_btn) = host_kbd_shift_flags() & 3;
    if (host_kbd_peek(NULL)) return getkey_kbd_ctrl(dx);
    u16 j = joy_read_dx(dx);
    u8 hi = (u8)(j >> 4);
    DSB(DS_kbd_shift_btn) |= hi;
    u16 r = hi ? 0x000D : DSW((u16)(DS_joy_menu_scan + (j & 0x0F) * 2));
    if (r == DSW(DS_joy_menu_last)) return 0;
    DSW(DS_joy_menu_last) = r;
    return r;
}

/* 0x67E8 getkey dispatch through CS:67F2[input_mode] */
static u16 getkey_dx(u16 *dx)
{
    switch (DSW(DS_input_mode)) {
    case 0: return getkey_kbd();
    case 2: return getkey_kbd_joy_level(dx);
    case 4: return getkey_kbd_joy_edge(dx);
    default:
        /* PORT: other modes jump through code bytes in the original; treat as "no key". */
        return 0;
    }
}

u16 getkey(void)
{
    u16 dx = 0;
    return getkey_dx(&dx);
}

/* 0x67DD */
static u16 getkey_wait_dx(u16 *dx)
{
    u16 k;
    while ((k = getkey_dx(dx)) == 0) host_pump();
    return k;
}

u16 getkey_wait(void)
{
    u16 dx = 0;
    return getkey_wait_dx(&dx);
}

/* 0x6939 */
void kbd_flush(void)
{
    host_kbd_flush();
}

/* 0x8AFD */
void input_set_mode(u16 mode)
{
    DSW(DS_input_mode) = mode;
}

/* PORT: held-key driving controls. Direction codes as in the original (1 up, 2 up-right, 3 right,
 * 4 down-right, 5 down, 6 down-left, 7 left, 8 up-left); A/Z held = fire + up/down (clutch + shift). */
static bool is_held_key(u16 key)
{
    u8 al = (u8)key, scan = (u8)(key >> 8);
    if (al == 'a' || al == 'A' || al == 'z' || al == 'Z') return true;
    return al == 0 && scan >= 0x47 && scan <= 0x51;
}

/* Keys buffered since the last poll count as held for this tick, so a tap shorter than one 80 ms tick
 * still registers, as a buffered keystroke did in the original. */
enum { TAP_UP = 1, TAP_DOWN = 2, TAP_LEFT = 4, TAP_RIGHT = 8, TAP_A = 16, TAP_Z = 32 };

static u8 tap_bits(u16 key)
{
    u8 al = (u8)key;
    if (al == 'a' || al == 'A') return TAP_A;
    if (al == 'z' || al == 'Z') return TAP_Z;
    switch ((u8)(key >> 8)) {
    case 0x47: return TAP_UP | TAP_LEFT;
    case 0x48: return TAP_UP;
    case 0x49: return TAP_UP | TAP_RIGHT;
    case 0x4B: return TAP_LEFT;
    case 0x4D: return TAP_RIGHT;
    case 0x4F: return TAP_DOWN | TAP_LEFT;
    case 0x50: return TAP_DOWN;
    case 0x51: return TAP_DOWN | TAP_RIGHT;
    default:   return 0;
    }
}

static u16 held_controls(u8 taps)
{
    if (host_xt_key_down(0x1E) || (taps & TAP_A)) return 0x11;
    if (host_xt_key_down(0x2C) || (taps & TAP_Z)) return 0x15;
    bool up    = host_xt_key_down(0x48) || host_xt_key_down(0x47) || host_xt_key_down(0x49) || (taps & TAP_UP);
    bool down  = host_xt_key_down(0x50) || host_xt_key_down(0x4F) || host_xt_key_down(0x51) || (taps & TAP_DOWN);
    bool left  = host_xt_key_down(0x4B) || host_xt_key_down(0x47) || host_xt_key_down(0x4F) || (taps & TAP_LEFT);
    bool right = host_xt_key_down(0x4D) || host_xt_key_down(0x49) || host_xt_key_down(0x51) || (taps & TAP_RIGHT);
    if (up && down) up = down = false;
    if (left && right) left = right = false;
    if (up)   return left ? 8 : right ? 2 : 1;
    if (down) return left ? 6 : right ? 4 : 5;
    if (left) return 7;
    if (right) return 3;
    return 0;
}

static u16 input_poll_drive_bios(void);
static u16 drive_key(u16 dx);

/* 0x5C24 input_poll_drive */
u16 input_poll_drive(void)
{
    if (!host_held_keys()) return input_poll_drive_bios();
    /* Drain the buffer; buffered repeats of the held keys are skipped so they cannot displace a real
     * keystroke ("last key wins" applies to the remaining keys). */
    u16 key, last = 0;
    u8 taps = 0;
    bool other = false;
    while (host_kbd_read(&key)) {
        if (is_held_key(key)) taps |= tap_bits(key);
        else { last = key; other = true; }
    }
    if (other) {
        u16 r = drive_key(last);
        if (r != 0) return r;
    }
    u16 held = held_controls(taps);
    if (held) return held;
    return input_poll_drive_bios();              /* joystick path (no key buffered) */
}

static u16 input_poll_drive_bios(void)
{
    if (!host_kbd_peek(NULL)) {
        u16 dx = 0;
        u16 j = joy_read_dx(&dx);
        u16 r = DSB((u16)(DS_joy_dir_map + (j & 0x0F)));
        if (j & 0x30) r |= 0x10;
        return r;
    }
    return drive_key(kbd_drain_last());
}

/* 0x5C24 continued: decode the last buffered key (DX) */
static u16 drive_key(u16 dx)
{
    u8 al = (u8)dx;
    if (al == 'p' || al == 'P') goto pause;
    if (al == 'a' || al == 'A') return 0x11;                /* up + fire */
    if (al == 'z' || al == 'Z') return 0x15;                /* down + fire */
    if ((s8)al >= 0x20) {
        if (al >= '0' && al <= '9') return DSB((u16)(DS_digit_dir_map + (al - '0')));
        return (u16)(0xFF00 | al);
    }
    if (al == 0) {                                          /* extended key */
        s16 bx = (s16)((dx >> 8) - 0x46);
        if (bx <= 0 || bx >= 12) return 0;
        return DSB((u16)(DS_ext_scan_dir_map + bx));
    }
    /* PORT: AL >= 0x80 indexes past the CS:5CC5 jump table in the original (crash); ignore the key. */
    if (al >= 0x80) return 0;
    switch (al) {                                           /* CS:5CC5 */
    case 0x0A: {                                            /* Ctrl-J */
        DSB(DS_joy_enabled) = 1;
        if (DSB(DS_joy_calibrated) == 0) {
            DSB(DS_modal_pause) = 1;
            joy_calibrate_screen();                         /* push dx / pop dx around the call */
            DSB(DS_modal_pause) = 0;
        }
        break;
    }
    case 0x0B:                                              /* Ctrl-K */
        DSB(DS_joy_enabled) = 0;
        break;
    case 0x10:                                              /* Ctrl-P */
    pause:
        DSB(DS_modal_pause) = 1;
        getkey_wait_dx(&dx);                                /* clobbers DX like the original */
        DSB(DS_modal_pause) = 0;
        break;
    case 0x11:                                              /* Ctrl-Q: sound off */
        DSB(DS_snd_flags) &= 3;
        break;
    case 0x13:                                              /* Ctrl-S: sound on */
        DSB(DS_snd_flags) |= 4;
        if (DSB(DS_snd_flags) & 2) DSB(DS_snd_playing) |= 2;
        break;
    case 0x1B:                                              /* Esc */
        return 0xFFFF;
    default:
        break;
    }
    return (u16)(0xFF00 | (dx >> 8));                       /* AH = FF, AL = DH */
}

/* 0x95F4 joy_calibrate_screen (reduced) */
static void joy_calibrate_screen(void)
{
    if (input_poll_drive() & 0x10) {                        /* fire held on entry -> cancel (0x97C2) */
        DSB(DS_joy_enabled) = 0;
        return;
    }
    DSB(DS_joy_calibrated) = 1;
    /* PORT: the 3x3 grid screen (screen save, grid, "move the joystick" loop until fire, restore,
     * delay_ticks(100)) is dropped: the emulated joystick needs no calibration. */
}

/* 0x7864 */
u16 getkey_until_deadline(void)
{
    for (;;) {
        u16 k = getkey();
        if (k != 0) return k;
        if (ticks_elapsed(DSW(DS_deadline_start)) >= DSW(DS_deadline_len)) return 0;
        host_pump();
    }
}

/* 0x95B0 */
int menu_key(void)
{
    s16 k = (s16)getkey_until_deadline();
    if (k == 0) return -1;
    if (k == 0x1B) return 1;
    return k;
}

/* 0x95D9 (signed char compare, result sign-extended) */
int toupper_c(int c)
{
    s8 ch = (s8)c;
    if (ch >= 'a' && ch <= 'z') ch = (s8)(ch - 0x20);
    return ch;
}

/* 0x92A8 text_input_line — high-score name editor */
int text_input_line(char *buf, int maxlen, s16 x, s16 y, u16 timeout)
{
    s16 len = (s16)maxlen;
    s16 i;
    for (i = 0; i < len; i++) buf[i] = ' ';
    buf[len] = 0;
    gfx_draw_text(buf, x, y);

    s16 pos = 0;
    s16 cur_h = 2;                              /* cursor glyph index: 2 overwrite, 8 insert */
    s16 insert = 0;
    draw_glyph(x, y, 2);
    set_deadline(timeout);

    for (;;) {
        int key = menu_key();
        if (key == -1) break;                   /* timeout */
        if (key == 0x0D) break;                 /* Enter */
        set_deadline(timeout);                  /* idle timeout restarts after every other key */

        if (key == 0x4D00) {                    /* Right */
            draw_glyph((s16)(x + (pos << 3)), y, cur_h);
            if (len - 1 > pos) pos++;
        } else if (key == 0x4B00) {             /* Left */
            draw_glyph((s16)(x + (pos << 3)), y, cur_h);
            if (pos != 0) pos--;
        } else if (key == 0x5200) {             /* Ins */
            draw_glyph((s16)(x + (pos << 3)), y, cur_h);
            if (insert == 0) { insert = 1; cur_h = 8; }
            else             { insert = 0; cur_h = 2; }
        } else if (key == 0x5300) {             /* Del */
            for (i = pos; len - 1 > i; i++) buf[i] = buf[i + 1];
            buf[len - 1] = ' ';
            gfx_draw_text(buf, x, y);
        } else if (key == 0x08) {               /* Backspace */
            if (pos == 0) continue;             /* no redraw */
            pos--;
            buf[pos] = ' ';
            gfx_draw_text(buf, x, y);
        } else if (key >= 0x20 && key <= 0x7A) {
            if (insert) {
                /* Faithful quirk: the loop stops at i > pos, so buf[pos+1] keeps its old value and the
                 * old buf[pos] is overwritten instead of shifted. */
                for (i = (s16)(len - 2); i > pos; i--) buf[i + 1] = buf[i];
            }
            buf[pos] = (char)key;
            if (len - 1 > pos) pos++;
            gfx_draw_text(buf, x, y);
        } else {
            continue;                           /* other keys (incl. Esc = 1): no redraw */
        }
        draw_glyph((s16)(x + (pos << 3)), y, cur_h);
    }
    draw_glyph((s16)(x + (pos << 3)), y, cur_h);  /* remove the cursor */
    /* TODO(verify): the original returns whatever AX draw_glyph left; the only caller (0x1482) ignores it. */
    return 0;
}
