/* syncmoo1_input.c -- socket read loop, ESC/CSI/APC state machine, key +
 * mouse decode, capability-probe reply parsing.
 *
 * Structure and proven patterns are ported from src/doors/syncduke/
 * syncduke_input.c (the NORMAL -> ESC -> {CSI | APC/DCS/OSC/PM} byte state
 * machine, csi_params(), the lone-ESC follow-up timer) and
 * src/doors/syncconquer/door/door_input.c + door_io.c (the SGR mouse decode,
 * the probe-reply CSI cases 't'/'R'/'c'/'y'/'n'). Unlike those two doors this
 * module does NOT reimplement key/mouse mapping itself: the actual terminal
 * byte -> 1oom mookey_t/mod/char and SGR col/row -> game (x,y) arithmetic is
 * Tasks 3/4's pure, unit-tested syncmoo1_map.c (sm_map_ascii/sm_map_csi/
 * sm_map_mouse) -- this file's job is purely the I/O/state-machine plumbing
 * around it and the 1oom-side injection calls.
 *
 * Injection targets (1oom globals, not a queue we own):
 *   keys  -> kbd_add_keypress(mookey_t key, uint32_t mod, char c)  (kbd.h)
 *            M1 synthesizes PRESS only -- no kbd_set_pressed() held-key
 *            state; menu/point-click navigation doesn't need it.
 *   mouse -> mouse_set_xy_from_hw(gx,gy), mouse_set_buttons_from_hw(mask),
 *            mouse_set_scroll_from_hw(+-1)                          (mouse.h)
 *
 * Probe-reply parsing (DESIGN.md Sec9) feeds sm_io's geometry setters
 * (syncmoo1_io.c / syncmoo1.h: sm_io_set_canvas/set_grid/set_pixel_mode) so
 * sm_io_geom() -- and therefore sm_map_mouse() -- becomes probe-driven
 * instead of sitting on the 640x400/80x25/8x16 default forever.
 *
 * DSR-ACK pacing (Task 9, DESIGN.md Sec9's "DSR-ACK backpressure" bullet):
 * the 'R' case below distinguishes the startup grid-probe reply (latches the
 * grid, not a pace-ack) from a pace-ack for a present()-sent frame (via
 * sm_io_pace_ack() -- syncmoo1_io.c owns the in-flight/AIMD state). It does
 * so via sm_io_take_grid_probe(), a one-shot flag sm_io ARMS when it actually
 * sends the probe -- NOT by assuming "the first R we see is the grid" -- so a
 * malformed or lost grid reply can't misroute a later genuine pace-ack into
 * sm_io_set_grid() (which would corrupt the grid and leak an in-flight slot).
 *
 * Portable: the non-blocking read of the door descriptor -- read() on POSIX,
 * Winsock recv() on Windows -- and the transient-vs-fatal classification of a
 * failure both live behind syncmoo1_plat.h, as they do in syncmoo1_io.c.
 */
#include "syncmoo1_input.h"

#include <stdint.h>

#include "syncmoo1_plat.h"   /* clock + non-blocking descriptor I/O */

#include "kbd.h"    /* 1oom: mookey_t, kbd_add_keypress */
#include "mouse.h"  /* 1oom: mouse_set_xy_from_hw/set_buttons_from_hw/set_scroll_from_hw, MOUSE_BUTTON_MASK_* */

#include "caps.h"      /* termgfx: termgfx_caps_parse_jxl */
#include "sgrmouse.h" /* termgfx: termgfx_sgr_classify (SGR button-field decode) */
#include "audio_mgr.h" /* termgfx: termgfx_audio_feed */

/* --- byte state machine ---------------------------------------------------- */

static enum { SM_P_NORMAL, SM_P_ESC, SM_P_CSI, SM_P_APC, SM_P_APC_ESC } pstate;

static char     csi_intro;         /* '[' or 'O' */
static char     csi_par[40];       /* parameter bytes between intro and final */
static int      csi_len;
static unsigned apc_len;           /* bytes swallowed in the current APC/string seq (bail if unterminated) */

/* A lone ESC (Escape key, no follow-up CSI/SS3 byte) can't be told apart from
 * the start of a multi-byte sequence until either the next byte arrives or a
 * short follow-up window elapses -- port of syncduke_input.c's g_esc_at_ms/
 * SYNCDUKE_ESC_MS. Checked every pump call (even one with no new bytes) so a
 * real Escape keypress isn't stuck waiting for more input that never comes. */
#define SM_ESC_MS 50
static uint32_t g_esc_at_ms;

/* Monotonic millisecond clock (syncmoo1_plat.h, xpdev's xp_timer64) -- the same
 * clock domain syncmoo1_io.c paces frames on. */
#define sm_in_now_ms() sm_plat_now_ms()

/* Parse up to `max` ';'-separated decimal params from csi_par; return the
 * count. A leading non-digit/non-';' marker byte (SGR mouse's '<', DA1's
 * '='/'?', etc.) is simply skipped -- syncduke_input.c/door_io.c port. */
static int sm_csi_params(int *out, int max)
{
    int n = 0, i, have = 0, v = 0;

    for (i = 0; i <= csi_len && n < max; i++) {
        char c = (i < csi_len) ? csi_par[i] : ';';   /* sentinel terminator */

        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            have = 1;
        } else if (c == ';') {
            if (have)
                out[n++] = v;
            v = 0;
            have = 0;
        }
    }
    return n;
}

/* --- mouse: SGR decode + injection (Step 3) -------------------------------- */

/* Running button mask (mirrors syncconquer's mouse_event()): a single SGR
 * report only tells us ONE button's press/release edge, but
 * mouse_set_buttons_from_hw() wants the full current mask every call, so we
 * have to track it across reports ourselves. */
static int g_mouse_mask;

/* button/col/row straight from the SGR report (b;col;row); release is true
 * for the trailing 'm' final ('M' = press/motion, 'm' = release -- xterm SGR
 * mouse mode 1006). Port of syncconquer door_input.c:285-385's mouse_event(),
 * MINUS the RTS legacy-modifier-synthesis (no held-modifier state to fake
 * here -- M1 has no native modifier tracking to prefer over it either). */
static void sm_mouse_event(int button, int col, int row, int release)
{
    int gx, gy, b = 0, wheel = 0, bit;

    sm_map_mouse(sm_io_geom(), col, row, &gx, &gy);
    mouse_set_xy_from_hw(gx, gy);

    /* The button field is classified in termgfx (sgrmouse.h), not here: the
     * motion bit has to be tested before the wheel bit, because SyncTERM
     * encodes a no-button hover as 96 (motion|64) where xterm sends 35. The
     * other way round reads every hover as a wheel notch -- which scrolled the
     * MoO1 menu selection out from under the pointer. */
    switch (termgfx_sgr_classify(button, &b, &wheel)) {
    case TERMGFX_SGR_MOVE:
        return;   /* hover or drag: the position set above is the whole event */

    case TERMGFX_SGR_WHEEL:
        /* Match 1oom's own backends' scroll polarity: wheel-up -> -1, down ->
         * +1 (hw/sdl/1/hwsdl1.c:279, hw/sdl/2/hwsdl2.c:375, hw/alleg/4). */
        mouse_set_scroll_from_hw(wheel);
        return;

    case TERMGFX_SGR_BUTTON:
        bit = (b == 0) ? MOUSE_BUTTON_MASK_LEFT : (b == 2) ? MOUSE_BUTTON_MASK_RIGHT : 0;
        if (bit == 0)
            return;   /* middle button: no mask bit in 1oom's mouse.h, nothing to inject */
        if (release)
            g_mouse_mask &= ~bit;
        else
            g_mouse_mask |= bit;
        mouse_set_buttons_from_hw(g_mouse_mask);
        return;
    }
}

/* --- probe-reply parsing (Step 4) ------------------------------------------ */

/* NOTE: the "is this R the startup grid reply or a pace-ack?" state is NOT
 * kept here as a local "first R ever" latch anymore (Task 9 fix). It lives in
 * sm_io (armed at probe-SEND time in sm_io_enter, consumed via
 * sm_io_take_grid_probe) so a malformed/lost grid reply can't cause a later
 * genuine pace-ack to be mis-latched as the grid -- see the 'R' case below. */

/* Capability flags recorded from the 'c'/'n' probe replies. Not yet consumed
 * anywhere (M1 is sixel-only per DESIGN.md Sec10 -- tier selection is a later
 * task) but parsed now, alongside the geometry replies, per the brief; kept
 * as module state rather than dropped so a later task has them ready. */
static int g_probe_replied;
static int g_is_syncterm;
static int g_have_sixel;
static int g_jxl_supported;

/* Did the terminal identify itself as SyncTERM (CTDA '<'/'=' marker, or a CTerm
 * state report)? Read by syncmoo1_io.c's present path to decide whether the
 * sixel colour registers can be trusted to persist across images. Until a probe
 * reply lands this is 0 -- i.e. we assume the SAFE case (re-send the palette),
 * which costs a few early frames some bytes and can never render wrong. */
int sm_input_is_syncterm(void)
{
    return g_is_syncterm;
}


static void sm_csi_final(char fin)
{
    int p[16], np;

    switch (fin) {
    case 't':   /* window-op reply: ESC[4;h;wt = exact text-area size in pixels */
        np = sm_csi_params(p, 4);
        if (np >= 3 && p[0] == 4)
            sm_io_set_canvas(p[2], p[1]);   /* p[1]=height px, p[2]=width px */
        return;
    case 'R':   /* ESC[rows;colsR: cursor-position report */
        np = sm_csi_params(p, 2);
        if (sm_io_take_grid_probe()) {
            /* This is the reply to the startup 999;999 grid probe
             * (termgfx_term_probe's own trailing ESC[6n, sent once from
             * sm_io_enter() -- before any present() frame exists to be
             * acked). Latch the grid; this is NOT a pace-ack (Task 9,
             * DESIGN.md §9): syncmoo1_io.c's DSR ring only ever records DSRs
             * a real present() send appended, so this reply has no matching
             * in-flight entry to retire. sm_io_take_grid_probe() has ALREADY
             * consumed the one-shot flag, so a malformed reply here (np < 2)
             * just keeps the default grid -- the NEXT R will correctly route
             * to sm_io_pace_ack() below rather than being mistaken for a
             * still-outstanding grid reply. */
            if (np >= 2)
                sm_io_set_grid(p[0], p[1]);     /* rows, cols */
            return;
        }
        /* Every report after the grid probe has been consumed acks one
         * in-flight present()-sent frame's DSR (Task 9): decrements the
         * pipeline count and feeds the round-trip into the shared AIMD depth
         * controller (termgfx/pace.c) that sm_io_present()'s backpressure
         * gate checks. */
        sm_io_pace_ack();
        return;
    case 'y':   /* DECRPM: ESC[?1016;Ps$y -- reply to our SGR-Pixels DECRQM.
                 * Ps 1/3 = mode set (pixel-granular mouse reports active);
                 * 0 = unrecognized, 2/4 = reset -- all leave us cell-granular. */
        np = sm_csi_params(p, 2);
        if (np >= 2 && p[0] == 1016)
            sm_io_set_pixel_mode(p[1] == 1 || p[1] == 3);
        return;
    case 'c':   /* device-attributes reply: DA1 (ESC[c) / CTDA (ESC[<c) */
        g_probe_replied = 1;
        if (csi_len > 0 && (csi_par[0] == '<' || csi_par[0] == '='))
            g_is_syncterm = 1;   /* '<'/'=' marker: SyncTERM-flavored reply */
        np = sm_csi_params(p, 16);
        {
            int k;
            for (k = 0; k < np; k++) {
                if (p[k] == 4)
                    g_have_sixel = 1;   /* DA1 param 4 / CTDA cap 4 = sixel */
            }
        }
        /* CTerm >= 1.329: enable inline A;LoadBlob audio (version is in the DA1
         * reply "ESC[=67;84;101;114;109;MAJ;MIN;...c"). */
        if (termgfx_caps_cterm_version(p, np, (char)csi_par[0]) >= TERMGFX_CTERM_VER_BLOB)
            termgfx_audio_set_blob_ok(sm_io_audio(), 1);
        return;
    case 'n':   /* CTerm state report: our Q;JXL query's reply is ESC[=1;{0,1}n */
        if (csi_len > 0 && csi_par[0] == '=') {
            uint8_t seq[48];
            int     sl = 0, k, r;

            seq[sl++] = 0x1b;
            seq[sl++] = (uint8_t)csi_intro;
            for (k = 0; k < csi_len && sl < (int)sizeof(seq) - 1; k++)
                seq[sl++] = (uint8_t)csi_par[k];
            seq[sl++] = (uint8_t)fin;
            r = termgfx_caps_parse_jxl(seq, sl);
            if (r >= 0) {
                g_is_syncterm   = 1;
                g_jxl_supported = (r == 1);
            }
        }
        return;
    case 'M':   /* xterm SGR mouse: ESC[<b;col;row M (press/motion) */
    case 'm':   /*                                m (release)        */
        if (csi_len > 0 && csi_par[0] == '<' && sm_csi_params(p, 3) >= 3)
            sm_mouse_event(p[0], p[1], p[2], fin == 'm');
        return;
    default: {
        /* Everything else: arrow/nav/function keys, via Task 3's sm_map_csi
         * (params + final byte; modifiers on the arrow/Home/End finals are
         * intentionally not decoded here -- out of sm_map_csi's contract). */
        mookey_t key;
        uint32_t mod;

        np = sm_csi_params(p, 16);
        if (sm_map_csi(p, np, fin, &key, &mod))
            kbd_add_keypress(key, mod, 0);
        return;
    }
    }
}

/* --- read loop (Steps 1-2) -------------------------------------------------- */

int sm_input_pump(int sockfd)
{
    uint8_t buf[256];
    int     n;
    int     i;

    if (sockfd < 0)
        return 0;   /* no live source (a Windows dev run with no door socket) */

    /* Deliver a pending lone ESC once its follow-up window has elapsed, even
     * on a pump call that reads no new bytes -- the menu Escape key shouldn't
     * wait for the next keystroke. A real ESC-sequence byte arrives in the
     * same burst as the ESC that started it, so it will have already
     * advanced past SM_P_ESC before this fires. */
    if (pstate == SM_P_ESC && (uint32_t)(sm_in_now_ms() - g_esc_at_ms) > SM_ESC_MS) {
        kbd_add_keypress(MOO_KEY_ESCAPE, 0, 0x1b);
        pstate = SM_P_NORMAL;
    }

    n = sm_plat_read(sockfd, buf, sizeof buf);
    if (n > 0) {
        sm_io_wiredump_in(buf, (size_t)n);   /* debug capture; no-op unless SYNCMOO1_WIREDUMP is set */
        {   /* Resolve the audio capability probe (SyncTERM replies with an
             * APC the manager parses); harmless for every other byte. */
            termgfx_audio_t *am = sm_io_audio();
            if (am != NULL)
                termgfx_audio_feed(am, buf, n);
        }
    }
    if (n < 0) {
        if (n == SM_IO_AGAIN || n == SM_IO_INTR)
            return 0;      /* no input yet / transient: not a hangup */
        return -1;         /* real read error: treat as hangup */
    }
    if (n == 0)
        return -1;         /* peer closed */

    for (i = 0; i < n; i++) {
        uint8_t c = buf[i];

        switch (pstate) {
        case SM_P_NORMAL:
            /* ESC begins a sequence (SM_P_ESC); every other byte is a
             * key on the plain-byte path via sm_map_ascii(). */
            if (c == 0x1b) {
                pstate = SM_P_ESC;
                g_esc_at_ms = sm_in_now_ms();
            } else {
                mookey_t key;
                uint32_t mod;
                char     ch;

                if (sm_map_ascii(c, &key, &mod, &ch))
                    kbd_add_keypress(key, mod, ch);
            }
            break;
        case SM_P_ESC:
            if (c == '[' || c == 'O') {
                pstate     = SM_P_CSI;
                csi_intro  = (char)c;
                csi_len    = 0;
            } else if (c == '_' || c == 'P' || c == ']' || c == '^') {
                /* String sequences -- APC (_), DCS (P), OSC (]), PM (^) --
                 * end at ST (ESC \) and carry no keys; swallow them.
                 * SyncTERM's C;L cache-list reply is an APC literally
                 * containing "C;L", so passing it through would type
                 * stray letters into the game. */
                pstate  = SM_P_APC;
                apc_len = 0;
            } else {
                /* Lone ESC (Escape key), reprocess c in NORMAL. */
                kbd_add_keypress(MOO_KEY_ESCAPE, 0, 0x1b);
                pstate = SM_P_NORMAL;
                i--;
            }
            break;
        case SM_P_APC:
            if (c == 0x1b)
                pstate = SM_P_APC_ESC;              /* maybe the ST terminator */
            else if (c == 0x07)
                pstate = SM_P_NORMAL;               /* BEL also ends OSC/strings */
            else if (++apc_len > (1u << 20))
                pstate = SM_P_NORMAL;               /* unterminated -> bail (no input lockup) */
            break;                                  /* else: swallow body byte */
        case SM_P_APC_ESC:
            pstate = (c == '\\') ? SM_P_NORMAL : SM_P_APC;   /* ESC '\' = ST -> done */
            break;
        case SM_P_CSI:
            if (c >= 0x40 && c <= 0x7e) {
                sm_csi_final((char)c);
                pstate = SM_P_NORMAL;
            } else if (csi_len < (int)sizeof(csi_par)) {
                csi_par[csi_len++] = (char)c;
            }
            break;
        }
    }
    return 0;
}
