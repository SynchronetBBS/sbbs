/* probe_core.c -- report what a libretro core ACTUALLY does, before we commit
 * a console install to it.
 *
 * Every SyncRetro console so far was scoped from measurement rather than from
 * the core's documentation: M3_MULTICORE.md sec 2 is a table of what fceumm was
 * observed to do (48 kHz, not our hardcoded 44100; a CONTENT_INFO_OVERRIDE we
 * deliberately refuse; a 60.0998 fps that is not 60). That table was produced by
 * a throwaway harness. This is that harness, kept.
 *
 * It exists because the questions that decide a console's cost are all
 * empirical, and all cheap to get wrong from a distance:
 *
 *   - Does the core ask the FRONTEND to rotate the picture (env 1), or does it
 *     hand us an already-rotated buffer? Decides whether a portrait arcade game
 *     (Pac-Man, Galaga, ...) needs a rotate path in retro_bridge.c or nothing.
 *   - What geometry / fps / sample_rate does it report, and does it CHANGE them
 *     after load (env 32/37)? Our pacer and Opus encoder read these once.
 *   - Which core options does it read, and is it playable on their defaults?
 *     Answering that question here is what produced retro_options.c: MAME
 *     2003-Plus reads all 23 options it advertises and, told nothing, reports a
 *     0 Hz sample rate and emits no audio at all.
 *   - How many distinct colours does a frame really have? Under 256 the sixel
 *     quantizer is exact (README "Constraints").
 *
 * HOW IT STAYS HONEST: it links the door's real retro_core.c and real
 * retro_options.c, and its environment callback answers exactly what
 * retro_env.c answers. So the report is the door's own view of the core, not a
 * more generous frontend's -- and a probe run is a live test of the option
 * store the door ships. Each policy the door does NOT implement can be flipped
 * individually (-accept-rotation, -optver), and each one it DOES can be flipped
 * back off (-no-options), to measure the fork -- that difference IS the work
 * estimate.
 *
 * It links no termgfx, no xpdev and no libjxl: it opens no terminal, needs no
 * BBS, and is safe to run from a shell. Build with -DSYNCRETRO_PROBE=ON.
 *
 *     probe_core <core> [rom] [options]
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "retro_core.h"
#include "retro_options.h"   /* the shipped option store, under test */
#include "syncretro.h"      /* sr_bridge_install() -- ours, below */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Options
 * ------------------------------------------------------------------------ */

static struct {
	const char *core;
	const char *rom;
	const char *system_dir;
	const char *save_dir;
	const char *ppm;
	const char *wav;
	int frames;
	int accept_rotation;           /* answer env 1 true (door: false) */
	int optver;                    /* answer env 52 with this (<0: door's false) */
	int no_options;                /* answer GET_VARIABLE false: the pre-M5 door */
	int coin;                      /* frame to insert a coin on; 0 = never */
	int thumb;                     /* ASCII thumbnail of the last frame */
	int quiet_env;                 /* tally env calls, don't log each first-time */
	int hold;                      /* JOYPAD id to hold down, or -1 */
	int hold_from;                 /* first frame to hold it on */
} opt = { NULL, NULL, ".", ".", NULL, NULL, 300, 0, -1, 0, 0, 1, 0, -1, 500 };

static void usage(void)
{
	fputs(
		"probe_core <core.so|core.dll> [rom] [options]\n"
		"\n"
		"  -frames N          run N frames after load (default 300)\n"
		"  -system <dir>      answer GET_SYSTEM_DIRECTORY with this (default \".\")\n"
		"  -save <dir>        answer GET_SAVE_DIRECTORY with this (default \".\")\n"
		"  -accept-rotation   answer SET_ROTATION true; the door answers false\n"
		"  -optver N          answer GET_CORE_OPTIONS_VERSION with N (0|1|2);\n"
		"                     the door answers false, which means version 0\n"
		"  -no-options        answer GET_VARIABLE false, as the door did before\n"
		"                     retro_options.c existed -- for measuring what the\n"
		"                     option store is worth against a NEW core\n"
		"  -set key=value     pin one option (repeatable): the door's -option\n"
		"  -coin [N]          hold SELECT at frame N (default 60) and START at\n"
		"                     N+90: insert a coin, start a 1-player arcade game.\n"
		"                     A coin before the driver has booted is swallowed --\n"
		"                     MAME 2003-Plus needs N of several hundred\n"
		"  -hold N            hold RetroPad id N (0-15) from -hold-from onward:\n"
		"                     which BUTTON is this cabinet's fire? MAME 2003-Plus\n"
		"                     sends no SET_INPUT_DESCRIPTORS, so the only way to\n"
		"                     find out is to press one and look at the frame\n"
		"  -hold-from N       first frame to hold it on (default 500). Must be\n"
		"                     after -coin's start, or you measure attract mode\n"
		"  -ppm <file>        write the last frame as a binary PPM\n"
		"  -wav <file>        capture the core's PCM as a 16-bit stereo WAV --\n"
		"                     what the door would encode and stream\n"
		"  -no-thumb          suppress the ASCII thumbnail of the last frame\n"
		"  -quiet-env         only the environment tally, not each first call\n",
		stderr);
}

/* ---------------------------------------------------------------------------
 * Environment-call tally
 *
 * Cores poll some env calls EVERY FRAME (fceumm does env 17 and env 40; a MAME
 * core polls GET_VARIABLE far more than that). Logging each one would bury the
 * handful that matter under thousands of lines, and suppressing them would hide
 * that the polling is happening at all -- so log each cmd the first time and
 * count the rest.
 * ------------------------------------------------------------------------ */

typedef struct {
	unsigned cmd;
	unsigned long count;
	int answered;
} env_tally_t;

#define MAX_TALLY 128
static env_tally_t g_tally[MAX_TALLY];
static int         g_ntally;

static const char *env_name(unsigned cmd)
{
	switch (cmd & ~(unsigned)RETRO_ENVIRONMENT_EXPERIMENTAL) {
		case 1:  return "SET_ROTATION";
		case 2:  return "GET_OVERSCAN";
		case 3:  return "GET_CAN_DUPE";
		case 6:  return "SET_MESSAGE";
		case 8:  return "SET_PERFORMANCE_LEVEL";
		case 9:  return "GET_SYSTEM_DIRECTORY";
		case 10: return "SET_PIXEL_FORMAT";
		case 11: return "SET_INPUT_DESCRIPTORS";
		case 12: return "SET_KEYBOARD_CALLBACK";
		case 13: return "SET_DISK_CONTROL_INTERFACE";
		case 14: return "SET_HW_RENDER";
		case 15: return "GET_VARIABLE";
		case 16: return "SET_VARIABLES";
		case 17: return "GET_VARIABLE_UPDATE";
		case 18: return "SET_SUPPORT_NO_GAME";
		case 19: return "GET_LIBRETRO_PATH";
		case 21: return "SET_FRAME_TIME_CALLBACK";
		case 22: return "SET_AUDIO_CALLBACK";
		case 23: return "GET_RUMBLE_INTERFACE";
		case 24: return "GET_INPUT_DEVICE_CAPABILITIES";
		case 27: return "GET_LOG_INTERFACE";
		case 28: return "GET_PERF_INTERFACE";
		case 30: return "GET_CORE_ASSETS_DIRECTORY";
		case 31: return "GET_SAVE_DIRECTORY";
		case 32: return "SET_SYSTEM_AV_INFO";
		case 33: return "SET_PROC_ADDRESS_CALLBACK";
		case 34: return "SET_SUBSYSTEM_INFO";
		case 35: return "SET_CONTROLLER_INFO";
		case 36: return "SET_MEMORY_MAPS";
		case 37: return "SET_GEOMETRY";
		case 38: return "GET_USERNAME";
		case 39: return "GET_LANGUAGE";
		case 40: return "GET_CURRENT_SOFTWARE_FRAMEBUFFER";
		case 41: return "GET_HW_RENDER_INTERFACE";
		case 42: return "SET_SUPPORT_ACHIEVEMENTS";
		case 44: return "SET_SERIALIZATION_QUIRKS";
		case 45: return "GET_INPUT_BITMASKS";
		case 46: return "GET_VFS_INTERFACE";
		case 47: return "GET_LED_INTERFACE";
		case 48: return "GET_AUDIO_VIDEO_ENABLE";
		case 50: return "GET_FASTFORWARDING";
		case 52: return "GET_CORE_OPTIONS_VERSION";
		case 53: return "SET_CORE_OPTIONS";
		case 54: return "SET_CORE_OPTIONS_INTL";
		case 55: return "SET_CORE_OPTIONS_DISPLAY";
		case 57: return "GET_DISK_CONTROL_INTERFACE_VERSION";
		case 58: return "SET_DISK_CONTROL_EXT_INTERFACE";
		case 59: return "GET_MESSAGE_INTERFACE_VERSION";
		case 60: return "SET_MESSAGE_EXT";
		case 62: return "SET_AUDIO_BUFFER_STATUS_CALLBACK";
		case 63: return "SET_MINIMUM_AUDIO_LATENCY";
		case 64: return "SET_FASTFORWARDING_OVERRIDE";
		case 65: return "SET_CONTENT_INFO_OVERRIDE";
		case 66: return "GET_GAME_INFO_EXT";
		case 67: return "SET_CORE_OPTIONS_V2";
		case 68: return "SET_CORE_OPTIONS_V2_INTL";
		case 69: return "SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK";
		case 70: return "SET_VARIABLE";
		case 77: return "GET_DEVICE_POWER";
		default: return "?";
	}
}

/* Record the call. Returns 1 the first time this cmd is seen, so the caller can
 * log it once. */
static int env_seen(unsigned cmd, int answered)
{
	int i;

	for (i = 0; i < g_ntally; i++) {
		if (g_tally[i].cmd == cmd) {
			g_tally[i].count++;
			return 0;
		}
	}
	if (g_ntally >= MAX_TALLY)
		return 0;
	g_tally[g_ntally].cmd      = cmd;
	g_tally[g_ntally].count    = 1;
	g_tally[g_ntally].answered = answered;
	g_ntally++;
	return 1;
}

/* ---------------------------------------------------------------------------
 * What the run observed
 * ------------------------------------------------------------------------ */

static struct {
	int pixfmt;                      /* RETRO_PIXEL_FORMAT_*; -1 = never set */
	int rotation;                    /* quarter-turns CCW the core asked for; -1 none */
	int rotation_frame;              /* the frame it asked on (-1 = before run) */
	int can_dupe_asked;
	unsigned long dupe_frames;       /* video_refresh with data == NULL */
	unsigned long video_frames;
	unsigned long audio_frames;      /* sample FRAMES (a stereo pair is one) */
	int av_reset;                    /* SET_SYSTEM_AV_INFO / SET_GEOMETRY count */
	unsigned new_w, new_h;           /* what it tried to change them TO */
	float new_aspect;
	double new_fps, new_rate;
	unsigned w, h;                   /* last frame */
	uint8_t * rgb;                   /* last frame as RGB24, w*h*3 */
	size_t rgb_cap;
	/* Audio LEVEL, not just the frame count. "Is this game silent?" has three
	 * different answers -- digital silence, a constant non-zero DC level, and
	 * real quiet sound -- which sound identical in a frame count and do NOT
	 * behave identically once chunked and encoded. See the audio report. */
	double audio_sum_l, audio_sum_r;          /* running sums -> the DC mean */
	int audio_min, audio_max;                 /* peak excursion, both channels */
	unsigned long audio_zero;                 /* frames with l == 0 && r == 0 */
	FILE * wav;                               /* -wav capture, or NULL */
	unsigned long wav_frames;
} obs = { -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0, 0.0, 0, 0, NULL, 0,
	      0.0, 0.0, 32767, -32768, 0, NULL, 0 };

/* Distinct geometries, in the order first seen -- a core that resizes mid-run
 * (arcade cores do, between service mode / attract / play) makes the ONE
 * av_info read at load a lie, which is exactly the bug we are hunting. */
typedef struct { unsigned w, h; size_t pitch; unsigned long first; } geom_t;
static geom_t g_geom[16];
static int    g_ngeom;

static void geom_seen(unsigned w, unsigned h, size_t pitch, unsigned long frame)
{
	int i;

	for (i = 0; i < g_ngeom; i++)
		if (g_geom[i].w == w && g_geom[i].h == h && g_geom[i].pitch == pitch)
			return;
	if (g_ngeom >= (int)(sizeof g_geom / sizeof g_geom[0]))
		return;
	g_geom[g_ngeom].w     = w;
	g_geom[g_ngeom].h     = h;
	g_geom[g_ngeom].pitch = pitch;
	g_geom[g_ngeom].first = frame;
	g_ngeom++;
}

/* ---------------------------------------------------------------------------
 * The libretro callbacks
 * ------------------------------------------------------------------------ */

static void probe_log(enum retro_log_level level, const char *fmt, ...)
{
	static const char *tag[] = { "dbg", "inf", "wrn", "ERR" };
	va_list            ap;

	fprintf(stderr, "  [core %s] ", (unsigned)level < 4 ? tag[level] : "?");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void dump_input_descriptors(const struct retro_input_descriptor *d)
{
	/* The M2/M3 bind tables were hand-written, and M3_MULTICORE.md sec 4 records
	 * WHY they are not derived from these. They are still the fastest way to
	 * learn what a new core reads -- read them, then decide by hand. */
	printf("  input descriptors:\n");
	for (; d != NULL && d->description != NULL; d++) {
		printf("    port %u device %u index %u id %2u : %s\n",
		       d->port, d->device, d->index, d->id, d->description);
	}
}

static void dump_controller_info(const struct retro_controller_info *ci)
{
	unsigned i;

	printf("  controller info:\n");
	for (; ci != NULL && ci->types != NULL; ci++) {
		for (i = 0; i < ci->num_types; i++)
			printf("    device %u : %s\n", ci->types[i].id,
			       ci->types[i].desc ? ci->types[i].desc : "");
		printf("    --\n");
	}
}

/* --- core options: the door's OWN store, under test ------------------------
 *
 * The probe does not keep an option table of its own. It calls the shipped
 * retro_options.c -- ro_declare_*() to record what the core advertises,
 * ro_option_get() to answer a query, sr_option_pin() for -set -- so a probe run
 * against a real core IS a test of the code the door ships. A private copy
 * would only ever prove that the copy works.
 *
 * What stays duplicated here, deliberately, is the environment DISPATCH: the
 * probe needs to log every call and to flip individual policies (-no-options),
 * which is exactly what retro_env.c must not do.
 */

/* SET_VARIABLES (options version 0): value is "description; a|b|c", and the
 * FIRST listed value is the default. */
static void dump_variables(const struct retro_variable *v)
{
	printf("  core options (v0 / SET_VARIABLES)   [default first]:\n");
	for (; v != NULL && v->key != NULL; v++)
		printf("    %-34s %s\n", v->key, v->value ? v->value : "");
}

static void dump_option_values(const struct retro_core_option_value *val,
                               const char *dflt)
{
	int i;

	for (i = 0; val[i].value != NULL && i < RETRO_NUM_CORE_OPTION_VALUES_MAX; i++) {
		printf("%s%s%s", i ? "|" : "", val[i].value,
		       (dflt != NULL && strcmp(val[i].value, dflt) == 0) ? "*" : "");
	}
	printf("\n");
}

static void dump_options_v1(const struct retro_core_option_definition *d)
{
	printf("  core options (v1 / SET_CORE_OPTIONS)   [* = default]:\n");
	for (; d != NULL && d->key != NULL; d++) {
		printf("    %-34s ", d->key);
		dump_option_values(d->values, d->default_value);
	}
}

static void dump_options_v2(const struct retro_core_options_v2 *o)
{
	const struct retro_core_option_v2_definition *d;

	printf("  core options (v2 / SET_CORE_OPTIONS_V2)   [* = default]:\n");
	if (o == NULL)
		return;
	for (d = o->definitions; d != NULL && d->key != NULL; d++) {
		printf("    %-34s ", d->key);
		dump_option_values(d->values, d->default_value);
	}
}

bool sr_environment(unsigned cmd, void *data);   /* the door's name for it */

bool sr_environment(unsigned cmd, void *data)
{
	int  first;
	bool ret = false;

	switch (cmd) {

		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
			obs.pixfmt = (int)*(const enum retro_pixel_format *)data;
			ret = true;
			break;

		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
			*(const char **)data = opt.system_dir;
			ret = true;
			break;

		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
			*(const char **)data = opt.save_dir;
			ret = true;
			break;

		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			((struct retro_log_callback *)data)->log = probe_log;
			ret = true;
			break;

		case RETRO_ENVIRONMENT_SET_HW_RENDER:
			ret = false;              /* software cores only, as the door does */
			break;

		/* THE QUESTION, for a portrait arcade game. `data` is quarter-turns
		 * COUNTER-clockwise (1 = 90 CCW). Answering false is the door's current
		 * behaviour and tells the core we cannot rotate -- some cores then rotate
		 * into their own framebuffer, some just draw sideways. Run it both ways. */
		case RETRO_ENVIRONMENT_SET_ROTATION:
			obs.rotation       = (int)*(const unsigned *)data;
			obs.rotation_frame = (int)obs.video_frames;
			ret = opt.accept_rotation ? true : false;
			break;

		case RETRO_ENVIRONMENT_GET_CAN_DUPE:
			obs.can_dupe_asked = 1;
			ret = false;              /* the door's default: never answered */
			break;

		/* A core that resizes or re-times itself AFTER load. The door reads
		 * av_info exactly once (main.c) and would keep the stale numbers, so
		 * record what it tried to change TO, not just that it did. */
		case RETRO_ENVIRONMENT_SET_GEOMETRY: {
			const struct retro_game_geometry *g = data;
			obs.av_reset++;
			obs.new_w      = g->base_width;
			obs.new_h      = g->base_height;
			obs.new_aspect = g->aspect_ratio;
			ret = false;
			break;
		}

		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
			const struct retro_system_av_info *a = data;
			obs.av_reset++;
			obs.new_w      = a->geometry.base_width;
			obs.new_h      = a->geometry.base_height;
			obs.new_aspect = a->geometry.aspect_ratio;
			obs.new_fps    = a->timing.fps;
			obs.new_rate   = a->timing.sample_rate;
			ret = false;
			break;
		}

		/* Routed through the SHIPPED store (retro_options.c), exactly as
		 * retro_env.c does. -no-options reproduces the pre-M5 behaviour --
		 * answer false to everything -- which is how the cost of the store gets
		 * re-measured against a new core rather than assumed. */
		case RETRO_ENVIRONMENT_GET_VARIABLE: {
			struct retro_variable *v = data;
			const char *           val;

			if (v == NULL || v->key == NULL || opt.no_options)
				break;
			if ((val = ro_option_get(v->key)) == NULL)
				break;
			v->value = val;
			ret      = true;
			break;
		}

		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			if (opt.no_options)
				break;
			*(bool *)data = ro_options_changed() ? true : false;
			ret = true;
			break;

		/* Declaration goes to the store no matter what -no-options says: the
		 * dump below is the report, and a store that never heard the keys could
		 * not answer even when asked to. */
		case RETRO_ENVIRONMENT_SET_VARIABLES:
			ro_declare_v0(data);
			ret = !opt.no_options;
			break;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
			ro_declare_v1(data);
			ret = !opt.no_options;
			break;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
			ro_declare_v1(((const struct retro_core_options_intl *)data)->us);
			ret = !opt.no_options;
			break;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
			ro_declare_v2(data);
			ret = !opt.no_options;
			break;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
			ro_declare_v2(((const struct retro_core_options_v2_intl *)data)->us);
			ret = !opt.no_options;
			break;

		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
			if (opt.optver >= 0) {
				*(unsigned *)data = (unsigned)opt.optver;
				ret = true;
			}
			break;

		default:
			ret = false;
			break;
	}

	first = env_seen(cmd, ret);

	/* Dump the payload of the informative calls, once. These are the ones whose
	 * CONTENT is the answer we came for -- everything else is just a count. */
	if (first) {
		switch (cmd) {
			case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
				dump_input_descriptors(data);
				break;
			case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
				dump_controller_info(data);
				break;
			case RETRO_ENVIRONMENT_SET_VARIABLES:
				dump_variables(data);
				break;
			case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
				dump_options_v1(data);
				break;
			case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
				dump_options_v1(((const struct retro_core_options_intl *)data)->us);
				break;
			case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
				dump_options_v2(data);
				break;
			case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
				dump_options_v2(((const struct retro_core_options_v2_intl *)data)->us);
				break;
			default:
				break;
		}
		if (!opt.quiet_env) {
			printf("  env %-3u %-40s -> %s\n", cmd, env_name(cmd),
			       ret ? "true" : "false");
		}
	}
	return ret;
}

/* --- video ---------------------------------------------------------------- */

/* One source pixel -> 8-bit RGB. The three formats libretro defines; the door's
 * retro_bridge.c converts the same three. */
static void px_rgb(const uint8_t *p, int fmt, uint8_t *out)
{
	uint16_t v;

	if (fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
		out[0] = p[2];
		out[1] = p[1];
		out[2] = p[0];
		return;
	}
	memcpy(&v, p, sizeof v);
	if (fmt == RETRO_PIXEL_FORMAT_RGB565) {
		out[0] = (uint8_t)(((v >> 11) & 0x1f) * 255 / 31);
		out[1] = (uint8_t)(((v >>  5) & 0x3f) * 255 / 63);
		out[2] = (uint8_t)((v        & 0x1f) * 255 / 31);
	} else {                                          /* 0RGB1555, the default */
		out[0] = (uint8_t)(((v >> 10) & 0x1f) * 255 / 31);
		out[1] = (uint8_t)(((v >>  5) & 0x1f) * 255 / 31);
		out[2] = (uint8_t)((v        & 0x1f) * 255 / 31);
	}
}

static void probe_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
	const uint8_t *src = data;
	size_t         need, bpp;
	unsigned       x, y;

	obs.video_frames++;
	if (data == NULL) {                 /* a dupe frame: "repeat the last one" */
		obs.dupe_frames++;
		return;
	}
	geom_seen(w, h, pitch, obs.video_frames);

	/* Keep the frame, converted, because the pointer dies with this call. */
	need = (size_t)w * h * 3;
	if (need > obs.rgb_cap) {
		uint8_t *grown = realloc(obs.rgb, need);
		if (grown == NULL)
			return;
		obs.rgb     = grown;
		obs.rgb_cap = need;
	}
	bpp = (obs.pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) ? 4 : 2;
	for (y = 0; y < h; y++) {
		const uint8_t *row = src + (size_t)y * pitch;
		for (x = 0; x < w; x++)
			px_rgb(row + x * bpp, obs.pixfmt, obs.rgb + ((size_t)y * w + x) * 3);
	}
	obs.w = w;
	obs.h = h;
}

/* --- audio ---------------------------------------------------------------- */

/* Little-endian stores: a WAV is LE regardless of the host, and the int16
 * samples libretro hands us are host-endian. Writing them byte-wise keeps the
 * capture correct on a big-endian host instead of accidentally byte-swapped. */
static void put16le(uint8_t *p, unsigned v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void put32le(uint8_t *p, unsigned long v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

/* 44-byte canonical WAV header: 16-bit stereo PCM at `rate`. */
static void wav_header(uint8_t *h, unsigned long rate, unsigned long data_bytes)
{
	memcpy(h, "RIFF", 4);
	put32le(h + 4, 36 + data_bytes);
	memcpy(h + 8, "WAVEfmt ", 8);
	put32le(h + 16, 16);            /* fmt chunk size */
	put16le(h + 20, 1);             /* PCM */
	put16le(h + 22, 2);             /* channels */
	put32le(h + 24, rate);
	put32le(h + 28, rate * 4);      /* byte rate: rate * channels * 2 */
	put16le(h + 32, 4);             /* block align */
	put16le(h + 34, 16);            /* bits per sample */
	memcpy(h + 36, "data", 4);
	put32le(h + 40, data_bytes);
}

static void wav_open(const char *path, unsigned long rate)
{
	uint8_t h[44];

	obs.wav = fopen(path, "wb");
	if (obs.wav == NULL) {
		fprintf(stderr, "probe_core: cannot write '%s'\n", path);
		return;
	}
	wav_header(h, rate, 0);
	fwrite(h, 1, sizeof h, obs.wav);
}

static void wav_close(unsigned long rate)
{
	uint8_t h[44];

	if (obs.wav == NULL)
		return;
	wav_header(h, rate, obs.wav_frames * 4);
	if (fseek(obs.wav, 0, SEEK_SET) == 0)
		fwrite(h, 1, sizeof h, obs.wav);
	fclose(obs.wav);
	obs.wav = NULL;
}

/* Every frame goes through here, from either callback. */
static void audio_tally(int16_t l, int16_t r)
{
	obs.audio_frames++;
	obs.audio_sum_l += l;
	obs.audio_sum_r += r;
	if (l < obs.audio_min)
		obs.audio_min = l;
	if (r < obs.audio_min)
		obs.audio_min = r;
	if (l > obs.audio_max)
		obs.audio_max = l;
	if (r > obs.audio_max)
		obs.audio_max = r;
	if (l == 0 && r == 0)
		obs.audio_zero++;

	if (obs.wav != NULL) {
		uint8_t s[4];

		put16le(s, (unsigned)(uint16_t)l);
		put16le(s + 2, (unsigned)(uint16_t)r);
		fwrite(s, 1, sizeof s, obs.wav);
		obs.wav_frames++;
	}
}

static void probe_audio_sample(int16_t l, int16_t r)
{
	audio_tally(l, r);
}

static size_t probe_audio_batch(const int16_t *data, size_t frames)
{
	size_t i;

	for (i = 0; i < frames; i++)
		audio_tally(data[i * 2], data[i * 2 + 1]);
	return frames;
}

/* --- input ---------------------------------------------------------------- */

/* No keyboard here. -coin drives the two buttons an arcade core needs to leave
 * attract mode: SELECT is the coin slot and START is the 1P button in every
 * MAME/FBNeo RetroPad mapping. Ten frames is ~1/6 s, long enough for any core's
 * edge detector and short enough not to insert two credits.
 *
 * The frame is an ARGUMENT, not a constant: a coin inserted before the driver
 * has finished booting is swallowed, and how long that takes is per-core (MAME
 * 2003-Plus spends several hundred frames on its disclaimer and ROM checks).
 * Getting this wrong looks exactly like a core that ignores input. */
static int16_t probe_input(unsigned port, unsigned device, unsigned index, unsigned id)
{
	long f = (long)obs.video_frames;

	(void)index;
	if (opt.coin <= 0 || port != 0 || device != RETRO_DEVICE_JOYPAD)
		return 0;
	/* A button held to the end of the run, so the captured frame shows what it
	 * did. The frame threshold matters: a cabinet ignores every button until a
	 * credit is in and the game has started, and a hold that begins before that
	 * measures the attract mode -- which looks exactly like a dead button. */
	if (opt.hold >= 0 && (int)id == opt.hold && f >= opt.hold_from)
		return 1;
	if (id == RETRO_DEVICE_ID_JOYPAD_SELECT && f >= opt.coin && f < opt.coin + 10)
		return 1;
	if (id == RETRO_DEVICE_ID_JOYPAD_START && f >= opt.coin + 90 && f < opt.coin + 100)
		return 1;
	return 0;
}

static void probe_input_poll(void) { }

/* retro_core.c calls this instead of the real retro_bridge.c's. Same contract:
 * wire all six callbacks before retro_init(). */
void sr_bridge_install(struct rc_core *c)
{
	c->set_environment(sr_environment);
	c->set_video_refresh(probe_video);
	c->set_audio_sample(probe_audio_sample);
	c->set_audio_sample_batch(probe_audio_batch);
	c->set_input_poll(probe_input_poll);
	c->set_input_state(probe_input);
}

/* ---------------------------------------------------------------------------
 * Reporting the last frame
 * ------------------------------------------------------------------------ */

/* Exact count of distinct RGB triples, by open addressing. README's "Constraints"
 * claims a legacy console lands well under 256 so the sixel quantization is
 * exact; this is how that gets checked for a new console rather than assumed. */
#define CSET_BITS 15
#define CSET_SIZE (1u << CSET_BITS)

static unsigned distinct_colors(const uint8_t *rgb, size_t px, int *over)
{
	uint32_t *set = calloc(CSET_SIZE, sizeof *set);
	unsigned  n   = 0;
	size_t    i;

	*over = 0;
	if (set == NULL)
		return 0;
	for (i = 0; i < px; i++) {
		uint32_t key = ((uint32_t)rgb[i * 3] << 16) | ((uint32_t)rgb[i * 3 + 1] << 8)
		               | rgb[i * 3 + 2];
		uint32_t h   = (key * 2654435761u) >> (32 - CSET_BITS);

		/* Slot 0 of `set` cannot distinguish "empty" from "pure black", so the
		 * stored value is key+1 and 0 stays the empty marker. */
		while (set[h] != 0 && set[h] != key + 1)
			h = (h + 1) & (CSET_SIZE - 1);
		if (set[h] == 0) {
			if (n >= CSET_SIZE / 2) {     /* keep the table under half full */
				*over = 1;
				break;
			}
			set[h] = key + 1;
			n++;
		}
	}
	free(set);
	return n;
}

/* An ASCII thumbnail, because "is the maze upright or on its side?" is a
 * question about the PICTURE, and a geometry line cannot answer it. Point
 * sampling (not averaging) keeps thin sprites visible at this scale. */
static void print_thumb(const uint8_t *rgb, unsigned w, unsigned h)
{
	static const char ramp[] = " .:-=+*#%@";
	const unsigned    cols   = 64;
	unsigned          rows, cx, cy;

	if (w == 0 || h == 0)
		return;
	/* Terminal cells are about twice as tall as wide, so halve the row count to
	 * keep the aspect honest -- a portrait frame must LOOK portrait here. */
	rows = (unsigned)((double)h / w * cols / 2.0 + 0.5);
	if (rows < 1)
		rows = 1;
	if (rows > 120)
		rows = 120;

	printf("  last frame, %ux%u:\n", w, h);
	for (cy = 0; cy < rows; cy++) {
		printf("    |");
		for (cx = 0; cx < cols; cx++) {
			unsigned       sx = cx * w / cols, sy = cy * h / rows;
			const uint8_t *p  = rgb + ((size_t)sy * w + sx) * 3;
			int            lum = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;

			putchar(ramp[lum * (int)(sizeof ramp - 2) / 255]);
		}
		printf("|\n");
	}
}

static int write_ppm(const char *path, const uint8_t *rgb, unsigned w, unsigned h)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL) {
		fprintf(stderr, "probe_core: cannot write '%s'\n", path);
		return -1;
	}
	fprintf(f, "P6\n%u %u\n255\n", w, h);
	fwrite(rgb, 3, (size_t)w * h, f);
	fclose(f);
	printf("  wrote %s (%ux%u PPM)\n", path, w, h);
	return 0;
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------ */

static const char *pixfmt_name(int fmt)
{
	switch (fmt) {
		case RETRO_PIXEL_FORMAT_0RGB1555: return "0RGB1555";
		case RETRO_PIXEL_FORMAT_XRGB8888: return "XRGB8888";
		case RETRO_PIXEL_FORMAT_RGB565:   return "RGB565";
		default:                          return "(never set -> 0RGB1555)";
	}
}

static int parse_args(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (a[0] != '-') {
			if (opt.core == NULL)
				opt.core = a;
			else if (opt.rom == NULL)
				opt.rom = a;
			else
				return -1;
		} else if (strcmp(a, "-frames") == 0 && i + 1 < argc) {
			opt.frames = atoi(argv[++i]);
		} else if (strcmp(a, "-system") == 0 && i + 1 < argc) {
			opt.system_dir = argv[++i];
		} else if (strcmp(a, "-save") == 0 && i + 1 < argc) {
			opt.save_dir = argv[++i];
		} else if (strcmp(a, "-ppm") == 0 && i + 1 < argc) {
			opt.ppm = argv[++i];
		} else if (strcmp(a, "-wav") == 0 && i + 1 < argc) {
			opt.wav = argv[++i];
		} else if (strcmp(a, "-optver") == 0 && i + 1 < argc) {
			opt.optver = atoi(argv[++i]);
		} else if (strcmp(a, "-no-options") == 0) {
			opt.no_options = 1;
		} else if (strcmp(a, "-set") == 0 && i + 1 < argc) {
			sr_option_pin(argv[++i]);
		} else if (strcmp(a, "-accept-rotation") == 0) {
			opt.accept_rotation = 1;
		} else if (strcmp(a, "-coin") == 0) {
			opt.coin = (i + 1 < argc && argv[i + 1][0] != '-') ? atoi(argv[++i]) : 60;
		} else if (strcmp(a, "-hold") == 0 && i + 1 < argc) {
			opt.hold = atoi(argv[++i]);
		} else if (strcmp(a, "-hold-from") == 0 && i + 1 < argc) {
			opt.hold_from = atoi(argv[++i]);
		} else if (strcmp(a, "-no-thumb") == 0) {
			opt.thumb = 0;
		} else if (strcmp(a, "-quiet-env") == 0) {
			opt.quiet_env = 1;
		} else {
			return -1;
		}
	}
	return opt.core != NULL ? 0 : -1;
}

int main(int argc, char **argv)
{
	rc_core_t c;
	int       i, over = 0;
	unsigned  colors;

	if (parse_args(argc, argv) != 0) {
		usage();
		return 1;
	}
	if (opt.frames < 0)
		opt.frames = 0;

	/* Interleave our report with the core's own log, which arrives on stderr.
	 * UNBUFFERED, not line-buffered: MSVC's CRT rejects a zero size for any
	 * BUFFERED mode and its invalid-parameter handler then kills the process
	 * outright (0xC0000409 FAST_FAIL_INVALID_ARG, before a single byte of
	 * output) -- glibc accepts the same call. _IONBF ignores buffer and size on
	 * both. */
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("== probe: core '%s', rom '%s'\n", opt.core, opt.rom ? opt.rom : "(none)");
	printf("   SET_ROTATION %s; GET_CORE_OPTIONS_VERSION %s; GET_VARIABLE %s\n",
	       opt.accept_rotation ? "TRUE (not the door's behaviour)" : "false (as the door does)",
	       opt.optver >= 0 ? "answered with a version" : "false (as the door does)",
	       opt.no_options ? "false (the PRE-M5 door, for comparison)"
	                      : "answered from retro_options.c (as the door does)");

	if (rc_core_open(&c, opt.core) != 0) {
		fprintf(stderr, "probe_core: %s\n", rc_core_error());
		return 1;
	}

	printf("\n-- load --------------------------------------------------------\n");
	if (rc_core_load_game(&c, opt.rom) != 0) {
		fprintf(stderr, "probe_core: %s\n", rc_core_error());
		rc_core_close(&c);
		return 1;
	}

	printf("\n-- system ------------------------------------------------------\n");
	printf("  library          %s %s\n",
	       c.si.library_name ? c.si.library_name : "(null)",
	       c.si.library_version ? c.si.library_version : "");
	printf("  valid_extensions %s\n",
	       c.si.valid_extensions ? c.si.valid_extensions : "(null)");
	printf("  need_fullpath    %s\n", c.si.need_fullpath ? "true" : "false");
	printf("  block_extract    %s   <- true means the core opens the .zip ITSELF\n",
	       c.si.block_extract ? "true" : "false");
	printf("  geometry         %ux%u (max %ux%u), aspect %.4f\n",
	       c.av.geometry.base_width, c.av.geometry.base_height,
	       c.av.geometry.max_width, c.av.geometry.max_height,
	       c.av.geometry.aspect_ratio);
	printf("  fps              %.4f\n", c.av.timing.fps);
	printf("  sample_rate      %.1f\n", c.av.timing.sample_rate);
	printf("  pixel format     %s\n", pixfmt_name(obs.pixfmt));

	if (opt.wav != NULL)
		wav_open(opt.wav, (unsigned long)c.av.timing.sample_rate);

	printf("\n-- run %d frames -----------------------------------------------\n",
	       opt.frames);
	for (i = 0; i < opt.frames; i++)
		c.run();

	printf("\n-- observed ----------------------------------------------------\n");
	printf("  video frames     %lu (%lu of them dupes/NULL)\n",
	       obs.video_frames, obs.dupe_frames);
	printf("  audio frames     %lu = %.1f per video frame (av says %.1f)\n",
	       obs.audio_frames,
	       obs.video_frames ? (double)obs.audio_frames / obs.video_frames : 0.0,
	       c.av.timing.fps > 0 ? c.av.timing.sample_rate / c.av.timing.fps : 0.0);
	/* The level, not just the count. A game that "makes no sound yet" can be
	 * emitting digital silence (zero bytes on the wire: the door replays a
	 * cached silent chunk) or a constant non-zero DC level (NOT silence: every
	 * chunk is encoded and sent, and each chunk boundary is a step). Those two
	 * are indistinguishable in a frame count and audibly different in a door. */
	if (obs.audio_frames) {
		double n = (double)obs.audio_frames;

		printf("  audio level      DC mean %.1f / %.1f (L/R), peak %d..%d,\n"
		       "                   %lu of %lu frames digitally silent (%.1f%%)%s\n",
		       obs.audio_sum_l / n, obs.audio_sum_r / n,
		       obs.audio_min, obs.audio_max,
		       obs.audio_zero, obs.audio_frames,
		       100.0 * (double)obs.audio_zero / n,
		       obs.audio_zero == obs.audio_frames ? "   <- all silence" : "");
	}
	if (obs.wav != NULL) {
		wav_close((unsigned long)c.av.timing.sample_rate);
		printf("  wrote %s (%lu frames, 16-bit stereo)\n", opt.wav, obs.wav_frames);
	}
	printf("  geometries seen  %d\n", g_ngeom);
	for (i = 0; i < g_ngeom; i++) {
		printf("    %ux%u pitch %lu, from frame %lu%s\n",
		       g_geom[i].w, g_geom[i].h, (unsigned long)g_geom[i].pitch,
		       g_geom[i].first, g_geom[i].w < g_geom[i].h ? "   (PORTRAIT)" : "");
	}
	if (obs.av_reset) {
		printf("  !! the core reset its AV info/geometry %d time(s) after load,\n"
		       "     to %ux%u aspect %.4f", obs.av_reset, obs.new_w, obs.new_h,
		       obs.new_aspect);
		if (obs.new_fps > 0.0)
			printf(", fps %.4f, rate %.1f", obs.new_fps, obs.new_rate);
		printf(";\n     the door reads av_info ONCE (main.c) and would not follow\n");
	}
	/* Rotation 0 is a core POLITELY SAYING "upright", which several do at load.
	 * Only a non-zero quarter-turn is work for us. Reporting 0 as an alarm would
	 * cost a day chasing a rotate path nobody needs. */
	if (obs.rotation > 0)
		printf("  !! SET_ROTATION %d (%d degrees CCW) at frame %d -- the door\n"
		       "     must rotate the frame itself, or the picture is sideways\n",
		       obs.rotation, obs.rotation * 90, obs.rotation_frame);
	else if (obs.rotation == 0)
		printf("  SET_ROTATION     0 -- the core explicitly asked for NO rotation;\n"
		       "                   its buffer is already the way the player sees it\n");
	else
		printf("  SET_ROTATION     never called -- the core's buffer is already\n"
		       "                   in the orientation the player should see\n");
	if (obs.can_dupe_asked)
		printf("  GET_CAN_DUPE     asked (we said false: the core must always draw)\n");

	if (obs.rgb != NULL && obs.w && obs.h) {
		colors = distinct_colors(obs.rgb, (size_t)obs.w * obs.h, &over);
		printf("  distinct colours %u%s in the last frame -> sixel quantization %s\n",
		       colors, over ? "+ (counter full)" : "",
		       (!over && colors <= 256) ? "is EXACT" : "LOSES colour");
		if (opt.thumb)
			print_thumb(obs.rgb, obs.w, obs.h);
		if (opt.ppm != NULL)
			write_ppm(opt.ppm, obs.rgb, obs.w, obs.h);
	} else {
		printf("  !! the core produced no frame at all\n");
	}

	/* The store's own summary -- counts, plus a warning for any -set that matched
	 * nothing. Printed by the SHIPPED sr_options_report(), so the warning a sysop
	 * would find in the door log is the one that shows up here.
	 *
	 * The per-key detail above it is deliberately gone: the store owns which keys
	 * were read, and a second copy of that bookkeeping in the probe could only
	 * ever disagree with the thing it is meant to be testing. */
	printf("\n-- core options ------------------------------------------------\n");
	fflush(stdout);                 /* the report goes to stderr; keep the order */
	sr_options_report();

	printf("\n-- environment calls -------------------------------------------\n");
	for (i = 0; i < g_ntally; i++) {
		printf("  %-6lu env %-3u %-42s -> %s\n", g_tally[i].count, g_tally[i].cmd,
		       env_name(g_tally[i].cmd), g_tally[i].answered ? "true" : "false");
	}
	if (g_ntally >= MAX_TALLY)
		printf("  (tally full at %d distinct commands)\n", MAX_TALLY);

	rc_core_close(&c);
	free(obs.rgb);
	printf("\n== done\n");
	return 0;
}
