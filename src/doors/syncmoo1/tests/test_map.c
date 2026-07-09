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

    /* Mouse cell->game coord mapping: 640x400 image at (0,0), 8x16 cells. */
    sm_geom_t g = { .ew=640, .eh=400, .dx=0, .dy=0, .cw=8, .ch=16,
                    .pixel_mode=0 };
    int gx, gy;
    /* col=1,row=1 -> cell center (4,8) -> scaled (2,4) */
    sm_map_mouse(&g, 1, 1, &gx, &gy);
    assert(gx==2 && gy==4);
    /* near-bottom-right cell maps proportionally close to the max coord */
    sm_map_mouse(&g, 80, 25, &gx, &gy);
    assert(gx>=317 && gy>=195);
    /* pixel-mode: col-1,row-1 used directly as canvas px */
    g.pixel_mode = 1;
    sm_map_mouse(&g, 321, 201, &gx, &gy);
    assert(gx==160 && gy==100);

    /* Rect clamp: a 320x200 image centered in a 640x400 canvas (offset
       dx=160,dy=100). Cells whose centers fall outside the image rect must
       clamp to the rect edges, mapping to the game-coord extremes. This
       exercises all four rect-clamp branches (the subsequent [0,319]/[0,199]
       clamp is unreachable once px/py are held inside the rect). */
    sm_geom_t gc = { .ew=320, .eh=200, .dx=160, .dy=100, .cw=8, .ch=16,
                     .pixel_mode=0 };
    /* top-left cell center (4,8) is left of/above the rect -> clamps to (0,0) */
    sm_map_mouse(&gc, 1, 1, &gx, &gy);
    assert(gx==0 && gy==0);
    /* far cell center (636,392) is right of/below the rect -> (319,199) */
    sm_map_mouse(&gc, 80, 25, &gx, &gy);
    assert(gx==319 && gy==199);
    return 0;
}
