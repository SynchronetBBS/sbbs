/* syncmoo1_config.h -- header for syncmoo1_config.c.
 *
 * The module's public contract (consumed by hw_sbbs.c's main()) lives in
 * syncmoo1.h -- DESIGN.md's single cross-module contract header, shared by
 * every syncmoo1_*.c file. This header just pulls that in so
 * syncmoo1_config.c has a matching-name header to #include, per the usual
 * .c/.h convention (mirrors syncmoo1_door.h/syncmoo1_io.h); it carries no
 * declarations of its own.
 */
#ifndef SYNCMOO1_CONFIG_H
#define SYNCMOO1_CONFIG_H

#include "syncmoo1.h"

/* syncmoo1.ini [debug] wire -- nonzero when the sysop enabled the wire dump.
 * Valid only after sm_config_apply(). The SYNCMOO1_WIREDUMP env var overrides
 * this (see sm_io_wiredump_open()); the ini is the sysop-facing switch. */
int sm_config_wire_enabled(void);

/* syncmoo1.ini [video] hand_cursor -- nonzero to draw 1oom's own hand-shaped
 * mouse cursor. Default off: the terminal shows its own pointer, which the
 * game's hand doesn't align with. See hw_video_get_buf() in hw_sbbs.c. */
int sm_config_hand_cursor(void);

/* syncmoo1.ini [audio] music_quality -- Ogg/Vorbis VBR quality (0.0..1.0) for
 * encoded music tracks. Defaults to TERMGFX_MUSIC_QUALITY_DEFAULT. */
double sm_config_music_quality(void);

/* Apply syncmoo1.ini's [1oom] section to the engine's option variables via
 * 1oom's own cfg_load(), then snapshot every option to a temp file. Call after
 * sm_config_read_ini() and BEFORE main_1oom(): cfg_load() of the USER's config
 * (inside options_parse()) must run afterwards so it can override. */
void sm_config_seed_1oom(void);

/* Rewrite the user's 1oom config, dropping every key still equal to the base
 * snapshot. Register with atexit() BEFORE calling main_1oom(), not a plain
 * call after it returns -- see hw_sbbs.c's main() for why the ordering
 * matters (cfg_save() only runs via main_1oom()'s own atexit(main_shutdown),
 * which fires at actual process exit, not when main_1oom() merely returns
 * control to its caller). Best-effort: any failure leaves the file exactly
 * as 1oom wrote it. */
void sm_config_prune_user_cfg(void);

#endif /* SYNCMOO1_CONFIG_H */
