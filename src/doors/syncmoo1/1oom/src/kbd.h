#ifndef INC_1OOM_KBD_H
#define INC_1OOM_KBD_H

#include "types.h"

/* following enum based on SDL_keysym.h: */
/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
typedef enum {
    MOO_KEY_UNKNOWN     = 0,
    MOO_KEY_FIRST       = 0,
    MOO_KEY_RELEASE     = 1,
    MOO_KEY_BACKSPACE   = 8,
    MOO_KEY_TAB         = 9,
    MOO_KEY_CLEAR       = 12,
    MOO_KEY_RETURN      = 13,
    MOO_KEY_PAUSE       = 19,
    MOO_KEY_ESCAPE      = 27,
    MOO_KEY_SPACE       = 32,
    MOO_KEY_EXCLAIM     = 33,
    MOO_KEY_QUOTEDBL    = 34,
    MOO_KEY_HASH        = 35,
    MOO_KEY_DOLLAR      = 36,
    MOO_KEY_AMPERSAND   = 38,
    MOO_KEY_QUOTE       = 39,
    MOO_KEY_LEFTPAREN   = 40,
    MOO_KEY_RIGHTPAREN  = 41,
    MOO_KEY_ASTERISK    = 42,
    MOO_KEY_PLUS        = 43,
    MOO_KEY_COMMA       = 44,
    MOO_KEY_MINUS       = 45,
    MOO_KEY_PERIOD      = 46,
    MOO_KEY_SLASH       = 47,
    MOO_KEY_0           = 48,
    MOO_KEY_1           = 49,
    MOO_KEY_2           = 50,
    MOO_KEY_3           = 51,
    MOO_KEY_4           = 52,
    MOO_KEY_5           = 53,
    MOO_KEY_6           = 54,
    MOO_KEY_7           = 55,
    MOO_KEY_8           = 56,
    MOO_KEY_9           = 57,
    MOO_KEY_COLON       = 58,
    MOO_KEY_SEMICOLON   = 59,
    MOO_KEY_LESS        = 60,
    MOO_KEY_EQUALS      = 61,
    MOO_KEY_GREATER     = 62,
    MOO_KEY_QUESTION    = 63,
    MOO_KEY_AT          = 64,
    /*
       Skip uppercase letters
     */
    MOO_KEY_LEFTBRACKET = 91,
    MOO_KEY_BACKSLASH   = 92,
    MOO_KEY_RIGHTBRACKET= 93,
    MOO_KEY_CARET       = 94,
    MOO_KEY_UNDERSCORE  = 95,
    MOO_KEY_BACKQUOTE   = 96,
    MOO_KEY_a           = 97,
    MOO_KEY_b           = 98,
    MOO_KEY_c           = 99,
    MOO_KEY_d           = 100,
    MOO_KEY_e           = 101,
    MOO_KEY_f           = 102,
    MOO_KEY_g           = 103,
    MOO_KEY_h           = 104,
    MOO_KEY_i           = 105,
    MOO_KEY_j           = 106,
    MOO_KEY_k           = 107,
    MOO_KEY_l           = 108,
    MOO_KEY_m           = 109,
    MOO_KEY_n           = 110,
    MOO_KEY_o           = 111,
    MOO_KEY_p           = 112,
    MOO_KEY_q           = 113,
    MOO_KEY_r           = 114,
    MOO_KEY_s           = 115,
    MOO_KEY_t           = 116,
    MOO_KEY_u           = 117,
    MOO_KEY_v           = 118,
    MOO_KEY_w           = 119,
    MOO_KEY_x           = 120,
    MOO_KEY_y           = 121,
    MOO_KEY_z           = 122,
    MOO_KEY_DELETE      = 127,
    /* End of ASCII mapped keysyms */
        /*@}*/
    /** @name Numeric keypad */
        /*@{*/
    MOO_KEY_KP0         = 128,
    MOO_KEY_KP1,
    MOO_KEY_KP2,
    MOO_KEY_KP3,
    MOO_KEY_KP4,
    MOO_KEY_KP5,
    MOO_KEY_KP6,
    MOO_KEY_KP7,
    MOO_KEY_KP8,
    MOO_KEY_KP9,
    MOO_KEY_KP_PERIOD,
    MOO_KEY_KP_DIVIDE,
    MOO_KEY_KP_MULTIPLY,
    MOO_KEY_KP_MINUS,
    MOO_KEY_KP_PLUS,
    MOO_KEY_KP_NONE1,   //MOO_KEY_KP_ENTER
    MOO_KEY_KP_EQUALS,
        /*@}*/

    /** @name Arrows + Home/End pad */
        /*@{*/
    MOO_KEY_UP,
    MOO_KEY_DOWN,
    MOO_KEY_RIGHT,
    MOO_KEY_LEFT,
    MOO_KEY_INSERT,
    MOO_KEY_HOME,
    MOO_KEY_END,
    MOO_KEY_PAGEUP,
    MOO_KEY_PAGEDOWN,
        /*@}*/

    /** @name Function keys */
        /*@{*/
    MOO_KEY_F1,
    MOO_KEY_F2,
    MOO_KEY_F3,
    MOO_KEY_F4,
    MOO_KEY_F5,
    MOO_KEY_F6,
    MOO_KEY_F7,
    MOO_KEY_F8,
    MOO_KEY_F9,
    MOO_KEY_F10,
    MOO_KEY_F11,
    MOO_KEY_F12,
    MOO_KEY_F13,
    MOO_KEY_F14,
    MOO_KEY_F15,
        /*@}*/
    /** @name Key state modifier keys */
        /*@{*/
    MOO_KEY_NUMLOCK,
    MOO_KEY_CAPSLOCK,
    MOO_KEY_SCROLLOCK,
    MOO_KEY_RSHIFT,
    MOO_KEY_LSHIFT,
    MOO_KEY_RCTRL,
    MOO_KEY_LCTRL,
    MOO_KEY_RALT,
    MOO_KEY_LALT,
    MOO_KEY_RMETA,
    MOO_KEY_LMETA,
    MOO_KEY_LSUPER,     /**< Left "Windows" key */
    MOO_KEY_RSUPER,     /**< Right "Windows" key */
    MOO_KEY_MODE,       /**< "Alt Gr" key */
    MOO_KEY_COMPOSE,    /**< Multi-key compose key */
        /*@}*/

    /** @name Miscellaneous function keys */
        /*@{*/
    MOO_KEY_HELP,
    MOO_KEY_PRINT,
    MOO_KEY_SYSREQ,
    MOO_KEY_BREAK,
    MOO_KEY_MENU,
    MOO_KEY_POWER,  /**< Power Macintosh power key */
    MOO_KEY_EURO,   /**< Some european keyboards */
    MOO_KEY_UNDO,   /**< Atari keyboard has Undo */
        /*@}*/

    /* Add any other keys here */
    MOO_KEY_LAST
} mookey_t;

#define MOO_MOD_SHIFT   (1 << 16)
#define MOO_MOD_ALT     (1 << 17)
#define MOO_MOD_CTRL    (1 << 18)
#define MOO_MOD_META    (1 << 19)
#define MOO_MOD_ALL     (MOO_MOD_SHIFT | MOO_MOD_ALT | MOO_MOD_CTRL | MOO_MOD_META)

#define KBD_GET_KEY(_k_)    ((mookey_t)((_k_) & 0xff))
#define KBD_GET_CHAR(_k_)   ((char)(((_k_) >> 8) & 0xff))
#define KBD_GET_MOD(_k_)   ((_k_) & MOO_MOD_ALL)
#define KBD_GET_KEYMOD(_k_) ((_k_) & (MOO_MOD_ALL | 0xff))
#define KBD_MOD_ONLY_SHIFT(_k_) (KBD_GET_MOD(_k_) == MOO_MOD_SHIFT)
#define KBD_MOD_ONLY_CTRL(_k_)  (KBD_GET_MOD(_k_) == MOO_MOD_CTRL)
#define KBD_MOD_ONLY_ALT(_k_)   ((((_k_) & (MOO_MOD_ALT | MOO_MOD_META)) != 0) && (((_k_) & (MOO_MOD_SHIFT | MOO_MOD_CTRL)) == 0))

extern void kbd_clear(void);
extern void kbd_add_keypress(mookey_t key, uint32_t mod, char c);
extern bool kbd_have_keypress(void);
extern uint32_t kbd_get_keypress(void);
extern void kbd_set_pressed(mookey_t key, uint32_t mod, bool pressed);
extern bool kbd_is_pressed(mookey_t key, uint32_t modon, uint32_t modoff);
extern bool kbd_is_modifier(uint32_t mod);

#endif
