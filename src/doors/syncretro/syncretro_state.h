/* syncretro_state.h -- the suspend/resume snapshot: its staleness key, its
 * path, and the write/restore pair.
 *
 * A libretro save-state blob carries NO version stamp, so restoring one into a
 * core, romset or option set it was not taken from feeds the emulator garbage.
 * Everything here exists to make that unreachable rather than unlikely: the key
 * derives from all three, and it is carried in the FILENAME so the lobby can
 * tell a live snapshot from a stale one with a single directory read.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#ifndef SYNCRETRO_STATE_H_
#define SYNCRETRO_STATE_H_

#include <stddef.h>

#include "retro_core.h"   /* rc_core_t */

/* <home>/<rom-basename-sans-extension>.<key8>.state. Returns 0 on success. */
int sr_state_path(char *out, size_t max, const char *home,
                  const char *rom_path, const char *key8);

/* Write the core's current state to `path`. 0 on success. Non-fatal to the
 * caller: -1 covers everything from "this core cannot snapshot" (no
 * serialize entry points, or a zero serialize_size) to a write failure. */
int sr_state_save(rc_core_t *core, const char *path);

/* Restore the core's state from `path`. 0 on success, negative on a failure
 * the caller should report and recover from (start fresh rather than continue
 * into a half-restored machine). A missing file is the ordinary case, not an
 * error, and is folded into the same negative return. */
int sr_state_load(rc_core_t *core, const char *path);

#endif /* SYNCRETRO_STATE_H_ */
