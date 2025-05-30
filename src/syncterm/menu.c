/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: menu.c,v 1.67 2020/05/07 18:12:10 deuce Exp $ */

#include <ciolib.h>
#include <genwrap.h>
#include <uifc.h>
#include <vidmodes.h>

#include "bbslist.h"
#include "conn.h"
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
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
	ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
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
							case CIOLIB_BUTTON_1_DRAG_START:
								mousedrag(scrollback);
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
						    "~ L ~ or ~ Page Down ~  Scrolls down one screen\n";
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

int
syncmenu(struct bbslist *bbs, int *speed)
{
	char                 *opts[16];
	int                   opt = 0;
	int                   i, j;
	struct  text_info     txtinfo;
	struct ciolib_screen *savscrn;
	int                   ret;

	opts[opt++] = strdup("Scrollback ("ALT_KEY_NAMEP "-B)");
	opts[opt++] = strdup("Disconnect (Ctrl-Q)");
	opts[opt++] = strdup("Send Login ("ALT_KEY_NAMEP "-L)");
	opts[opt++] = strdup("Upload ("ALT_KEY_NAMEP "-U)");
	opts[opt++] = strdup("Download ("ALT_KEY_NAMEP "-D)");
	opts[opt++] = strdup("Change Output Rate (" ALT_KEY_NAMEP "-Up/" ALT_KEY_NAMEP "-Down)");
	opts[opt++] = strdup("Change Log Level");
	opts[opt++] = strdup("Capture Control ("ALT_KEY_NAMEP "-C)");
	opts[opt++] = strdup("ANSI Music Control ("ALT_KEY_NAMEP "-M)");
	opts[opt++] = strdup("Font Setup ("ALT_KEY_NAMEP "-F)");
	opts[opt++] = strdup("Toggle Doorway Mode");
	opts[opt++] = strdup("Toggle Remote Mouse (" ALT_KEY_NAMEP "-O)");
#ifndef WITHOUT_OOII
	opts[opt++] = strdup("Toggle Operation Overkill ][ Mode");
#endif
	opts[opt++] = strdup("Exit ("ALT_KEY_NAMEP "-X)");
	opts[opt++] = strdup("Edit Dialing Directory ("ALT_KEY_NAMEP "-E)");
	opts[opt++] = "";
	opt = 0;

	gettextinfo(&txtinfo);
	savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);

	if ((cio_api.mode != CIOLIB_MODE_CURSES)
	    && (cio_api.mode != CIOLIB_MODE_CURSES_IBM)
	    && (cio_api.mode != CIOLIB_MODE_ANSI)) {
		free(opts[1]);
		opts[1] = strdup("Disconnect ("ALT_KEY_NAMEP "-H)");
	}
	else {
		for (i = 0; opts[i][0]; i++) {
			char *p = strstr(opts[i], " (" ALT_KEY_NAMEP "-");
			if (p)
				*p = 0;
		}
	}

	for (ret = 0; (!ret) && (!quitting);) {
		init_uifc(false, !(bbs->nostatus));
		uifc.helpbuf = "`Online Menu`\n\n"
		    "`Scrollback`     Allows to you to view the scrollback buffer\n"
		    "`Disconnect`     Disconnects the current connection\n"
		    "`Send Login`     Sends the username and password pair separated by CR\n"
		    "`Upload`         Initiates a file upload\n"
		    "`Download`       Initiates a file download\n"
		    "`Output Rate`    Changes the speed characters are output to the screen\n"
		    "`Log Level`      Changes the minimum log level for transfer information\n"
		    "`Capture`        Enables/Disables screen capture\n"
		    "`ANSI Music`     Enables/Disables ANSI Music\n"
		    "`Font`           Changes the current font (when supported)\n"
		    "`Doorway Mode`   Toggles the current DoorWay (keyboard input) setting\n"
		    "`Remote Mouse`   Toggles remote mouse events\n"
#ifndef WITHOUT_OOII
		    "`Operation Overkill ][ Mode`\n"
		    "               Toggles the current Operation Overkill ][ setting\n"
#endif
		    "`Exit`           Disconnects and closes Syncterm\n"
		    "`Edit Dialing Directory`\n"
		    "               Opens the directory/setting menu\n";
		i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &opt, NULL, "SyncTERM Online Menu", opts);
		switch (i) {
			case -1: /* Cancel */
				check_exit(false);
				ret = 1;
				break;
			case 0: /* Scrollback */
				uifcbail();
				restorescreen(savscrn);
				viewscroll();
				break;
			case 1: /* Disconnect */
				ret = -1;
				break;
			case 2: /* Login */
				ret = 1;
				send_login(bbs);
				break;
			case 5: /* Output rate */
				if ((bbs->conn_type == CONN_TYPE_MODEM) || (bbs->conn_type == CONN_TYPE_SERIAL)
				    || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)) {
					uifcmsg("Not supported for this connection type",
					    "Cannot change the display rate for Modem/Serial connections.");
				}
				else if (speed != NULL) {
					j = get_rate_num(*speed);
					uifc.helpbuf = "`Output Rate`\n\n"
					    "The output rate is the rate in emulated \"bits per second\" to draw incoming\n"
					    "data on the screen.  This rate is a maximum, not guaranteed to be attained\n"
					    "In general, you will only use this option for ANSI animations.";
					i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, NULL, "Output Rate", rate_names);
					if (i == -1)
						check_exit(false);
					if (i >= 0)
						*speed = rates[i];
				}
				ret = 5;
				break;
			case 6: /* Change log level (temporarily) */
				j = log_level;
				uifc.helpbuf = "`Log Level\n\n"
				    "The log level changes the verbosity of messages shown in the transfer\n"
				    "window.  For the selected log level, messages of that level and those above\n"
				    "it will be displayed.";
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, NULL, "Log Level", log_levels);
				if (i == -1)
					check_exit(false);
				if (i >= 0)
					log_level = j;
				ret = 6;
				break;
			default:
				ret = i;
				uifcbail();
				restorescreen(savscrn);
				freescreen(savscrn);
				goto cleanup;
		}
	}

	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);

cleanup:
	for (i = 0; opts[i][0]; i++)
		free(opts[i]);
	return ret;
}
