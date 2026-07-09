/* syncmoo1_io.h -- header for syncmoo1_io.c.
 *
 * The module's public contract (consumed by hw_sbbs.c and, later, the input
 * module) lives in syncmoo1.h -- DESIGN.md's single cross-module contract
 * header, shared by every syncmoo1_*.c file. This header just pulls that in
 * so syncmoo1_io.c has a matching-name header to #include, per the usual .c/
 * .h convention; it carries no declarations of its own (nothing in this
 * module is private to a second consumer yet).
 */
#ifndef SYNCMOO1_IO_H
#define SYNCMOO1_IO_H

#include "syncmoo1.h"

#endif /* SYNCMOO1_IO_H */
