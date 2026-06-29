/*
 * syncduke_input.c -- terminal input -> Build/Duke keyboard state.
 *
 * The engine's keyboard.c pulls a raw scancode byte from _readlastkeyhit() and
 * runs it through keyhandler() (low 7 bits = scancode, 0x80 bit = release). The
 * SDL build fed that from SDL key-down/up events; we feed it from terminal bytes.
 *
 * The hard part is that a terminal has NO key-up: holding a key produces auto-
 * repeated key-DOWN bytes and nothing on release. So each mapped byte enqueues a
 * key-down immediately and arms a synthetic key-up SYNCDUKE_KEY_HOLD *frames* later; a
 * fresh byte for the same key re-arms the hold (auto-repeat keeps it down).
 *
 * The hold is counted in PRESENTED FRAMES, not totalclock ticks. Duke reads the
 * keyboard by POLLING KB_KeyDown[] once per frame (the menu also self-debounces
 * via KB_ClearKeyDown), so a key must stay down across at least one full frame to
 * be reliably seen. Tying expiry to frames (advanced once per _nextpage) -- rather
 * than to wall-clock ticks, which also advance during the engine's busy-wait
 * _idle()/KB_KeyWaiting loops -- guarantees that and is independent of how long a
 * frame takes to encode/transmit. (Ticks raced: a fast key-up fired mid-frame
 * before the menu ever polled it, so taps were dropped.)
 *
 * This file is self-contained (engine-independent: it only needs the sc_* #define
 * constants from keyboard.h) so the keymap can be unit-tested standalone. The
 * caller passes the engine's totalclock as `now`.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>   /* the door input source is a Winsock SOCKET (recv/ioctlsocket) */
  #include <windows.h>    /* QueryPerformanceCounter (monotonic clock) */
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <time.h>
#endif

#include "syncduke.h"
#include "keyboard.h"   /* sc_* scancode constants (pure #defines) */
#include "caps.h"       /* termgfx: termgfx_caps_parse_jxl (cap-probe reply scan) */
#include "audio_mgr.h"  /* termgfx: SyncTERM audio-APC manager (cap-probe feed) */

extern termgfx_audio_t *sd_audio;   /* the audio manager (owned by syncduke_io.c) */

/* How many presented frames a key stays "down" after its last byte -- this bridges
 * the gaps in terminal auto-repeat so a held key reads as continuously down. Three
 * sliders (Setup Controls, ala SyncDoom): KEY TAP (a fresh single press), KEY HOLD (a
 * repeat byte of an already-held key), and a separate TURN HOLD for the turn keys
 * (Left/Right arrows). Stored as 0..63 slider values; frames = 1 + v/9 (so default
 * 9 -> 2 frames, the old constant). FAST TURN defeats Duke's turn-accel ramp (which a
 * terminal's gappy repeat keeps resetting -> you'd otherwise stay stuck turning slow). */
static int     g_tap_bar   = 9;   /* KEY TAP   slider (0..63) -> 2 frames (a fresh press) */
static int     g_hold_bar  = 9;   /* KEY HOLD  slider (0..63) -> 2 frames (a repeat byte) */
static int     g_turn_bar  = 27;  /* TURN HOLD slider (0..63) -> 4 frames (turn keys) */
/* FAST TURN: a turbo boost to the continuous arrow-key turn rate (Setup Controls). The base
 * TURN SPEED is already responsive; this adds ~50% for extra-fast turning. Default OFF;
 * toggleable in Setup Controls and persisted to duke3d.cfg. */
volatile int   syncduke_fast_turn;  /* -> syncduke_key_turn rate in this file */

/* ---- raw scancode-byte queue (what _readlastkeyhit drains) ---- */
#define RAWQ_SZ 256
static uint8_t rawq[RAWQ_SZ];
static int     rawq_head, rawq_tail;

static void rawq_push(uint8_t b)
{
	int nt = (rawq_tail + 1) & (RAWQ_SZ - 1);
	if (nt == rawq_head)
		return;                 /* full: drop (never happens in practice) */
	rawq[rawq_tail] = b;
	rawq_tail = nt;
}

int syncduke_input_has_raw(void) { return rawq_head != rawq_tail; }

int syncduke_input_pop_raw(void)
{
	uint8_t b;
	if (rawq_head == rawq_tail)
		return 0;
	b = rawq[rawq_head];
	rawq_head = (rawq_head + 1) & (RAWQ_SZ - 1);
	return b;
}

/* ---- per-scancode synthetic-release tracking ---- */
static uint8_t held[128];        /* scancode currently down (awaiting auto-release) */
static int32_t release_at[128];  /* frame counter value at which to release it */

/* kitty keyboard protocol active: the terminal answered our CSI?u progressive-enhancement
 * query, so keys arrive as CSI-u events with explicit press/repeat/release.  That gives the
 * door true key-up (crisp hold-to-move) instead of the auto-release timeout below.  Set when
 * the report lands (csi_final 'u' case), which also pushes our flags (CSI>11u). */
static int g_kitty_active;

/* Sticky-crouch toggle: a terminal can't hold a key (no key-up; auto-repeat is laggy), so
 * momentary crouch (hold Z) is unusable.  Instead 'z' toggles this latch and we re-assert the
 * Crouch scancode every pump while it's on -- press once to crouch, again to stand.  (Under the
 * kitty protocol real key-up exists, but crouch stays a toggle for consistency.) */
static int g_crouch_toggle;

/* Pitch "look" notch posted by PgUp/PgDn taps, consumed once per tic in processinput()
 * (Game/src/player.c).  Signed: + = look up, - = look down; magnitude = pending taps.  This
 * is deliberately NOT a synthetic key-hold: holding Duke's Aim key on a terminal (no key-up,
 * auto-repeat) pinned the view to the sky and fought Center_View.  A one-shot notch can't. */
volatile int syncduke_pitch_step;

/* F1 help request (door -> game).  The door sets this to 1 when F1 is pressed in gameplay;
 * game.c opens the GAME CONTROLS screen (menu case 707) over the game and flips it to 2
 * ("showing"); menues.c clears it to 0 and resumes gameplay when the screen is dismissed.
 * Replaces Duke's built-in F1HELP (the DOS keyboard chart, wrong for a terminal door). */
volatile int syncduke_help_request;

/* `now` is the presented-frame counter (see file header), NOT totalclock. */
static void press(int sc, int now)
{
	if (sc <= 0 || sc >= 128)
		return;
	rawq_push((uint8_t)sc);          /* key-down */
	{   /* Pick the hold: turn keys (Left/Right) use TURN HOLD; otherwise a fresh press
		 * uses KEY TAP, a repeat byte of an already-held key uses KEY HOLD. */
		int bar;
		if (sc == sc_LeftArrow || sc == sc_RightArrow)
			bar = g_turn_bar;
		else if (held[sc])
			bar = g_hold_bar;
		else
			bar = g_tap_bar;
		release_at[sc] = now + 1 + bar / 9;
	}
	held[sc] = 1;
}

void syncduke_input_expire(int now)
{
	int sc;
	/* Auto-release timed-out presses.  kitty-HELD movement keys carry a far-future release_at
	 * (see kitty_press) so they stay down until the explicit key-up; momentary press() keys
	 * (F-keys, Center) still time out here -- needed so they don't stick under kitty. */
	for (sc = 1; sc < 128; sc++) {
		if (held[sc] && (int32_t)(now - release_at[sc]) >= 0) {
			rawq_push((uint8_t)(sc | 0x80));    /* key-up */
			held[sc] = 0;
		}
	}
}

/* kitty key-down/up: driven by the terminal's explicit press/release events instead of the
 * auto-release timeout.  A repeat re-asserts an existing down (idempotent); release clears it. */
static void kitty_press(int sc)
{
	if (sc <= 0 || sc >= 128)
		return;
	if (!held[sc])
		rawq_push((uint8_t)sc);          /* key-down (once; repeats don't re-push) */
	held[sc]       = 1;
	release_at[sc] = INT32_MAX;          /* held until the explicit kitty key-up; expire() won't fire */
}

static void kitty_release(int sc)
{
	if (sc <= 0 || sc >= 128)
		return;
	if (held[sc]) {
		rawq_push((uint8_t)(sc | 0x80)); /* key-up */
		held[sc] = 0;
	}
}

int syncduke_kitty_active(void) { return g_kitty_active; }

/*
 * Map one terminal byte or escape sequence to a Build scancode (or -1).
 *
 * The control scheme mirrors src/doors/syncdoom's: a terminal has no mouse and
 * no key-up, so movement/turn/strafe live on WASD + the arrow keys and the
 * action keys are plain bytes. We emit the Build SCANCODE that Duke's default
 * config binds to each action (see Game/src/_functio.h keydefaults):
 *
 *   W/S  -> forward/back   = Up/Down arrow scancodes (Move_Forward/Backward)
 *   A/D  -> strafe L/R     = ',' / '.'               (Strafe_Left/Right)
 *   arrows: Up/Down move, Left/Right TURN            (Turn_Left/Right)
 *   Space-> Fire           = Left Ctrl               (Fire)   <- the plain fire key
 *   E    -> Open/Use       = Space                   (Open)
 *   Q    -> Jump           = A                       (Jump)
 *   R    -> AutoRun toggle = CapsLock                (AutoRun)
 *   Tab  -> Map;  Enter/Esc/Backspace -> menu/turn-around;  1..0 -> weapon select
 *   other letters keep their own scancode, so Duke's letter-bound inventory
 *   (H/J/N/M ...) and Z=Crouch still work.
 *
 * `gameplay` gates the action layer: when 0 (in a menu / typing a save name) the
 * WASD/Space remaps are skipped so letters and Space arrive literally. Arrow and
 * edit keys map the same in both modes (menu navigation needs them).
 *
 * Duke's arrow scancodes are single bytes here (0x5a/0x6a/0x6b/0x6c) -- the
 * engine already collapsed the PC extended-key (0xe0) encoding.
 */
static int syncduke_letter_sc(unsigned char c)
{
	static const uint8_t a2sc[26] = {
		sc_A, sc_B, sc_C, sc_D, sc_E, sc_F, sc_G, sc_H, sc_I, sc_J,
		sc_K, sc_L, sc_M, sc_N, sc_O, sc_P, sc_Q, sc_R, sc_S, sc_T,
		sc_U, sc_V, sc_W, sc_X, sc_Y, sc_Z
	};
	if (c >= 'a' && c <= 'z')
		c -= 32;
	if (c >= 'A' && c <= 'Z')
		return a2sc[c - 'A'];
	if (c >= '1' && c <= '9')
		return sc_1 + (c - '1');        /* sc_1..sc_9 contiguous */
	if (c == '0')
		return sc_0;
	return -1;
}

int syncduke_map_key(const char *seq, int len, int gameplay)
{
	unsigned char c;

	if (len <= 0)
		return -1;

	/* arrow / cursor escape sequences (CSI "ESC [ X" or SS3 "ESC O X") */
	if (len >= 3 && seq[0] == 0x1b && (seq[1] == '[' || seq[1] == 'O')) {
		switch (seq[2]) {
			case 'A': return sc_UpArrow;     /* forward / menu-up    */
			case 'B': return sc_DownArrow;   /* back / menu-down     */
			case 'C': return sc_RightArrow;  /* turn right / menu-rt */
			case 'D': return sc_LeftArrow;   /* turn left / menu-lt  */
			default:  return -1;
		}
	}

	if (len != 1)
		return -1;
	c = (unsigned char)seq[0];

	/* universal edit/control keys (both modes) */
	switch (c) {
		case 0x1b: return sc_Escape;
		case '\r':
		case '\n': return sc_Return;
		case '\t': return sc_Tab;
		case 0x08:
		case 0x7f: return sc_BackSpace;
	}

	if (gameplay) {
		switch (c) {
			case ' ':           return sc_LeftControl;  /* Fire         */
			case 'w': case 'W': return sc_UpArrow;      /* forward      */
			case 's': case 'S': return sc_DownArrow;    /* back         */
			case 'a': case 'A': return sc_Comma;        /* strafe left  */
			case 'd': case 'D': return sc_Period;       /* strafe right */
			case 'e': case 'E': return sc_Space;        /* Open/Use     */
			case 'q': case 'Q': return sc_A;            /* Jump         */
			case 'r': case 'R': return sc_CapsLock;     /* AutoRun toggle */
		}
		/* fall through: z=crouch, inventory letters (H/J/N/M ...), weapon digits */
	} else {
		if (c == ' ')
			return sc_Space;        /* menus/text: space is its own key */
	}

	return syncduke_letter_sc(c);
}

/* ---- terminal pixel-canvas size, learned from probe replies (0 = unknown) ---- */
static int g_term_px_w, g_term_px_h;    /* text-area px (ESC[14t / cell*grid estimate): sixel/out sizing */
static int g_cell_w, g_cell_h;          /* cell pixel size from ESC[16t */
static int g_gfx_w, g_gfx_h;            /* graphics-canvas px from XTSMGRAPHICS: JXL fill/center */
static int g_grid_rows, g_grid_cols;    /* text grid, from the startup size probe's cursor report */
static int g_px_exact;                  /* term_px came from ESC[14t (authoritative): don't re-estimate */

/* syncduke.ini [video] use_cell_size: when the terminal didn't report exact pixels
 * (ESC[14t), estimate the window from its reported CELL size (ESC[16t) x the text grid
 * rather than assuming an 8x16 font -- so a non-SyncTERM sixel terminal with a larger
 * font still fills its window. Set by syncduke_config.c; default on. */
int syncduke_use_cell_size = 1;

/* (Re)derive the text-area pixel size from the best available probe data, unless
 * ESC[14t already gave it exactly. Called whenever a cell-size or grid report lands,
 * so the two replies can arrive in either order. */
static void syncduke_estimate_px(void)
{
	int cw, ch;
	if (g_px_exact || g_grid_rows <= 0 || g_grid_cols <= 0)
		return;
	cw = (syncduke_use_cell_size && g_cell_w > 0) ? g_cell_w : 8;
	ch = (syncduke_use_cell_size && g_cell_h > 0) ? g_cell_h : 16;
	g_term_px_w = g_grid_cols * cw;
	g_term_px_h = g_grid_rows * ch;
}

int syncduke_term_px_w(void) { return g_term_px_w; }
int syncduke_term_px_h(void) { return g_term_px_h; }
int syncduke_term_cell_w(void) { return g_cell_w; }
int syncduke_term_cell_h(void) { return g_cell_h; }
/* Real graphics canvas for image-fill: XTSMGRAPHICS if SyncTERM answered it, else
 * the text-area pixels (ESC[14t / estimate), so non-SyncTERM terminals still fill. */
int syncduke_canvas_w(void) { return g_gfx_w > 0 ? g_gfx_w : g_term_px_w; }
int syncduke_canvas_h(void) { return g_gfx_h > 0 ? g_gfx_h : g_term_px_h; }

/* Incremental CSI parser state -- bytes of an escape sequence (and of a probe
 * reply) can split across reads, so it persists between syncduke_input_pump() calls. */
static enum { P_NORMAL, P_ESC, P_CSI } pstate;

/* Lone-ESC disambiguation: a bare ESC (Escape key) shares its first byte with the
 * start of an escape sequence (arrows = ESC[A, etc.). A real sequence arrives as one
 * burst, so if no follow-up byte lands within SYNCDUKE_ESC_MS we deliver the pending
 * ESC as Escape -- without this the menu key only registers when the NEXT key is hit. */
#define SYNCDUKE_ESC_MS 50
static uint32_t g_esc_at_ms;

static uint32_t syncduke_in_now_ms(void)
{
#ifdef _WIN32
	static LARGE_INTEGER freq;
	LARGE_INTEGER        c;
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&c);
	return (uint32_t)((uint64_t)c.QuadPart * 1000ULL / (uint64_t)freq.QuadPart);
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
#endif
}
static char csi_intro;          /* '[' or 'O' */
static char csi_par[40];        /* parameter bytes between intro and final */

/* SyncTERM detected? A device-attributes reply to our ESC[<c (CTDA) query that
 * carries a '<' marker (CTDA) or a '=' "CTerm" version is SyncTERM-specific. Only
 * SyncTERM honors the cterm 2x-both-axes sixel scaling, so the door uses it only
 * when this is set (portable vertical-only aspect otherwise). */
static int g_is_syncterm;
int syncduke_is_syncterm(void) { return g_is_syncterm; }

/* JXL support, learned from SyncTERM's reply to our APC Q;JXL query (CTQJS, sent
 * once at term-enter): ESC[=1;{0,1}-n. 1 = the terminal can decode JPEG XL, so the
 * present path serves the JXL/APC tier (far smaller frames) instead of sixel. */
static int g_jxl_supported;
int syncduke_jxl_supported(void) { return g_jxl_supported; }

/* Sixel support + "the terminal answered our capability probe at all", from the
 * device-attributes reply: DA1 (ESC[c -> ESC[?...c) param 4 = sixel; CTDA
 * (ESC[<c -> ESC[<..c) cap 4 = pixel ops; any SyncTERM marker implies sixel. A
 * terminal that answers DA1 WITHOUT sixel (e.g. Windows conhost) -> the present
 * path falls back to the text tier instead of blasting it unrenderable sixel. */
static int g_have_sixel, g_probe_replied;
int syncduke_have_sixel(void)    { return g_have_sixel; }
int syncduke_probe_replied(void) { return g_probe_replied; }
static int csi_len;

/* --- terminal mouse (xterm SGR) steering, ala SyncDoom -----------------------------
 * A terminal can't warp the pointer or report relative motion, so we steer by the
 * pointer's ABSOLUTE column offset from the image centre (a virtual joystick): the
 * further off-centre, the faster the turn.  Left button = Fire.  Vertical does nothing
 * (movement stays on the keyboard).  The turn and fire are continuous LEVELS that the
 * engine's getinput() (Game/src/player.c) reads each tic; a brief idle relaxes them to
 * neutral so an abandoned / off-window pointer can't spin or fire forever.  Ctrl-O
 * toggles steering; syncduke_io.c keeps the terminal's SGR tracking in sync. */
#define SYNCDUKE_MOUSE_IDLE_MS   600   /* relax steer/fire after this long with no report */

volatile int syncduke_mouse_turn;      /* -> angvel  (player.c getinput) */
volatile int syncduke_mouse_fire;      /* -> Fire bit (player.c getinput) */

/* Continuous arrow-key turn level (gameplay), the real fix for sluggish/jerky turning.
 * Duke reads the turn keys per sim-tic from KB_KeyDown[], but on a terminal the door only
 * refreshes that once per PRESENTED FRAME (gappy auto-repeat + a frame-counted synthetic
 * release), so turning came in uneven, frame-rate-tied bursts -- no key-hold slider can fix
 * that. Instead each Left/Right arrow byte sets a turn rate here that the engine's getinput()
 * adds to angvel EVERY sim-tic (like the mouse steer), relaxed to neutral after a short idle
 * once the auto-repeat stops -- giving smooth, responsive turning like SyncDOOM's. */
volatile int    syncduke_key_turn;     /* signed angvel/tic: + = right, - = left */
static uint32_t g_key_turn_ms;         /* time of the last arrow byte (idle relax) */
#define SYNCDUKE_KEY_TURN_IDLE_MS 140  /* stop turning this long after the last arrow byte */

static int      g_mouse_on = 1;        /* steering enabled (default on; Ctrl-O toggles) */
static int      g_mouse_sens = 30;     /* steer sensitivity, a 0..63 slider (Setup Mouse);
                                        * max angvel at the image edge = sens*2 (30->60). */
static int      g_mouse_col;           /* last reported pointer cell (1-based) */
static int      g_mouse_have;          /* a report has arrived (neutral until then) */
static int      g_mouse_buttons;       /* bit0 left/fire, 1 middle, 2 right */
static uint32_t g_mouse_last_ms;       /* time of the last report (idle timeout) */

int  syncduke_mouse_enabled(void) { return g_mouse_on; }
int  syncduke_mouse_sens(void)    { return g_mouse_sens; }
/* Floor at 1: sensitivity 0 yields zero turn rate (steering effectively dead, like
 * disabling the mouse), so the slider's left end is 1 -- a small but real effect. */
void syncduke_mouse_sens_set(int v) { g_mouse_sens = v < 1 ? 1 : (v > 63 ? 63 : v); }

/* keyboard-feel sliders/toggle for the Setup Controls menu (0..63 sliders) */
int  syncduke_kb_tap(void)         { return g_tap_bar; }
void syncduke_kb_tap_set(int v)    { g_tap_bar = v < 0 ? 0 : (v > 63 ? 63 : v); }
int  syncduke_kb_hold(void)        { return g_hold_bar; }
void syncduke_kb_hold_set(int v)   { g_hold_bar = v < 0 ? 0 : (v > 63 ? 63 : v); }
int  syncduke_kb_turn(void)        { return g_turn_bar; }
void syncduke_kb_turn_set(int v)   { g_turn_bar = v < 0 ? 0 : (v > 63 ? 63 : v); }
int  syncduke_kb_fastturn(void)    { return syncduke_fast_turn; }
void syncduke_kb_fastturn_set(int v) { syncduke_fast_turn = v ? 1 : 0; }

/* Pointer column offset from the image centre -> signed turn rate, scaled so the image
 * edge is full deflection (mirrors SyncDoom's mouse_steer_rate).  The image's centre
 * cell and half-width come from the shared geometry (which accounts for centering on
 * terminals that report a cell size, and the top-left fallback elsewhere). */
static int syncduke_mouse_steer(void)
{
	int center, halfw, off, mag, dz;

	syncduke_hsteer(&center, &halfw);
	if (halfw < 2)
		halfw = 2;
	off = g_mouse_col - center;
	mag = off < 0 ? -off : off;
	dz  = halfw / 10;                  /* ~10% centre deadzone */
	if (dz < 1)
		dz = 1;
	if (mag <= dz)
		return 0;
	if (mag > halfw)
		mag = halfw;                   /* past the image edge -> full turn */
	mag = (mag - dz) * (g_mouse_sens * 2) / (halfw - dz);
	return off < 0 ? -mag : mag;
}

/* One xterm SGR mouse report: ESC[<button;col;row M(press/motion) | m(release).
 * button low 2 bits = which button; bit 5 (32) marks a motion (not click) report. */
static void syncduke_mouse_event(int button, int col, int row, int release)
{
	(void)row;
	g_mouse_col = col;
	g_mouse_have = 1;
	g_mouse_last_ms = syncduke_in_now_ms();
	if (!(button & 32)) {              /* a real button press/release, not motion */
		int bit = button & 3;
		if (release)
			g_mouse_buttons &= ~(1 << bit);
		else
			g_mouse_buttons |= (1 << bit);
	}
	if (g_mouse_on) {
		syncduke_mouse_turn = syncduke_mouse_steer();
		syncduke_mouse_fire = (g_mouse_buttons & 1) ? 1 : 0;
	}
}

/* Each pump: if the pointer has gone quiet, relax to neutral -- an abandoned off-centre
 * or off-window pointer (a terminal sends no focus-out) must not spin or hold fire. */
static void syncduke_mouse_idle(void)
{
	if (g_mouse_have && (int32_t)(syncduke_in_now_ms() - g_mouse_last_ms) > SYNCDUKE_MOUSE_IDLE_MS) {
		syncduke_mouse_turn = 0;
		syncduke_mouse_fire = 0;
		g_mouse_buttons = 0;
	}
}

/* Stop the continuous arrow-key turn once the terminal's auto-repeat has stopped (no arrow
 * byte for SYNCDUKE_KEY_TURN_IDLE_MS). Runs every pump, like the mouse idle relax. */
static void syncduke_key_turn_idle(void)
{
	if (syncduke_key_turn && (int32_t)(syncduke_in_now_ms() - g_key_turn_ms) > SYNCDUKE_KEY_TURN_IDLE_MS)
		syncduke_key_turn = 0;
}

void syncduke_mouse_toggle(void)
{
	g_mouse_on = !g_mouse_on;
	syncduke_mouse_turn = 0;
	syncduke_mouse_fire = 0;
	g_mouse_have = 0;
	g_mouse_buttons = 0;
	/* syncduke_io.c reconciles the terminal's SGR-tracking enable with g_mouse_on. */
}

/* Parse up to `max` ';'-separated decimal params from csi_par; return the count. */
static int csi_params(int *out, int max)
{
	int n = 0, i = 0, have = 0, v = 0;
	for (i = 0; i <= csi_len && n < max; i++) {
		char c = (i < csi_len) ? csi_par[i] : ';';   /* sentinel terminator */
		if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); have = 1; }
		else if (c == ';') { if (have)
								 out[n++] = v; v = 0; have = 0; }
	}
	return n;
}

/* Parse a kitty keyboard-protocol event from csi_par: "CSI <key> ; <mods> : <event> ..."
 * (mods/event optional).  Returns the key codepoint; *mod gets the modifier value (1 = none;
 * +1 shift, +2 alt, +4 ctrl, +8 super -- so ctrl alone = 5) and *ev the event type (1 = press,
 * 2 = repeat, 3 = release).  Works for both the CSI-u key events and the legacy CSI-letter /
 * CSI-~ functional keys, which carry the same ;mods:event tail under the protocol. */
static int kitty_key(int *mod, int *ev)
{
	int key = 0, v = 0, sub = 0, field = 0, in_sub = 0, i;
	*mod = 1;
	*ev  = 1;
	for (i = 0; i <= csi_len; i++) {
		char c = (i < csi_len) ? csi_par[i] : ';';   /* sentinel flushes the last field */
		if (c >= '0' && c <= '9') {
			if (in_sub)
				sub = sub * 10 + (c - '0');
			else
				v = v * 10 + (c - '0');
		} else if (c == ':') {
			in_sub = 1;                              /* a field's first sub-param is the event type */
			sub = 0;
		} else if (c == ';') {
			if (field == 0) {
				key = v;
			} else if (field == 1) {
				if (v)
					*mod = v;
				if (in_sub && sub)
					*ev = sub;
			}
			field++;
			v = 0;
			sub = 0;
			in_sub = 0;
		}
	}
	return key;
}

/* True when csi_par is a kitty RELEASE event (and the protocol is active).  Lets the press-only
 * key cases (PgUp/PgDn look, Center, F-keys) ignore the matching key-up so they don't fire twice
 * per tap once every key reports press AND release. */
static int kitty_release_event(void)
{
	int m, e;
	if (!g_kitty_active)
		return 0;
	kitty_key(&m, &e);
	return e == 3;
}

/* Dispatch one decoded key byte to its action.  `kev` selects the input model:
 *   0 = legacy byte path  -> press() with an auto-release timeout (terminal gives no key-up)
 *   1 = kitty press, 2 = kitty repeat, 3 = kitty release
 * Door shortcuts/toggles (Ctrl-S/T/O, Ctrl-A..F, sticky crouch) act on a fresh press only; the
 * game keys hold (kitty, until key-up) or auto-release (byte). */
static void handle_key(int c, int gameplay, int now, int kev)
{
	char cb = (char)c;
	int  sc;

	if (kev == 3) {                                  /* key-up: release the held game scancode */
		if (gameplay && (c == 'z' || c == 'Z'))
			return;                                  /* crouch is a latch, not a hold */
		sc = syncduke_map_key(&cb, 1, gameplay);
		if (sc > 0)
			kitty_release(sc);
		return;
	}

	if (kev != 2) {                                  /* a fresh press (byte path or kitty press) */
		if (c == 0x13) { syncduke_stats_toggle(); return; }   /* Ctrl-S: stats overlay */
		if (c == 0x14) { syncduke_depth_cycle(); return; }    /* Ctrl-T: pipeline depth */
		if (c == 0x0f) { syncduke_mouse_toggle(); return; }   /* Ctrl-O: mouse steering */
		if (c >= 0x01 && c <= 0x06) {                         /* Ctrl-A..F mirror the door's F1..F6 */
			if (c == 0x01) {                                  /* Ctrl-A = F1 = GAME CONTROLS help */
				if (gameplay)
					syncduke_help_request = 1;
			} else if (c == 0x04) {                           /* Ctrl-D = F4 = cycle graphics tier (like the real F4) */
				syncduke_tier_cycle();
			} else {
				press(sc_F1 + (c - 1), now);                  /* Ctrl-B/C/E/F = Duke F2/F3/F5/F6 (save/load/quicksave) */
			}
			return;
		}
		if (gameplay && (c == 'z' || c == 'Z')) { g_crouch_toggle = !g_crouch_toggle; return; }   /* sticky crouch */
	} else if (c == 0x13 || c == 0x14 || c == 0x0f || (c >= 0x01 && c <= 0x06)
	           || (gameplay && (c == 'z' || c == 'Z'))) {
		return;                                      /* swallow kitty auto-repeat of a shortcut/toggle */
	}

	sc = syncduke_map_key(&cb, 1, gameplay);
	if (sc > 0) {
		if (kev == 0)
			press(sc, now);                          /* byte path: timeout release */
		else
			kitty_press(sc);                         /* kitty: hold until the release event */
	}
}

/* A CSI/SS3 sequence has terminated with final byte `fin`. */
static void csi_final(char fin, int gameplay, int now)
{
	int p[4];

	switch (fin) {
		case 'A':                                          /* up arrow   -> forward */
		case 'B':                                          /* down arrow -> back    */
		{       /* same in both modes; under kitty, hold until the explicit key-up */
			int sc = (fin == 'A') ? sc_UpArrow : sc_DownArrow;
			if (g_kitty_active) {
				int mod, ev;
				kitty_key(&mod, &ev);
				if (ev == 3)
					kitty_release(sc);
				else
					kitty_press(sc);
			} else
				press(sc, now);
		}
			return;
		case 'C':   /* right arrow */
		case 'D':   /* left arrow */
		{
			int sc = (fin == 'C') ? sc_RightArrow : sc_LeftArrow;
			if (gameplay && g_kitty_active) {
				/* True key-up available: drive Duke's REAL turn keys so the engine's own
					 * turn-accel ramp runs -- responsive like the original game.  (The synthetic
					 * rate below exists only because byte-path auto-repeat flickers KB_KeyDown and
					 * keeps resetting that ramp; with held key state we don't need it.) */
				int mod, ev;
				kitty_key(&mod, &ev);
				if (ev == 3)
					kitty_release(sc);
				else
					kitty_press(sc);
				return;
			}
			if (gameplay) {
				/* Byte path: gappy auto-repeat -> a smooth synthetic turn rate applied per
					 * sim-tic, from the TURN SPEED slider with a FAST TURN turbo boost. */
				int rate = 2 + g_turn_bar * 3 / 4;         /* TURN SPEED: ~2 (min) .. ~49 angvel/tic */
				if (syncduke_fast_turn)
					rate += rate / 2;                      /* FAST TURN: +50% */
				syncduke_key_turn = (fin == 'C') ? rate : -rate;
				g_key_turn_ms     = syncduke_in_now_ms();
			} else
				press(sc, now);                            /* menu navigation */
		}
			return;
		/* Look up/down, terminal-friendly:
		 *   PgUp / PgDn -> post one fixed pitch "notch" (syncduke_pitch_step), applied once in
		 *     processinput().  The view tilts a deterministic step and STAYS there; tap again to
		 *     look further, hold (auto-repeat) to ramp.  Unlike holding Duke's Aim keys, the
		 *     notch is frame-rate independent -- a single tap can't saturate to "flying" on a
		 *     slow link, and there's no held bit to fight Center_View.  Gated on `gameplay`.
		 *   Home / End -> in gameplay, Center_View (sc_kpad_5): snap the pitch back to level (the
		 *     reliable "un-look", since the notch does not auto-center).  In a MENU (no pitch to
		 *     level) they jump to the first / last item instead (sc_Home/sc_End -> probeXduke).
		 * SyncTERM/cterm sends PgUp=ESC[V, PgDn=ESC[U, Home=ESC[H, End=ESC[K (src/conio/cterm.c);
		 * we also accept the xterm forms (ESC[5~/6~/1~/4~, ESC[F). */
		case 'V':                                          /* PgUp -> look up one notch   */
			if (kitty_release_event())
				return;                                    /* press-only: no double notch */
			if (gameplay)
				syncduke_pitch_step++;
			return;
		case 'U':                                          /* PgDn -> look down one notch */
			if (kitty_release_event())
				return;
			if (gameplay)
				syncduke_pitch_step--;
			return;
		case 'H':                                          /* Home */
			if (kitty_release_event())
				return;
			press(gameplay ? sc_kpad_5 : sc_Home, now);    /* gameplay: Center_View; menu: first item */
			return;
		case 'K': case 'F':                                /* End (cterm ESC[K / xterm ESC[F) */
			if (kitty_release_event())
				return;
			press(gameplay ? sc_kpad_5 : sc_End, now);     /* gameplay: Center_View; menu: last item */
			return;
		case '~':                                    /* xterm ESC[n~ nav keys */
			if (kitty_release_event())
				return;                                    /* press-only under kitty */
			if (csi_params(p, 1) >= 1)
				switch (p[0]) {
					case 5:                                       /* PgUp -> look up   */
						if (gameplay)
							syncduke_pitch_step++;
						return;
					case 6:                                       /* PgDn -> look down */
						if (gameplay)
							syncduke_pitch_step--;
						return;
					case 1: case 7:                                              /* Home */
						press(gameplay ? sc_kpad_5 : sc_Home, now); return;     /* gameplay: Center_View; menu: first item */
					case 4: case 8:                                              /* End */
						press(gameplay ? sc_kpad_5 : sc_End, now); return;      /* gameplay: Center_View; menu: last item */
					case 11:                                      /* F1 -> GAME CONTROLS help */
						if (gameplay)
							syncduke_help_request = 1;
						return;
					case 12: press(sc_F2, now); return;           /* F2 -> Duke save     (xterm 12~) */
					case 13: press(sc_F3, now); return;           /* F3 -> Duke load     (kitty/xterm 13~) */
					case 14:                                      /* F4 -> cycle graphics tier */
						syncduke_tier_cycle();
						return;
					case 15: press(sc_F5, now); return;           /* F5                  (15~) */
					case 17: press(sc_F6, now); return;           /* F6 -> Duke quicksave (17~) */
				}
			return;
		case 'u':                                       /* kitty keyboard protocol */
			/* The CSI?u progressive-enhancement REPLY (marker '?') announces support: enable the
			 * protocol and push our flags (1 disambiguate | 2 event-types | 8 all-keys), so every
			 * key now arrives here with a press/repeat/release event.  Otherwise this is a key
			 * event: CSI <codepoint> ; <mods> : <event> u. */
			if (csi_len > 0 && csi_par[0] == '?') {
				if (!g_kitty_active) {
					g_kitty_active = 1;
					syncduke_out_put("\x1b[>11u", 6);
				}
				return;
			}
			{
				int mod, ev, cp = kitty_key(&mod, &ev), c;
				if (cp <= 0)
					return;
				if (cp >= 57399 && cp <= 57414) {
					/* kitty reports the keypad in the Private Use Area (KP_0..9 = 57399.., KP_ENTER
					 * = 57414); fold to ASCII so numpad Enter/digits work like the main keys. */
					static const char kp[] = "0123456789./*-+\r";
					cp = (unsigned char)kp[cp - 57399];
				}
				if ((mod == 5 || mod == 7) && ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')))
					c = cp & 0x1f;          /* ctrl(+shift)+letter -> its control byte (Ctrl-S = 0x13) */
				else if (cp < 128)
					c = cp;                 /* plain ASCII codepoint (incl. folded keypad) */
				else
					return;                 /* other non-ASCII special key: not in the action map */
				handle_key(c, gameplay, now, ev);
			}
			return;
		case 't':                                       /* window-op reply: 4=text-area px, 6=cell px */
			if (csi_params(p, 4) >= 3) {
				if (p[0] == 4) { g_term_px_h = p[1]; g_term_px_w = p[2]; g_px_exact = 1; }  /* ESC[14t: exact text-area px (authoritative) */
				else if (p[0] == 6) { g_cell_h = p[1]; g_cell_w = p[2]; syncduke_estimate_px(); }  /* ESC[16t: cell px -> refine estimate */
			}
			return;
		case 'R':                                       /* ESC[rows;cols R (cursor-position report) */
			/* The FIRST report (from the startup size probe at 999;999) gives the text
			 * grid, from which we estimate the pixel canvas (cell size x grid, see
			 * syncduke_estimate_px); every report ALSO acks one in-flight frame's DSR
			 * (pacing). Capture the grid once -- later reports sit at the cursor we set. */
			if (csi_params(p, 2) >= 2 && g_grid_rows == 0) {
				g_grid_rows = p[0]; g_grid_cols = p[1];
				syncduke_estimate_px();
			}
			syncduke_pace_ack();
			return;
		case 'c':                                       /* device-attributes reply (DA1 ESC[c / CTDA ESC[<c) */
			/* '<' marker = SyncTERM CTDA; '=' = SyncTERM "CTerm" version. Either means
			 * SyncTERM (xterm's DA1 reply uses '?'), which also implies sixel. Beyond that,
			 * scan the params for 4 (DA1 param 4 / CTDA cap 4 = sixel/pixel ops). Any 'c'
			 * reply marks the terminal as having answered the capability probe. */
			g_probe_replied = 1;
			if (csi_len > 0 && (csi_par[0] == '<' || csi_par[0] == '=')) {
				g_is_syncterm = 1;
				g_have_sixel  = 1;
			}
			{
				int p[16], np = csi_params(p, 16), k;
				for (k = 0; k < np; k++)
					if (p[k] == 4)
						g_have_sixel = 1;
			}
			return;
		case 'P':                                       /* F1 (CSI P / SS3 O P) -> GAME CONTROLS help */
			if (kitty_release_event())
				return;
			if (gameplay)
				syncduke_help_request = 1;
			return;
		case 'Q':                                       /* F2 (CSI Q / SS3 O Q) -> Duke save menu */
			if (kitty_release_event())
				return;
			press(sc_F2, now);
			return;
		case 'S':
			if (csi_intro == 'O') {                     /* SS3 ESC O S = F4 -> cycle graphics tier */
				syncduke_tier_cycle();
				return;
			}
			if (csi_len == 0 || csi_par[0] != '?') {    /* CSI S = F4 (kitty) -> cycle graphics tier */
				if (!kitty_release_event())
					syncduke_tier_cycle();
				return;
			}
			/* XTSMGRAPHICS reply ESC[?2;0;W;HS = the graphics-canvas pixels (what
			 * SyncTERM answers instead of ESC[14t).  Used to scale/center the JXL
			 * image to fill the real canvas. */
			if (csi_params(p, 4) >= 4 && p[0] == 2 && p[1] == 0) {
				g_gfx_w = p[2];
				g_gfx_h = p[3];
			}
			return;
		case 'n':                                       /* CTerm state report; we care about CTQJS */
			/* JXL-support reply to our APC Q;JXL: ESC[=1;{0,1}-n. Reconstruct the raw
			 * sequence and let termgfx's scanner (the same one SyncDoom uses) classify
			 * it -- keeps both doors on one parser. Any 0/1 answer also means SyncTERM. */
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
					g_is_syncterm  = 1;
					g_jxl_supported = (r == 1);
				}
			}
			return;
		case 'M':                                       /* xterm SGR mouse: ESC[<b;col;row M (press/motion) */
		case 'm':                                       /*                                m (release)        */
			if (csi_len > 0 && csi_par[0] == '<' && csi_params(p, 3) >= 3)
				syncduke_mouse_event(p[0], p[1], p[2], fin == 'm');
			return;
		default: return;                                /* other reports: ignore */
	}
}

/*
 * Feed terminal bytes through the parser: map keystrokes (single bytes + arrow
 * sequences) into the scancode queue, and absorb the terminal's size-probe
 * replies (ESC[4;H;Wt, ESC[r;cR) to learn the pixel canvas -- without delivering
 * those report bytes to the game as keys.
 */
void syncduke_input_pump(int fd, int now, int gameplay)
{
	uint8_t buf[256];
	int     n, i;

	if (fd < 0)
		return;

	/* Relax mouse steering/fire if the pointer has gone quiet (runs every pump, even a
	 * no-input one -- the report stream stops the instant the pointer stops moving). */
	syncduke_mouse_idle();
	syncduke_key_turn_idle();

	/* Keep crouch asserted while the latch is on (must run even on a no-input pump,
	 * so it sits above the read()-returns-0 early-out below). Only in gameplay -- a
	 * menu/text field should get literal keys. */
	if (g_crouch_toggle && gameplay)
		press(sc_Z, now);

	/* Deliver a pending lone ESC once its follow-up window has elapsed (the menu key
	 * shouldn't wait for the next keystroke). A real ESC[ sequence arrives in the same
	 * burst, so it'll have advanced past P_ESC before this fires. */
	if (pstate == P_ESC && (uint32_t)(syncduke_in_now_ms() - g_esc_at_ms) > SYNCDUKE_ESC_MS) {
		press(sc_Escape, now);
		pstate = P_NORMAL;
	}

	/* On a real door socket, EOF (peer closed) or a hard error means the user
	 * disconnected -> exit and free the node. In dev/tty mode (stdin fallback,
	 * no door socket) a finite piped script hitting EOF is normal, so just stop
	 * reading -- don't exit. "no data yet" (EWOULDBLOCK) returns quietly. */
#ifdef _WIN32
	n = recv((SOCKET)fd, (char *)buf, (int)sizeof(buf), 0);
	if (n == SOCKET_ERROR) {
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK || e == WSAENOBUFS || e == WSAEINTR)
			return;                                  /* no input yet / transient */
		if (syncduke_door_socket() >= 0) {
			char r[48];
			snprintf(r, sizeof r, "input recv error wsa=%d", e);
			syncduke_hangup(r);
		}
		return;
	}
	if (n == 0) {                                        /* peer closed */
		if (syncduke_door_socket() >= 0)
			syncduke_hangup("client closed (input EOF)");
		return;
	}
#else
	n = (int)read(fd, buf, sizeof(buf));
	if (n <= 0) {
		if (syncduke_door_socket() >= 0) {
			if (n == 0)
				syncduke_hangup("client closed (input EOF)");
			if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				syncduke_hangup("input read error");
		}
		return;
	}
#endif

	termgfx_audio_feed(sd_audio, buf, n);   /* resolve the audio cap probe (ESC[=7;100;Xn) */
	{
		static int sd_prev_tier = -2;       /* log the negotiated audio tier once it resolves */
		int        t = termgfx_audio_tier(sd_audio);
		if (t != sd_prev_tier) {
			sd_prev_tier = t;
			syncduke_log("audio: tier=%d (%s)", t,
			             t == 1 ? "digital -- SFX should play" :
			             t == 0 ? "audio APC but no libsndfile -- silent" :
			             "no audio APC reply -- old SyncTERM or no audio");
		}
	}

	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];
		switch (pstate) {
			case P_NORMAL:
				/* ESC begins a sequence (P_ESC); every other byte is a key on the legacy byte
				 * path -- handle_key() does the door shortcuts (Ctrl-S/T/O, Ctrl-A..F = F1..F6,
				 * the SyncTERM <=1.4 fallback), sticky crouch, and the scancode map.  Under the
				 * kitty protocol these same keys instead arrive as CSI-u events (csi_final 'u'),
				 * which route through the same handle_key() with real press/release. */
				if (c == 0x1b) { pstate = P_ESC; g_esc_at_ms = syncduke_in_now_ms(); }
				else
					handle_key(c, gameplay, now, 0);
				break;
			case P_ESC:
				if (c == '[' || c == 'O') { pstate = P_CSI; csi_intro = (char)c; csi_len = 0; }
				else { press(sc_Escape, now); pstate = P_NORMAL; i--; }   /* lone ESC, reprocess c */
				break;
			case P_CSI:
				if (c >= 0x40 && c <= 0x7e) { csi_final((char)c, gameplay, now); pstate = P_NORMAL; }
				else if (csi_len < (int)sizeof(csi_par)) { csi_par[csi_len++] = (char)c; }
				break;
		}
	}
}

/* Resolve the input fd once: the door socket (SYNCDUKE_SOCK) or stdin, set
 * non-blocking so the game loop never stalls waiting for a key. */
int syncduke_input_fd(void)
{
	static int fd = -2;     /* -2 = not yet resolved */
	int        ds;

	if (fd != -2)
		return fd;
#ifdef _WIN32
	/* The only input source on Windows is the door's Winsock socket (already set
	 * non-blocking in syncduke_io.c). No socket (a dev run) => no input (-1, which
	 * syncduke_input_pump() treats as a no-op). */
	if ((ds = syncduke_door_socket()) >= 0) {
		u_long nb = 1;
		fd = ds;
		ioctlsocket((SOCKET)fd, FIONBIO, &nb);
	} else {
		fd = -1;
	}
#else
	{
		int         fl;
		const char *s;
		if ((ds = syncduke_door_socket()) >= 0)      /* real BBS client socket */
			fd = ds;
		else if ((s = getenv("SYNCDUKE_SOCK")) != NULL)
			fd = atoi(s);
		else
			fd = 0;                            /* default stdin (dev/tty) */
		if ((fl = fcntl(fd, F_GETFL, 0)) != -1)
			fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	}
#endif
	return fd;
}

/* Clear transient input latches/state when a game is loaded (called from loadplayer), so the
 * door's sticky crouch, queued scancodes, synthetic key-holds, and turn/look momentum from the
 * pre-load session don't carry into the restored game. */
void syncduke_input_reset(void)
{
	int i;

	g_crouch_toggle     = 0;
	syncduke_key_turn   = 0;
	syncduke_pitch_step = 0;
	rawq_head = rawq_tail = 0;
	for (i = 0; i < 128; i++) {
		held[i]       = 0;
		release_at[i] = 0;
	}
}
