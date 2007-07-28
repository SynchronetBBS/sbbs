#ifndef _HTMLWIN_H_
#define _HTMLWIN_H_

#ifdef __cplusplus
extern "C" {
#endif

int run_html(void);
void hide_html(void);
void iconize_html(void);
void raise_html(void);
void add_html_char(char ch);
void add_html(const char *buf);
void show_html(const char *address, int width, int height, int xpos, int ypos, void(*callback)(const char *), const char *page);

#ifdef __cplusplus
}
#endif

#endif
