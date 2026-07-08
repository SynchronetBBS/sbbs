#include <assert.h>
#include "kbd.h"
#include "syncmoo1_map.h"
int main(void) {
    mookey_t k; uint32_t m; char c;
    /* lowercase letter */
    assert(sm_map_ascii('a', &k, &m, &c) && k==MOO_KEY_a && m==0 && c=='a');
    /* uppercase -> shift + lowercase key, char preserved */
    assert(sm_map_ascii('A', &k, &m, &c) && k==MOO_KEY_a
           && (m&MOO_MOD_SHIFT) && c=='A');
    /* Enter / Escape / Backspace */
    assert(sm_map_ascii('\r', &k, &m, &c) && k==MOO_KEY_RETURN);
    assert(sm_map_ascii(0x1b, &k, &m, &c) && k==MOO_KEY_ESCAPE);
    assert(sm_map_ascii(0x08, &k, &m, &c) && k==MOO_KEY_BACKSPACE);
    /* Ctrl-A -> 'a' key + CTRL */
    assert(sm_map_ascii(0x01, &k, &m, &c) && k==MOO_KEY_a
           && (m&MOO_MOD_CTRL));
    /* CSI arrows: ESC [ A/B/C/D */
    int none[1];
    assert(sm_map_csi(none, 0, 'A', &k, &m) && k==MOO_KEY_UP);
    assert(sm_map_csi(none, 0, 'D', &k, &m) && k==MOO_KEY_LEFT);
    /* CSI ~ function keys: ESC [ 1 5 ~ == F5 */
    int p1[1] = {15};
    assert(sm_map_csi(p1, 1, '~', &k, &m) && k==MOO_KEY_F5);
    return 0;
}
