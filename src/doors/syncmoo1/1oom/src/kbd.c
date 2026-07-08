#include "config.h"

#include "boolvec.h"
#include "kbd.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define KBD_BUFSIZE 16
static struct {
    bool full;
    int head;
    int tail;
    uint32_t mod;
    uint32_t buf[KBD_BUFSIZE];
    BOOLVEC_DECLARE(pressed, MOO_KEY_LAST);
} kbd = { 0 };

/* -------------------------------------------------------------------------- */

void kbd_clear(void)
{
    kbd.full = false;
    kbd.head = 0;
    kbd.tail = 0;
    kbd.mod = 0;
    BOOLVEC_CLEAR(kbd.pressed, MOO_KEY_LAST);
}

void kbd_add_keypress(mookey_t key, uint32_t mod, char c)
{
    uint32_t value = ((uint32_t)key) | mod | (((uint32_t)c) << 8);
    if (key == MOO_KEY_LAST) {
        return;
    }

    if (key == MOO_KEY_RSHIFT || key == MOO_KEY_LSHIFT
     || key == MOO_KEY_RCTRL  || key == MOO_KEY_LCTRL
     || key == MOO_KEY_RALT   || key == MOO_KEY_LALT) {
        if (kbd_is_pressed(key, 0, 0)) {
            return;
        }
    }

    if (kbd.full) {
        log_warning("kbd: full while inserting 0x%x\n", value);
    } else {
        kbd.buf[kbd.head] = value;
        if (++kbd.head == KBD_BUFSIZE) { kbd.head = 0; }
        if (kbd.head == kbd.tail) {
            kbd.full = true;
        }
    }
}

bool kbd_have_keypress(void)
{
    return kbd.full || (kbd.head != kbd.tail);
}

uint32_t kbd_get_keypress(void)
{
    mookey_t key = MOO_KEY_UNKNOWN;
    if (kbd_have_keypress()) {
        key = kbd.buf[kbd.tail];
        if (++kbd.tail == KBD_BUFSIZE) { kbd.tail = 0; }
        kbd.full = false;
    }
    return key;
}

void kbd_set_pressed(mookey_t key, uint32_t mod, bool pressed)
{
    kbd.mod = mod;
    if ((key != MOO_KEY_UNKNOWN) && (key < MOO_KEY_LAST)) {
        BOOLVEC_SET(kbd.pressed, key, pressed);
    }
}

bool kbd_is_pressed(mookey_t key, uint32_t modon, uint32_t modoff)
{
    return (((kbd.mod & modon) == modon) && ((kbd.mod & modoff) == 0) && BOOLVEC_IS1(kbd.pressed, key));
}

bool kbd_is_modifier(uint32_t mod) { return kbd.mod & mod; }
