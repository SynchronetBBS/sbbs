/* Copyright (C), 2007 by Stephen Hurd */

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

int run_html(int width, int height, int xpos, int ypos, void(*callback)(const char *), int(*ucallback)(const char *, char *, size_t, char *, size_t));
void hide_html(void);
void iconize_html(void);
void raise_html(void);
void lower_html(void);
void add_html_char(char ch);
void add_html(const char *buf);
void html_commit(void);
void show_html(const char *page);

#ifdef __cplusplus
}
#endif

#endif
