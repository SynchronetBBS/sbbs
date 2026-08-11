#ifndef TERMGFX_FONT_H_
#define TERMGFX_FONT_H_

#include <stdint.h>

/* font.h -- a 5x7 bitmap font drawn INTO an indexed framebuffer.
 *
 * WHY A DOOR NEEDS ITS OWN FONT. Text written to the terminal owns whole
 * character cells: a cell carries a background colour and there is no
 * "leave what is there" value for it, in cterm or anywhere else, so a
 * caption over a picture arrives as a rectangle of background with the
 * caption in it. That is fine for a status line on its own row and wrong for
 * anything meant to float over the game.
 *
 * Drawing the glyphs as PIXELS avoids it entirely: they land in the
 * framebuffer with everything else, cost no character cells, and can be
 * shadowed so they stay readable over whatever they happen to cross.
 *
 * INDEXED, not RGB: a door's framebuffer holds palette indices, so a colour
 * here is an index the caller already owns. Most termgfx doors have a light
 * and a dark chrome entry for exactly this, which makes a shadowed glyph
 * free -- no new registers.
 *
 * UPPERCASE ONLY, plus digits and the punctuation a game label actually
 * uses. Lowercase folds to uppercase rather than rendering blank, so a
 * caller need not shout in its format strings. Five pixels wide is not
 * enough for descenders to be worth having, and a mixed-case font at this
 * size reads worse than a clean set of capitals.
 */

#define TERMGFX_FONT_W  5    /* glyph width, in pixels, before scaling  */
#define TERMGFX_FONT_H  7    /* glyph height                            */
#define TERMGFX_FONT_GAP 1   /* blank columns between glyphs            */

/* Pixels one string occupies at this scale, including the gaps between
 * glyphs but not after the last one. For centring, or for deciding a label
 * will not fit before drawing half of it. */
int termgfx_font_width(const char *s, int scale);

/* Draw `s` with its top-left at (x, y), every pixel `scale` times bigger.
 *
 * `shadow` is a palette index drawn one scaled pixel down and right first,
 * or -1 for none. A shadow is what makes a caption survive crossing from
 * sky to grass mid-word, so it is a parameter rather than a policy: the
 * caller knows whether its background is busy.
 *
 * Clipped against the framebuffer, so a caption running off the edge loses
 * its tail rather than the door. Returns the width drawn.
 */
int termgfx_font_text(uint8_t *fb, int fbw, int fbh, int x, int y,
                      const char *s, int scale, uint8_t color, int shadow);

/* One glyph, same rules. `ch` is folded to uppercase; anything with no
 * glyph draws nothing and still advances, so an unexpected byte leaves a
 * space rather than a smear. */
void termgfx_font_glyph(uint8_t *fb, int fbw, int fbh, int x, int y,
                        char ch, int scale, uint8_t color, int shadow);

#endif /* TERMGFX_FONT_H_ */
