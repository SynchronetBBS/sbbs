/* retro_options.h -- the core-option store: what a core advertises, what a
 * console pins, and what the core gets told when it asks.
 *
 * WHY THIS EXISTS AT ALL. Through M4 the door answered `false` to every
 * GET_VARIABLE, on the documented assumption that a core then falls back to its
 * own defaults. That assumption is not reliable. Measured with probe_core.c
 * against MAME 2003-Plus: answering false leaves `av_info.sample_rate` at
 * **0.0** and the core emits **zero audio samples** -- a silent door -- while
 * answering each query with the value the core ITSELF advertised as the default
 * yields 48000 Hz and exactly 792.0 audio frames per video frame. The core reads
 * all 23 options it advertises. "Answer nothing" and "answer the default" are
 * not the same thing, whatever the header comments say.
 *
 * So the store does two jobs:
 *
 *   1. Remember the default the core advertised, and hand it back on
 *      GET_VARIABLE. This is a bug fix, and it applies to every console.
 *   2. Let a CONSOLE pin an option (`-option key=value`), because some options
 *      are part of what the console IS, not a sysop's taste -- an arcade core
 *      that opens on a disclaimer screen a BBS player cannot dismiss is not
 *      configured, it is broken. Per M3_MULTICORE.md sec 3 that belongs in the
 *      console's lobby.js spec, which is what builds the command line.
 *
 * Not here, deliberately: a sysop-facing `[options]` section in syncretro.ini.
 * M3's rule is that syncretro.ini holds what is genuinely the sysop's ([audio],
 * [disc], [roms]) and the console's identity is code. Brightness and frameskip
 * would be defensible sysop knobs; add them when a sysop asks, with the console
 * pin still winning, rather than guessing the precedence now.
 *
 * See DESIGN.md sec 15 (M5) and retro_env.c.
 */
#ifndef SYNCRETRO_RETRO_OPTIONS_H_
#define SYNCRETRO_RETRO_OPTIONS_H_

#include "libretro.h"   /* VENDORED MIT-licensed libretro API header */

/* Record what a core advertises. One of these is called from retro_env.c per
 * SET_VARIABLES / SET_CORE_OPTIONS[_V2][_INTL]; each stores every key with its
 * default and applies any pin for that key.
 *
 * We answer GET_CORE_OPTIONS_VERSION with false -- i.e. "version 0" -- so a
 * conforming core only ever calls SET_VARIABLES, and ro_declare_v0() is the one
 * that runs in practice. The v1/v2 readers exist because a core that skips the
 * version handshake would otherwise leave us with an EMPTY table and no
 * complaint, which is precisely the silent failure this file was written to
 * end. */
void ro_declare_v0(const struct retro_variable *v);
void ro_declare_v1(const struct retro_core_option_definition *d);
void ro_declare_v2(const struct retro_core_options_v2 *o);

/* GET_VARIABLE: the pinned value if the console pinned one, else the core's own
 * advertised default. Returns NULL when the key was never advertised -- answer
 * false then, rather than inventing a value. The returned pointer is stable for
 * the process's lifetime, which libretro requires: a core may hold it. */
const char *ro_option_get(const char *key);

/* GET_VARIABLE_UPDATE. Always 0: options are resolved before the core runs and
 * nothing changes them mid-session (no in-game options menu -- M5's remainder).
 * A core that polls this every frame, as MAME 2003-Plus does, must get a
 * definite "no", not an unanswered call. */
int ro_options_changed(void);

#endif /* SYNCRETRO_RETRO_OPTIONS_H_ */
