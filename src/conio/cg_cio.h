#ifndef CG_CIO_H
#define CG_CIO_H

#ifdef __DARWIN__

#include "bitmap_con.h"

#ifdef __cplusplus
extern "C" {
#endif

int cg_initciolib(int mode);
int cg_kbhit(void);
int cg_kbwait(int ms);
int cg_getch(void);
void cg_beep(void);
void cg_textmode(int mode);
void cg_settitle(const char *title);
void cg_setname(const char *name);
void cg_seticon(const void *icon, unsigned long size);
void cg_copytext(const char *text, size_t buflen);
char *cg_getcliptext(void);
int cg_get_window_info(int *width, int *height, int *xpos, int *ypos);
void cg_drawrect(struct rectlist *data);
void cg_flush(void);
bool cg_openurl(const char *url);
int cg_mousepointer(enum ciolib_mouse_ptr type);
void cg_setwinposition(int x, int y);
void cg_setwinsize(int w, int h);
double cg_getscaling(void);
void cg_setscaling(double newval);
enum ciolib_scaling cg_getscaling_type(void);
void cg_setscaling_type(enum ciolib_scaling newtype);

/* Called from ciolib.c main() override */
int cg_start_app_thread(int argc, char **argv);
void cg_run_event_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __DARWIN__ */
#endif
