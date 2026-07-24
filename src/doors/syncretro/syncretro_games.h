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

/* Read <dir>/games.ini and select the section named by rom_path's basename,
 * minus its extension. Replaces whatever was loaded before -- including on
 * failure, so a console with no file cannot inherit the previous cabinet's
 * labels. `dir` is passed explicitly because the door chdirs into the per-user
 * sandbox before play (main.c); callers pass sr_config_launch_dir(). A NULL or
 * empty rom_path selects nothing and is not an error. */
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

#endif /* SYNCRETRO_GAMES_H_ */
