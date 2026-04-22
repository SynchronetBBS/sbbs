/*
 * audio_apc.c — BBS audio APC dispatcher.
 *
 * Implements the SyncTERM:A;* verbs (Load / Synth / Copy / Queue /
 * Flush / Volume / Wait / Update), the SyncTERM:Q;* feature query,
 * and the CSI = 7 n DSR response callback.  See cterm.adoc for the
 * wire-protocol spec and audio_apc.h for the interface.
 *
 * State:
 *   patches[256]      — slot table of loaded/synth'd PCM buffers.
 *                       Each slot owns its int16_t* until Queue
 *                       transfers it to the mixer, after which the
 *                       slot is empty.
 *   apc_channels[2..15] — xp_audio_handle_t per APC-dedicated
 *                       channel, lazy-opened at -12 dB base.  Ch=0/1
 *                       are resolved via cterm->music_stream /
 *                       cterm->fx_stream and cannot be Queue'd onto.
 *   apc_notify[0..15] — Update-armed flags per channel.
 *   apc_last_state[0..15] — last poll-observed running-state per
 *                       channel, for running→stopped transition
 *                       detection.
 */

#include "audio_apc.h"
#include "sndfile.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xpbeep.h>      /* xp_audio_*, XPBEEP_SAMPLE_RATE, WAVE_SHAPE_* */
#include <genwrap.h>     /* strnicmp, MAX_PATH */
#include <dirwrap.h>     /* clean_path */

/* ----- external from term.c ----- */
extern int clean_path(char *path, size_t buflen);
extern size_t conn_send(const void *buf, size_t buflen, unsigned timeout);

/* ----- constants ----- */
#define AUDIO_APC_PATCH_SLOTS      256
#define AUDIO_APC_CHANNELS         16
#define AUDIO_APC_FIRST_APC_CH     2     /* 0,1 = cterm-owned */
#define AUDIO_APC_BASE_DB          -12.0f
#define AUDIO_APC_FEATURE_SNDFILE  100

/* Synth shape sentinel — not a WAVE_SHAPE_* value; maps freq→0. */
#define APC_SHAPE_SILENCE          (-2)

/* ----- slot + channel state ----- */
struct audio_patch {
	int16_t *frames;
	size_t   nframes;
};
static struct audio_patch patches[AUDIO_APC_PATCH_SLOTS];
static xp_audio_handle_t  apc_channels[AUDIO_APC_CHANNELS - AUDIO_APC_FIRST_APC_CH];
static bool               apc_channels_init = false;
static bool               apc_notify[AUDIO_APC_CHANNELS];
static int                apc_last_state[AUDIO_APC_CHANNELS];

static void
init_state_once(void)
{
	int i;
	if (apc_channels_init)
		return;
	for (i = 0; i < (int)(sizeof(apc_channels) / sizeof(apc_channels[0])); i++)
		apc_channels[i] = -1;
	apc_channels_init = true;
}

/* ----- channel handle resolver -----
 * Ch=0  → cterm->music_stream (may be -1 if MML never played)
 * Ch=1  → cterm->fx_stream    (may be -1 if no fx ever played)
 * Ch>=2 → apc_channels[c-2], lazy-opened at -12 dB.
 * Returns -1 on any out-of-range or unopenable channel. */
static xp_audio_handle_t
channel_handle(struct cterminal *cterm, int c)
{
	if (cterm == NULL)
		return -1;
	if (c == 0)
		return cterm->music_stream;
	if (c == 1)
		return cterm->fx_stream;
	if (c < AUDIO_APC_FIRST_APC_CH || c >= AUDIO_APC_CHANNELS)
		return -1;
	init_state_once();
	if (apc_channels[c - AUDIO_APC_FIRST_APC_CH] < 0) {
		if (!xptone_open())
			return -1;
		apc_channels[c - AUDIO_APC_FIRST_APC_CH] = xp_audio_open(AUDIO_APC_BASE_DB,
		                                                          AUDIO_APC_BASE_DB);
		if (apc_channels[c - AUDIO_APC_FIRST_APC_CH] < 0) {
			xptone_close();
			return -1;
		}
	}
	return apc_channels[c - AUDIO_APC_FIRST_APC_CH];
}

/* ----- parsers ----- */

/* Parse "<N>" (ms default), "<N>ms", "<N>f", or "<N>p" (periods).
 * `freq_hz` is required for `p`; `allow_periods` gates the `p` suffix.
 * Returns 0 on any parse error / invalid unit — callers treat 0 as a
 * "skip this envelope" sentinel, never as an error. */
static size_t
apc_parse_duration(const char *s, double freq_hz, bool allow_periods)
{
	char         *end;
	unsigned long n;

	if (s == NULL)
		return 0;
	n = strtoul(s, &end, 10);
	if (end == s)
		return 0;
	/* no suffix == ms */
	if (*end == '\0' || *end == ';')
		return (size_t)((uint64_t)n * XPBEEP_SAMPLE_RATE / 1000);
	if (end[0] == 'm' && end[1] == 's' && (end[2] == '\0' || end[2] == ';'))
		return (size_t)((uint64_t)n * XPBEEP_SAMPLE_RATE / 1000);
	if (end[0] == 'f' && (end[1] == '\0' || end[1] == ';'))
		return (size_t)n;
	if (end[0] == 'p' && (end[1] == '\0' || end[1] == ';')) {
		if (!allow_periods || freq_hz <= 0.0)
			return 0;
		return (size_t)((double)n * (double)XPBEEP_SAMPLE_RATE / freq_hz + 0.5);
	}
	return 0;
}

/* Parse a volume value: "<pct>" (0..100 linear) or "<dB>dB" (literal).
 * Returns dB.  0 pct → -60 dB floor.  Parse errors → 0 dB (unity). */
static float
apc_parse_volume(const char *s)
{
	char *end;
	float v;

	if (s == NULL)
		return 0.0f;
	v = strtof(s, &end);
	if (end == s)
		return 0.0f;
	if ((end[0] == 'd' || end[0] == 'D') &&
	    (end[1] == 'b' || end[1] == 'B') &&
	    (end[2] == '\0' || end[2] == ';'))
		return v;
	/* no suffix: linear 0..100 → dB */
	if (v <= 0.0f)
		return -60.0f;
	if (v > 100.0f)
		v = 100.0f;
	return 20.0f * log10f(v / 100.0f);
}

/* Parse a Synth shape name.  Returns WAVE_SHAPE_* value, or
 * APC_SHAPE_SILENCE for "SILENCE", or -1 on unrecognized input.
 * Longest-prefix match to avoid "SIN" eating "SINE_SAW". */
static int
parse_shape(const char *s)
{
	static const struct {
		const char *name;
		int         value;
	} shapes[] = {
		{ "SINE_SAW_HARM",  WAVE_SHAPE_SINE_SAW_HARM  },
		{ "SINE_SAW_CHORD", WAVE_SHAPE_SINE_SAW_CHORD },
		{ "SINE_HARM",      WAVE_SHAPE_SINE_HARM      },
		{ "SINE_SAW",       WAVE_SHAPE_SINE_SAW       },
		{ "SILENCE",        APC_SHAPE_SILENCE         },
		{ "SAW",            WAVE_SHAPE_SAWTOOTH       },
		{ "SIN",            WAVE_SHAPE_SINE           },
		{ "SQ",             WAVE_SHAPE_SQUARE         },
	};
	size_t i;

	if (s == NULL)
		return -1;
	for (i = 0; i < sizeof(shapes) / sizeof(shapes[0]); i++) {
		size_t len = strlen(shapes[i].name);
		if (strncmp(s, shapes[i].name, len) == 0 &&
		    (s[len] == '\0' || s[len] == ';'))
			return shapes[i].value;
	}
	return -1;
}

/* ----- verb handlers ----- */

static void
do_load(struct cterminal *cterm, char *str, char *fn)
{
	char         *p;
	char         *end;
	char         *audiofn = NULL;
	int16_t      *frames  = NULL;
	size_t        nframes = 0;
	unsigned long slot    = AUDIO_APC_PATCH_SLOTS;

	(void)cterm;
	/* Walk K=V tokens; break on first unknown (which is the filename). */
	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'S' && p[2] == '=') {
			slot = strtoul(p + 3, &end, 10);
			continue;
		}
		break;
	}
	if (slot >= AUDIO_APC_PATCH_SLOTS)
		goto done;
	if (p == NULL || *p != ';')
		goto done;

	if (asprintf(&audiofn, "%s%s", fn, p + 1) == -1) {
		audiofn = NULL;
		goto done;
	}
	if (!clean_path(audiofn, MAX_PATH))
		goto done;
	if (!sndfile_available())
		goto done;
	if (!sndfile_decode(audiofn, &frames, &nframes))
		goto done;

	free(patches[slot].frames);
	patches[slot].frames  = frames;
	patches[slot].nframes = nframes;
	frames = NULL;                /* ownership transferred */
done:
	free(audiofn);
	free(frames);
}

static void
do_synth(char *str)
{
	char         *p;
	char         *end;
	unsigned long slot     = AUDIO_APC_PATCH_SLOTS;
	int           shape    = -1;
	double        freq     = 0.0;
	size_t        nframes  = 0;
	int16_t      *buf;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'S' && p[2] == '=') {
			slot = strtoul(p + 3, &end, 10);
		}
		else if (p[1] == 'W' && p[2] == '=') {
			shape = parse_shape(p + 3);
		}
		else if (p[1] == 'F' && p[2] == '=') {
			freq = strtod(p + 3, &end);
		}
		else if (p[1] == 'T' && p[2] == '=') {
			nframes = apc_parse_duration(p + 3, freq, true);
		}
	}

	if (slot >= AUDIO_APC_PATCH_SLOTS)
		return;
	if (shape < 0 && shape != APC_SHAPE_SILENCE)
		return;
	if (nframes == 0)
		return;
	if (shape == APC_SHAPE_SILENCE) {
		freq  = 0.0;
		shape = WAVE_SHAPE_SINE; /* shape is irrelevant at freq=0 */
	}

	buf = (int16_t *)malloc(nframes * XPBEEP_FRAMESIZE);
	if (buf == NULL)
		return;
	xptone_makewave(freq, buf, (int)nframes, (unsigned)shape);

	free(patches[slot].frames);
	patches[slot].frames  = buf;
	patches[slot].nframes = nframes;
}

static void
do_copy(char *str)
{
	char         *p;
	char         *end;
	unsigned long src = AUDIO_APC_PATCH_SLOTS;
	unsigned long dst = AUDIO_APC_PATCH_SLOTS;
	int16_t      *copy;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'S' && p[2] == '=')
			src = strtoul(p + 3, &end, 10);
		else if (p[1] == 'D' && p[2] == '=')
			dst = strtoul(p + 3, &end, 10);
	}
	if (src >= AUDIO_APC_PATCH_SLOTS || dst >= AUDIO_APC_PATCH_SLOTS)
		return;
	if (patches[src].frames == NULL || patches[src].nframes == 0)
		return;

	copy = (int16_t *)malloc(patches[src].nframes * XPBEEP_FRAMESIZE);
	if (copy == NULL)
		return;
	memcpy(copy, patches[src].frames,
	       patches[src].nframes * XPBEEP_FRAMESIZE);

	free(patches[dst].frames);
	patches[dst].frames  = copy;
	patches[dst].nframes = patches[src].nframes;
}

static void
do_queue(struct cterminal *cterm, char *str)
{
	char             *p;
	char             *end;
	unsigned long     ch    = AUDIO_APC_CHANNELS;
	unsigned long     slot  = AUDIO_APC_PATCH_SLOTS;
	xp_audio_opts_t   opts  = XP_AUDIO_OPTS_INIT;
	xp_audio_handle_t h;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'C' && p[2] == '=') {
			ch = strtoul(p + 3, &end, 10);
		}
		else if (p[1] == 'S' && p[2] == '=') {
			slot = strtoul(p + 3, &end, 10);
		}
		else if (p[1] == 'I' && p[2] == '=') {
			opts.fade_in_frames = apc_parse_duration(p + 3, 0.0, false);
		}
		else if (p[1] == 'O' && p[2] == '=') {
			opts.fade_out_frames = apc_parse_duration(p + 3, 0.0, false);
		}
		else if (p[1] == 'X' && (p[2] == ';' || p[2] == '\0')) {
			opts.crossfade = true;
		}
		else if (p[1] == 'L' && (p[2] == ';' || p[2] == '\0')) {
			opts.loop = true;
		}
		else if (p[1] == 'V' && p[2] == 'L' && p[3] == '=') {
			opts.volume_l = apc_parse_volume(p + 4);
		}
		else if (p[1] == 'V' && p[2] == 'R' && p[3] == '=') {
			opts.volume_r = apc_parse_volume(p + 4);
		}
		else if (p[1] == 'V' && p[2] == '=') {
			float v = apc_parse_volume(p + 3);
			opts.volume_l = v;
			opts.volume_r = v;
		}
	}

	/* Reject Queue on cterm-owned channels 0/1. */
	if (ch >= AUDIO_APC_CHANNELS || ch < AUDIO_APC_FIRST_APC_CH)
		return;
	if (slot >= AUDIO_APC_PATCH_SLOTS)
		return;
	if (patches[slot].frames == NULL || patches[slot].nframes == 0)
		return;

	h = channel_handle(cterm, (int)ch);
	if (h < 0)
		return;

	/* Ownership transfer — on append success, clear the slot.  On
	 * append failure, xp_audio_append has already free()d the buffer,
	 * so clear the slot either way to avoid a double-free. */
	if (xp_audio_append(h, patches[slot].frames, patches[slot].nframes, &opts)) {
		/* transferred to mixer */
	}
	patches[slot].frames  = NULL;
	patches[slot].nframes = 0;
}

static void
do_flush(struct cterminal *cterm, char *str)
{
	char             *p;
	char             *end;
	unsigned long     ch          = AUDIO_APC_CHANNELS;
	size_t            fadeout     = 0;
	bool              have_fadeout = false;
	xp_audio_handle_t h;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'C' && p[2] == '=') {
			ch = strtoul(p + 3, &end, 10);
		}
		else if (p[1] == 'O' && p[2] == '=') {
			fadeout = apc_parse_duration(p + 3, 0.0, false);
			have_fadeout = true;
		}
	}
	if (ch >= AUDIO_APC_CHANNELS)
		return;
	h = channel_handle(cterm, (int)ch);
	if (h < 0)
		return;

	if (have_fadeout && fadeout > 0) {
		/* Overlay fade-out then immediate drain.  Implemented by
		 * queuing a silent crossfade buffer whose fade_in_frames is
		 * the desired fade length — that triggers the head buf's
		 * overlay_fade_out to decay over `fadeout` frames. */
		xp_audio_opts_t opts = XP_AUDIO_OPTS_INIT;
		int16_t        *silence;

		silence = (int16_t *)calloc(fadeout, XPBEEP_FRAMESIZE);
		if (silence == NULL) {
			xp_audio_clear(h);
			return;
		}
		opts.fade_in_frames = fadeout;
		opts.crossfade      = true;
		if (!xp_audio_append(h, silence, fadeout, &opts)) {
			/* append already freed silence on failure */
			xp_audio_clear(h);
			return;
		}
		/* The silent tail auto-drains; don't block the APC handler. */
	}
	else {
		xp_audio_clear(h);
	}
}

static void
do_volume(struct cterminal *cterm, char *str)
{
	char             *p;
	unsigned long     ch    = AUDIO_APC_CHANNELS;
	char             *end;
	float             vl    = 0.0f;
	float             vr    = 0.0f;
	size_t            nframes = 0;
	bool              have_l  = false;
	bool              have_r  = false;
	xp_audio_handle_t h;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'C' && p[2] == '=') {
			ch = strtoul(p + 3, &end, 10);
		}
		else if (p[1] == 'V' && p[2] == 'L' && p[3] == '=') {
			vl = apc_parse_volume(p + 4);
			have_l = true;
		}
		else if (p[1] == 'V' && p[2] == 'R' && p[3] == '=') {
			vr = apc_parse_volume(p + 4);
			have_r = true;
		}
		else if (p[1] == 'V' && p[2] == '=') {
			float v = apc_parse_volume(p + 3);
			vl = v;
			vr = v;
			have_l = true;
			have_r = true;
		}
		else if (p[1] == 'T' && p[2] == '=') {
			nframes = apc_parse_duration(p + 3, 0.0, false);
		}
	}
	if (ch >= AUDIO_APC_CHANNELS)
		return;
	h = channel_handle(cterm, (int)ch);
	if (h < 0)
		return;
	if (!have_l && !have_r)
		return;
	/* Fill in unspecified side from the stream's current value so a
	 * VL-only or VR-only command doesn't clobber the other channel. */
	if (!have_l || !have_r) {
		float cur_l = AUDIO_APC_BASE_DB;
		float cur_r = AUDIO_APC_BASE_DB;
		xp_audio_get_volume(h, &cur_l, &cur_r);
		if (!have_l) vl = cur_l;
		if (!have_r) vr = cur_r;
	}
	if (nframes > 0)
		xp_audio_ramp_volume(h, vl, vr, nframes);
	else
		xp_audio_set_volume(h, vl, vr);
}

static void
do_wait(struct cterminal *cterm, char *str)
{
	char             *p;
	char             *end;
	unsigned long     ch = AUDIO_APC_CHANNELS;
	xp_audio_handle_t h;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'C' && p[2] == '=')
			ch = strtoul(p + 3, &end, 10);
	}
	if (ch >= AUDIO_APC_CHANNELS)
		return;
	h = channel_handle(cterm, (int)ch);
	if (h < 0)
		return;
	/* xp_audio_drain blocks until head drains (or returns immediately if
	 * head is a looping buf — matches the "no effect on a loop" spec). */
	xp_audio_drain(h);
}

static void
do_update(char *str)
{
	char         *p;
	char         *end;
	unsigned long ch = AUDIO_APC_CHANNELS;

	for (p = str; p && *p == ';'; p = strchr(p + 1, ';')) {
		if (p[1] == 'C' && p[2] == '=')
			ch = strtoul(p + 3, &end, 10);
	}
	if (ch >= AUDIO_APC_CHANNELS)
		return;
	apc_notify[ch] = true;
}

/* ----- top-level dispatchers ----- */

void
audio_apc_handler(char *strbuf, size_t slen, char *fn, void *apcd)
{
	struct bbslist *bbs = apcd;
	extern struct cterminal *cterm;   /* from term.c */
	(void)slen;
	(void)bbs;

	init_state_once();

	/* strbuf starts at the verb, e.g. "Load;S=0;test.wav".  Find the
	 * first ';' — everything beyond is the arg list. */
	if (strncmp(strbuf, "Load", 4) == 0 && (strbuf[4] == ';' || strbuf[4] == '\0'))
		do_load(cterm, strbuf + 4, fn);
	else if (strncmp(strbuf, "Synth", 5) == 0 && (strbuf[5] == ';' || strbuf[5] == '\0'))
		do_synth(strbuf + 5);
	else if (strncmp(strbuf, "Copy", 4) == 0 && (strbuf[4] == ';' || strbuf[4] == '\0'))
		do_copy(strbuf + 4);
	else if (strncmp(strbuf, "Queue", 5) == 0 && (strbuf[5] == ';' || strbuf[5] == '\0'))
		do_queue(cterm, strbuf + 5);
	else if (strncmp(strbuf, "Flush", 5) == 0 && (strbuf[5] == ';' || strbuf[5] == '\0'))
		do_flush(cterm, strbuf + 5);
	else if (strncmp(strbuf, "Volume", 6) == 0 && (strbuf[6] == ';' || strbuf[6] == '\0'))
		do_volume(cterm, strbuf + 6);
	else if (strncmp(strbuf, "Wait", 4) == 0 && (strbuf[4] == ';' || strbuf[4] == '\0'))
		do_wait(cterm, strbuf + 4);
	else if (strncmp(strbuf, "Update", 6) == 0 && (strbuf[6] == ';' || strbuf[6] == '\0'))
		do_update(strbuf + 6);
}

void
feature_query_handler(char *strbuf, size_t slen, void *apcd)
{
	char  tmp[64];
	int   avail = 0;
	(void)slen;
	(void)apcd;

	init_state_once();
	if (strcmp(strbuf, "libsndfile") == 0) {
		avail = sndfile_available() ? 1 : 0;
		snprintf(tmp, sizeof(tmp), "\x1b[=7;%d;%dn",
		         AUDIO_APC_FEATURE_SNDFILE, avail);
		conn_send(tmp, strlen(tmp), 0);
	}
}

/* DSR builder — emits either all-channels or a single-channel state.
 * Also emits any feature-ID pairs (>=100) if the query referenced one.
 * The single-DSR / many-pairs grammar is:
 *   CSI = 7 [ ; <id> ; <state> ]... n
 */
void
audio_ext_state_7(struct cterminal *cterm, int nparams, const uint64_t *params)
{
	char tmp[512];
	size_t pos;
	int    i;
	int    target_ch = -1;        /* -1 = all-channels */

	init_state_once();
	if (nparams >= 2)
		target_ch = (int)params[1];

	pos = 0;
	pos += (size_t)snprintf(tmp + pos, sizeof(tmp) - pos, "\x1b[=7");

	if (target_ch < 0) {
		/* Query-all: one pair per running channel. */
		for (i = 0; i < AUDIO_APC_CHANNELS; i++) {
			xp_audio_handle_t h = channel_handle(cterm, i);
			if (h < 0)
				continue;
			if (xp_audio_is_idle(h))
				continue;
			pos += (size_t)snprintf(tmp + pos, sizeof(tmp) - pos, ";%d;1", i);
			if (pos >= sizeof(tmp) - 16)
				break;
		}
	}
	else if (target_ch >= 0 && target_ch < AUDIO_APC_CHANNELS) {
		xp_audio_handle_t h = channel_handle(cterm, target_ch);
		int               state = (h >= 0 && !xp_audio_is_idle(h)) ? 1 : 0;
		pos += (size_t)snprintf(tmp + pos, sizeof(tmp) - pos, ";%d;%d",
		                        target_ch, state);
	}
	pos += (size_t)snprintf(tmp + pos, sizeof(tmp) - pos, "n");
	cterm_respond(cterm, tmp, pos);
}

void
audio_apc_poll(struct cterminal *cterm)
{
	int i;
	init_state_once();
	for (i = 0; i < AUDIO_APC_CHANNELS; i++) {
		xp_audio_handle_t h;
		int               state;

		if (!apc_notify[i])
			continue;
		h = channel_handle(cterm, i);
		state = (h >= 0 && !xp_audio_is_idle(h)) ? 1 : 0;
		if (apc_last_state[i] == 1 && state == 0) {
			char tmp[32];
			int  n;
			n = snprintf(tmp, sizeof(tmp), "\x1b[=7;%d;0n", i);
			conn_send(tmp, (size_t)n, 0);
			apc_notify[i] = false;
		}
		apc_last_state[i] = state;
	}
}

void
audio_apc_cleanup(void)
{
	int i;
	for (i = 0; i < AUDIO_APC_PATCH_SLOTS; i++) {
		free(patches[i].frames);
		patches[i].frames  = NULL;
		patches[i].nframes = 0;
	}
	if (apc_channels_init) {
		for (i = 0; i < AUDIO_APC_CHANNELS - AUDIO_APC_FIRST_APC_CH; i++) {
			if (apc_channels[i] >= 0) {
				xp_audio_close(apc_channels[i]);
				xptone_close();
				apc_channels[i] = -1;
			}
			apc_notify[i + AUDIO_APC_FIRST_APC_CH] = false;
			apc_last_state[i + AUDIO_APC_FIRST_APC_CH] = 0;
		}
	}
}
