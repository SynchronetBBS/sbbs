#ifndef WREN_BIND_CONN_H
#define WREN_BIND_CONN_H

/* Conn / CTerm / ExtAttr / LastColumnFlag — connection + emulator
 * state foreigns.  Allocators / finalizers and field accessors pulled
 * in by wren_bind.c so the BINDINGS table and wren_bind_lookup_class
 * can reach the implementations defined in wren_bind_conn.c. */

#include "wren.h"

void fn_Conn_send(WrenVM *vm);
void fn_Conn_sendRaw(WrenVM *vm);
void fn_Conn_close(WrenVM *vm);
void fn_Conn_connected(WrenVM *vm);
void fn_Conn_type(WrenVM *vm);
void fn_Conn_pending(WrenVM *vm);
void fn_Conn_queued(WrenVM *vm);
void fn_Conn_peek(WrenVM *vm);
void fn_Conn_recv(WrenVM *vm);

void fn_CTerm_emulation(WrenVM *vm);
void fn_CTerm_x(WrenVM *vm);
void fn_CTerm_y(WrenVM *vm);
void fn_CTerm_attr(WrenVM *vm);
void fn_CTerm_doorwayMode(WrenVM *vm);
void fn_CTerm_music(WrenVM *vm);
void fn_CTerm_width(WrenVM *vm);
void fn_CTerm_height(WrenVM *vm);
void fn_CTerm_write(WrenVM *vm);
void fn_CTerm_suspended_get(WrenVM *vm);
void fn_CTerm_suspended_set(WrenVM *vm);
void fn_CTerm_originX(WrenVM *vm);
void fn_CTerm_originY(WrenVM *vm);
void fn_CTerm_topMargin(WrenVM *vm);
void fn_CTerm_bottomMargin(WrenVM *vm);
void fn_CTerm_leftMargin(WrenVM *vm);
void fn_CTerm_rightMargin(WrenVM *vm);
void fn_CTerm_fontSlot(WrenVM *vm);
void fn_CTerm_scrollbackLines(WrenVM *vm);
void fn_CTerm_scrollbackWidth(WrenVM *vm);
void fn_CTerm_scrollbackPos(WrenVM *vm);
void fn_CTerm_scrollbackStart(WrenVM *vm);
void fn_CTerm_started(WrenVM *vm);
void fn_CTerm_skypix(WrenVM *vm);
void fn_CTerm_hasPaletteOverride(WrenVM *vm);
void fn_CTerm_statusDisplay(WrenVM *vm);
void fn_CTerm_refreshStatus(WrenVM *vm);
void fn_CTerm_sftpActive_get(WrenVM *vm);
void fn_CTerm_sftpActive_set(WrenVM *vm);
void fn_CTerm_fgColor(WrenVM *vm);
void fn_CTerm_bgColor(WrenVM *vm);
void fn_CTerm_paletteOverride(WrenVM *vm);
void fn_CTerm_altFonts(WrenVM *vm);
void fn_CTerm_logMode(WrenVM *vm);
void fn_CTerm_logPaused(WrenVM *vm);
void fn_CTerm_extAttr(WrenVM *vm);
void fn_CTerm_lastColumnFlag(WrenVM *vm);

void wren_extattr_allocate(WrenVM *vm);
void wren_extattr_finalize(void *data);
void wren_last_column_flag_allocate(WrenVM *vm);
void wren_last_column_flag_finalize(void *data);

/* Macro-generated EXTATTR_BOOL accessors. */
void fn_ExtAttr_autoWrap(WrenVM *vm);
void fn_ExtAttr_originMode(WrenVM *vm);
void fn_ExtAttr_sxScroll(WrenVM *vm);
void fn_ExtAttr_decLrmm(WrenVM *vm);
void fn_ExtAttr_bracketPaste(WrenVM *vm);
void fn_ExtAttr_decBkm(WrenVM *vm);
void fn_ExtAttr_prestelMosaic(WrenVM *vm);
void fn_ExtAttr_prestelDoubleHeight(WrenVM *vm);
void fn_ExtAttr_prestelConceal(WrenVM *vm);
void fn_ExtAttr_prestelSeparated(WrenVM *vm);
void fn_ExtAttr_prestelHold(WrenVM *vm);
void fn_ExtAttr_alternateKeypad(WrenVM *vm);

/* Macro-generated LCF_BOOL accessors. */
void fn_LCF_set(WrenVM *vm);
void fn_LCF_enabled(WrenVM *vm);
void fn_LCF_forced(WrenVM *vm);

#endif /* WREN_BIND_CONN_H */
