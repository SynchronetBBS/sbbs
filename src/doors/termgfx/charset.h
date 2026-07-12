#ifndef TERMGFX_CHARSET_H_
#define TERMGFX_CHARSET_H_

// charset.h -- what character set the player's terminal is reading, and the
// line-drawing glyphs to match it.
//
// A game door writes RAW BYTES. Synchronet hands a native door its socket
// through a passthru thread and translates nothing (an EX_BIN door gets a raw
// pty; there is no CP437->UTF-8 conversion on the way out, the way there is for
// the BBS's own text). So a door that emits 0xC9 for a box corner draws a box on
// a CP437 terminal and mojibake on a UTF-8 one -- and both are common: SyncTERM
// speaks CP437, a player on ssh from Windows Terminal or a Linux xterm speaks
// UTF-8.
//
// The BBS already knows which, and writes it down: $SBBSNODE/terminal.ini holds
// a `chars` key with the negotiated client charset. That file is the answer, and
// this module reads it -- so a door asks once and gets glyphs that render.
//
// (SyncDuke and SyncDOOM each grew their own copy of this probe; they can drop
// theirs onto this one whenever someone is in those files.)

typedef enum {
	TERMGFX_CP437 = 0,       // the BBS default, and what SyncTERM speaks
	TERMGFX_UTF8  = 1
} termgfx_charset_t;

// The client's charset, from $SBBSNODE/terminal.ini. Read once and cached.
// CP437 when the BBS said nothing (no SBBSNODE, no file, no key): that is the
// historical default, and the glyphs below still render as *something* on a
// UTF-8 terminal, where guessing UTF-8 wrongly would emit multi-byte sequences a
// CP437 terminal draws as three pieces of line noise per corner.
termgfx_charset_t termgfx_client_charset(void);

// One box-drawing set. Each field is a NUL-terminated string, not a char: a
// glyph is one byte in CP437 and three in UTF-8, so the caller printf()s them
// with %s and never has to know which.
typedef struct {
	const char *tl, *tr, *bl, *br;    // corners
	const char *h,  *v;               // horizontal / vertical run
	const char *lt, *rt;              // left/right tee (a full-width divider)
} termgfx_box_t;

// Double-line and single-line sets for `cs`. Never NULL.
const termgfx_box_t *termgfx_box_double(termgfx_charset_t cs);
const termgfx_box_t *termgfx_box_single(termgfx_charset_t cs);

#endif // TERMGFX_CHARSET_H_
