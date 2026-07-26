/* SyncSCUMM -- the door's own key-help card.
 *
 * WHY IT EXISTS. This was the only termgfx door with no help of any kind, and
 * the games it hosts are the ones that need it most: SCUMM v1/v2 (Maniac
 * Mansion) has no menu bar to stumble into, so a caller who wants OUT has
 * nothing to find. The one key that matters -- the GMM hotkey, which is where
 * Quit lives -- was announced only to stderr, and a BBS-launched door has its
 * stderr discarded, so the single place it was written down was invisible in
 * exactly the situation it was needed.
 *
 * WHY A DEDICATED KEY, and not "any unmapped key" the way syncretro does it.
 * syncretro owns its entire binding table, so "unbound" is a fact it knows.
 * This door is a HOST: it forwards nearly every key to ScummVM and cannot see
 * whether the engine consumed one. Worse, ScummVM's save/load dialogs take free
 * text -- a catch-all would fire on every letter of a save-game name.
 *
 * WHY Ctrl-K AND F1. Ctrl-K is what syncconquer chose and is free in SCUMM.
 * F1 matches the other doors' muscle memory but is forwarded to the engine
 * today, so it is the one to drop if a game turns out to want it. NOT Ctrl-H:
 * that is Backspace (0x08), which SCUMM needs for save-game name entry -- and
 * terminals disagree about whether Backspace sends 0x08 or 0x7f, so binding it
 * would fire the card on a routine keystroke for half of them.
 *
 * HOW IT DRAWS. Plain positioned ANSI over the picture -- a solid centred
 * panel, no box-drawing glyphs (those split CP437 vs UTF-8 across SyncTERM and
 * Windows Terminal), the same choice syncconquer's card made. The door stops
 * presenting frames while it is up, so the image cannot repaint over it, and
 * invalidates the frame cache on dismissal so the picture comes back whole.
 */
#ifndef SYNCSCUMM_HELP_TERM_H_
#define SYNCSCUMM_HELP_TERM_H_

/* Is the card up? While it is, the door swallows keys and stops presenting. */
bool help_term_active();

/* Draw it (Ctrl-K / F1). */
void help_term_show();

/* Take it down and repaint the game. Called for ANY key while it is up: a help
 * page you cannot get out of is worse than no help page, and a caller should
 * not have to guess which key closes it. */
void help_term_dismiss();

/* The GMM hotkey letter, so the card can name the key the sysop configured
 * rather than a hardcoded Ctrl-G; 0 when the sysop turned it off. */
void help_term_set_menu_key(int letter);

#endif /* SYNCSCUMM_HELP_TERM_H_ */
