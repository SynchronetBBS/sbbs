#ifndef WIN32GDI_H
#define WIN32GDI_H

#include "bitmap_con.h"

int gdi_kbhit(void);
int gdi_getch(void);
void gdi_beep(void);
void gdi_textmode(int mode);
void gdi_setname(const char *name);
void gdi_settitle(const char *title);
void gdi_seticon(const void *icon, unsigned long size);
void gdi_copytext(const char *text, size_t buflen);
char * gdi_getcliptext(void);
int gdi_get_window_info(int *width, int *height, int *xpos, int *ypos);
int gdi_init(int mode);
int gdi_initciolib(int mode);
void gdi_drawrect(struct rectlist *data);
void gdi_flush(void);
void gdi_setscaling(int newval);
int gdi_getscaling(void);
int gdi_mousepointer(enum ciolib_mouse_ptr type);
int gdi_showmouse(void);
int gdi_hidemouse(void);
void gdi_setwinposition(int x, int y);
void gdi_setwinsize(int w, int h);

#endif
