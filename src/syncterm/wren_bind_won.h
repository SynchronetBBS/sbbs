#ifndef WREN_BIND_WON_H
#define WREN_BIND_WON_H

/* Per-module declaration for the WON (Wren Object Notation) deserialiser.
 * The Wren-side WON class lives in scripts/syncterm.wren; serialise is
 * pure Wren, deserialise calls into the C parser implemented in
 * wren_bind_won.c.  The BINDINGS table in wren_bind.c references
 * fn_WON_deserialize. */

#include "wren.h"

void fn_WON_deserialize(WrenVM *vm);

void wren_won_error_allocate(WrenVM *vm);
void wren_won_error_finalize(void *data);

void fn_WONError_code(WrenVM *vm);
void fn_WONError_offset(WrenVM *vm);
void fn_WONError_message(WrenVM *vm);
void fn_WONError_toString(WrenVM *vm);

#endif /* WREN_BIND_WON_H */
