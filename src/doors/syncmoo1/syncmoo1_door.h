/* syncmoo1_door.h -- header for syncmoo1_door.c.
 *
 * The module's public contract (consumed by hw_term.c's main() and
 * hw_video_draw_buf()) lives in syncmoo1.h -- DESIGN.md's single cross-
 * module contract header, shared by every syncmoo1_*.c file. This header
 * just pulls that in so syncmoo1_door.c has a matching-name header to
 * #include, per the usual .c/.h convention (mirrors syncmoo1_io.h/
 * syncmoo1_input.h); it carries no declarations of its own.
 */
#ifndef SYNCMOO1_DOOR_H
#define SYNCMOO1_DOOR_H

#include "syncmoo1.h"

#endif /* SYNCMOO1_DOOR_H */
