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

/* syncmoo1.ini [audio] music_quality -- Ogg/Vorbis VBR quality (0.0..1.0) for
 * encoded music tracks. Defaults to TERMGFX_MUSIC_QUALITY_DEFAULT. */
double sm_config_music_quality(void);

#endif /* SYNCMOO1_CONFIG_H */
