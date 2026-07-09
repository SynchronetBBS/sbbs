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

#endif /* SYNCMOO1_CONFIG_H */
