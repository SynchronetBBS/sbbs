#ifndef _SDL_CON_H_
#define _SDL_CON_H_

#ifdef __cplusplus
extern "C" {
#endif
/* Low-Level stuff (Shouldn't be used by ciolib programs */
int sdl_init(int mode);
int sdl_load_font(char *filename, int width, int height, double scale);
int sdl_init_mode(int mode);
int sdl_setup_colours(void);
int sdl_draw_char(unsigned short vch, int xpos, int ypos);
int sdl_screen_redraw(void);

/* High-level stuff */
int sdl_puttext(int sx, int sy, int ex, int ey, void *fill);
int sdl_gettext(int sx, int sy, int ex, int ey, void *fill);
void sdl_textattr(int attr);
int sdl_kbhit(void);
void sdl_delay(long msec);
int sdl_wherey(void);
int sdl_wherex(void);
int sdl_putch(int ch);
void sdl_gotoxy(int x, int y);
void sdl_initciolib(long inmode);
void sdl_gettextinfo(struct text_info *info);
void sdl_setcursortype(int type);
int sdl_getch(void);
int sdl_getche(void);
int sdl_beep(void);
void sdl_textmode(int mode);
void sdl_setname(const char *name);
void sdl_settitle(const char *title);
int sdl_hidemouse(void);
int sdl_showmouse(void);
void sdl_copytext(const char *text, size_t buflen);
char *sdl_getcliptext(void);
int sdl_setfont(int font, int force);
int sdl_getfont(void);
int sdl_loadfont(char *filename);
#ifdef __cplusplus
}
#endif

#endif
