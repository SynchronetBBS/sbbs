/* SyncRPG -- BaseUi_termgfx: EasyRPG's UI backend for a Synchronet door.
 *
 * EasyRPG picks its UI at compile time in src/baseui.cpp: USE_SDL==N selects
 * an SdlUi, otherwise the PLAYER_UI macro names the class BaseUi::CreateUi
 * instantiates -- the same hook EasyRPG's own libretro/3ds/switch ports use.
 * The door's CMakeLists.txt force-includes THIS header into the engine's
 * baseui.cpp (a per-source -include, no vendored edit) so that, for that one
 * translation unit, USE_SDL is undone and PLAYER_UI points here -- making
 * CreateUi return a BaseUi_termgfx instead of an Sdl2Ui. See the CreateUi
 * substitution note in syncrpg/CMakeLists.txt.
 *
 * Responsibilities: a real main_surface presented through termgfx's truecolor
 * path (UpdateDisplay -> present_rgbx), a ProcessEvents() that folds terminal
 * quit/hang-up into Player's exit path and translates termgfx key events into
 * EasyRPG's Input::Keys, and a GenericAudio-backed audio interface whose mixed
 * PCM feeds termgfx's audio stream. GPLv3+, like the EasyRPG tree this links
 * into.
 */
#ifndef SYNCRPG_UI_TERM_H_
#define SYNCRPG_UI_TERM_H_

/* When force-included into baseui.cpp, steer BaseUi::CreateUi to us: undo the
 * build's -DUSE_SDL=2 and select our class through PLAYER_UI. A source-level
 * #undef beats the command-line -D regardless of how CMake orders -D/-U on the
 * compiler line, so this is robust build-wiring, not a vendored patch. Both
 * are harmless when this header is included normally (by ui_term.cpp): that TU
 * never references USE_SDL or PLAYER_UI. Undefining USE_SDL only for baseui.cpp
 * unsets SUPPORT_KEYBOARD there too, but that macro gates only code paths in
 * other .cpp files -- no header/struct layout -- so there is no ABI/ODR skew. */
#ifdef USE_SDL
#  undef USE_SDL
#endif
#ifndef PLAYER_UI
#  define PLAYER_UI BaseUi_termgfx
#endif

#include <memory>

#include "baseui.h"

class SyncrpgAudio;

class BaseUi_termgfx : public BaseUi {
public:
	BaseUi_termgfx(long width, long height, const Game_Config& cfg);
	/* Out-of-line (not defaulted) on purpose: the audio_ member is a
	 * unique_ptr to a forward-declared SyncrpgAudio here, so its destructor
	 * must be emitted where that type is complete (ui_term.cpp), not inline in
	 * every TU baseui.cpp's make_shared expands into. */
	~BaseUi_termgfx() override;

	bool ProcessEvents() override;
	void UpdateDisplay() override;

#ifdef SUPPORT_AUDIO
	AudioInterface& GetAudio() override;
#endif

protected:
	void vGetConfig(Game_ConfigVideo& cfg) const override;
	bool vChangeDisplaySurfaceResolution(int new_width, int new_height) override;

private:
#ifdef SUPPORT_AUDIO
	/* Concrete type (not AudioInterface), not just for GetAudio()'s return --
	 * ProcessEvents() also calls audio_->tick() each poll, which only
	 * SyncrpgAudio declares (see audio_term.h). Forward-declared above;
	 * needs the complete type only where the dtor is emitted (ui_term.cpp,
	 * already out-of-line for exactly this reason). */
	std::unique_ptr<SyncrpgAudio> audio_;
#endif
};

#endif /* SYNCRPG_UI_TERM_H_ */
