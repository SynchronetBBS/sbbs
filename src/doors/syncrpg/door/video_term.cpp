/* SyncRPG -- terminal video present (Task 4 / M1). The engine draws each frame
 * into main_surface (32-bit, memory byte order R,G,B,X -- pinned in
 * ui_term.cpp's ctor); BaseUi_termgfx::UpdateDisplay() hands that surface here,
 * and this forwards its pixels to termgfx_termio_present_rgbx(), termgfx's
 * truecolor path (sixel-quantized or JXL-encoded, tier chosen at run time).
 * GPLv3+, like the EasyRPG tree.
 */
#include "video_term.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include <string>

#include "bitmap.h"
#include "player.h"     /* Player::screen_width / has_custom_resolution / game_config / ChangeResolution */
#include "baseui.h"     /* DisplayUi, BaseUi::SetGameResolution */
#include "options.h"    /* SCREEN_TARGET_WIDTH / SCREEN_TARGET_HEIGHT */

/* NOT inside the extern "C" below: ini_file.h reaches <winsock2.h> on MSVC,
 * whose templates are a hard error under 'C' linkage (same reason syncrpg.cpp
 * keeps it out of its extern "C"). */
#include "ini_file.h"   /* xpdev: per-user config.ini persistence of the F4 choice */

extern "C" {
#include "termgfx_termio.h"
}

void video_term_present(const Bitmap& surf)
{
	const int w = surf.width();
	const int h = surf.height();
	if (w <= 0 || h <= 0)
		return;

	const uint8_t *pixels = static_cast<const uint8_t *>(surf.pixels());
	if (pixels == NULL)
		return;

	const int pitch     = surf.pitch();
	const int row_bytes = w * 4;   /* present_rgbx contract: 32bpp, stride w*4 */

	if (pitch == row_bytes) {
		/* Zero-copy: the surface is already tightly packed R,G,B,X, exactly
		 * present_rgbx's stride -- hand its pixels straight through. This is
		 * the norm (a 320x240x4 pixman surface pitches to w*4). */
		termgfx_termio_present_rgbx(pixels, w, h);
		return;
	}

	/* Padded rows (pitch > w*4): repack into a contiguous w*h*4 buffer so the
	 * stride matches present_rgbx's fixed w*4. A safety net, not the norm;
	 * the scratch persists across calls so a steady-state stream re-uses it. */
	static std::vector<uint8_t> packed;
	packed.resize((size_t)row_bytes * h);
	for (int y = 0; y < h; y++)
		memcpy(&packed[(size_t)y * row_bytes],
		       pixels + (size_t)y * pitch, (size_t)row_bytes);
	termgfx_termio_present_rgbx(packed.data(), w, h);
}

/* The cycle F4 walks, in order; index 0 (Original) is the RPG Maker native size
 * and the only one with fake metrics off. Kept in step with Scene_Title::Start()'s
 * own switch (scene_title.cpp) -- 416/560 wide, height unchanged. The tag is
 * EasyRPG's own config token (game_config.h), reused when persisting the choice. */
namespace {
struct ResMode { int width; ConfigEnum::GameResolution res; const char *tag; };
const ResMode RES_MODES[] = {
	{ SCREEN_TARGET_WIDTH, ConfigEnum::GameResolution::Original,   "original" },
	{ 416,                 ConfigEnum::GameResolution::Widescreen, "widescreen" },
	{ 560,                 ConfigEnum::GameResolution::Ultrawide,  "ultrawide" },
};
const int RES_COUNT = (int)(sizeof RES_MODES / sizeof RES_MODES[0]);

/* Full path to the per-user EasyRPG config.ini, or empty to not persist (no
 * per-user dir -- see video_term_set_config_dir). */
std::string g_cfg_ini;

int width_to_index(int w)
{
	for (int i = 0; i < RES_COUNT; i++)
		if (RES_MODES[i].width == w)
			return i;
	return -1;
}

/* Record the F4 choice in the caller's own config.ini as EasyRPG's [Video]
 * GameResolution, so it is the resolution their NEXT session boots at (syncrpg.cpp
 * defers to a saved value over the sysop default). Written with xpdev's ini
 * (iniSetString preserves every other key), and read back by EasyRPG's own
 * loader next launch -- one key, its native section, no separate door state. */
void persist_resolution(const char *tag)
{
	if (g_cfg_ini.empty())
		return;

	FILE *f = iniOpenFile(g_cfg_ini.c_str(), TRUE);   /* r+ if it exists, else w+ */
	if (f == NULL)
		return;

	str_list_t ini = iniReadFile(f);
	iniSetString(&ini, "Video", "GameResolution", tag, NULL);
	iniWriteFile(f, ini);
	strListFree(&ini);
	fclose(f);
}
}  // namespace

void video_term_set_config_dir(const char *dir)
{
	g_cfg_ini = (dir != NULL && *dir != '\0')
	            ? std::string(dir) + "/config.ini"
	            : std::string();
}

void video_term_cycle_resolution()
{
	if (Player::has_custom_resolution || !DisplayUi)
		return;

	int cur  = width_to_index(Player::screen_width);
	int next = (cur < 0) ? 1 : (cur + 1) % RES_COUNT;   /* off-grid -> Widescreen */
	const ResMode &m = RES_MODES[next];

	/* Mirror Scene_Title::Start()'s switch: fake metrics on for the wide modes,
	 * SetGameResolution keeps the engine config in step (so a later title reload
	 * agrees), then ChangeResolution recreates the surface -- which routes through
	 * BaseUi_termgfx::vChangeDisplaySurfaceResolution and updates the Ctrl-S
	 * readout. The map repaints to the new viewport on the next frame. */
	Player::game_config.fake_resolution.Set(next != 0);
	DisplayUi->SetGameResolution(m.res);
	Player::ChangeResolution(m.width, SCREEN_TARGET_HEIGHT);
	persist_resolution(m.tag);
}
