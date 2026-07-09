/* syncmoo1_map.c -- terminal input -> 1oom mookey_t/mod/char mapping.
 *
 * Pure mapping logic: no engine/socket/termgfx dependency (see the .h).
 * The mookey_t enum (1oom/src/kbd.h) is ASCII-valued for printables --
 * MOO_KEY_a..MOO_KEY_z == 97..122, MOO_KEY_0 == 48, MOO_KEY_SPACE == 32,
 * etc. -- so most bytes map straight through via a cast. There is no
 * separate uppercase-letter key in the enum ("skip uppercase letters" in
 * kbd.h), so an uppercase letter or Ctrl combo is expressed as the base
 * key plus a modifier bit, with the literal input byte kept as the char
 * out-param for sm_map_ascii.
 */
#include "syncmoo1_map.h"

bool sm_map_ascii(uint8_t c, mookey_t *key, uint32_t *mod, char *ch)
{
    if (key == NULL || mod == NULL || ch == NULL)
        return false;

    *mod = 0;
    *ch = (char)c;

    switch (c) {
    case '\r':
        *key = MOO_KEY_RETURN;
        return true;
    case '\t':
        *key = MOO_KEY_TAB;
        return true;
    case 0x08: /* backspace */
    case 0x7f: /* delete, treated as backspace from a terminal */
        *key = MOO_KEY_BACKSPACE;
        return true;
    case 0x1b:
        *key = MOO_KEY_ESCAPE;
        return true;
    default:
        break;
    }

    if (c >= 0x01 && c <= 0x1a) {
        /* Ctrl-A..Ctrl-Z. Ctrl-H/I/M (backspace/tab/return) are already
         * handled above by the switch, so this only ever fires for the
         * remaining control bytes in the range. */
        *key = (mookey_t)(MOO_KEY_a + (c - 1));
        *mod = MOO_MOD_CTRL;
        return true;
    }

    if (c >= 'A' && c <= 'Z') {
        /* Uppercase: shift + the lowercase key, literal char preserved. */
        *key = (mookey_t)(c - 'A' + MOO_KEY_a);
        *mod = MOO_MOD_SHIFT;
        return true;
    }

    if (c >= 0x20 && c <= 0x7e) {
        /* Remaining printable ASCII (digits, space, punctuation/symbols,
         * lowercase letters): the enum value equals the byte itself. */
        *key = (mookey_t)c;
        return true;
    }

    return false;
}

bool sm_map_csi(const int *params, int nparams, char final, mookey_t *key,
                 uint32_t *mod)
{
    int n;

    if (key == NULL || mod == NULL)
        return false;

    *mod = 0;

    switch (final) {
    case 'A':
        *key = MOO_KEY_UP;
        return true;
    case 'B':
        *key = MOO_KEY_DOWN;
        return true;
    case 'C':
        *key = MOO_KEY_RIGHT;
        return true;
    case 'D':
        *key = MOO_KEY_LEFT;
        return true;
    case 'H':
        *key = MOO_KEY_HOME;
        return true;
    case 'F':
        *key = MOO_KEY_END;
        return true;
    case '~':
        if (nparams < 1 || params == NULL)
            return false;
        n = params[0];
        switch (n) {
        case 2:
            *key = MOO_KEY_INSERT;
            return true;
        case 3:
            *key = MOO_KEY_DELETE;
            return true;
        case 5:
            *key = MOO_KEY_PAGEUP;
            return true;
        case 6:
            *key = MOO_KEY_PAGEDOWN;
            return true;
        case 11: case 12: case 13: case 14: case 15: /* F1-F5 */
            *key = (mookey_t)(MOO_KEY_F1 + (n - 11));
            return true;
        case 17: case 18: case 19: case 20: case 21: /* F6-F10 */
            *key = (mookey_t)(MOO_KEY_F6 + (n - 17));
            return true;
        case 23: case 24: /* F11-F12 */
            *key = (mookey_t)(MOO_KEY_F11 + (n - 23));
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

void sm_map_mouse(const sm_geom_t *g, int col, int row, int *gx, int *gy)
{
    int px, py;

    if (g->pixel_mode) {
        px = col - 1;
        py = row - 1;
    } else {
        px = (col - 1) * g->cw + g->cw / 2;
        py = (row - 1) * g->ch + g->ch / 2;
    }

    if (px < g->dx)
        px = g->dx;
    if (px > g->dx + g->ew - 1)
        px = g->dx + g->ew - 1;
    if (py < g->dy)
        py = g->dy;
    if (py > g->dy + g->eh - 1)
        py = g->dy + g->eh - 1;

    int x = (px - g->dx) * 320 / g->ew;
    int y = (py - g->dy) * 200 / g->eh;

    if (x < 0)
        x = 0;
    if (x > 319)
        x = 319;
    if (y < 0)
        y = 0;
    if (y > 199)
        y = 199;

    *gx = x;
    *gy = y;
}
