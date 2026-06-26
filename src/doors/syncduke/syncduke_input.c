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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "syncduke.h"
#include "keyboard.h"   /* sc_* scancode constants (pure #defines) */
#include "caps.h"       /* termgfx: termgfx_caps_parse_jxl (cap-probe reply scan) */

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
volatile int   syncduke_fast_turn; /* FAST TURN toggle -> player.c getinput */

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
	for (sc = 1; sc < 128; sc++) {
		if (held[sc] && (int32_t)(now - release_at[sc]) >= 0) {
			rawq_push((uint8_t)(sc | 0x80));    /* key-up */
			held[sc] = 0;
		}
	}
}

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
static int g_term_px_w, g_term_px_h;

int syncduke_term_px_w(void) { return g_term_px_w; }
int syncduke_term_px_h(void) { return g_term_px_h; }

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
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
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

volatile int    syncduke_mouse_turn;   /* -> angvel  (player.c getinput) */
volatile int    syncduke_mouse_fire;   /* -> Fire bit (player.c getinput) */

static int      g_mouse_on = 1;        /* steering enabled (default on; Ctrl-O toggles) */
static int      g_mouse_sens = 30;     /* steer sensitivity, a 0..63 slider (Setup Mouse);
                                        * max angvel at the image edge = sens*2 (30->60). */
static int      g_mouse_col;           /* last reported pointer cell (1-based) */
static int      g_mouse_have;          /* a report has arrived (neutral until then) */
static int      g_mouse_buttons;       /* bit0 left/fire, 1 middle, 2 right */
static uint32_t g_mouse_last_ms;       /* time of the last report (idle timeout) */

int  syncduke_mouse_enabled(void) { return g_mouse_on; }
int  syncduke_mouse_sens(void)    { return g_mouse_sens; }
void syncduke_mouse_sens_set(int v) { g_mouse_sens = v < 0 ? 0 : (v > 63 ? 63 : v); }

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
 * edge is full deflection (mirrors SyncDoom's mouse_steer_rate).  The sixel sits at the
 * top-left (drawn at home), 8 px per cell, so the centre cell is (width_px/8)/2. */
static int syncduke_mouse_steer(void)
{
	int cols   = g_term_px_w > 0 ? g_term_px_w / 8 : 80;
	int center = cols / 2;
	int halfw  = cols / 2;
	int off, mag, dz;

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

/* A CSI/SS3 sequence has terminated with final byte `fin`. */
static void csi_final(char fin, int gameplay, int now)
{
	int p[4];

	switch (fin) {
		case 'A': press(sc_UpArrow, now);    return;   /* arrows: same in both modes */
		case 'B': press(sc_DownArrow, now);  return;
		case 'C': press(sc_RightArrow, now); return;
		case 'D': press(sc_LeftArrow, now);  return;
		/* Look up/down, terminal-friendly:
		 *   PgUp / PgDn -> post one fixed pitch "notch" (syncduke_pitch_step), applied once in
		 *     processinput().  The view tilts a deterministic step and STAYS there; tap again to
		 *     look further, hold (auto-repeat) to ramp.  Unlike holding Duke's Aim keys, the
		 *     notch is frame-rate independent -- a single tap can't saturate to "flying" on a
		 *     slow link, and there's no held bit to fight Center_View.  Gated on `gameplay`.
		 *   Home / End -> Center_View (sc_kpad_5): snap the pitch straight back to level (the
		 *     reliable "un-look", since the notch does not auto-center).
		 * SyncTERM/cterm sends PgUp=ESC[V, PgDn=ESC[U, Home=ESC[H, End=ESC[K (src/conio/cterm.c);
		 * we also accept the xterm forms (ESC[5~/6~/1~/4~, ESC[F). */
		case 'V':                                          /* PgUp -> look up one notch   */
			if (gameplay)
				syncduke_pitch_step++;
			return;
		case 'U':                                          /* PgDn -> look down one notch */
			if (gameplay)
				syncduke_pitch_step--;
			return;
		case 'H': case 'K': case 'F': press(sc_kpad_5, now); return; /* Home/End -> Center_View (re-level) */
		case '~':                                    /* xterm ESC[n~ nav keys */
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
					case 1: case 7: press(sc_kpad_5, now); return;               /* Home -> Center_View */
					case 4: case 8: press(sc_kpad_5, now); return;               /* End  -> Center_View */
					case 11:                                      /* F1 -> GAME CONTROLS help */
						if (gameplay)
							syncduke_help_request = 1;
						return;
					case 14:                                      /* F4 -> cycle graphics tier */
						syncduke_tier_cycle();
						return;
				}
			return;
		case 't':                                       /* ESC[4;H;W t = text-area pixels */
			if (csi_params(p, 4) >= 3 && p[0] == 4) { g_term_px_h = p[1]; g_term_px_w = p[2]; }
			return;
		case 'R':                                       /* ESC[rows;cols R (cursor-position report) */
			/* First report (from the startup size probe at 999;999) gives the canvas
			 * size; every report ALSO acks one in-flight frame's DSR (pacing). */
			if (csi_params(p, 2) >= 2 && g_term_px_w == 0) { g_term_px_h = p[0] * 16; g_term_px_w = p[1] * 8; }
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
		case 'S':                                       /* SS3 ESC O S = F4 -> cycle graphics tier */
			if (csi_intro == 'O')
				syncduke_tier_cycle();
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
/* Sticky-crouch toggle: a terminal can't hold a key (no key-up; auto-repeat is
 * laggy), so momentary crouch (hold Z) is unusable. Instead 'z' toggles this latch
 * and we re-assert the Crouch scancode every pump while it's on -- press once to
 * crouch, press again to stand. Mirrors how AutoRun is a toggle. */
static int g_crouch_toggle;

void syncduke_input_pump(int fd, int now, int gameplay)
{
	uint8_t buf[256];
	int     n, i;

	if (fd < 0)
		return;

	/* Relax mouse steering/fire if the pointer has gone quiet (runs every pump, even a
	 * no-input one -- the report stream stops the instant the pointer stops moving). */
	syncduke_mouse_idle();

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

	n = (int)read(fd, buf, sizeof(buf));
	if (n <= 0) {
		/* On a real door socket, EOF (peer closed) or a hard error means the user
		 * disconnected -> exit and free the node. In dev/tty mode (stdin fallback,
		 * no door socket) a finite piped script hitting EOF is normal, so just stop
		 * reading -- don't exit. EAGAIN/EWOULDBLOCK is "no input yet" either way. */
		if (syncduke_door_socket() >= 0) {
			if (n == 0)
				syncduke_hangup("client closed (input EOF)");
			if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				syncduke_hangup("input read error");
		}
		return;
	}

	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];
		switch (pstate) {
			case P_NORMAL:
				if (c == 0x1b) { pstate = P_ESC; g_esc_at_ms = syncduke_in_now_ms(); }
				else if (c == 0x13) { syncduke_stats_toggle(); }   /* Ctrl-S: door-level stats overlay, never reaches Duke */
				else if (c == 0x14) { syncduke_depth_cycle(); }    /* Ctrl-T: cycle pipeline depth, never reaches Duke */
				else if (c == 0x0f) { syncduke_mouse_toggle(); }   /* Ctrl-O: toggle mouse steering, never reaches Duke */
				else if (c >= 0x01 && c <= 0x06) {
					/* Ctrl-A..Ctrl-F stand in for F1..F6 on terminals that can't send the
					 * F-key escape sequences -- notably SyncTERM <=1.4, whose function keys
					 * are mis-encoded. (Modern SyncTERM sends ESC[11~.. and F1 works directly
					 * via csi_final(); this is the fallback.) Ctrl-A = F1 = GAME CONTROLS (via
					 * the help flag, NOT sc_F1, so Duke's built-in F1HELP stays bypassed);
					 * Ctrl-B..Ctrl-F = sc_F2..sc_F6 for Duke's own save/load/quicksave. */
					if (c != 0x01)
						press(sc_F1 + (c - 1), now);    /* Ctrl-B..Ctrl-F = F2..F6 */
					else if (gameplay)
						syncduke_help_request = 1;      /* Ctrl-A = F1 = GAME CONTROLS */
				}
				else if (gameplay && (c == 'z' || c == 'Z')) { g_crouch_toggle = !g_crouch_toggle; }   /* toggle sticky crouch */
				else { int sc = syncduke_map_key((const char *)&c, 1, gameplay); if (sc > 0)
						   press(sc, now); }
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
	static int  fd = -2;    /* -2 = not yet resolved */
	int         fl, ds;
	const char *s;

	if (fd != -2)
		return fd;
	if ((ds = syncduke_door_socket()) >= 0)      /* real BBS client socket */
		fd = ds;
	else if ((s = getenv("SYNCDUKE_SOCK")) != NULL)
		fd = atoi(s);
	else
		fd = 0;                            /* default stdin (dev/tty) */
	if ((fl = fcntl(fd, F_GETFL, 0)) != -1)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	return fd;
}
