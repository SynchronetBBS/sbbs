#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SDLMAIN
#include "SDL_main.h"
#endif

#include "SDL.h"
#include "SDL_keycode.h"

#include "hw.h"
#include "hwsdl_audio.h"
#include "hwsdl_mouse.h"
#include "hwsdl_opt.h"
#include "hwsdl_video.h"
#include "hwsdl2_video.h"
#include "kbd.h"
#include "log.h"
#include "main.h"
#include "mouse.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

const char *idstr_hw = "sdl2";

/* -------------------------------------------------------------------------- */

static mookey_t key_xlat_key[0x80];
static mookey_t key_xlat_scan[SDL_NUM_SCANCODES];
static char key_xlat_char[SDL_NUM_SCANCODES];

#define SDLK_TBLI_FROM_SCAN(i) ((i) & (~SDLK_SCANCODE_MASK))

static void build_key_xlat(void)
{
    memset(key_xlat_key, 0, sizeof(key_xlat_key));
    memset(key_xlat_scan, 0, sizeof(key_xlat_scan));
    memset(key_xlat_char, 0, sizeof(key_xlat_char));
    key_xlat_key[SDLK_BACKSPACE] = MOO_KEY_BACKSPACE;
    key_xlat_key[SDLK_TAB] = MOO_KEY_TAB;
    key_xlat_key[SDLK_RETURN] = MOO_KEY_RETURN;
    key_xlat_key[SDLK_ESCAPE] = MOO_KEY_ESCAPE;
    key_xlat_key[SDLK_SPACE] = MOO_KEY_SPACE;
    key_xlat_key[SDLK_EXCLAIM] = MOO_KEY_EXCLAIM;
    key_xlat_key[SDLK_QUOTEDBL] = MOO_KEY_QUOTEDBL;
    key_xlat_key[SDLK_HASH] = MOO_KEY_HASH;
    key_xlat_key[SDLK_DOLLAR] = MOO_KEY_DOLLAR;
    key_xlat_key[SDLK_AMPERSAND] = MOO_KEY_AMPERSAND;
    key_xlat_key[SDLK_QUOTE] = MOO_KEY_QUOTE;
    key_xlat_key[SDLK_LEFTPAREN] = MOO_KEY_LEFTPAREN;
    key_xlat_key[SDLK_RIGHTPAREN] = MOO_KEY_RIGHTPAREN;
    key_xlat_key[SDLK_ASTERISK] = MOO_KEY_ASTERISK;
    key_xlat_key[SDLK_PLUS] = MOO_KEY_PLUS;
    key_xlat_key[SDLK_COMMA] = MOO_KEY_COMMA;
    key_xlat_key[SDLK_MINUS] = MOO_KEY_MINUS;
    key_xlat_key[SDLK_PERIOD] = MOO_KEY_PERIOD;
    key_xlat_key[SDLK_SLASH] = MOO_KEY_SLASH;
    key_xlat_key[SDLK_0] = MOO_KEY_0;
    key_xlat_key[SDLK_1] = MOO_KEY_1;
    key_xlat_key[SDLK_2] = MOO_KEY_2;
    key_xlat_key[SDLK_3] = MOO_KEY_3;
    key_xlat_key[SDLK_4] = MOO_KEY_4;
    key_xlat_key[SDLK_5] = MOO_KEY_5;
    key_xlat_key[SDLK_6] = MOO_KEY_6;
    key_xlat_key[SDLK_7] = MOO_KEY_7;
    key_xlat_key[SDLK_8] = MOO_KEY_8;
    key_xlat_key[SDLK_9] = MOO_KEY_9;
    key_xlat_key[SDLK_COLON] = MOO_KEY_COLON;
    key_xlat_key[SDLK_SEMICOLON] = MOO_KEY_SEMICOLON;
    key_xlat_key[SDLK_LESS] = MOO_KEY_LESS;
    key_xlat_key[SDLK_EQUALS] = MOO_KEY_EQUALS;
    key_xlat_key[SDLK_GREATER] = MOO_KEY_GREATER;
    key_xlat_key[SDLK_QUESTION] = MOO_KEY_QUESTION;
    key_xlat_key[SDLK_AT] = MOO_KEY_AT;
    key_xlat_key[SDLK_LEFTBRACKET] = MOO_KEY_LEFTBRACKET;
    key_xlat_key[SDLK_BACKSLASH] = MOO_KEY_BACKSLASH;
    key_xlat_key[SDLK_RIGHTBRACKET] = MOO_KEY_RIGHTBRACKET;
    key_xlat_key[SDLK_CARET] = MOO_KEY_CARET;
    key_xlat_key[SDLK_UNDERSCORE] = MOO_KEY_UNDERSCORE;
    key_xlat_key[SDLK_BACKQUOTE] = MOO_KEY_BACKQUOTE;
    key_xlat_key[SDLK_a] = MOO_KEY_a;
    key_xlat_key[SDLK_b] = MOO_KEY_b;
    key_xlat_key[SDLK_c] = MOO_KEY_c;
    key_xlat_key[SDLK_d] = MOO_KEY_d;
    key_xlat_key[SDLK_e] = MOO_KEY_e;
    key_xlat_key[SDLK_f] = MOO_KEY_f;
    key_xlat_key[SDLK_g] = MOO_KEY_g;
    key_xlat_key[SDLK_h] = MOO_KEY_h;
    key_xlat_key[SDLK_i] = MOO_KEY_i;
    key_xlat_key[SDLK_j] = MOO_KEY_j;
    key_xlat_key[SDLK_k] = MOO_KEY_k;
    key_xlat_key[SDLK_l] = MOO_KEY_l;
    key_xlat_key[SDLK_m] = MOO_KEY_m;
    key_xlat_key[SDLK_n] = MOO_KEY_n;
    key_xlat_key[SDLK_o] = MOO_KEY_o;
    key_xlat_key[SDLK_p] = MOO_KEY_p;
    key_xlat_key[SDLK_q] = MOO_KEY_q;
    key_xlat_key[SDLK_r] = MOO_KEY_r;
    key_xlat_key[SDLK_s] = MOO_KEY_s;
    key_xlat_key[SDLK_t] = MOO_KEY_t;
    key_xlat_key[SDLK_u] = MOO_KEY_u;
    key_xlat_key[SDLK_v] = MOO_KEY_v;
    key_xlat_key[SDLK_w] = MOO_KEY_w;
    key_xlat_key[SDLK_x] = MOO_KEY_x;
    key_xlat_key[SDLK_y] = MOO_KEY_y;
    key_xlat_key[SDLK_z] = MOO_KEY_z;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_CAPSLOCK)] = MOO_KEY_CAPSLOCK;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F1)] = MOO_KEY_F1;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F2)] = MOO_KEY_F2;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F3)] = MOO_KEY_F3;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F4)] = MOO_KEY_F4;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F5)] = MOO_KEY_F5;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F6)] = MOO_KEY_F6;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F7)] = MOO_KEY_F7;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F8)] = MOO_KEY_F8;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F9)] = MOO_KEY_F9;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F10)] = MOO_KEY_F10;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F11)] = MOO_KEY_F11;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F12)] = MOO_KEY_F12;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_SCROLLLOCK)] = MOO_KEY_SCROLLOCK;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAUSE)] = MOO_KEY_PAUSE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_INSERT)] = MOO_KEY_INSERT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_HOME)] = MOO_KEY_HOME;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAGEUP)] = MOO_KEY_PAGEUP;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_DELETE)] = MOO_KEY_DELETE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_END)] = MOO_KEY_END;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAGEDOWN)] = MOO_KEY_PAGEDOWN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RIGHT)] = MOO_KEY_RIGHT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LEFT)] = MOO_KEY_LEFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_DOWN)] = MOO_KEY_DOWN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_UP)] = MOO_KEY_UP;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_CLEAR)] = MOO_KEY_CLEAR;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F13)] = MOO_KEY_F13;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F14)] = MOO_KEY_F14;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F15)] = MOO_KEY_F15;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_0)] = MOO_KEY_KP0;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_1)] = MOO_KEY_KP1;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_2)] = MOO_KEY_KP2;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_3)] = MOO_KEY_KP3;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_4)] = MOO_KEY_KP4;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_5)] = MOO_KEY_KP5;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_6)] = MOO_KEY_KP6;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_7)] = MOO_KEY_KP7;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_8)] = MOO_KEY_KP8;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_9)] = MOO_KEY_KP9;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_PERIOD)] = MOO_KEY_KP_PERIOD;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_DIVIDE)] = MOO_KEY_KP_DIVIDE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_MULTIPLY)] = MOO_KEY_KP_MULTIPLY;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_MINUS)] = MOO_KEY_KP_MINUS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_PLUS)] = MOO_KEY_KP_PLUS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_ENTER)] = MOO_KEY_RETURN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_EQUALS)] = MOO_KEY_KP_EQUALS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_NUMLOCKCLEAR)] = MOO_KEY_NUMLOCK;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RSHIFT)] = MOO_KEY_RSHIFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LSHIFT)] = MOO_KEY_LSHIFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RCTRL)] = MOO_KEY_RCTRL;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LCTRL)] = MOO_KEY_LCTRL;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RALT)] = MOO_KEY_RALT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LALT)] = MOO_KEY_LALT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LGUI)] = MOO_KEY_LSUPER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RGUI)] = MOO_KEY_RSUPER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_MODE)] = MOO_KEY_MODE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_HELP)] = MOO_KEY_HELP;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PRINTSCREEN)] = MOO_KEY_PRINT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_SYSREQ)] = MOO_KEY_SYSREQ;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_MENU)] = MOO_KEY_MENU;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_POWER)] = MOO_KEY_POWER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_UNDO)] = MOO_KEY_UNDO;

    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_PERIOD)] = SDLK_PERIOD;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_DIVIDE)] = SDLK_SLASH;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_MULTIPLY)] = SDLK_ASTERISK;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_MINUS)] = SDLK_MINUS;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_PLUS)] = SDLK_PLUS;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_EQUALS)] = SDLK_EQUALS;
}

static inline uint32_t mod_xlat(SDL_Keymod smod)
{
    uint32_t mod = 0;
    if (smod & KMOD_SHIFT) { mod |= MOO_MOD_SHIFT; }
    if (smod & KMOD_ALT) { mod |= MOO_MOD_ALT; }
    if (smod & KMOD_CTRL) { mod |= MOO_MOD_CTRL; }
    return mod;
}

static bool hw_textinput_active = false;
static bool hw_key_press_repeat_active = true;

/* -------------------------------------------------------------------------- */

#define SDL1or2Key  SDL_Keycode
#define SDL1or2Mod  SDL_Keymod

#include "hwsdl.c"

/* -------------------------------------------------------------------------- */

static void hw_event_handle_window(SDL_WindowEvent *e)
{
    switch (e->event) {
        case SDL_WINDOWEVENT_RESIZED:
            hw_video_resize(0, 0);
            break;
        case SDL_WINDOWEVENT_EXPOSED:
            hw_video_update();
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            hw_mouse_ungrab();
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            if (!hw_opt_relmouse && hw_opt_nograbmouse) {
                hw_mouse_grab();
            }
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            hw_video_set_visible(false);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            hw_video_set_visible(true);
            break;
        default:
            break;
    }
}

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
    if (hw_audio_init()) {
        return 12;
    }
    build_key_xlat();
    if (hw_opt_relmouse) {
        SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    }
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
    SDL_StartTextInput();
    hw_textinput_active = true;
}

void hw_textinput_stop(void)
{
    SDL_StopTextInput();
    hw_textinput_active = false;
}

int hw_event_handle(void)
{
    SDL_Event e;

    SDL_PumpEvents();

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            uint32_t mod;
            case SDL_KEYDOWN:
                if (e.key.repeat && !hw_key_press_repeat_active) {
                    break;
                }
                {
                    SDL_Keycode sym;
                    SDL_Keymod smod;
                    char c;
                    sym = e.key.keysym.sym;
                    smod = e.key.keysym.mod;
                    c = 0;
                    if (!(hw_kbd_check_hotkey(sym, smod, c))) {
                        mookey_t key;
                        if (sym & SDLK_SCANCODE_MASK) {
                            key = key_xlat_scan[SDLK_TBLI_FROM_SCAN(sym)];
                            c = 0;
                            if (!hw_textinput_active) {
                                c = key_xlat_char[SDLK_TBLI_FROM_SCAN(sym)];
                            }
                        } else {
                            key = key_xlat_key[sym];
                            c = (char)sym; /* TODO SDL 2 */
                            /* ignore ASCII range when expecting SDL_TEXTINPUT */
                            if (hw_textinput_active && ((key >= MOO_KEY_SPACE) && (key <= MOO_KEY_z))) {
                                key = MOO_KEY_LAST;
                            }
                        }
                        mod = mod_xlat(smod);
                        if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) mod |= MOO_MOD_CTRL;
                        if ((key != MOO_KEY_UNKNOWN) && (key < MOO_KEY_LAST)) {
                            kbd_add_keypress(key, mod, c);
                        }
                        kbd_set_pressed(key, mod, true);
                    }
                }
                break;
            case SDL_KEYUP:
                {
                    SDL_Keycode sym;
                    SDL_Keymod smod;
                    mookey_t key;
                    sym = e.key.keysym.sym;
                    smod = e.key.keysym.mod;
                    mod = mod_xlat(smod);
                    if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) mod &= ~MOO_MOD_CTRL;
                    if (sym & SDLK_SCANCODE_MASK) {
                        key = key_xlat_scan[SDLK_TBLI_FROM_SCAN(sym)];
                    } else {
                        key = key_xlat_key[sym];
                    }
                    kbd_set_pressed(key, mod, false);
                }
                break;
            case SDL_TEXTINPUT:
                if (hw_textinput_active && (e.text.text[0] != 0)) {
                    char c = e.text.text[0];
                    SDL_StopTextInput();
                    SDL_StartTextInput();
                    kbd_add_keypress(MOO_KEY_UNKNOWN, 0, c);
                }
                break;
            case SDL_MOUSEMOTION:
                if (!hw_opt_relmouse && hw_mouse_enabled) {
                    int x, y;
                    x = e.motion.x;
                    y = e.motion.y;
                    if (hw_opt_aspect_ratio_correct) {
                        y = (y * 5 + 5) / 6;
                    }
                    mouse_set_xy_from_hw(x, y);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                hw_mouse_button((int)(e.button.button), (e.button.state == SDL_PRESSED));
                break;
            case SDL_MOUSEWHEEL:
                if (e.wheel.y != 0) {
                    hw_mouse_scroll((e.wheel.y > 0) ? -1 : 1);
                }
                break;
            case SDL_QUIT:
                hw_audio_shutdown_pre();
                exit(EXIT_SUCCESS);
                break;
            case SDL_WINDOWEVENT:
                if (e.window.windowID == hw_video_get_window_id()) {
                    hw_event_handle_window(&e.window);
                }
            default:
                break;
        }
    }
    if (hw_opt_relmouse) {
        int x, y;
        SDL_GetRelativeMouseState(&x, &y);
        if (hw_mouse_enabled) {
            if ((x != 0) || (y != 0)) {
                hw_mouse_move(x, y);
            }
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
    hw_key_press_repeat_active = enabled;
    return true;
}
