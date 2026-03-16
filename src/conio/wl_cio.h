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

#include <threadwrap.h>

#include "ciolib.h"
#include "bitmap_con.h"
#include "wl_events.h"

#ifdef __cplusplus
extern "C" {
#endif

int wl_initciolib(int mode);
int wl_kbhit(void);
int wl_kbwait(int ms);
int wl_getch(void);
void wl_beep(void);
void wl_textmode(int mode);
void wl_settitle(const char *title);
void wl_setname(const char *name);
void wl_seticon(const void *icon, unsigned long size);
void wl_drawrect(struct rectlist *data);
void wl_flush(void);
double wl_getscaling(void);
void wl_setscaling(double newval);
enum ciolib_scaling wl_getscaling_type(void);
void wl_setscaling_type(enum ciolib_scaling newval);
void wl_setwinsize(int w, int h);
void wl_copytext(const char *text, size_t buflen);
char *wl_getcliptext(void);
int wl_mousepointer(enum ciolib_mouse_ptr type);

#ifdef __cplusplus
}
#endif

#endif
