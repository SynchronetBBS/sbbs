#ifndef WREN_BIND_SCREEN_H
#define WREN_BIND_SCREEN_H

/* Screen / Input / Cell / Cells / Font / Hyperlinks / Color / Palette
 * / CustomCursor / VideoFlags / KeyEvent / MouseEvent — the visual
 * + input foreign surface.  Declarations pulled in by wren_bind.c
 * so the BINDINGS table and wren_bind_lookup_class can reach the
 * implementations defined in wren_bind_screen.c. */

#include "wren.h"

struct mouse_event;

bool wren_bind_resume_parked_key(int code);
bool wren_bind_resume_parked_mouse(struct mouse_event *ev);
void fnColor_fromAttr(WrenVM *vm);
void fnColor_fromRgb(WrenVM *vm);
void fnColor_toLegacyAttr(WrenVM *vm);
void fnCustomCursor_apply(WrenVM *vm);
void fnCustomCursorpresetLegacy_(WrenVM *vm);
void fnPalette_mode(WrenVM *vm);
void fnPalette_mode_set(WrenVM *vm);
void fnPalette_subscript(WrenVM *vm);
void fnPalette_subscript_set(WrenVM *vm);
void fnScreenFonts_subscript(WrenVM *vm);
void fnScreenFonts_subscript_set(WrenVM *vm);
void fnScreenWindow_bounds(WrenVM *vm);
void fnScreenWindow_bounds_set(WrenVM *vm);
void fnScreenWindow_clear(WrenVM *vm);
void fnScreenWindow_clearToLineEnd(WrenVM *vm);
void fnScreenWindow_deleteLine(WrenVM *vm);
void fnScreenWindow_insertLine(WrenVM *vm);
void fnScreenWindow_position(WrenVM *vm);
void fnScreenWindow_position_set(WrenVM *vm);
void fnScreenWindow_print(WrenVM *vm);
void fnScreenWindow_putChar(WrenVM *vm);
void fnScreenWindow_scroll(WrenVM *vm);
void fnVideoFlags_apply(WrenVM *vm);
void fnVideoFlags_expand(WrenVM *vm);
void fnVideoFlags_expand_static(WrenVM *vm);
void fn_Cell_bgPalette(WrenVM *vm);
void fn_Cell_bgPalette_set(WrenVM *vm);
void fn_Cell_bgRgb(WrenVM *vm);
void fn_Cell_bgRgb_set(WrenVM *vm);
void fn_Cell_blink(WrenVM *vm);
void fn_Cell_blink_set(WrenVM *vm);
void fn_Cell_bright(WrenVM *vm);
void fn_Cell_bright_set(WrenVM *vm);
void fn_Cell_ch(WrenVM *vm);
void fn_Cell_chByte(WrenVM *vm);
void fn_Cell_chByte_set(WrenVM *vm);
void fn_Cell_ch_set(WrenVM *vm);
void fn_Cell_fgPalette(WrenVM *vm);
void fn_Cell_fgPalette_set(WrenVM *vm);
void fn_Cell_fgRgb(WrenVM *vm);
void fn_Cell_fgRgb_set(WrenVM *vm);
void fn_Cell_font(WrenVM *vm);
void fn_Cell_font_set(WrenVM *vm);
void fn_Cell_hyperlinkId(WrenVM *vm);
void fn_Cell_hyperlinkId_set(WrenVM *vm);
void fn_Cell_legacyAttr(WrenVM *vm);
void fn_Cell_legacyAttr_set(WrenVM *vm);
void fn_Cell_toString(WrenVM *vm);
void fn_Font_available(WrenVM *vm);
void fn_Font_codepage(WrenVM *vm);
void fn_Font_codepageOf(WrenVM *vm);
void fn_Font_count(WrenVM *vm);
void fn_Font_name(WrenVM *vm);
void fn_Hyperlinks_add(WrenVM *vm);
void fn_Hyperlinks_containsKey(WrenVM *vm);
void fn_Hyperlinks_params(WrenVM *vm);
void fn_Hyperlinks_subscript(WrenVM *vm);
void fn_Input_mouseVisible_set(WrenVM *vm);
void fn_Input_mouseEvents(WrenVM *vm);
void fn_Input_mouseEvents_set(WrenVM *vm);
void fn_Input_enableMouseEvent(WrenVM *vm);
void fn_Input_disableMouseEvent(WrenVM *vm);
void fn_Input_mousedrag(WrenVM *vm);
void fn_Input_next(WrenVM *vm);
void fn_Input_nextEvent(WrenVM *vm);
void fn_Input_next_ms(WrenVM *vm);
void fn_Input_poll(WrenVM *vm);
void fn_Input_ungetKey_(WrenVM *vm);
void fn_Input_ungetMouse_(WrenVM *vm);
void fn_KeyEvent_code(WrenVM *vm);
void fn_KeyEvent_codepoint(WrenVM *vm);
void fn_KeyEvent_text(WrenVM *vm);
void fn_KeyEvent_toString(WrenVM *vm);
void fn_MouseEvent_endX(WrenVM *vm);
void fn_MouseEvent_endY(WrenVM *vm);
void fn_MouseEvent_event(WrenVM *vm);
void fn_MouseEvent_modifiers(WrenVM *vm);
void fn_MouseEvent_startX(WrenVM *vm);
void fn_MouseEvent_startY(WrenVM *vm);
void fn_MouseEvent_toString(WrenVM *vm);
void fn_Screen_attr_set(WrenVM *vm);
void fn_Screen_hyperlinkId(WrenVM *vm);
void fn_Screen_hyperlinkId_set(WrenVM *vm);
void fn_Screen_moveRect(WrenVM *vm);
void fn_Screen_readRect(WrenVM *vm);
void fn_Screen_restore(WrenVM *vm);
void fn_Screen_save(WrenVM *vm);
void fn_Screen_size(WrenVM *vm);
void fn_Screen_writeRect(WrenVM *vm);
void fn_Screen_putRect_3(WrenVM *vm);
void fn_Screen_putRect_7(WrenVM *vm);
void fn_Surface_count(WrenVM *vm);
void fn_Surface_iterate(WrenVM *vm);
void fn_Surface_iteratorValue(WrenVM *vm);
void fn_Surface_subscript(WrenVM *vm);
void fn_Surface_width(WrenVM *vm);
void fn_Surface_height(WrenVM *vm);
void fn_Surface_cellAt(WrenVM *vm);
void fn_Surface_putRect_3(WrenVM *vm);
void fn_Surface_putRect_7(WrenVM *vm);
void fn_Surface_fill_(WrenVM *vm);
void fn_Surface_toString(WrenVM *vm);
void wren_cell_allocate(WrenVM *vm);
void wren_cell_finalize(void *data);
void wren_surface_allocate(WrenVM *vm);
void wren_surface_finalize(void *data);
void wren_custom_cursor_allocate(WrenVM *vm);
void wren_custom_cursor_finalize(void *data);
void wren_key_event_allocate(WrenVM *vm);
void wren_key_event_finalize(void *data);
void wren_mouse_event_allocate(WrenVM *vm);
void wren_mouse_event_finalize(void *data);
void wren_video_flags_allocate(WrenVM *vm);
void wren_video_flags_finalize(void *data);


/* Macro-generated CustomCursor / VideoFlags accessors (CC_INT_FIELD,
 * CC_BOOL_FIELD, VF_FLAG).  Each field NAME expands into four fns:
 * NAME_static, NAME_set_static, NAME, NAME_set. */
void fnCustomCursor_startLine(WrenVM *vm);
void fnCustomCursor_startLine_set(WrenVM *vm);
void fnCustomCursor_startLine_static(WrenVM *vm);
void fnCustomCursor_startLine_set_static(WrenVM *vm);
void fnCustomCursor_endLine(WrenVM *vm);
void fnCustomCursor_endLine_set(WrenVM *vm);
void fnCustomCursor_endLine_static(WrenVM *vm);
void fnCustomCursor_endLine_set_static(WrenVM *vm);
void fnCustomCursor_range(WrenVM *vm);
void fnCustomCursor_range_set(WrenVM *vm);
void fnCustomCursor_range_static(WrenVM *vm);
void fnCustomCursor_range_set_static(WrenVM *vm);
void fnCustomCursor_blink(WrenVM *vm);
void fnCustomCursor_blink_set(WrenVM *vm);
void fnCustomCursor_blink_static(WrenVM *vm);
void fnCustomCursor_blink_set_static(WrenVM *vm);
void fnCustomCursor_visible(WrenVM *vm);
void fnCustomCursor_visible_set(WrenVM *vm);
void fnCustomCursor_visible_static(WrenVM *vm);
void fnCustomCursor_visible_set_static(WrenVM *vm);
void fnVideoFlags_altChars(WrenVM *vm);
void fnVideoFlags_altChars_set(WrenVM *vm);
void fnVideoFlags_altChars_static(WrenVM *vm);
void fnVideoFlags_altChars_set_static(WrenVM *vm);
void fnVideoFlags_noBright(WrenVM *vm);
void fnVideoFlags_noBright_set(WrenVM *vm);
void fnVideoFlags_noBright_static(WrenVM *vm);
void fnVideoFlags_noBright_set_static(WrenVM *vm);
void fnVideoFlags_bgBright(WrenVM *vm);
void fnVideoFlags_bgBright_set(WrenVM *vm);
void fnVideoFlags_bgBright_static(WrenVM *vm);
void fnVideoFlags_bgBright_set_static(WrenVM *vm);
void fnVideoFlags_blinkAltChars(WrenVM *vm);
void fnVideoFlags_blinkAltChars_set(WrenVM *vm);
void fnVideoFlags_blinkAltChars_static(WrenVM *vm);
void fnVideoFlags_blinkAltChars_set_static(WrenVM *vm);
void fnVideoFlags_noBlink(WrenVM *vm);
void fnVideoFlags_noBlink_set(WrenVM *vm);
void fnVideoFlags_noBlink_static(WrenVM *vm);
void fnVideoFlags_noBlink_set_static(WrenVM *vm);
void fnVideoFlags_expand(WrenVM *vm);
void fnVideoFlags_expand_set(WrenVM *vm);
void fnVideoFlags_expand_static(WrenVM *vm);
void fnVideoFlags_expand_set_static(WrenVM *vm);
void fnVideoFlags_lineGraphicsExpand(WrenVM *vm);
void fnVideoFlags_lineGraphicsExpand_set(WrenVM *vm);
void fnVideoFlags_lineGraphicsExpand_static(WrenVM *vm);
void fnVideoFlags_lineGraphicsExpand_set_static(WrenVM *vm);


/* Macro-generated SCREEN_SUPPORTS accessors — bit-test against
 * cio_api.options. */
void fnScreenSupports_loadableFonts(WrenVM *vm);
void fnScreenSupports_altBlinkFont(WrenVM *vm);
void fnScreenSupports_altBoldFont(WrenVM *vm);
void fnScreenSupports_brightBackground(WrenVM *vm);
void fnScreenSupports_paletteChange(WrenVM *vm);
void fnScreenSupports_pixels(WrenVM *vm);
void fnScreenSupports_customCursor(WrenVM *vm);
void fnScreenSupports_fontSelection(WrenVM *vm);
void fnScreenSupports_windowTitle(WrenVM *vm);
void fnScreenSupports_windowName(WrenVM *vm);
void fnScreenSupports_windowIcon(WrenVM *vm);
void fnScreenSupports_extendedPalette(WrenVM *vm);
void fnScreenSupports_blockyScaling(WrenVM *vm);
void fnScreenSupports_externalScaling(WrenVM *vm);
void fnScreenSupports_closeLock(WrenVM *vm);


#endif /* WREN_BIND_SCREEN_H */
