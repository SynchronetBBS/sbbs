/* SyncRPG -- BaseUi_termgfx implementation. See ui_term.h for how this
 * class is wired in as EasyRPG's UI backend. GPLv3+, like EasyRPG.
 *
 * Task 3 boots a game to its title headless (video/input/audio landed in
 * later tasks: video in Task 4, input in Task 5, audio in Task 6/M3 -- see
 * audio_term.h for the GenericAudio -> termgfx_stream bridge). What is real
 * here: a main_surface the engine draws every frame into, the global Bitmap
 * pixel format the whole render pipeline shares, resolution-change support
 * (RPG Maker titles switch to their own 320x240 shortly after boot), and a
 * ProcessEvents() that turns a dropped carrier / user quit into a clean
 * Player shutdown.
 */
#include "ui_term.h"

#include <cstdio>
#include <cstdlib>

#include "bitmap.h"
#include "pixel_format.h"
#include "color.h"
#include "output.h"

#include "audio_term.h"
#include "video_term.h"
#include "input_term.h"

extern "C" {
#include "termgfx_termio.h"
}

/* Format probe (Task 4 step 1): confirm which 32-bit "_n" pixel format lays
 * out as R,G,B,X in memory -- the byte order termgfx_termio_present_rgbx()
 * expects (R at [0], G at [1], B at [2], pad at [3]). Fills a 1x1 bitmap with a
 * known color, reads the raw bytes, and reports each candidate. Runs only when
 * SYNCRPG_FMT_PROBE is set; the ctor pins the winning format unconditionally
 * below. Kept as living documentation of, and a re-runnable check on, the
 * choice. The fill color (r=10,g=20,b=30) reads back off-by-one -- EasyRPG's
 * fill scales each channel *255>>8 (10->9, 20->19, 30->29), an artifact of the
 * fill path, not the byte order -- so the match test is on channel ORDER (r at
 * [0] < g at [1] < b at [2], pad 0xff at [3]), which is unambiguous. On this
 * build (little-endian) format_R8G8B8A8_n wins:
 *   R8G8B8A8_n bytes =   9  19  29 255  <= R,G,B,X MATCH */
static void syncrpg_probe_pixel_formats()
{
	struct { const char *name; DynamicFormat fmt; } cands[] = {
		{ "R8G8B8A8_n", format_R8G8B8A8_n().format() },
		{ "B8G8R8A8_n", format_B8G8R8A8_n().format() },
		{ "A8R8G8B8_n", format_A8R8G8B8_n().format() },
		{ "A8B8G8R8_n", format_A8B8G8R8_n().format() },
	};
	for (size_t i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
		Bitmap::SetFormat(Bitmap::ChooseFormat(cands[i].fmt));
		BitmapRef bm = Bitmap::Create(1, 1, Color(10, 20, 30, 255));
		const uint8_t *p = static_cast<const uint8_t *>(bm->pixels());
		bool match = (p[0] < p[1] && p[1] < p[2] && p[3] == 255);
		fprintf(stderr, "syncrpg fmt-probe %-11s bytes = %3u %3u %3u %3u  %s\n",
			cands[i].name, p[0], p[1], p[2], p[3],
			match ? "<= R,G,B,X MATCH" : "");
	}
}

BaseUi_termgfx::BaseUi_termgfx(long width, long height, const Game_Config& cfg)
	: BaseUi(cfg)
{
	if (getenv("SYNCRPG_FMT_PROBE") != NULL)
		syncrpg_probe_pixel_formats();

	/* Pin the global surface format the whole engine renders through. The
	 * probe above confirms format_R8G8B8A8_n lays out as R,G,B,X in memory --
	 * exactly what termgfx_termio_present_rgbx() reads -- so main_surface feeds
	 * the present path zero-copy (no per-frame channel swizzle). Also matches
	 * the SDL backend's little-endian default (sdl2_ui.cpp's GetDefaultFormat). */
	DynamicFormat format = format_R8G8B8A8_n().format();
	Bitmap::SetFormat(Bitmap::ChooseFormat(format));

	main_surface = Bitmap::Create((int)width, (int)height, Color(0, 0, 0, 255));

	current_display_mode.width = (int)width;
	current_display_mode.height = (int)height;
	current_display_mode.bpp = 32;
	current_display_mode.effective = true;

#ifdef SUPPORT_AUDIO
	/* M3 (Task 6): route EasyRPG's mixed PCM into termgfx's shared audio
	 * stream. See audio_term.h for the GenericAudio subclass and its
	 * rate/format contract. */
	audio_ = std::make_unique<SyncrpgAudio>(cfg.audio);
#endif
}

BaseUi_termgfx::~BaseUi_termgfx() = default;

bool BaseUi_termgfx::ProcessEvents() {
	/* Service the terminal so carrier-loss and a user quit request are seen.
	 * Returning false is BaseUi's contract for "shut the Player down now"
	 * (player.cpp's MainLoop pops to Scene::Null and calls Player::Exit). */
	termgfx_termio_pump();
	/* Retry a present the backpressure gate deferred (see present_rgbx's doc):
	 * called every poll, a no-op when nothing is pending. Mirrors syncscumm's
	 * per-poll tick. Harmless even though the rgbx path retains no frame today
	 * -- consistent with the indexed path and future-proof if it ever does. */
	termgfx_termio_tick();
	if (termgfx_termio_quit_requested() || termgfx_termio_hung_up())
		return false;

	/* Keyboard (Task 5 / M2): drain whatever termgfx_termio_pump() just queued
	 * into EasyRPG's raw key state. Must run every poll regardless of quit/
	 * hang-up above having already returned -- it hasn't here -- so a key
	 * held across the quit check still gets its tap released next poll. */
	input_term_pump(keys);

#ifdef SUPPORT_AUDIO
	/* Audio (Task 6 / M3): wall-clock driven, so calling it once per real
	 * poll (this runs exactly once per Player::MainLoop() iteration,
	 * regardless of how many logical game steps that iteration runs) is
	 * sufficient -- see audio_term.h. */
	audio_->tick();
#endif

	return true;
}

void BaseUi_termgfx::UpdateDisplay() {
	/* The engine has drawn this frame into main_surface (32-bit R,G,B,X);
	 * forward it to termgfx's truecolor present path. */
	video_term_present(*GetDisplaySurface());

	/* Static-screen investigation (Task 4 step 3): count present calls so a
	 * boot test can see whether EasyRPG keeps calling UpdateDisplay() on a
	 * still title screen. Gated by SYNCRPG_FRAMELOG so it costs nothing in
	 * production; logs every 60th frame to keep the stream readable. */
	if (getenv("SYNCRPG_FRAMELOG") != NULL) {
		static unsigned long frames;
		if (++frames % 60 == 1)
			fprintf(stderr, "syncrpg framelog: UpdateDisplay #%lu\n", frames);
	}
}

#ifdef SUPPORT_AUDIO
AudioInterface& BaseUi_termgfx::GetAudio() {
	return *audio_;
}
#endif

void BaseUi_termgfx::vGetConfig(Game_ConfigVideo& cfg) const {
	/* Headless: report a fixed software renderer and hide the window/display
	 * options a terminal door can't act on. */
	cfg.renderer.Lock("Synchronet (headless)");
	cfg.vsync.SetOptionVisible(false);
	cfg.fullscreen.SetOptionVisible(false);
	cfg.window_zoom.SetOptionVisible(false);
	cfg.scaling_mode.SetOptionVisible(false);
	cfg.stretch.SetOptionVisible(false);
	cfg.pause_when_focus_lost.SetOptionVisible(false);
	cfg.screen_scale.SetOptionVisible(false);
	cfg.fps_limit.SetOptionVisible(true);
	cfg.game_resolution.SetOptionVisible(true);
}

bool BaseUi_termgfx::vChangeDisplaySurfaceResolution(int new_width, int new_height) {
	/* RPG Maker titles pick their own resolution right after boot (the scout
	 * saw "Resolution changed to 320x240"); recreate the surface so the
	 * change succeeds instead of Player logging a warning and running at the
	 * wrong size. Mirrors Sdl2Ui::vChangeDisplaySurfaceResolution. */
	BitmapRef new_surface = Bitmap::Create(new_width, new_height, Color(0, 0, 0, 255));
	if (!new_surface) {
		Output::Warning("ChangeDisplaySurfaceResolution Bitmap::Create failed");
		return false;
	}
	main_surface = new_surface;
	current_display_mode.width = new_width;
	current_display_mode.height = new_height;
	return true;
}
