/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifdef __unix__
#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#undef beep
#undef getch
#undef ungetch
#undef getmouse
#undef ungetmouse
#include "curs_fix.h"

#ifdef __cplusplus
extern "C" {
#endif
int curs_puttext(int sx, int sy, int ex, int ey, void *fillbuf);
int curs_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fillbuf);
int curs_vmem_gettext(int sx, int sy, int ex, int ey, struct vmem_cell *fillbuf);
void curs_textattr(int attr);
int curs_kbhit(void);
void curs_gotoxy(int x, int y);
void curs_suspend(void);
void curs_resume(void);
int curs_initciolib(int inmode);
void curs_setcursortype(int type);
int curs_getch(void);
void curs_textmode(int mode);
int curs_hidemouse(void);
int curs_showmouse(void);
void curs_beep(void);
int curs_getvideoflags(void);
void curs_setvideoflags(int flags);
int curs_setfont(int font, int force, int font_num);
int curs_getfont(int font_num);
int curs_set_modepalette(uint32_t p[16]);
int curs_get_modepalette(uint32_t p[16]);
int curs_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b);
int curs_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
#ifdef __cplusplus
}
#endif

#endif
