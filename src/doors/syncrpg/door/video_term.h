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

#endif /* SYNCRPG_VIDEO_TERM_H_ */
