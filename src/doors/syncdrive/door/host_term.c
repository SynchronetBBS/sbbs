/* host_term.c -- tdport's host.h implemented on termgfx_termio: the door's
 * replacement for upstream's SDL host.c. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "mem.h"
#include "symbols.h"

#include "termgfx_termio.h"
#include "termgfx_plat.h"
#include "dirwrap.h"
#include "genwrap.h"

#include "alias.h"
#include "frame.h"
#include "help_card.h"
#include "host_term_ext.h"
#include "keymap.h"
#include "keyscript.h"
#include "scores_lock.h"
#include "speaker.h"

#define AUDIO_RATE     24000              /* termio's mixer rate (TERMGFX_AUDIO_RATE) */
#define PRESENT_MIN_MS 8
#define KBD_SIZE       16
#define CATCHUP_TICKS  50                 /* at most 0.5 s of ticks per pump */
#define SCRIPT_MAX     256

static char             game_dir[1024] = ".";
static char             data_dir[1024] = ".";
static char             player[ALIAS_FIELD + 1];

static void             (*tick_handler)(void);
static bool             (*frame_source)(u32 *);
static u32              frame[HOST_FRAME_MAX_W * HOST_FRAME_MAX_H];
static uint8_t          frame_idx[FRAME_W * FRAME_H];
static uint8_t          frame_pal[768];
static uint32_t         last_present_ms;

static uint64_t         clock_start_ms;
static uint64_t         ticks_run;

static u16              kbd_buf[KBD_SIZE];
static int              kbd_head, kbd_tail;
static bool             held_keys;        /* off until the terminal sends a release */
static bool             held_keys_allowed = true; /* syncdrive.ini [input] held_keys = off */
static bool             held[128];

static speaker_t        spk;
static int              audio_on = -1;    /* -1 = not asked yet */
static int16_t          pcm[2 * 4096];
static size_t           pcm_frames;
static double           pcm_frac;

static keyscript_step_t script[SCRIPT_MAX];
static int              script_len, script_pos;
static uint32_t         script_due_ms;

static int              frame_rate = 8;
static uint64_t         next_frame_ms;

/* ------------------------------------------------------------ door-side API */

void host_set_player_name(const char *alias)
{
	alias_sanitize(alias, player);
}

const char *host_player_name(void)
{
	return player;
}

void host_set_data_dir(const char *dir)
{
	snprintf(data_dir, sizeof data_dir, "%s", dir);
	mkpath(data_dir);
	scores_lock_init(data_dir);
}

void host_scores_lock(void)
{
	if (scores_lock_acquire() != 0)
		fprintf(stderr, "syncdrive: SCORES lock failed in %s\n", data_dir);
}

void host_scores_unlock(void)
{
	scores_lock_release();
}

void host_set_held_keys_allowed(bool on)
{
	held_keys_allowed = on;
}

void host_set_keyscript(const char *s)
{
	script_len = keyscript_parse(s, script, SCRIPT_MAX);
	if (script_len < 0) {
		fprintf(stderr, "syncdrive: bad SYNCDRIVE_KEYS script\n");
		script_len = 0;
	}
	script_pos    = 0;
	script_due_ms = termgfx_plat_now_ms();
}

/* ------------------------------------------------------------ lifecycle */

bool host_init(const char *dir, int window_scale)
{
	(void)window_scale;
	snprintf(game_dir, sizeof game_dir, "%s", dir);
	frame_palette(frame_pal);
	speaker_init(&spk, AUDIO_RATE);
	clock_start_ms = termgfx_plat_now_ms();
	ticks_run      = 0;
	return true;
}

void host_shutdown(void)
{
	termgfx_termio_shutdown();
}

void host_set_tick_handler(void (*handler)(void))
{
	tick_handler = handler;
}

void host_set_frame_source(bool (*compose)(u32 *xrgb), int w, int h)
{
	(void)w;
	(void)h;              /* EGA build: always 320x200 */
	frame_source = compose;
}

/* ------------------------------------------------------------ keyboard */

static void kbd_push(u16 key)
{
	int next = (kbd_tail + 1) % KBD_SIZE;

	if (next == kbd_head)
		return;           /* full: the BIOS drops the key */
	kbd_buf[kbd_tail] = key;
	kbd_tail          = next;
}

/* The help card: the game is frozen while it is up (no ticks, no frames), and
 * the tick and frame clocks skip the paused time so nothing catches up. */
static void help_modal(void)
{
	termgfx_input_event_t ev;
	uint32_t              t0 = termgfx_plat_now_ms();
	uint32_t              paused;

	help_card_show();
	for (;;) {
		termgfx_termio_pump();
		if (termgfx_termio_hung_up() || termgfx_termio_quit_requested())
			exit(0);
		if (termgfx_termio_next_event(&ev)) {
			if (ev.type == TERMGFX_EV_KEY_DOWN)
				break;
			continue;
		}
		termgfx_plat_sleep_ms(10);
	}
	memset(held, 0, sizeof held);
	paused          = termgfx_plat_now_ms() - t0;
	clock_start_ms += paused;
	if (next_frame_ms != 0)
		next_frame_ms += paused;
	help_card_dismiss();
	termgfx_termio_present(frame_idx, frame_pal);
}

static void handle_key(const termgfx_input_event_t *ev)
{
	keymap_result_t r = keymap_translate(ev);

	if (ev->type == TERMGFX_EV_KEY_UP && held_keys_allowed && !held_keys) {
		held_keys = true;         /* this terminal reports releases */
		fputs("syncdrive: held-key driving on (terminal reports key releases)\n", stderr);
		/* Keys pressed on the legacy (no-release) path before this
		 * negotiation never got a release; without this they would
		 * read as held forever. */
		memset(held, 0, sizeof held);
	}
	if (r.xt != 0 && r.xt < 128)
		held[r.xt] = r.down != 0;
	if (r.bios != 0)
		kbd_push(r.bios);
	if (r.action == KEYMAP_ACT_SOUND_TOGGLE)
		kbd_push(keymap_sound_toggle_key((DSB(DS_snd_flags) & 4) != 0));
	else if (r.action == KEYMAP_ACT_FIT_CYCLE)
		termgfx_termio_fit_cycle();
	else if (r.action == KEYMAP_ACT_HELP)
		help_modal();
}

static void run_script(uint32_t now)
{
	while (script_pos < script_len && (int32_t)(now - script_due_ms) >= 0) {
		const keyscript_step_t *st = &script[script_pos++];

		if (st->kind == KEYSCRIPT_WAIT)
			script_due_ms = now + (uint32_t)st->ms;
		else if (st->kind == KEYSCRIPT_QUIT)
			exit(0);
		else
			handle_key(&st->ev);
	}
}

static void poll_input(void)
{
	termgfx_input_event_t ev;

	termgfx_termio_pump();
	if (termgfx_termio_hung_up() || termgfx_termio_quit_requested())
		exit(0);          /* atexit runs termgfx_termio_shutdown() */
	while (termgfx_termio_next_event(&ev))
		handle_key(&ev);
	run_script(termgfx_plat_now_ms());
}

bool host_kbd_peek(u16 *key)
{
	poll_input();
	if (kbd_head == kbd_tail)
		return false;
	if (key)
		*key = kbd_buf[kbd_head];
	return true;
}

bool host_kbd_read(u16 *key)
{
	poll_input();
	if (kbd_head == kbd_tail)
		return false;
	if (key)
		*key = kbd_buf[kbd_head];
	kbd_head = (kbd_head + 1) % KBD_SIZE;
	return true;
}

void host_kbd_flush(void)
{
	poll_input();
	kbd_head = kbd_tail = 0;
}

u8 host_kbd_shift_flags(void)
{
	return 0;             /* no live modifier state from a terminal */
}

void host_set_held_keys(bool on)
{
	held_keys = on;
}

bool host_held_keys(void)
{
	return held_keys;
}

bool host_xt_key_down(u8 xt_scan)
{
	return xt_scan < 128 && held[xt_scan];
}

bool host_joy_read(s16 *x, s16 *y, u8 *buttons)
{
	(void)x;
	(void)y;
	(void)buttons;
	return false;
}

/* ------------------------------------------------------------ audio */

static void audio_flush(void)
{
	if (pcm_frames == 0)
		return;
	termgfx_termio_audio_stream(pcm, pcm_frames);
	pcm_frames = 0;
}

static void audio_for_one_tick(void)
{
	size_t n;

	if (audio_on < 0)
		audio_on = termgfx_termio_audio_available();
	if (!audio_on)
		return;
	pcm_frac += (double)AUDIO_RATE * PIT_DIV_GAME / PIT_HZ;
	n         = (size_t)pcm_frac;
	pcm_frac -= (double)n;
	if (pcm_frames + n > sizeof pcm / sizeof pcm[0] / 2)
		audio_flush();
	if (n > sizeof pcm / sizeof pcm[0] / 2)
		n = sizeof pcm / sizeof pcm[0] / 2;
	speaker_render(&spk, pcm + pcm_frames * 2, n);
	pcm_frames += n;
}

void host_speaker(u16 divisor, bool on)
{
	speaker_set(&spk, divisor, on);
}

/* ------------------------------------------------------------ video + clock */

static bool present_if_changed(void)
{
	if (frame_source == NULL || !frame_source(frame))
		return false;
	frame_convert(frame, frame_idx, FRAME_W * FRAME_H);
	termgfx_termio_present(frame_idx, frame_pal);
	last_present_ms = termgfx_plat_now_ms();
	return true;
}

void host_present_now(void)
{
	present_if_changed();
}

static uint64_t tick_due_ms(uint64_t n)
{
	return clock_start_ms + n * PIT_DIV_GAME * 1000u / PIT_HZ;
}

void host_pump(void)
{
	bool     worked = false;
	uint64_t now;
	int      budget = CATCHUP_TICKS;

	poll_input();
	now = termgfx_plat_now_ms();
	while (tick_due_ms(ticks_run + 1) <= now && budget-- > 0) {
		ticks_run++;
		if (tick_handler)
			tick_handler();
		audio_for_one_tick();
		worked = true;
	}
	if (budget < 0)           /* fell behind: resynchronize the clock */
		clock_start_ms = now - (tick_due_ms(ticks_run) - clock_start_ms);
	audio_flush();

	if ((uint32_t)now - last_present_ms >= PRESENT_MIN_MS && present_if_changed())
		worked = true;
	termgfx_termio_tick();
	if (!worked)
		termgfx_plat_sleep_ms(1);
}

void host_set_frame_rate(int fps)
{
	frame_rate = fps < 0 ? 0 : fps;
}

int host_frame_rate(void)
{
	return frame_rate;
}

void host_frame_begin(void)
{
	uint64_t period, now;

	host_pump();
	if (frame_rate <= 0)
		return;
	period = 1000u / (uint64_t)frame_rate;
	now    = termgfx_plat_now_ms();
	if (next_frame_ms == 0 || now > next_frame_ms + period)
		next_frame_ms = now;  /* resync after a stall */
	while (termgfx_plat_now_ms() < next_frame_ms)
		host_pump();
	next_frame_ms += period;
}

/* ------------------------------------------------------------ files + errors */

char *host_game_path(const char *name, bool create)
{
	char path[2048];

	if (stricmp(name, "SCORES") == 0) {
		snprintf(path, sizeof path, "%s/SCORES", data_dir);
		if (create || fexist(path))
			return strdup(path);
		return NULL;
	}
	snprintf(path, sizeof path, "%s/%s", game_dir, name);
	if (fexistcase(path) || create)
		return strdup(path);
	return NULL;
}

void host_free(void *p)
{
	free(p);
}

_Noreturn void host_fatal(const char *fmt, ...)
{
	char    msg[512];
	char    line[600];
	va_list ap;
	int     n;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	fprintf(stderr, "syncdrive: fatal: %s\n", msg);
	if (termgfx_termio_active()) {
		n = snprintf(line, sizeof line, "\x1b[1;1H\x1b[0m\x1b[2KTest Drive: %s\r\n", msg);
		termgfx_termio_write(line, (size_t)n);
		termgfx_termio_flush();
		termgfx_plat_sleep_ms(3000);
	}
	exit(3);
}
