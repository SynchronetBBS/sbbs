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

/* 8 lowercase hex digits + NUL. `opts` is the resolved [options] flattened to
 * sorted "name=value" lines joined with '\n'. exec/load/syncretro_lib.js's
 * syncretro_state_key() implements the identical recipe; the two are pinned to
 * one golden value by test_statekey.c and exec/tests/syncretro_state_test.js. */
void sr_state_key(char out[9], const char *core_md5, const char *rom_md5,
                  const char *opts);

#endif /* SYNCRETRO_STATE_H_ */
