/* syncretro_title.h -- a display title from a ROM filename.
 *
 * The who's-online line names the CARTRIDGE ("playing Astrosmash
 * (Intellivision)"), and the cartridge's name normally has to come out of its
 * filename, because that is all a dumped ROM set carries.
 *
 * THE LOBBY'S PARSER IS THE AUTHORITATIVE ONE. syncretro_parse_title() in
 * exec/load/syncretro_lib.js does strictly more than this: it resolves the
 * sysop's curated games.ini names, and it strips cataloging tags against ~100
 * region / language / status words. Neither is duplicated here. A curated name
 * cannot be derived from a filename at all, so the lobby still passes -title
 * when one applies; and a second copy of that word list would be a table to
 * drift out of step, bought for no more than a slightly tidier status line.
 *
 * What this covers is the shape those word lists cannot help with, and that
 * every older console set uses: a trailing "(year)", optionally followed by the
 * publisher, plus any stacked "[!]" / "[a1]" dump markers.
 */
#ifndef SYNCRETRO_TITLE_H_
#define SYNCRETRO_TITLE_H_

#include <stddef.h>

/* "<roms>/Astrosmash (1981) (Mattel).int" -> "Astrosmash".
 *
 * Always writes a NUL-terminated string of at most cap-1 characters: the raw
 * basename when nothing matches, and "a cartridge" for a NULL/empty path, so a
 * caller never has to handle an empty status line. cap must be >= 1. */
void sr_title_from_rom(char *out, size_t cap, const char *path);

#endif /* SYNCRETRO_TITLE_H_ */
