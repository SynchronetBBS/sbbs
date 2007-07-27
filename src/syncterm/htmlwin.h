#ifndef _HTMLWIN_H_
#define _HTMLWIN_H_

#ifdef __cplusplus
extern "C" {
#endif

int run_html(void);
void hide_html(void);
void show_html(const char *address, int width, int height, int xpos, int ypos, void(*callback)(const char *), const char *page);

#ifdef __cplusplus
}
#endif

#endif
