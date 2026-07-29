/* syncretro_games.h -- what the CORE will not tell us about the cabinet.
 *
 * MAME 2003-Plus reports how many buttons a driver wants but sends no
 * SET_INPUT_DESCRIPTORS, so the door knows a cabinet has buttons and cannot
 * know what any of them IS. Measured: Battlezone fires on RetroPad Y, Centipede
 * on B -- there is no rule to infer, the driver decides. games.ini is that
 * table, entered by hand; this module reads the one section that matches the
 * romset being played. See GAMES_INI.md.
 *
 * Every getter answers "nothing known" when the file, the section or the key is
 * absent, which is the normal case: an arcade install with no games.ini is a
 * working install.
 */
#ifndef SYNCRETRO_GAMES_H_
#define SYNCRETRO_GAMES_H_

/* Read <dir>/games.ini, then <dir>/games.local.ini over the top of it, and
 * select the section named by rom_path's basename minus its extension.
 *
 * games.local.ini is the SYSOP'S file and the one to edit: games.ini is
 * shipped, so a pull or a merge overwrites anything put there. It holds only
 * what differs -- a key present in it wins at the same scope, everything else
 * keeps coming from the shipped file, so new titles upstream adds still arrive.
 * Both are optional; an install with neither is a working install.
 *
 * Replaces whatever was loaded before -- including on failure, so a console
 * with no file cannot inherit the previous cabinet's labels. `dir` is passed
 * explicitly because the door chdirs into the per-user sandbox before play
 * (main.c); callers pass sr_config_launch_dir(). A NULL or empty rom_path
 * selects nothing and is not an error. */
void sr_games_load(const char *dir, const char *rom_path);

/* What RetroPad `id` (RETRO_DEVICE_ID_JOYPAD_B / _A / _Y / _X / _L / _R) is on
 * this cabinet -- "Fire", "Thrust" -- or NULL if unrecorded. The pointer is
 * valid until the next sr_games_load(). */
const char *sr_games_button_label(int id);

/* The label for this cabinet's SECOND STICK ("Right tread"), or NULL if it has
 * one stick. The arcade profile binds I / K to the right stick on every
 * cabinet, so this is what keeps those keys off the help screen of the ~46 that
 * do not have one. */
const char *sr_games_stick2(void);

/* Nonzero when the section labelled at least one button. The help screen then
 * renders one line per key and OMITS unlabelled ids, so a section that labels
 * anything is asserting the rest do nothing -- see GAMES_INI.md sec 6. */
int sr_games_labelled(void);

/* How many frames to run UNPACED before the player is shown anything: the
 * cabinet's power-on self-test, which he can neither act on nor skip. 0 (the
 * default) runs the boot in real time, as the door always has.
 *
 * A root-level `boot_frames` -- one written before any [section] -- is the
 * install-wide default, and a section's own key overrides it. Unlike the button
 * labels this needs no per-romset measurement to be useful: warming up PAST a
 * short boot only lands the player further into the attract loop, which is
 * where they would have been sitting anyway. See GAMES_INI.md sec 13. */
int sr_games_boot_frames(void);

/* Where this cabinet's stick SITS when nobody is touching it, as a percentage
 * of the stick's own travel on each axis: -100 is hard left / fully up, 0 is the
 * middle, +100 hard right / fully down. Returns 1 when the section declared it
 * and fills both; 0 otherwise, with both set to 0.
 *
 * A control panel whose stick is a set of switches -- nearly all of them -- has
 * no rest position to declare and must not have one invented, so absence is the
 * normal answer and it means "leave the analog stick centred". The key exists
 * for the panels whose control is a POTENTIOMETER: a handlebar, a wheel, a
 * throttle. MAME centres those on its own declared middle, and that middle is
 * not where the real control rests -- Paperboy's bars read 45% of travel at rest
 * and its speed axis 57%, so a centred stick is a machine steering left at full
 * speed. There is no default and no root-level form: it is measured per cabinet
 * (GAMES_INI.md sec 15), and a guess drives the game on its own. */
int sr_games_analog_rest(int *x_pct, int *y_pct);

#endif /* SYNCRETRO_GAMES_H_ */
