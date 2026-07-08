#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include <allegro.h>

#include "hw.h"
#include "hwalleg_audio.h"
#include "hwalleg_mouse.h"
#include "hwalleg_opt.h"
#include "hwalleg_video.h"
#include "kbd.h"
#include "log.h"
#include "main.h"
#include "mouse.h"
#include "options.h"

/* -------------------------------------------------------------------------- */

const char *idstr_hw = "alleg4";

/* -------------------------------------------------------------------------- */

static mookey_t key_xlat[KEY_MAX];

static void build_key_xlat(void)
{
    memset(key_xlat, 0, sizeof(key_xlat));
    key_xlat[KEY_A] = MOO_KEY_a;
    key_xlat[KEY_B] = MOO_KEY_b;
    key_xlat[KEY_C] = MOO_KEY_c;
    key_xlat[KEY_D] = MOO_KEY_d;
    key_xlat[KEY_E] = MOO_KEY_e;
    key_xlat[KEY_F] = MOO_KEY_f;
    key_xlat[KEY_G] = MOO_KEY_g;
    key_xlat[KEY_H] = MOO_KEY_h;
    key_xlat[KEY_I] = MOO_KEY_i;
    key_xlat[KEY_J] = MOO_KEY_j;
    key_xlat[KEY_K] = MOO_KEY_k;
    key_xlat[KEY_L] = MOO_KEY_l;
    key_xlat[KEY_M] = MOO_KEY_m;
    key_xlat[KEY_N] = MOO_KEY_n;
    key_xlat[KEY_O] = MOO_KEY_o;
    key_xlat[KEY_P] = MOO_KEY_p;
    key_xlat[KEY_Q] = MOO_KEY_q;
    key_xlat[KEY_R] = MOO_KEY_r;
    key_xlat[KEY_S] = MOO_KEY_s;
    key_xlat[KEY_T] = MOO_KEY_t;
    key_xlat[KEY_U] = MOO_KEY_u;
    key_xlat[KEY_V] = MOO_KEY_v;
    key_xlat[KEY_W] = MOO_KEY_w;
    key_xlat[KEY_X] = MOO_KEY_x;
    key_xlat[KEY_Y] = MOO_KEY_y;
    key_xlat[KEY_Z] = MOO_KEY_z;
    key_xlat[KEY_0] = MOO_KEY_0;
    key_xlat[KEY_1] = MOO_KEY_1;
    key_xlat[KEY_2] = MOO_KEY_2;
    key_xlat[KEY_3] = MOO_KEY_3;
    key_xlat[KEY_4] = MOO_KEY_4;
    key_xlat[KEY_5] = MOO_KEY_5;
    key_xlat[KEY_6] = MOO_KEY_6;
    key_xlat[KEY_7] = MOO_KEY_7;
    key_xlat[KEY_8] = MOO_KEY_8;
    key_xlat[KEY_9] = MOO_KEY_9;
    key_xlat[KEY_0_PAD] = MOO_KEY_KP0;
    key_xlat[KEY_1_PAD] = MOO_KEY_KP1;
    key_xlat[KEY_2_PAD] = MOO_KEY_KP2;
    key_xlat[KEY_3_PAD] = MOO_KEY_KP3;
    key_xlat[KEY_4_PAD] = MOO_KEY_KP4;
    key_xlat[KEY_5_PAD] = MOO_KEY_KP5;
    key_xlat[KEY_6_PAD] = MOO_KEY_KP6;
    key_xlat[KEY_7_PAD] = MOO_KEY_KP7;
    key_xlat[KEY_8_PAD] = MOO_KEY_KP8;
    key_xlat[KEY_9_PAD] = MOO_KEY_KP9;
    key_xlat[KEY_F1] = MOO_KEY_F1;
    key_xlat[KEY_F2] = MOO_KEY_F2;
    key_xlat[KEY_F3] = MOO_KEY_F3;
    key_xlat[KEY_F4] = MOO_KEY_F4;
    key_xlat[KEY_F5] = MOO_KEY_F5;
    key_xlat[KEY_F6] = MOO_KEY_F6;
    key_xlat[KEY_F7] = MOO_KEY_F7;
    key_xlat[KEY_F8] = MOO_KEY_F8;
    key_xlat[KEY_F9] = MOO_KEY_F9;
    key_xlat[KEY_F10] = MOO_KEY_F10;
    key_xlat[KEY_F11] = MOO_KEY_F11;
    key_xlat[KEY_F12] = MOO_KEY_F12;
    key_xlat[KEY_ESC] = MOO_KEY_ESCAPE;
    /* KEY_TILDE */
    key_xlat[KEY_MINUS] = MOO_KEY_MINUS;
    key_xlat[KEY_EQUALS] = MOO_KEY_EQUALS;
    key_xlat[KEY_BACKSPACE] = MOO_KEY_BACKSPACE;
    key_xlat[KEY_TAB] = MOO_KEY_TAB;
    key_xlat[KEY_OPENBRACE] = MOO_KEY_LEFTBRACKET;
    key_xlat[KEY_CLOSEBRACE] = MOO_KEY_RIGHTBRACKET;
    key_xlat[KEY_ENTER] = MOO_KEY_RETURN;
    key_xlat[KEY_COLON] = MOO_KEY_COLON;
    key_xlat[KEY_QUOTE] = MOO_KEY_QUOTE;
    key_xlat[KEY_BACKSLASH] = MOO_KEY_BACKSLASH;
    /* KEY_BACKSLASH2 */
    key_xlat[KEY_COMMA] = MOO_KEY_COMMA;
    /* KEY_STOP */
    key_xlat[KEY_SLASH] = MOO_KEY_SLASH;
    key_xlat[KEY_SPACE] = MOO_KEY_SPACE;
    key_xlat[KEY_INSERT] = MOO_KEY_INSERT;
    key_xlat[KEY_DEL] = MOO_KEY_DELETE;
    key_xlat[KEY_HOME] = MOO_KEY_HOME;
    key_xlat[KEY_END] = MOO_KEY_END;
    key_xlat[KEY_PGUP] = MOO_KEY_PAGEUP;
    key_xlat[KEY_PGDN] = MOO_KEY_PAGEDOWN;
    key_xlat[KEY_LEFT] = MOO_KEY_LEFT;
    key_xlat[KEY_RIGHT] = MOO_KEY_RIGHT;
    key_xlat[KEY_UP] = MOO_KEY_UP;
    key_xlat[KEY_DOWN] = MOO_KEY_DOWN;
    key_xlat[KEY_SLASH_PAD] = MOO_KEY_KP_DIVIDE;
    key_xlat[KEY_ASTERISK] = MOO_KEY_ASTERISK;  /* or MOO_KEY_KP_MULTIPLY? */
    key_xlat[KEY_MINUS_PAD] = MOO_KEY_KP_MINUS;
    key_xlat[KEY_PLUS_PAD] = MOO_KEY_KP_PLUS;
    key_xlat[KEY_DEL_PAD] = MOO_KEY_KP_PERIOD;
    key_xlat[KEY_ENTER_PAD] = MOO_KEY_RETURN;
    key_xlat[KEY_PRTSCR] = MOO_KEY_PRINT;
    key_xlat[KEY_PAUSE] = MOO_KEY_PAUSE;
    /* KEY_ABNT_C1, KEY_YEN, KEY_KANA, KEY_CONVERT, KEY_NOCONVERT */
    key_xlat[KEY_AT] = MOO_KEY_AT;
    /* KEY_CIRCUMFLEX, KEY_COLON2, KEY_KANJI */
    key_xlat[KEY_LSHIFT] = MOO_KEY_LSHIFT;
    key_xlat[KEY_RSHIFT] = MOO_KEY_RSHIFT;
    key_xlat[KEY_LCONTROL] = MOO_KEY_LCTRL;
    key_xlat[KEY_RCONTROL] = MOO_KEY_RCTRL;
    key_xlat[KEY_ALT] = MOO_KEY_LALT;
    key_xlat[KEY_ALTGR] = MOO_KEY_RALT;
    key_xlat[KEY_LWIN] = MOO_KEY_LSUPER;
    key_xlat[KEY_RWIN] = MOO_KEY_RSUPER;
    key_xlat[KEY_MENU] = MOO_KEY_MENU;
    key_xlat[KEY_SCRLOCK] = MOO_KEY_SCROLLOCK;
    key_xlat[KEY_NUMLOCK] = MOO_KEY_NUMLOCK;
    key_xlat[KEY_CAPSLOCK] = MOO_KEY_CAPSLOCK;
    key_xlat[KEY_EQUALS_PAD] = MOO_KEY_KP_EQUALS;
    key_xlat[KEY_BACKQUOTE] = MOO_KEY_BACKQUOTE;
    key_xlat[KEY_SEMICOLON] = MOO_KEY_SEMICOLON;
    /* KEY_COMMAND */
    /*
    key_xlat[KEY_KP_PERIOD] = MOO_KEY_KP_PERIOD;
    key_xlat[KEY_CLEAR] = MOO_KEY_CLEAR;
    key_xlat[KEY_EXCLAIM] = MOO_KEY_EXCLAIM;
    key_xlat[KEY_QUOTEDBL] = MOO_KEY_QUOTEDBL;
    key_xlat[KEY_HASH] = MOO_KEY_HASH;
    key_xlat[KEY_DOLLAR] = MOO_KEY_DOLLAR;
    key_xlat[KEY_AMPERSAND] = MOO_KEY_AMPERSAND;
    key_xlat[KEY_LEFTPAREN] = MOO_KEY_LEFTPAREN;
    key_xlat[KEY_RIGHTPAREN] = MOO_KEY_RIGHTPAREN;
    key_xlat[KEY_PLUS] = MOO_KEY_PLUS;
    key_xlat[KEY_PERIOD] = MOO_KEY_PERIOD;
    key_xlat[KEY_LESS] = MOO_KEY_LESS;
    key_xlat[KEY_GREATER] = MOO_KEY_GREATER;
    key_xlat[KEY_QUESTION] = MOO_KEY_QUESTION;
    key_xlat[KEY_LEFTBRACKET] = MOO_KEY_LEFTBRACKET;
    key_xlat[KEY_RIGHTBRACKET] = MOO_KEY_RIGHTBRACKET;
    key_xlat[KEY_CARET] = MOO_KEY_CARET;
    key_xlat[KEY_UNDERSCORE] = MOO_KEY_UNDERSCORE;
    key_xlat[KEY_RMETA] = MOO_KEY_RMETA;
    key_xlat[KEY_LMETA] = MOO_KEY_LMETA;
    key_xlat[KEY_MODE] = MOO_KEY_MODE;
    key_xlat[KEY_COMPOSE] = MOO_KEY_COMPOSE;
    key_xlat[KEY_HELP] = MOO_KEY_HELP;
    key_xlat[KEY_SYSREQ] = MOO_KEY_SYSREQ;
    key_xlat[KEY_BREAK] = MOO_KEY_BREAK;
    key_xlat[KEY_POWER] = MOO_KEY_POWER;
    key_xlat[KEY_EURO] = MOO_KEY_EURO;
    key_xlat[KEY_UNDO] = MOO_KEY_UNDO;
    */
}

/* -------------------------------------------------------------------------- */

#include "hwalleg.c"

/* -------------------------------------------------------------------------- */

volatile int64_t hw_timer_count = 0;

static void hw_timer_tick(void)
{
    ++hw_timer_count;
}

END_OF_STATIC_FUNCTION(hw_timer_tick)

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    return main_1oom(argc, argv);
}
END_OF_MAIN()

int hw_early_init(void)
{
    return 0;
}

int hw_init(void)
{
    log_message_direct("allegro_init, version " ALLEGRO_VERSION_STR "\n");
    if (allegro_init()) {
        log_error("allegro_init failed!\n");
        return -1;
    }
    if (install_keyboard() < 0) {
        log_error("install_keyboard failed!\n");
        return -1;
    }
    if (install_mouse() < 0) {
        log_error("install_mouse failed!\n");
        return -1;
    }
    enable_hardware_cursor();
    show_mouse(NULL);
    if (install_timer() < 0) {
        log_error("install_timer failed!\n");
        return -1;
    }
    if (install_int_ex(hw_timer_tick, MSEC_TO_TIMER(1)) != 0) {
        log_error("install_int_ex failed!\n");
        return -1;
    }
    LOCK_VARIABLE(hw_timer_count);
    LOCK_FUNCTION(hw_timer_tick);

    if (hw_audio_init()) {
        return 12;
    }
    build_key_xlat();
    return 0;
}

void hw_shutdown(void)
{
    hw_audio_shutdown();
    hw_video_shutdown();
    log_message_direct("allegro_exit\n");
    allegro_exit();
}

void hw_textinput_start(void)
{
}

void hw_textinput_stop(void)
{
}

int hw_event_handle(void)
{
    if (keyboard_needs_poll()) {
        poll_keyboard();
    }
    while (keypressed()) {
        uint32_t k, smod;
        k = readkey();
        smod = key_shifts;
        if (!(hw_kbd_check_hotkey(k, smod))) {
            mookey_t key;
            uint32_t mod;
            char c;
            mod = 0;
            if (smod & KB_SHIFT_FLAG) { mod |= MOO_MOD_SHIFT; }
            if (smod & KB_ALT_FLAG) { mod |= MOO_MOD_ALT; }
            if (smod & KB_CTRL_FLAG) { mod |= MOO_MOD_CTRL; }
            c = scancode_to_ascii(k >> 8);
            if (((k >> 8) >= KEY_0_PAD) && ((k >> 8) <= KEY_9_PAD)) {
                c = k & 0xff;
            }
            key = key_xlat[(k >> 8)];
            kbd_add_keypress(key, mod, c);
        }
    }
    {
        uint32_t mod = 0;
        if (key_shifts & KB_SHIFT_FLAG) { mod |= MOO_MOD_SHIFT; }
        if (key_shifts & KB_CTRL_FLAG) { mod |= MOO_MOD_CTRL; }
        if (key_shifts & KB_ALT_FLAG) { mod |= MOO_MOD_ALT; }
        kbd_set_pressed(MOO_KEY_UNKNOWN, mod, false);
    }
    if (mouse_needs_poll()) {
        poll_mouse();
    }
    {
        static int prev_x = -1, prev_y, prev_z;
        int pos, x, y, z;
        pos = mouse_pos;
        x = ((unsigned int)pos) >> 16;
        y = pos & 0x0000ffff;
        z = mouse_z;
        if (prev_x < 0) {
            prev_x = x;
            prev_y = y;
            prev_z = z;
        }
        if (hw_opt_relmouse) {
            hw_mouse_move(x - prev_x, y - prev_y);
            if ((x <= 10) || (x >= (hw_mouse_w - 10)) || (y <= 10) || (y >= (hw_mouse_h - 10))) {
                x = hw_mouse_w / 2;
                y = hw_mouse_h / 2;
                position_mouse(x, y);
            }
        } else {
            mouse_set_xy_from_hw(x, y);
        }
        prev_x = x;
        prev_y = y;
        if (prev_z != z) {
            hw_mouse_scroll((prev_z < z) ? -1 : 1);
            prev_z = z;
        }
    }
    hw_mouse_buttons(mouse_b);
    /* rest(10) here would be nice for multitasking OSs,
       but seems to cause crashes on DOS mouse handler (?!) */
    return 0;
}

int64_t hw_get_time_us(void)
{
    return hw_timer_count * 1000;
}

bool hw_kbd_set_repeat(bool enabled)
{
    if (enabled) {
        set_keyboard_rate(250, 33);
    } else {
        set_keyboard_rate(0, 0);
    }
    return true;
}
