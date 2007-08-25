#ifndef BITMAP_CON_H
#define BITMAP_CON_H

#include "vidmodes.h"
#include "threadwrap.h"

extern struct video_stats vstat;
extern pthread_mutex_t vstatlock;
extern sem_t	drawn_sem;

int bitmap_gettext(int sx, int sy, int ex, int ey, void *fill);
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill);
void bitmap_gotoxy(int x, int y);
void bitmap_setcursortype(int type);
int bitmap_setfont(int font, int force);
int bitmap_getfont(void);
int bitmap_loadfont(char *filename);

void send_rectangle(int xoffset, int yoffset, int width, int height, int force);
int bitmap_init_mode(int mode, int *width, int *height);
int bitmap_init(void	(*drawrect_cb)		(int xpos, int ypos, int width, int height, unsigned char *data));

#endif
