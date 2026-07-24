/* SyncRPG -- terminal video present. Bridges an EasyRPG display surface (the
 * 32-bit main_surface, R,G,B,X in memory -- pinned in ui_term.cpp's ctor) to
 * termgfx's truecolor present path (termgfx_termio_present_rgbx). GPLv3+, like
 * the EasyRPG tree this links into.
 */
#ifndef SYNCRPG_VIDEO_TERM_H_
#define SYNCRPG_VIDEO_TERM_H_

class Bitmap;

/* Hand the surface's pixel pointer + width/height to termgfx's truecolor
 * present. The surface must be 32-bit with memory byte order R,G,B,X (the pad
 * byte is ignored) -- ui_term.cpp pins that global format. A no-op off a live
 * terminal session (handled inside termgfx). */
void video_term_present(const Bitmap& surf);

/* Step the game resolution to the next of Original (320x240) -> Widescreen
 * (416x240) -> Ultrawide (560x240) -> Original, applying it live. The door's F4
 * hotkey (input_term.cpp) is the only caller. A no-op for a game that pins its
 * own resolution (Player::has_custom_resolution) -- the same games EasyRPG locks
 * the option out for. Widescreen/Ultrawide are EasyRPG's experimental fake-
 * resolution modes. */
void video_term_cycle_resolution();

/* Point the F4 persistence at a caller's per-user EasyRPG config.ini directory
 * (the --save-path dir), so an F4 choice is remembered for that user's next
 * session. Pass NULL/"" to disable persistence (no per-user dir -- the choice is
 * then session-only). Called once at startup from main(). */
void video_term_set_config_dir(const char *dir);

#endif /* SYNCRPG_VIDEO_TERM_H_ */
