#ifndef WREN_BIND_HOOK_H
#define WREN_BIND_HOOK_H

/* Console / Hook / HookHandle — log buffer, hook registration entry
 * points, and the HookHandle foreign that wraps a registered entry.
 * Allocator / finalizer and accessor surface pulled in by wren_bind.c
 * so the BINDINGS table and wren_bind_lookup_class can reach the
 * implementations defined in wren_bind_hook.c. */

#include "wren.h"

void fn_Console_count(WrenVM *vm);
void fn_Console_total(WrenVM *vm);
void fn_Console_subscript(WrenVM *vm);
void fn_Console_clear(WrenVM *vm);
void fn_Console_markSeen(WrenVM *vm);
void fn_Console_iterate(WrenVM *vm);
void fn_Console_iteratorValue(WrenVM *vm);

void fn_Hook_onKey(WrenVM *vm);
void fn_Hook_onInput(WrenVM *vm);
void fn_Hook_onMouse(WrenVM *vm);
void fn_Hook_onStatus(WrenVM *vm);
void fn_Hook_onShellClose(WrenVM *vm);
void fn_Hook_onDisconnect(WrenVM *vm);
void fn_Hook_onKey_filtered(WrenVM *vm);
void fn_Hook_onInput_filtered(WrenVM *vm);
void fn_Hook_onMouse_filtered(WrenVM *vm);
void fn_Hook_onMatch(WrenVM *vm);
void fn_Hook_onMatchClean(WrenVM *vm);
void fn_Hook_every(WrenVM *vm);

void wren_hook_handle_allocate(WrenVM *vm);
void wren_hook_handle_finalize(void *data);
void fn_HookHandle_remove(WrenVM *vm);
void fn_HookHandle_callCount(WrenVM *vm);
void fn_HookHandle_totalRuntime(WrenVM *vm);
void fn_HookHandle_minRuntime(WrenVM *vm);
void fn_HookHandle_maxRuntime(WrenVM *vm);
void fn_HookHandle_toString(WrenVM *vm);

#endif /* WREN_BIND_HOOK_H */
