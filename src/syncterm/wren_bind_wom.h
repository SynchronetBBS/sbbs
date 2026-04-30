#ifndef WREN_BIND_WOM_H
#define WREN_BIND_WOM_H

/* Per-module declaration for the WOM (Wren Object Model) deserialiser.
 * The Wren-side WOM class lives in scripts/syncterm.wren; serialise is
 * pure Wren, deserialise calls into the C parser implemented in
 * wren_bind_wom.c.  The BINDINGS table in wren_bind.c references
 * fn_WOM_deserialize. */

#include "wren.h"

void fn_WOM_deserialize(WrenVM *vm);

#endif /* WREN_BIND_WOM_H */
