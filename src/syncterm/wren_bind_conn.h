#ifndef WREN_BIND_CONN_H
#define WREN_BIND_CONN_H

/* Conn / CTerm / ExtAttr / LastColumnFlag — connection + emulator
 * state foreigns.  Allocators / finalizers and field accessors pulled
 * in by wren_bind.c so the BINDINGS table and wren_bind_lookup_class
 * can reach the implementations defined in wren_bind_conn.c. */

#include "wren.h"

void fn_Conn_send(WrenVM *vm);
void fn_Conn_sendRaw(WrenVM *vm);

void wren_conn_error_allocate(WrenVM *vm);
void wren_conn_error_finalize(void *data);
void fn_ConnError_code(WrenVM *vm);
void fn_ConnError_bytesSent(WrenVM *vm);
void fn_ConnError_message(WrenVM *vm);
void fn_ConnError_toString(WrenVM *vm);
void fn_Conn_close(WrenVM *vm);
void fn_Conn_endSession(WrenVM *vm);
void fn_Conn_paste(WrenVM *vm);
void fn_Conn_scrollback(WrenVM *vm);
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
void fn_CTerm_doorwayMode_set(WrenVM *vm);
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
void fn_CTerm_saveScreenshot(WrenVM *vm);
void fn_CTerm_atasciiInverse(WrenVM *vm);

/* Capture (streaming-log) verbs and state.  Replaces the old
 * CTerm.logMode / CTerm.logPaused getters. */
void fn_Capture_active(WrenVM *vm);
void fn_Capture_paused(WrenVM *vm);
void fn_Capture_start(WrenVM *vm);
void fn_Capture_stop(WrenVM *vm);
void fn_Capture_pause(WrenVM *vm);
void fn_Capture_resume(WrenVM *vm);
void fn_CTerm_ooiiMode(WrenVM *vm);
void fn_CTerm_ooiiMode_set(WrenVM *vm);
void fn_CTerm_mouseMode(WrenVM *vm);
void fn_CTerm_mouseDisabled(WrenVM *vm);
void fn_CTerm_mouseDisabled_set(WrenVM *vm);
void fn_CTerm_throttleSpeed(WrenVM *vm);
void fn_CTerm_throttleSpeed_set(WrenVM *vm);
void fn_CTerm_throttleSpeedUp(WrenVM *vm);
void fn_CTerm_throttleSpeedDown(WrenVM *vm);
void fn_Input_setupMouseEvents(WrenVM *vm);
void fn_CTerm_extAttr(WrenVM *vm);
void fn_CTerm_lastColumnFlag(WrenVM *vm);

void fn_BBS_elapsedSeconds(WrenVM *vm);
void fn_BBS_connTypeName(WrenVM *vm);

void fn_Host_safeMode(WrenVM *vm);
void fn_Host_textTerminal(WrenVM *vm);
void fn_Host_altKeyName(WrenVM *vm);
void fn_Host_altKeyShort(WrenVM *vm);
void fn_Host_print(WrenVM *vm);
void fn_Host_launchScript(WrenVM *vm);
void fn_Host_haveOOII(WrenVM *vm);
void fn_Host_maxOOIIMode(WrenVM *vm);
void fn_Host_outputRates(WrenVM *vm);
void fn_Host_outputRateNames(WrenVM *vm);
void fn_Host_logLevel(WrenVM *vm);
void fn_Host_logLevel_set(WrenVM *vm);
void fn_Host_logLevelNames(WrenVM *vm);
void fn_Host_fontControl(WrenVM *vm);
void fn_Host_editBBSList(WrenVM *vm);
void fn_Host_logUnread(WrenVM *vm);
void fn_Host_logUnreadError(WrenVM *vm);
void fn_Host_musicNames(WrenVM *vm);
void fn_Host_musicHelp(WrenVM *vm);

void fn_CTerm_music_set(WrenVM *vm);

void fn_Status_callable_get(WrenVM *vm);
void fn_Status_callable_set(WrenVM *vm);
void fn_Status_enabled(WrenVM *vm);

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
