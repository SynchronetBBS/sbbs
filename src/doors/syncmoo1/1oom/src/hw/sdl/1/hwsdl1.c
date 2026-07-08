#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SDLMAIN
#include "SDL_main.h"
#endif

#include "SDL.h"
#include "SDL_keysym.h"

#include "hw.h"
#include "hwsdl_audio.h"
#include "hwsdl_mouse.h"
#include "hwsdl_opt.h"
#include "hwsdl_video.h"
#include "kbd.h"
#include "log.h"
#include "main.h"
#include "options.h"
#include "types.h"
#include "version.h"

/* -------------------------------------------------------------------------- */

const char *idstr_hw = "sdl1";

/* -------------------------------------------------------------------------- */

static mookey_t key_xlat[SDLK_LAST];

static void build_key_xlat(void)
{
    memset(key_xlat, 0, sizeof(key_xlat));
    key_xlat[SDLK_FIRST] = MOO_KEY_FIRST;
    key_xlat[SDLK_BACKSPACE] = MOO_KEY_BACKSPACE;
    key_xlat[SDLK_TAB] = MOO_KEY_TAB;
    key_xlat[SDLK_CLEAR] = MOO_KEY_CLEAR;
    key_xlat[SDLK_RETURN] = MOO_KEY_RETURN;
    key_xlat[SDLK_PAUSE] = MOO_KEY_PAUSE;
    key_xlat[SDLK_ESCAPE] = MOO_KEY_ESCAPE;
    key_xlat[SDLK_SPACE] = MOO_KEY_SPACE;
    key_xlat[SDLK_EXCLAIM] = MOO_KEY_EXCLAIM;
    key_xlat[SDLK_QUOTEDBL] = MOO_KEY_QUOTEDBL;
    key_xlat[SDLK_HASH] = MOO_KEY_HASH;
    key_xlat[SDLK_DOLLAR] = MOO_KEY_DOLLAR;
    key_xlat[SDLK_AMPERSAND] = MOO_KEY_AMPERSAND;
    key_xlat[SDLK_QUOTE] = MOO_KEY_QUOTE;
    key_xlat[SDLK_LEFTPAREN] = MOO_KEY_LEFTPAREN;
    key_xlat[SDLK_RIGHTPAREN] = MOO_KEY_RIGHTPAREN;
    key_xlat[SDLK_ASTERISK] = MOO_KEY_ASTERISK;
    key_xlat[SDLK_PLUS] = MOO_KEY_PLUS;
    key_xlat[SDLK_COMMA] = MOO_KEY_COMMA;
    key_xlat[SDLK_MINUS] = MOO_KEY_MINUS;
    key_xlat[SDLK_PERIOD] = MOO_KEY_PERIOD;
    key_xlat[SDLK_SLASH] = MOO_KEY_SLASH;
    key_xlat[SDLK_0] = MOO_KEY_0;
    key_xlat[SDLK_1] = MOO_KEY_1;
    key_xlat[SDLK_2] = MOO_KEY_2;
    key_xlat[SDLK_3] = MOO_KEY_3;
    key_xlat[SDLK_4] = MOO_KEY_4;
    key_xlat[SDLK_5] = MOO_KEY_5;
    key_xlat[SDLK_6] = MOO_KEY_6;
    key_xlat[SDLK_7] = MOO_KEY_7;
    key_xlat[SDLK_8] = MOO_KEY_8;
    key_xlat[SDLK_9] = MOO_KEY_9;
    key_xlat[SDLK_COLON] = MOO_KEY_COLON;
    key_xlat[SDLK_SEMICOLON] = MOO_KEY_SEMICOLON;
    key_xlat[SDLK_LESS] = MOO_KEY_LESS;
    key_xlat[SDLK_EQUALS] = MOO_KEY_EQUALS;
    key_xlat[SDLK_GREATER] = MOO_KEY_GREATER;
    key_xlat[SDLK_QUESTION] = MOO_KEY_QUESTION;
    key_xlat[SDLK_AT] = MOO_KEY_AT;
    key_xlat[SDLK_LEFTBRACKET] = MOO_KEY_LEFTBRACKET;
    key_xlat[SDLK_BACKSLASH] = MOO_KEY_BACKSLASH;
    key_xlat[SDLK_RIGHTBRACKET] = MOO_KEY_RIGHTBRACKET;
    key_xlat[SDLK_CARET] = MOO_KEY_CARET;
    key_xlat[SDLK_UNDERSCORE] = MOO_KEY_UNDERSCORE;
    key_xlat[SDLK_BACKQUOTE] = MOO_KEY_BACKQUOTE;
    key_xlat[SDLK_a] = MOO_KEY_a;
    key_xlat[SDLK_b] = MOO_KEY_b;
    key_xlat[SDLK_c] = MOO_KEY_c;
    key_xlat[SDLK_d] = MOO_KEY_d;
    key_xlat[SDLK_e] = MOO_KEY_e;
    key_xlat[SDLK_f] = MOO_KEY_f;
    key_xlat[SDLK_g] = MOO_KEY_g;
    key_xlat[SDLK_h] = MOO_KEY_h;
    key_xlat[SDLK_i] = MOO_KEY_i;
    key_xlat[SDLK_j] = MOO_KEY_j;
    key_xlat[SDLK_k] = MOO_KEY_k;
    key_xlat[SDLK_l] = MOO_KEY_l;
    key_xlat[SDLK_m] = MOO_KEY_m;
    key_xlat[SDLK_n] = MOO_KEY_n;
    key_xlat[SDLK_o] = MOO_KEY_o;
    key_xlat[SDLK_p] = MOO_KEY_p;
    key_xlat[SDLK_q] = MOO_KEY_q;
    key_xlat[SDLK_r] = MOO_KEY_r;
    key_xlat[SDLK_s] = MOO_KEY_s;
    key_xlat[SDLK_t] = MOO_KEY_t;
    key_xlat[SDLK_u] = MOO_KEY_u;
    key_xlat[SDLK_v] = MOO_KEY_v;
    key_xlat[SDLK_w] = MOO_KEY_w;
    key_xlat[SDLK_x] = MOO_KEY_x;
    key_xlat[SDLK_y] = MOO_KEY_y;
    key_xlat[SDLK_z] = MOO_KEY_z;
    key_xlat[SDLK_DELETE] = MOO_KEY_DELETE;
    key_xlat[SDLK_KP0] = MOO_KEY_KP0;
    key_xlat[SDLK_KP1] = MOO_KEY_KP1;
    key_xlat[SDLK_KP2] = MOO_KEY_KP2;
    key_xlat[SDLK_KP3] = MOO_KEY_KP3;
    key_xlat[SDLK_KP4] = MOO_KEY_KP4;
    key_xlat[SDLK_KP5] = MOO_KEY_KP5;
    key_xlat[SDLK_KP6] = MOO_KEY_KP6;
    key_xlat[SDLK_KP7] = MOO_KEY_KP7;
    key_xlat[SDLK_KP8] = MOO_KEY_KP8;
    key_xlat[SDLK_KP9] = MOO_KEY_KP9;
    key_xlat[SDLK_KP_PERIOD] = MOO_KEY_KP_PERIOD;
    key_xlat[SDLK_KP_DIVIDE] = MOO_KEY_KP_DIVIDE;
    key_xlat[SDLK_KP_MULTIPLY] = MOO_KEY_KP_MULTIPLY;
    key_xlat[SDLK_KP_MINUS] = MOO_KEY_KP_MINUS;
    key_xlat[SDLK_KP_PLUS] = MOO_KEY_KP_PLUS;
    key_xlat[SDLK_KP_ENTER] = MOO_KEY_RETURN;
    key_xlat[SDLK_KP_EQUALS] = MOO_KEY_KP_EQUALS;
    key_xlat[SDLK_UP] = MOO_KEY_UP;
    key_xlat[SDLK_DOWN] = MOO_KEY_DOWN;
    key_xlat[SDLK_RIGHT] = MOO_KEY_RIGHT;
    key_xlat[SDLK_LEFT] = MOO_KEY_LEFT;
    key_xlat[SDLK_INSERT] = MOO_KEY_INSERT;
    key_xlat[SDLK_HOME] = MOO_KEY_HOME;
    key_xlat[SDLK_END] = MOO_KEY_END;
    key_xlat[SDLK_PAGEUP] = MOO_KEY_PAGEUP;
    key_xlat[SDLK_PAGEDOWN] = MOO_KEY_PAGEDOWN;
    key_xlat[SDLK_F1] = MOO_KEY_F1;
    key_xlat[SDLK_F2] = MOO_KEY_F2;
    key_xlat[SDLK_F3] = MOO_KEY_F3;
    key_xlat[SDLK_F4] = MOO_KEY_F4;
    key_xlat[SDLK_F5] = MOO_KEY_F5;
    key_xlat[SDLK_F6] = MOO_KEY_F6;
    key_xlat[SDLK_F7] = MOO_KEY_F7;
    key_xlat[SDLK_F8] = MOO_KEY_F8;
    key_xlat[SDLK_F9] = MOO_KEY_F9;
    key_xlat[SDLK_F10] = MOO_KEY_F10;
    key_xlat[SDLK_F11] = MOO_KEY_F11;
    key_xlat[SDLK_F12] = MOO_KEY_F12;
    key_xlat[SDLK_F13] = MOO_KEY_F13;
    key_xlat[SDLK_F14] = MOO_KEY_F14;
    key_xlat[SDLK_F15] = MOO_KEY_F15;
    key_xlat[SDLK_NUMLOCK] = MOO_KEY_NUMLOCK;
    key_xlat[SDLK_CAPSLOCK] = MOO_KEY_CAPSLOCK;
    key_xlat[SDLK_SCROLLOCK] = MOO_KEY_SCROLLOCK;
    key_xlat[SDLK_RSHIFT] = MOO_KEY_RSHIFT;
    key_xlat[SDLK_LSHIFT] = MOO_KEY_LSHIFT;
    key_xlat[SDLK_RCTRL] = MOO_KEY_RCTRL;
    key_xlat[SDLK_LCTRL] = MOO_KEY_LCTRL;
    key_xlat[SDLK_RALT] = MOO_KEY_RALT;
    key_xlat[SDLK_LALT] = MOO_KEY_LALT;
    key_xlat[SDLK_RMETA] = MOO_KEY_RMETA;
    key_xlat[SDLK_LMETA] = MOO_KEY_LMETA;
    key_xlat[SDLK_LSUPER] = MOO_KEY_LSUPER;
    key_xlat[SDLK_RSUPER] = MOO_KEY_RSUPER;
    key_xlat[SDLK_MODE] = MOO_KEY_MODE;
    key_xlat[SDLK_COMPOSE] = MOO_KEY_COMPOSE;
    key_xlat[SDLK_HELP] = MOO_KEY_HELP;
    key_xlat[SDLK_PRINT] = MOO_KEY_PRINT;
    key_xlat[SDLK_SYSREQ] = MOO_KEY_SYSREQ;
    key_xlat[SDLK_BREAK] = MOO_KEY_BREAK;
    key_xlat[SDLK_MENU] = MOO_KEY_MENU;
    key_xlat[SDLK_POWER] = MOO_KEY_POWER;
    key_xlat[SDLK_EURO] = MOO_KEY_EURO;
    key_xlat[SDLK_UNDO] = MOO_KEY_UNDO;
}

static inline uint32_t mod_xlat(SDLMod smod)
{
    uint32_t mod = 0;
    if (smod & KMOD_SHIFT) { mod |= MOO_MOD_SHIFT; }
    if (smod & KMOD_ALT) { mod |= MOO_MOD_ALT; }
    if (smod & KMOD_CTRL) { mod |= MOO_MOD_CTRL; }
    if (smod & KMOD_META) { mod |= MOO_MOD_META; }
    return mod;
}

/* -------------------------------------------------------------------------- */

#define SDL1or2Key  SDLKey
#define SDL1or2Mod  SDLMod

#include "hwsdl.c"

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    return main_1oom(argc, argv);
}

int hw_early_init(void)
{
    return 0;
}

int hw_init(void)
{
    int flags = SDL_INIT_VIDEO | (opt_audio_enabled ? SDL_INIT_AUDIO : 0);
    log_message("SDL_Init\n");
    if (SDL_Init(flags) < 0) {
        log_error("SDL_Init(0x%x) failed: %s\n", flags, SDL_GetError());
        return 11;
    }
    SDL_WM_SetCaption(PACKAGE_NAME " " VERSION_STR, PACKAGE_NAME " " VERSION_STR);
    if (hw_audio_init()) {
        return 12;
    }
    SDL_EnableUNICODE(1);
    build_key_xlat();
    return 0;
}

void hw_shutdown(void)
{
    hw_audio_shutdown();
    hw_video_shutdown();
    log_message("SDL_Quit\n");
    SDL_Quit();
}

void hw_textinput_start(void)
{
}

void hw_textinput_stop(void)
{
}

int hw_event_handle(void)
{
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            uint32_t mod;
            case SDL_KEYDOWN:
                {
                    uint16_t c;
                    SDLMod smod;
                    c = e.key.keysym.unicode;
                    smod = e.key.keysym.mod;
                    if (((c & 0xff80) == 0) && ((c & 0x7f) != 0)) {
                        c &= 0x7f;
                    } else {
                        c = 0;
                    }
                    if (!(hw_kbd_check_hotkey(e.key.keysym.sym, smod, c))) {
                        mookey_t key;
                        key = key_xlat[e.key.keysym.sym];
                        mod = mod_xlat(smod);
                        if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL) mod |= MOO_MOD_CTRL;
                        if (e.key.keysym.sym == SDLK_LALT || e.key.keysym.sym == SDLK_RALT) mod |= MOO_MOD_ALT;
                        if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT) mod |= MOO_MOD_SHIFT;
                        kbd_add_keypress(key, mod, c);
                        kbd_set_pressed(key, mod, true);
                    }
                }
                break;
            case SDL_KEYUP:
                mod = mod_xlat(e.key.keysym.mod);
                if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL) mod &= ~MOO_MOD_CTRL;
                kbd_set_pressed(key_xlat[e.key.keysym.sym], mod, false);
                break;
            case SDL_MOUSEMOTION:
                if (hw_mouse_enabled) {
                    hw_mouse_move((int)(e.motion.xrel), (int)(e.motion.yrel));
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if ((e.button.state == SDL_PRESSED) && ((e.button.button == SDL_BUTTON_WHEELUP) || (e.button.button == SDL_BUTTON_WHEELDOWN))) {
                    hw_mouse_scroll((e.button.button == SDL_BUTTON_WHEELUP) ? -1 : 1);
                } else {
                    hw_mouse_button((int)(e.button.button), (e.button.state == SDL_PRESSED));
                }
                break;
            case SDL_QUIT:
                hw_audio_shutdown_pre();
                exit(EXIT_SUCCESS);
                break;
            case SDL_VIDEORESIZE:
                hw_video_resize((unsigned int)e.resize.w, (unsigned int)e.resize.h);
                break;
            case SDL_VIDEOEXPOSE:
                hw_video_update();
                break;
            case SDL_ACTIVEEVENT:
                {
                    Uint8 state;
                    state = SDL_GetAppState();
                    if ((state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) != (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
                        hw_mouse_ungrab();
                    }
                }
                break;
            default:
                break;
        }
    }

    if (hw_audio_check_process()) {
        exit(EXIT_FAILURE);
    }

    SDL_Delay(10);
    return 0;
}

bool hw_kbd_set_repeat(bool enabled)
{
    if (enabled) {
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    } else {
        SDL_EnableKeyRepeat(0, 0);
    }
    return true;
}
