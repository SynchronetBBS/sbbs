#ifndef _HTMLWIN_H_
#define _HTMLWIN_H_

#ifdef __cplusplus
extern "C" {
#endif

enum url_actions {
	 URL_ACTION_REDIRECT
	,URL_ACTION_DOWNLOAD
	,URL_ACTION_ISGOOD
};

int run_html(void);
void hide_html(void);
void iconize_html(void);
void raise_html(void);
void add_html_char(char ch);
void add_html(const char *buf);
void show_html(int width, int height, int xpos, int ypos, void(*callback)(const char *), int(*url_callback)(const char *, char *, size_t, char *, size_t), const char *page);

#ifdef __cplusplus
}
#endif

#endif
