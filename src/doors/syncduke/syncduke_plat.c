/*
 * syncduke_plat.c -- SyncDuke headless platform shim.
 *
 * Replaces the vendored Engine/src/display.c (the SDL platform layer): provides
 * the display.h "your driver implements these" interface plus the platform
 * globals display.c owned, headlessly. The renderer draws into our own 8-bit
 * framebuffer (Build "classic" mode = 320x200, 1 byte/pixel); _nextpage() is the
 * capture point that later tasks turn into a sixel emit (for now it can dump a
 * PPM, env-gated, so we can eyeball what the engine renders).
 *
 * Task 3 status: _setgamemode does the real engine render-var init (the non-SDL
 * half of display.c's init_new_res_vars) and the timer drives totalclock from a
 * monotonic clock, so the intro/title sequence actually advances. Input + the
 * sixel/socket path land in Tasks 4-6.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>   /* QueryPerformanceCounter, Sleep */
#else
  #include <time.h>
#endif
#include "build.h"
#include "display.h"
#include "engine.h"   /* setview, setbrightness */
#include "draw.h"     /* setBytesPerLine */
#include "syncduke.h" /* syncduke_present + the fb/palette accessors we provide */

/* --- platform globals normally defined in display.c --- */
int32_t        xres, yres;
int32_t        bytesperline, imageSize;
int32_t        buffermode, origbuffermode, linearmode;
uint8_t        permanentupdate, vesachecked, vgacompatible;
uint8_t *      frameplace;
uint8_t *      frameoffset;
uint8_t        lastPalette[768];

/* Our 8-bit framebuffer. Build classic mode is 320x200 @ 1 byte/pixel. */
static uint8_t syncduke_screen[320 * 200];

/* Presented-frame counter: bumped once per _nextpage(), used as the time base for
 * input key-hold/expiry (see syncduke_input.c). Not totalclock -- frame-paced so a
 * key tap survives the engine's once-per-frame keyboard poll independent of frame
 * timing or busy-wait _idle() loops. */
static int syncduke_frame;

/* The live display palette as 8-bit RGB, 256 entries. Filled by
 * VBE_setPalette() from the engine's 4-byte 6-bit BGRA palette. This is what
 * the framebuffer indices map through for the PPM/sixel emit. */
static uint8_t syncduke_pal[256 * 3];
static int     syncduke_pal_dirty = 1;   /* re-emit sixel registers when the palette changes */

/* --- accessors for syncduke_io.c (the sixel present path) --- */
const uint8_t *syncduke_fb(void) { return syncduke_screen; }
const uint8_t *syncduke_palette(void) { return syncduke_pal; }
int syncduke_palette_take_dirty(void) { int d = syncduke_pal_dirty; syncduke_pal_dirty = 0; return d; }

/* ====================================================================== */
/* Timer -- drives the engine's totalclock from a monotonic clock.        */
/* Mirrors display.c's inittimer/sampletimer/getticks, but with           */
/* clock_gettime() instead of SDL_GetTicks(). game.c calls inittimer      */
/* (TICRATE) and sampletimer() in its main loop; without this totalclock  */
/* is frozen and the logo/title/menu timing never advances.               */
/* ====================================================================== */
static int64_t timerfreq;          /* platform ticks per second (== 1e9, ns) */
static int32_t timerticspersec;    /* requested Build tick rate (TICRATE)    */
static int32_t timerlastsample;
static void    (*usertimercallback)(void);

static int64_t plat_ticks(void)
{
#ifdef _WIN32
	/* QueryPerformanceCounter -> nanoseconds, split to avoid int64 overflow. */
	static LARGE_INTEGER freq;
	LARGE_INTEGER        c;
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&c);
	return (int64_t)(c.QuadPart / freq.QuadPart) * 1000000000LL
	       + (int64_t)(c.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

int inittimer(int tickspersecond)
{
	if (timerfreq)
		return 0;              /* already installed */
	timerfreq = 1000000000LL;
	timerticspersec = tickspersecond;
	timerlastsample = (int32_t)(plat_ticks() * timerticspersec / timerfreq);
	usertimercallback = NULL;
	return 0;
}

void uninittimer(void) { timerfreq = 0; timerticspersec = 0; }

void sampletimer(void)
{
	int64_t i;
	int32_t n;
	if (!timerfreq)
		return;
	i = plat_ticks();
	n = (int32_t)(i * timerticspersec / timerfreq) - timerlastsample;
	if (n > 0) { totalclock += n; timerlastsample += n; }
	if (usertimercallback)
		for (; n > 0; n--) usertimercallback();
}

uint32_t getticks(void) { return (uint32_t)(plat_ticks() * 1000LL / 1000000000LL); }
int gettimerfreq(void) { return timerticspersec; }

/* ====================================================================== */
/* Video / framebuffer                                                    */
/* ====================================================================== */
void _platform_init(int argc, char **argv, const char *title, const char *iconName)
{
	(void)argc; (void)argv; (void)title; (void)iconName;
	syncduke_node_init();   /* Synchronet who's-online / status (no-op off a BBS) */
}

/*
 * Port of display.c's init_new_res_vars(), minus everything SDL: set up the
 * engine's render geometry for 320x200x8 into our own framebuffer. Without
 * ylookup[]/horizlookup/setview/setbrightness the classic renderer writes
 * garbage (or out of bounds), so this is what makes any drawing valid.
 */
int32_t _setgamemode(uint8_t davidoption, int32_t daxdim, int32_t daydim)
{
	int32_t i, j;
	(void)davidoption; (void)daxdim; (void)daydim;

	/* v1: force Build classic 320x200 into our own buffer. */
	xdim = xres = 320;
	ydim = yres = 200;
	bytesperline = 320;
	imageSize = bytesperline * ydim;
	vesachecked = 1;
	vgacompatible = 1;
	linearmode = 1;
	qsetmode = 200;
	activepage = visualpage = 0;
	frameoffset = frameplace = syncduke_screen;

	/* horizlookup / horizlookup2: ydim*4 int32s (room for both lookups). */
	j = ydim * 4 * sizeof(int32_t);
	if (horizlookup)
		free(horizlookup);
	if (horizlookup2)
		free(horizlookup2);
	horizlookup  = (int32_t *)malloc(j);
	horizlookup2 = (int32_t *)malloc(j);

	/* ylookup: screenspace Y -> framebuffer offset. */
	j = 0;
	for (i = 0; i <= ydim; i++) { ylookup[i] = j; j += bytesperline; }

	horizycent = (ydim * 4) >> 1;

	/* Force drawrooms to recompute aspect/view. */
	oxyaspect = oxdimen = oviewingrange = -1;

	setBytesPerLine(bytesperline);
	setview(0L, 0L, xdim - 1, ydim - 1);
	setbrightness(curbrightness, palette);

	if (searchx < 0) { searchx = halfxdimen; searchy = (ydimen >> 1); }

	return 0;
}

void setvmode(int mode) { (void)mode; }

/* ---------------------------------------------------------------------- */
/* PPM capture (debug, env-gated): set SYNCDUKE_PPMDIR to a writable dir   */
/* and every SYNCDUKE_PPMSTRIDE-th frame (default 20, capped) is written   */
/* as a binary P6 PPM, with the 6-bit VGA palette scaled to 8-bit. Lets    */
/* us eyeball the headless render before the sixel/socket path exists.     */
/* ---------------------------------------------------------------------- */
static void syncduke_dump_ppm(void)
{
	static const char *dir = NULL;
	static int         inited = 0, stride = 20, cap = 400, count = 0, written = 0;
	static uint64_t    frame = 0;
	char               path[512];
	FILE *             fp;
	int                i;

	if (!inited) {
		const char *s;
		inited = 1;
		dir = getenv("SYNCDUKE_PPMDIR");
		if ((s = getenv("SYNCDUKE_PPMSTRIDE")) != NULL && atoi(s) > 0)
			stride = atoi(s);
		if ((s = getenv("SYNCDUKE_PPMCAP")) != NULL && atoi(s) > 0)
			cap = atoi(s);
	}
	frame++;
	if (!dir || written >= cap)
		return;
	if ((count++ % stride) != 0)
		return;

	snprintf(path, sizeof path, "%s/frame%05llu.ppm", dir, (unsigned long long)frame);
	fp = fopen(path, "wb");
	if (!fp)
		return;
	fprintf(fp, "P6\n%d %d\n255\n", xdim, ydim);
	for (i = 0; i < xdim * ydim; i++)
		fwrite(&syncduke_pal[syncduke_screen[i] * 3], 1, 3, fp);
	fclose(fp);
	written++;
}

static uint32_t plat_ms(void) { return (uint32_t)(plat_ticks() / 1000000LL); }

/* Short blocking sleep (cross-platform): Sleep() on Windows, nanosleep elsewhere. */
static void syncduke_sleep_ms(unsigned ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
#endif
}

/*
 * Cap frame PRODUCTION to the terminal's CONSUMPTION rate.
 *
 * The vendored engine busy-spins its render loop (no SDL vsync/sleep), so without
 * this it calls _nextpage()/present() thousands of times a second -- pegging the
 * CPU and handing the DSR pacing in present() far more frames than the terminal
 * can draw. Pacing then drops nearly all of them, and WHICH frame survives is
 * essentially random relative to the animation phase: the menu/intro "stutter"
 * and partial-looking refreshes the player sees. (SyncDOOM avoids this by sleeping
 * to its 35fps sim rate; the Build engine has no equivalent throttle.)
 *
 * So after each present we block here until the pipeline can accept another frame
 * (syncduke_pace_ready: an in-flight frame got acked and the last one drained) AND
 * a minimum inter-frame interval has passed (a hard fps ceiling). We service the
 * timer, input and output draining throughout, so totalclock stays on real time,
 * key latency is unaffected and DSR acks keep flowing. A max_wait bounds the block
 * so a terminal that never reports DSR can't wedge the game loop -- it degrades to
 * a slideshow (and present()'s deadline reclaim still feeds it), with the CPU
 * asleep instead of pegged.
 */
#define SYNCDUKE_CAP_FPS    30
#define SYNCDUKE_CAP_WAITMS 250          /* never block the loop longer than this */
#define SYNCDUKE_FRAME_STALL_MS 500      /* present+pace over this => log a frame-stall note (well
	                                      * above a normally-paced frame's ~cap+encode) */
static void syncduke_pace_engine(void)
{
	static uint32_t last_ms;
	const uint32_t  min_iv = 1000u / SYNCDUKE_CAP_FPS;   /* hard ceiling between frames */
	uint32_t        start  = plat_ms();

	for (;;) {
		uint32_t now;

		sampletimer();          /* keep totalclock on real time while we wait */
		_handle_events();       /* pump input + read DSR acks (drops inflight) */
		syncduke_out_flush();    /* keep draining the in-flight frame's bytes */
		now = plat_ms();
		if (syncduke_pace_ready()
		    && (last_ms == 0 || (int32_t)(now - last_ms) >= (int32_t)min_iv))
			break;
		if ((int32_t)(now - start) >= SYNCDUKE_CAP_WAITMS)
			break;              /* no-ack terminal: don't wedge the loop */
		syncduke_sleep_ms(2);   /* 2ms slice -> ~500Hz input poll */
	}
	last_ms = plat_ms();
}

void _nextpage(void)
{
	uint32_t lim = syncduke_door_time_limit_ms();

	syncduke_frame++;   /* one presented frame: advances the input key-hold time base */
	sampletimer();
	_handle_events();   /* service terminal input each frame (the SDL _nextpage did too) */
	syncduke_node_tick();   /* status broadcast; who's-online build; message poll */
	syncduke_events_tick();   /* events.jsonl activity log (no-op without -eventlog) */

	/* Idle-USER timeout. This door had no unified exit check to hang it on, so
	 * it rides here beside the session time limit -- the one place already
	 * reached every presented frame. */
	if (syncduke_idle_check()) {
		syncduke_log("idle timeout -- no user input; exiting");
		syncduke_term_restore();  /* restore the BBS terminal before we go */
		_exit(0);
	}

	/* Door session time limit: leave cleanly when it's up (the BBS reclaims the
	 * node). Crude hard-exit, but a door's lifetime is the BBS's to bound. */
	if (lim) {
		static uint32_t t0;
		uint32_t        now = getticks();
		if (!t0)
			t0 = now ? now : 1;
		if (now - t0 > lim)
			exit(0);
	}

	syncduke_dump_ppm();      /* debug: env-gated PPM (SYNCDUKE_PPMDIR) */

	/* Frame-stall watchdog: time the present + pacing block.  A frame that overruns badly
	 * (terminal not draining, a blocked socket write, ...) gets a note so a stall's cause can be
	 * pinned down after the fact.  (An audio transcode/upload blocks the game thread OUTSIDE this
	 * window and logs its own render=/xfer= line, so the two don't double-count.) */
	{
		uint32_t fs = getticks();
		syncduke_present();       /* encode the frame to sixel and emit to the terminal sink */
		syncduke_pace_engine();   /* throttle production to the terminal's draw rate (no busy-spin) */
		fs = getticks() - fs;
		if (fs >= SYNCDUKE_FRAME_STALL_MS) {
			extern void syncduke_log(const char *fmt, ...);
			syncduke_log("pace: frame stall %ums (inflight=%d depth=%d)",
			             (unsigned)fs, syncduke_pace_inflight(), syncduke_pace_curdepth());
		}
	}
}

void _updateScreenRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
	(void)x; (void)y; (void)w; (void)h;
}

void clear2dscreen(void) { memset(syncduke_screen, 0, sizeof syncduke_screen); }
void _uninitengine(void) { }
int  screencapture(char *filename, uint8_t inverseit) { (void)filename; (void)inverseit; return 0; }

/* --- palette --- the engine hands us 256 entries of 4-byte BGRA, 6-bit
 * (0..63) per channel (built by setbrightness()). Convert to the 8-bit RGB
 * table the framebuffer indices render through. --- */
int VBE_setPalette(uint8_t *palettebuffer)
{
	int i, changed = 0;

	/* Mark the sixel palette dirty ONLY when its contents actually change. Duke
	 * re-calls setpalette every frame during animations (spinning title icons, the
	 * attract demo) -- almost always with the SAME palette. Re-defining all 256
	 * sixel color registers every frame garbles SyncTERM's sixel decoder (the
	 * documented SyncDOOM gotcha: define-once, re-emit only on a real change), so a
	 * static screen rendered fine but any animation corrupted. memcmp-gate it. */
	for (i = 0; i < 256; i++) {
		uint8_t b  = palettebuffer[i * 4 + 0];
		uint8_t g  = palettebuffer[i * 4 + 1];
		uint8_t r  = palettebuffer[i * 4 + 2];
		uint8_t r8 = (uint8_t)(r * 255 / 63);
		uint8_t g8 = (uint8_t)(g * 255 / 63);
		uint8_t b8 = (uint8_t)(b * 255 / 63);
		if (syncduke_pal[i * 3 + 0] != r8 || syncduke_pal[i * 3 + 1] != g8 || syncduke_pal[i * 3 + 2] != b8) {
			syncduke_pal[i * 3 + 0] = r8;
			syncduke_pal[i * 3 + 1] = g8;
			syncduke_pal[i * 3 + 2] = b8;
			changed = 1;
		}
	}
	if (changed)
		syncduke_pal_dirty = 1;
	return 0;
}

/* --- 2D / 16-color helpers (used by menus/2D, not the 3D game view) --- */
uint8_t readpixel(uint8_t *location) { return location ? *location : 0; }
void drawpixel(uint8_t *location, uint8_t pixel) {
	if (location)
		*location = pixel;
}
void drawpixel16(int32_t offset) { (void)offset; }
void drawline16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)c; }
void setcolor16(uint8_t color) { (void)color; }

/* --- input: terminal bytes -> Build/Duke key state (syncduke_input.c) ---
 * keyhandler() (keyboard.c) pulls one raw scancode byte per call via
 * _readlastkeyhit(); we fill that queue from the terminal in _handle_events()
 * and drive keyhandler() once per queued byte. _idle() services input too so the
 * engine's `while(!keyIsWaiting) _idle();` key-wait loops make progress. */
extern void keyhandler(void);
extern void sd_music_poll(void);   /* syncduke_stubs.c: ship an async-rendered track when ready */

void _handle_events(void)
{
	/* read terminal -> enqueue key-downs; the action layer (WASD/Space=fire) only
	 * applies in gameplay, so menus/text-entry get literal letters & space */
	/* Key-hold/expiry is counted in presented frames (syncduke_frame), not totalclock,
	 * so a tapped key reliably survives the engine's once-per-frame KB_KeyDown[]
	 * poll regardless of frame duration or busy-wait _idle() loops. */
	/* Treat "dead" as not-gameplay so the spacebar reverts to Open -- what the death
	 * "PRESS SPACE TO RESTART LEVEL" prompt waits for (else Space = Fire and never restarts). */
	syncduke_input_pump(syncduke_input_fd(), syncduke_frame,
	                    syncduke_in_gameplay() && !syncduke_player_dead());
	syncduke_input_expire(syncduke_frame);            /* synth key-ups for held-out keys */
	while (syncduke_input_has_raw())
		keyhandler();                           /* consumes one raw byte each */
	sd_music_poll();                                  /* ship an async-rendered track once ready */
}

/* Build calls _idle() from its key-wait spins (`while(!KB_KeyWaiting()) _idle();`).
 * Those don't render, so they'd otherwise busy-spin at 100% CPU. A short sleep caps
 * the spin to ~500Hz -- still far faster than any human keypress -- while keeping the
 * timer and input (incl. DSR acks for the last frame) serviced. */
void _idle(void)
{
	sampletimer();
	_handle_events();
	syncduke_out_flush();
	syncduke_sleep_ms(2);   /* 2ms */
}
uint8_t _readlastkeyhit(void) { return (uint8_t)syncduke_input_pop_raw(); }
int  setupmouse(void) { return 0; }
void readmousexy(short *x, short *y) {
	if (x)
		*x = 0; if (y)
		*y = 0;
}
void readmousebstatus(short *b) {
	if (b)
		*b = 0;
}
void _joystick_init(void) { }
void _joystick_deinit(void) { }
int  _joystick_update(void) { return 0; }
int  _joystick_axis(int axis) { (void)axis; return 0; }
int  _joystick_hat(int hat) { (void)hat; return 0; }
int  _joystick_button(int button) { (void)button; return 0; }
