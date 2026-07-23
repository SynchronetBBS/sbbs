/* SyncRPG -- the door's own help/about overlay. GPLv3+, like the EasyRPG tree.
 *
 * WHY DOOR-SIDE. EasyRPG already has an about page (window_about.cpp, inside
 * Scene_Settings) reachable with F1 -- and forwarding F1 was trialled and
 * rejected: that scene also carries video/audio/input settings, key rebinding,
 * and a config write to a SYSTEM-WIDE profile directory, which escapes the
 * door's per-user sandbox. See the F1 note in input_term.cpp. So the door
 * draws its own page instead: controls, and the build stamp a caller would
 * otherwise only glimpse on the startup splash.
 *
 * HOW IT DRAWS. Straight onto the engine's display surface in the game's own
 * font, from BaseUi_termgfx::UpdateDisplay() just before video_term_present().
 * That deliberately avoids the alternative -- writing ANSI text over the
 * terminal and suppressing frame presents while it shows -- which would need
 * two new termgfx entry points (a raw-write and a repaint-invalidate) and
 * would have to fight the present path's whole-frame de-dupe to get the screen
 * back afterwards. Drawn per frame, so the engine repainting main_surface
 * underneath is not merely harmless but the mechanism by which the overlay
 * disappears the moment it is switched off. The game keeps running behind it:
 * RPG Maker has no pause, and inventing one for an overlay would change
 * gameplay.
 *
 * TRIGGERS (input_term.cpp / ui_term.cpp): F1, and the termgfx GMM hotkey
 * (Ctrl+<letter>, sysop-set via syncrpg.ini "[input] menu_key", off unless
 * configured). Both are explicit -- NOT a catch-all "any unmapped key", which
 * cannot be done safely here: the door's unmapped bucket is full of things
 * that are not keypresses (unrecognized CSI sequences, terminal replies,
 * stray bytes), and Ctrl+<letter> is indistinguishable from the bare letter at
 * this layer, so Ctrl-A would be the WASD movement key A.
 */
#ifndef SYNCRPG_HELP_TERM_H_
#define SYNCRPG_HELP_TERM_H_

class Bitmap;

/* Is the overlay currently up? While it is, input_term_pump() steers keys here
 * instead of into the engine's key state. */
bool help_term_active();

/* Show it (F1 / the GMM hotkey). */
void help_term_show();

/* Dismiss it. Called for ANY key while it is up -- a help page you cannot get
 * out of is worse than no help page, and a BBS caller should not have to guess
 * which key closes it. */
void help_term_dismiss();

/* Composite the page onto `surface`. Called once per frame from
 * UpdateDisplay() while active; a no-op otherwise. */
void help_term_draw(Bitmap &surface);

#endif /* SYNCRPG_HELP_TERM_H_ */
