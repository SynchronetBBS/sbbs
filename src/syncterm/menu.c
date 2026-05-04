/* Copyright (C), 2007 by Stephen Hurd */

#include <ciolib.h>
#include <genwrap.h>
#include <uifc.h>
#include <vidmodes.h>

#include "bbslist.h"
#include "conn.h"
#include "menu.h"
#include "cterm.h"
#include "syncterm.h"
#include "term.h"
#include "uifcinit.h"
#include "window.h"

void
viewscroll(void)
{
	int                   top;
	int                   key;
	int                   i;
	struct vmem_cell     *scrollback;
	struct  text_info     txtinfo;
	int                   x, y;
	uint16_t              vs_hover_id = 0;
	struct mouse_event    mevent;
	struct ciolib_screen *savscrn;

	x = wherex();
	y = wherey();
	uifcbail();
	gettextinfo(&txtinfo);

        /* too large for alloca() */
	scrollback =
	    malloc((scrollback_buf
	        == NULL ? 0 : (term.width * sizeof(*scrollback) * cterm->backlines))
	        + (txtinfo.screenheight * txtinfo.screenwidth * sizeof(*scrollback)));
	if (scrollback == NULL)
		return;
	int lines = 0;
	if (cterm->backstart > 0) {
		lines = cterm->backlines - cterm->backstart;
		memcpy(scrollback, cterm->scrollback + term.width * cterm->backstart, term.width * lines * sizeof(*scrollback));
	}
	memcpy(scrollback + term.width * lines, cterm->scrollback, term.width * sizeof(*scrollback) * cterm->backpos);
	int sblines = cterm->backpos + lines;
	vmem_gettext(1, 1, txtinfo.screenwidth, txtinfo.screenheight, scrollback + sblines * cterm->width);
	savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	drawwin();
	top = sblines;
	set_modepalette(palettes[COLOUR_PALETTE]);
	gotoxy(1, 1);
	textattr(uifc.hclr | (uifc.bclr << 4) | BLINK);
	ciomouse_addevent(CIOLIB_BUTTON_1_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
	ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
	ciomouse_addevent(CIOLIB_MOUSE_MOVE);
	for (i = 0; (!i) && (!quitting);) {
		if (top < 0)
			top = 0;
		if (top > sblines)
			top = sblines;
		vmem_puttext(term.x - 1, term.y - 1, term.x + term.width - 2, term.y + term.height - 2,
		    scrollback + (term.width * top));
		cputs("Scrollback");
		gotoxy(cterm->width - 9, 1);
		cputs("Scrollback");
		gotoxy(1, 1);
		key = getch();
		switch (key) {
			case 0xe0:
			case 0:
				switch (key | getch() << 8) {
					case CIO_KEY_QUIT:
						check_exit(true);
						break;
					case CIO_KEY_MOUSE:
						getmouse(&mevent);
						switch (mevent.event) {
							case CIOLIB_BUTTON_1_CLICK:
							{
								int cell = (top + mevent.starty - 1) * term.width + (mevent.startx - 1);
								int vis_rows = sblines + term.height - top;
								if (cell >= 0 && cell < (sblines + term.height) * term.width
								    && scrollback[cell].hyperlink_id) {
									if (!ciolib_open_hyperlink(scrollback[cell].hyperlink_id)) {
										char *url = ciolib_get_hyperlink_url(scrollback[cell].hyperlink_id);
										if (url) {
											copytext(url, strlen(url));
											uifcmsg("URL copied to clipboard", url);
											free(url);
										}
									}
								}
								else {
									char *url = detect_url_at(
									    scrollback + top * term.width,
									    term.width, vis_rows,
									    mevent.startx - 1,
									    mevent.starty - 1);
									if (url) {
										if (!cio_api.openurl
										    || !cio_api.openurl(url)) {
											copytext(url, strlen(url));
											uifcmsg("URL copied to clipboard",
											    url);
										}
										free(url);
									}
								}
								break;
							}
							case CIOLIB_MOUSE_MOVE:
							{
								int cell = (top + mevent.starty - 1) * term.width + (mevent.startx - 1);
								uint16_t hid = 0;
								if (cell >= 0 && cell < (sblines + term.height) * term.width)
									hid = scrollback[cell].hyperlink_id;
								if (hid != vs_hover_id) {
									vs_hover_id = hid;
									if (vs_hover_id) {
										char *url = ciolib_get_hyperlink_url(vs_hover_id);
										if (url) {
											show_status_url(url);
											free(url);
										}
										mousepointer(CIOLIB_MOUSEPTR_ARROW);
									}
									else {
										show_status_url("");
										mousepointer(CIOLIB_MOUSEPTR_BAR);
									}
								}
								break;
							}
							case CIOLIB_BUTTON_1_DRAG_START:
								mousedrag(scrollback, false);
								break;
							case CIOLIB_BUTTON_4_PRESS:
								top--;
								break;
							case CIOLIB_BUTTON_5_PRESS:
								top++;
								if (top > sblines)
									i = 1;
								break;
						}
						break;
					case CIO_KEY_UP:
						top--;
						break;
					case CIO_KEY_DOWN:
						top++;
						break;
					case CIO_KEY_PPAGE:
						top -= term.height;
						break;
					case CIO_KEY_NPAGE:
						top += term.height;
						break;
					case CIO_KEY_F(1):
						init_uifc(false, false);
						uifc.helpbuf = "`Scrollback Buffer`\n\n"
						    "~ J ~ or ~ Up Arrow ~   Scrolls up one line\n"
						    "~ K ~ or ~ Down Arrow ~ Scrolls down one line\n"
						    "~ H ~ or ~ Page Up ~    Scrolls up one screen\n"
						    "~ L ~ or ~ Page Down ~  Scrolls down one screen\n"
						    "~ Mouse Wheel ~         Scrolls up/down one line\n"
						    "~ Click ~               Opens hyperlink or URL\n"
						    "~ ESC ~                 Returns to terminal\n";
						uifc.showhelp();
						check_exit(false);
						uifcbail();
						drawwin();
						break;
				}
				break;
			case 'j':
			case 'J':
				top--;
				break;
			case 'k':
			case 'K':
				top++;
				break;
			case 'h':
			case 'H':
				top -= term.height;
				break;
			case 'l':
			case 'L':
				top += term.height;
				break;
			case ESC:
				i = 1;
				break;
		}
	}
	restorescreen(savscrn);
	gotoxy(x, y);
	free(scrollback);
	freescreen(savscrn);
	return;
}

