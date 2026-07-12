// SyncConquer door core -- interface between the vendored Vanilla Conquer
// engine's NEW_VIDEO_BUILD backend seam (video_termgfx.cpp,
// wwkeyboard_termgfx.cpp) and the door itself (door_core.c / door_io.c). See
// plans/m2-proto/FACTS.md for the verified seam this evolved from, and
// DESIGN.md for the tier/pacing/geometry decisions door_io.c implements.
//
// Command line (parsed by door_io.c's pre-main constructor, per-doors house
// pattern -- the vendored engine's own Parse_Command_Line() silently ignores
// any argv element it doesn't recognize, so these coexist with it; ALL of
// them are neutered back out of argv before the engine gets to scan it --
// door_io.c's door_sanitize_argv(), Task 5):
//   -s<fd>          client comm socket descriptor (e.g. a DOOR32.SYS handle)
//   <path>/door32.sys  Synchronet's DOOR32.SYS drop file (%f): socket handle,
//                   user alias, time-left minutes (Task 5)
//   -home <dir>     per-user directory: the display-fit-mode flag file, AND
//                   (Task 5) the vendored engine's UserPath (redalert.ini,
//                   savegames) via a REDALERT.INI [Paths] override written
//                   there -- FACTS.md #9
//   -assets <dir>   REDALERT.MIX/MAIN.MIX directory; else <the real binary's
//                   own directory>/assets. Validated before the engine's
//                   main() is allowed to run at all (Task 5)
//   -audiocache <dir>  SyncTERM-side transcoded-audio disk cache (Task 4)
// Env (checked once, no CLI equivalent -- test/offline conveniences, plus
// Task 5's real-BBS-install integration):
//   SYNCALERT_SIXELOUT=<path>  offline capture mode: every present() call
//                              OVERWRITES <path> with one self-contained
//                              full-res DECSIXEL frame, no socket needed.
//   SYNCALERT_SOCK=<fd>        explicit client fd (used when -s<fd> is
//                              absent, e.g. a dev/test run).
//   SYNCALERT_AUDIO_CACHE      audio-cache dir override/export (Task 4/5).
//   SBBSCTRL / SBBSDATA / SBBSNNUM / SBBSNODE   set by Synchronet for
//                              external programs -- who's-online/paging,
//                              the per-node debug log, and the default
//                              audio-cache dir (Task 5).
//
// Door-owned keys (intercepted by door_io.c's input parser, never reach the
// engine): F4 (SS3/CSI 'S', or the legacy Ctrl-D byte on a terminal with
// neither) cycles the graphics tier (sixel/JXL/PPM-APC/text); Ctrl-F (0x06)
// toggles the display-fit mode (Aspect <-> Fill), sticky per session via a
// flag file under -home; Ctrl-U (0x15) shows a who's-online overlay; Ctrl-P
// (0x10) opens a page-a-node compose line (Task 5, sbbs_node.h).
#ifndef DOOR_H_
#define DOOR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The engine renders 640x400 8bpp paletted frames (Red Alert's native
// resolution); the offline/present path is sized to that fixed frame.
#define DOOR_FB_WIDTH  640
#define DOOR_FB_HEIGHT 400

// Present one rendered frame. `fb` is DOOR_FB_WIDTH*DOOR_FB_HEIGHT palette
// index bytes (VisiblePage, copied out under lock by video_termgfx.cpp's
// Video_Render_Frame). `pal768` is the engine's raw CurrentPalette -- 256
// RGB triples, each component a 6-bit VGA value (0-63); door_present scales
// it (<<2) to 8-bit RGB itself before handing it to termgfx's sixel
// encoder, so the engine shim stays a plain copy-out with no color math.
// Implemented (door_io.c): tiered (sixel/JXL/PPM-APC/text), paced (DSR-ACK +
// AIMD pipeline depth) and whole-frame de-duped -- the engine may call this
// as fast as it renders (~115Hz at the menu); door_present decides whether
// there's anything new to send and whether the pipeline has room.
void door_present(const uint8_t *fb, const uint8_t *pal768);

// Pump door I/O: read whatever is available from the calling session and
// feed it into the engine's input path. Called from
// WWKeyboardClass::Fill_Buffer_From_System() every poll. This task's
// door_pump() resolves the socket, answers/consumes the terminal capability
// and geometry probes, and handles its own two keys (F4 tier cycle, Ctrl-F
// fit-mode toggle) -- everything else lands in door_input_byte() below,
// unconsumed, for a later task's real key/mouse parser.
void door_pump(void);

// Report the current mouse position in engine screen coordinates
// (0..DOOR_FB_WIDTH-1, 0..DOOR_FB_HEIGHT-1), for Get_Video_Mouse(). This
// task's stub always reports the screen center.
void door_mouse_xy(int *x, int *y);

// TASK-3 HOOK: every input byte door_pump()'s minimal parser doesn't claim
// for itself (probe replies, F4, Ctrl-F) lands here, in arrival order, via
// door_io.c. This task's implementation is a bounded ring buffer with
// nothing draining it yet; a later task (the terminal input parser, modeled
// on syncduke_input.c) replaces this function's body -- or adds a drain
// call from Fill_Buffer_From_System() -- to turn these bytes into real
// WWKeyboard key/mouse events. Kept as a single-byte call (rather than a
// buffer handoff) so the seam stays trivial to reason about and replace.
void door_input_byte(uint8_t b);

// The resolved game-data (MIX) directory (the door's -assets dir), or "" until
// the pre-main constructor has resolved it. Tiberian Dawn's Init_Game adds this
// to the engine's CCFileClass file-search path before loading any mix, because
// the engine CWD is the per-user -home dir, not where the shared game data is.
const char *door_engine_data_dir(void);

#ifdef __cplusplus
}
#endif

#endif // DOOR_H_
