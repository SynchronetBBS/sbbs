/* Copyright (C), 2007 by Stephen Hurd */

#include <assert.h>
#include <dirwrap.h>
#include <ini_file.h>
#include <netwrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uifc.h>
#include <xpprintf.h>

#include "bbslist.h"
#include "ciolib.h"
#include "comio.h"
#include "conn.h"
#include "cterm.h"
#include "filepick.h"
#include "fonts.h"
#include "menu.h"
#include "named_str_list.h"
#include "syncterm.h"
#include "term.h"
#include "uifcinit.h"
#include "vidmodes.h"
#include "window.h"
#include "xpbeep.h"

#if defined(__unix__) && defined(SOUNDCARD_H_IN) && (SOUNDCARD_H_IN > 0) && !defined(_WIN32)
	#if SOUNDCARD_H_IN==1
		#include <sys/soundcard.h>
	#elif SOUNDCARD_H_IN==2
		#include <soundcard.h>
	#elif SOUNDCARD_H_IN==3
		#include <linux/soundcard.h>
	#else
		#ifndef USE_ALSA_SOUND
			#warning Cannot find soundcard.h
		#endif
	#endif
#endif

#ifdef _MSC_VER
	#pragma warning(disable : 4244 4267 4018)
#endif

struct sort_order_info {
	char *name;
	int flags;
	size_t offset;
	int length;
};

#define SORT_ORDER_REVERSED (1 << 0)
#define SORT_ORDER_STRING (1 << 1)
#define DEFAULT_SORT_ORDER_VALUE (0)

static struct sort_order_info sort_order[] = {
	{
		NULL,
		0,
		0,
		0
	},
	{ // 1
		"Entry Name",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, name),
		sizeof(((struct bbslist *)NULL)->name)
	},
	{
		"Date Added",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, added),
		sizeof(((struct bbslist *)NULL)->added)
	},
	{
		"Date Last Connected",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, connected),
		sizeof(((struct bbslist *)NULL)->connected)
	},
	{
		"Total Calls",
		SORT_ORDER_REVERSED,
		offsetof(struct bbslist, calls),
		sizeof(((struct bbslist *)NULL)->calls)
	},
	{ // 5
		"Dialing List",
		0,
		offsetof(struct bbslist, type),
		sizeof(((struct bbslist *)NULL)->type)
	},
	{
		"Address",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, addr),
		sizeof(((struct bbslist *)NULL)->addr)
	},
	{
		"Port",
		0,
		offsetof(struct bbslist, port),
		sizeof(((struct bbslist *)NULL)->port)
	},
	{
		"Username",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, user),
		sizeof(((struct bbslist *)NULL)->user)
	},
	{
		"Password",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, password),
		sizeof(((struct bbslist *)NULL)->password)
	},
	{ // 10
		"System Password",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, syspass),
		sizeof(((struct bbslist *)NULL)->syspass)
	},
	{
		"Connection Type",
		0,
		offsetof(struct bbslist, conn_type),
		sizeof(((struct bbslist *)NULL)->conn_type)
	},
	{
		"Screen Mode",
		0,
		offsetof(struct bbslist, screen_mode),
		sizeof(((struct bbslist *)NULL)->screen_mode)
	},
	{
		"Status Line Visibility",
		0,
		offsetof(struct bbslist, nostatus),
		sizeof(((struct bbslist *)NULL)->nostatus)
	},
	{
		"Download Directory",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, dldir),
		sizeof(((struct bbslist *)NULL)->dldir)
	},
	{
		"Upload Directory",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, uldir),
		sizeof(((struct bbslist *)NULL)->uldir)
	},
	{
		"Log File",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, logfile),
		sizeof(((struct bbslist *)NULL)->logfile)
	},
	{
		"Transfer Log Level",
		0,
		offsetof(struct bbslist, xfer_loglevel),
		sizeof(((struct bbslist *)NULL)->xfer_loglevel)
	},
	{
		"BPS Rate",
		0,
		offsetof(struct bbslist, bpsrate),
		sizeof(((struct bbslist *)NULL)->bpsrate)
	},
	{
		"ANSI Music",
		0,
		offsetof(struct bbslist, music),
		sizeof(((struct bbslist *)NULL)->music)
	},
	{ // 20
		"Address Family",
		0,
		offsetof(struct bbslist, address_family),
		sizeof(((struct bbslist *)NULL)->address_family)
	},
	{
		"Font",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, font),
		sizeof(((struct bbslist *)NULL)->font)
	},
	{
		"Hide Popups",
		0,
		offsetof(struct bbslist, hidepopups),
		sizeof(((struct bbslist *)NULL)->hidepopups)
	},
	{
		"RIP",
		0,
		offsetof(struct bbslist, rip),
		sizeof(((struct bbslist *)NULL)->rip)
	},
	{
		"Flow Control",
		0,
		offsetof(struct bbslist, flow_control),
		sizeof(((struct bbslist *)NULL)->flow_control)
	},
	{
		"Comment",
		SORT_ORDER_STRING,
		offsetof(struct bbslist, comment),
		sizeof(((struct bbslist *)NULL)->comment)
	},
	{
		"Force LCF",
		0,
		offsetof(struct bbslist, force_lcf),
		sizeof(((struct bbslist *)NULL)->force_lcf)
	},
	{
		"Yellow Is Yellow",
		0,
		offsetof(struct bbslist, yellow_is_yellow),
		sizeof(((struct bbslist *)NULL)->yellow_is_yellow)
	},
	{
		"Terminal Type",
		0,
		offsetof(struct bbslist, term_name),
		sizeof(((struct bbslist *)NULL)->term_name)
	},
	{ // 29
		"Explicit Sort Order",
		0,
		offsetof(struct bbslist, sort_order),
		sizeof(((struct bbslist *)NULL)->sort_order)
	},
	{
		NULL,
		0,
		0,
		0
	}
};

int sortorder[sizeof(sort_order) / sizeof(struct sort_order_info)];

static char *screen_modes[] = {
	"Current", "80x25", "LCD 80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37 (16:9)", "132x52 (5:4)",
	"132x25", "132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128 (40col)", "C128 (80col)",
	"Atari", "Atari XEP80", "Custom", "EGA 80x25", "VGA 80x25", "Prestel", "BBC Micro Mode 7", "Atari ST 40x25", "Atari ST 80x25", "Atari ST 80x25 Mono", NULL
};
char *screen_modes_enum[] = {
	"Current", "80x25", "LCD80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37", "132x52", "132x25",
	"132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128-40col", "C128-80col", "Atari",
	"Atari-XEP80", "Custom", "EGA80x25", "VGA80x25", "Prestel", "BBCMicro7", "AtariST40x25", "AtariST80x25", "AtariST80x25Mono", NULL
};

char *log_levels[] = {
	"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL
};
static char *log_level_desc[] = {
	"None", "Alerts", "Critical Errors", "Errors", "Warnings", "Notices", "Normal", "All (Debug)", NULL
};

char *rate_names[] = {
	"300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "76800", "115200", "Current", NULL
};
int rates[] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

static char *rip_versions[] = {"Off", "RIPv1", "RIPv3", NULL};

static char *fc_names[] = {"RTS/CTS", "XON/XOFF", "RTS/CTS and XON/XOFF", "None", NULL};
static char *fc_enum[] = {"RTSCTS", "XONXOFF", "RTSCTS_XONXOFF", "None", NULL};

char *music_names[] = {"ESC [ | only", "BANSI Style", "All ANSI Music enabled", NULL};
char music_helpbuf[] = "`ANSI Music Setup`\n\n"
                       "~ ESC [ | only ~            Only CSI | (SyncTERM) ANSI music is supported.\n"
                       "                          Enables Delete Line\n"
                       "~ BANSI Style ~             Also enables BANSI-Style (CSI N)\n"
                       "                          Enables Delete Line\n"
                       "~ All ANSI Music Enabled ~  Enables both CSI M and CSI N ANSI music.\n"
                       "                          Delete Line is disabled.\n"
                       "\n"
                       "So-Called ANSI Music has a long and troubled history.  Although the\n"
                       "original ANSI standard has well defined ways to provide private\n"
                       "extensions to the spec, none of these methods were used.  Instead,\n"
                       "so-called ANSI music replaced the Delete Line ANSI sequence.  Many\n"
                       "full-screen editors use DL, and to this day, some programs (Such as\n"
                       "BitchX) require it to run.\n\n"
                       "To deal with this, BananaCom decided to use what *they* thought was an\n"
                       "unspecified escape code ESC[N for ANSI music.  Unfortunately, this is\n"
                       "broken also.  Although rarely implemented in BBS clients, ESC[N is\n"
                       "the erase field sequence.\n\n"
                       "SyncTERM has now defined a third ANSI music sequence which *IS* legal\n"
                       "according to the ANSI spec.  Specifically ESC[|.";

static char *address_families[] = {"PerDNS", "IPv4", "IPv6", NULL};
static char *address_family_names[] = {"As per DNS", "IPv4 only", "IPv6 only", NULL};

static char *parity_enum[] = {"None", "Even", "Odd", NULL};

static char *address_family_help = "`Address Family`\n\n"
                                   "Select the address family to resolve\n\n"
                                   "`As per DNS`..: Uses what is in the DNS system\n"
                                   "`IPv4 only`...: Only uses IPv4 addresses.\n"
                                   "`IPv6 only`...: Only uses IPv6 addresses.\n";

static char *address_help =
        "`Address`, `Phone Number`, `Serial Port`, or `Command`\n\n"
        "Enter the hostname, IP address, phone number, or serial port device of\n"
        "the system to connect to. Example: `nix.synchro.net`\n\n"
        "In the case of the Shell type, enter the command to run.\n"
        "Shell types are only functional under *nix\n";
static char *conn_type_help = "`Connection Type`\n\n"
                              "Select the type of connection you wish to make:\n\n"
                              "`RLogin`...........: Auto-login with RLogin protocol\n"
                              "`RLogin Reversed`..: RLogin using reversed username/password parameters\n"
                              "`Telnet`...........: Use more common Telnet protocol\n"
                              "`Raw`..............: Make a raw TCP socket connection\n"
                              "`SSH`..............: Connect using the Secure Shell (SSH-2) protocol\n"
                              "`SSH (no auth)`....: SSH-2, but will not send password or public key\n"
                              "`Modem`............: Connect using a dial-up modem\n"
                              "`Serial`...........: Connect directly to a serial communications port\n"
                              "`3-wire (no RTS)`..: As with Serial, but lower RTS\n"
                              "`Shell`............: Connect to a local PTY (*nix only)\n"
                              "`MBBS GHost`.......: Communicate using the Major BBS 'GHost' protocol\n"
                              "`TelnetS`..........: Telnet over TLS\n";

static char *YesNo[3] = {"Yes", "No", ""};

static char *scaling_names[4] = {"Blocky", "Pointy", "External"};

ini_style_t ini_style = {
	/* key_len */
	15,

	/* key_prefix */ "\t",

	/* section_separator */ "\n",

	/* value_separarator */ NULL,

	/* bit_separator */ NULL
};

char list_password[1024] = "";
enum iniCryptAlgo list_algo = INI_CRYPT_ALGO_NONE;
int list_keysize = 0;

static int
fc_to_enum(int fc)
{
	switch (fc & (COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF)) {
		case COM_FLOW_CONTROL_RTS_CTS:
			return 0;
		case COM_FLOW_CONTROL_XON_OFF:
			return 1;
		case (COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF):
			return 2;
		default:
			return 3;
	}
}

static int
fc_from_enum(int fc_enum)
{
	switch (fc_enum) {
		case 0:
			return COM_FLOW_CONTROL_RTS_CTS;
		case 1:
			return COM_FLOW_CONTROL_XON_OFF;
		case 2:
			return COM_FLOW_CONTROL_RTS_CTS | COM_FLOW_CONTROL_XON_OFF;
		default:
			return 0;
	}
}

static void
viewofflinescroll(void)
{
	int top;
	int key;
	int i;
	struct text_info txtinfo;
	struct text_info sbtxtinfo;
	struct mouse_event mevent;
	int scrollback_pos;

	if (scrollback_buf == NULL)
		return;
	uifcbail();
	gettextinfo(&txtinfo);

	textmode(scrollback_mode);
	set_default_cursor();
	switch (ciolib_to_screen(scrollback_mode)) {
		case SCREEN_MODE_C64:
			setfont(33, true, 1);
			break;
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			setfont(35, true, 1);
			break;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			setfont(36, true, 1);
			break;
	}

	/* Set up a shadow palette */
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE) {
		for (i = 0; i < sizeof(dac_default) / sizeof(struct dac_colors); i++)
			setpalette(i + 16,
			           dac_default[i].red << 8 | dac_default[i].red,
			           dac_default[i].green << 8 | dac_default[i].green,
			           dac_default[i].blue << 8 | dac_default[i].blue);
	}
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	drawwin();
	set_modepalette(palettes[COLOUR_PALETTE]);
	gotoxy(1, 1);
	textattr(uifc.hclr | (uifc.bclr << 4) | BLINK);
	gettextinfo(&sbtxtinfo);
	scrollback_pos = scrollback_lines - sbtxtinfo.screenheight;
	top = scrollback_pos;
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
	ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
	showmouse();

	for (i = 0; !i && !quitting;) {
		if (top < 0)
			top = 0;
		if (top > scrollback_pos)
			top = scrollback_pos;
		vmem_puttext(((sbtxtinfo.screenwidth - scrollback_cols) / 2) + 1, 1,
		             (sbtxtinfo.screenwidth - scrollback_cols) / 2 + scrollback_cols,
		             sbtxtinfo.screenheight,
		             scrollback_buf + (scrollback_cols * top));
		cputs("Scrollback");
		gotoxy(scrollback_cols - 9, 1);
		cputs("Scrollback");
		gotoxy(1, 1);
		key = getch();
		switch (key) {
			case 0xe0:
			case 0:
				switch (key | getch() << 8) {
					case CIO_KEY_QUIT:
						check_exit(true);
						if (quitting)
							i = 1;
						break;
					case CIO_KEY_MOUSE:
						getmouse(&mevent);
						switch (mevent.event) {
							case CIOLIB_BUTTON_1_DRAG_START:
								mousedrag(scrollback_buf);
								break;
							case CIOLIB_BUTTON_4_PRESS:
								top--;
								break;
							case CIOLIB_BUTTON_5_PRESS:
								top++;
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
						top -= sbtxtinfo.screenheight;
						break;
					case CIO_KEY_NPAGE:
						top += sbtxtinfo.screenheight;
						break;
					case CIO_KEY_F(1):
						init_uifc(false, false);
						uifc.helpbuf = "`Scrollback Buffer`\n\n"
						               "~ J ~ or ~ Up Arrow ~   Scrolls up one line\n"
						               "~ K ~ or ~ Down Arrow ~ Scrolls down one line\n"
						               "~ H ~ or ~ Page Up ~    Scrolls up one screen\n"
						               "~ L ~ or ~ Page Down ~  Scrolls down one screen\n";
						uifc.showhelp();
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

	textmode(txtinfo.currmode);
	set_default_cursor();
	init_uifc(true, true);
	return;
}

int
get_rate_num(int rate)
{
	int i;

	for (i = 0; rates[i] && (!rate || rate > rates[i]); i++)
		;
	return i;
}

static int
is_sorting(int chk)
{
	int i;

	for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++)
		if ((abs(sortorder[i])) == chk)
			return 1;


	return 0;
}

static int
intbufcmp(const void *a, const void *b, size_t size)
{
	int i;
	const unsigned char *ac = (const unsigned char *)a;
	const unsigned char *bc = (const unsigned char *)b;

#ifdef __BIG_ENDIAN__
	for (i = 0; i < size; i++) {
		bool last = i == 0;
#else
	for (i = size - 1; i >= 0; i--) {
		bool last = i == size - 1;
#endif
		if (ac[i] != bc[i]) {
			if (last) {
				return ((signed char)ac[i] - (signed char)bc[i]);
			}
			return ac[i] - bc[i];
		}
	}
	return 0;
}

static int
listcmp(const void *aptr, const void *bptr)
{
	const char *a = *(void **)(aptr);
	const char *b = *(void **)(bptr);
	int i;
	int item;
	int reverse;
	int ret = 0;

	for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++) {
		item = abs(sortorder[i]);
		reverse = (sortorder[i] < 0 ? 1 : 0) ^ ((sort_order[item].flags & SORT_ORDER_REVERSED) ? 1 : 0);
		if (sort_order[item].name) {
			if (sort_order[item].flags & SORT_ORDER_STRING)
				ret = stricmp(a + sort_order[item].offset, b + sort_order[item].offset);
			else {
				ret =
				        intbufcmp(a + sort_order[item].offset,
				                  b + sort_order[item].offset,
				                  sort_order[item].length);
			}
			if (ret) {
				if (reverse)
					ret = 0 - ret;
				return ret;
			}
		}
		else
			return ret;
	}
	return 0;
}

static void
sort_list(struct bbslist **list, int *listcount, int *cur, int *bar, char *current)
{
	int i;

	qsort(list, *listcount, sizeof(struct bbslist *), listcmp);
	if (cur && current) {
		for (i = 0; i < *listcount; i++) {
			if (strcmp(list[i]->name, current) == 0) {
				*cur = i;
				if (bar)
					*bar = i;
				break;
			}
		}
	}
}

static void
write_sortorder(void)
{
	char inipath[MAX_PATH + 1];
	FILE *inifile;
	str_list_t inicontents;
	str_list_t sortorders;
	char str[64];
	int i;

	sortorders = strListInit();
	for (i = 0; sort_order[abs(sortorder[i])].name != NULL; i++) {
		sprintf(str, "%d", sortorder[i]);
		strListPush(&sortorders, str);
	}

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) != NULL) {
		inicontents = iniReadFile(inifile);
		fclose(inifile);
	}
	else
		inicontents = strListInit();

	iniSetStringList(&inicontents, "SyncTERM", "SortOrder", ",", sortorders, &ini_style);
	if ((inifile = fopen(inipath, "w")) != NULL) {
		iniWriteFile(inifile, inicontents);
		fclose(inifile);
	}
	strListFree(&sortorders);
	strListFree(&inicontents);
}

static void
edit_sorting(struct bbslist **list, int *listcount, int *ocur, int *obar, char *current)
{
	char opt[sizeof(sort_order) / sizeof(struct sort_order_info)][80];
	char *opts[sizeof(sort_order) / sizeof(struct sort_order_info) + 1];
	char sopt[sizeof(sort_order) / sizeof(struct sort_order_info)][80];
	char *sopts[sizeof(sort_order) / sizeof(struct sort_order_info) + 1];
	int curr = 0, bar = 0;
	int scurr = 0, sbar = 0;
	int ret, sret;
	int i, j;

	for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++)
		opts[i] = opt[i];
	opts[i] = NULL;
	for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++)
		sopts[i] = sopt[i];
	sopts[i] = NULL;

	for (; !quitting;) {
		/* Build ordered list of sort order */
		for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++) {
			if (sort_order[abs(sortorder[i])].name) {
				SAFECOPY(opt[i], sort_order[abs(sortorder[i])].name);
				if (((sortorder[i]
				      < 0) ? 1 : 0)
				    ^ ((sort_order[abs(sortorder[i])].flags & SORT_ORDER_REVERSED) ? 1 : 0))
					strcat(opt[i], " (reversed)");
			}
			else
				opt[i][0] = 0;
		}
		uifc.helpbuf = "`Sort Order`\n\n"
		               "Move the highlight bar to the position you would like\n"
		               "to add a new ordering before and press ~INSERT~.  Choose\n"
		               "a field from the list and it will be inserted.\n\n"
		               "To remove a sort order, use ~DELETE~.\n\n"
		               "To reverse a sort order, highlight it and press enter";
		ret = uifc.list(WIN_XTR | WIN_DEL | WIN_INS | WIN_INSACT | WIN_ACT | WIN_SAV,
		                0, 0, 0, &curr, &bar, "Sort Order", opts);
		if (ret == -1) {
			if (uifc.exit_flags & UIFC_XF_QUIT) {
				if (!check_exit(false))
					continue;
			}
			break;
		}
		if (ret & MSK_INS) { /* Insert sorting */
			j = 0;
			for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++) {
				if (!is_sorting(i) && sort_order[i].name) {
					SAFECOPY(sopt[j], sort_order[i].name);
					j++;
				}
			}
			if (j == 0) {
				uifc.helpbuf = "All sort orders are present in the list.";
				uifc.msg("No more sort orders.");
				if (check_exit(false))
					break;
			}
			else {
				sopt[j][0] = 0;
				uifc.helpbuf = "Select a sort order to add and press enter";
				sret = uifc.list(WIN_SAV | WIN_BOT | WIN_RHT,
				                 0, 0, 0, &scurr, &sbar, "Sort Field", sopts);
				if (sret >= 0) {
					/* Insert into array */
					memmove(&(sortorder[ret & MSK_OFF]) + 1, &(sortorder[(ret & MSK_OFF)]),
					        sizeof(sortorder) - sizeof(sortorder[0]) * ((ret & MSK_OFF) + 1));
					j = 0;
					for (i = 0; i < sizeof(sort_order) / sizeof(struct sort_order_info); i++) {
						if (!is_sorting(i) && sort_order[i].name) {
							if (j == sret) {
								sortorder[ret & MSK_OFF] = i;
								break;
							}
							j++;
						}
					}
				}
				else {
					if (check_exit(false))
						break;
				}
			}
		}
		else if (ret & MSK_DEL) { /* Delete criteria */
			memmove(&(sortorder[ret & MSK_OFF]),
			        &(sortorder[(ret & MSK_OFF)]) + 1,
			        sizeof(sortorder) - sizeof(sortorder[0]) * ((ret & MSK_OFF) + 1));
		}
		else
			sortorder[ret & MSK_OFF] = 0 - sortorder[ret & MSK_OFF];
	}

	/* Write back to the .ini file */
	write_sortorder();
	sort_list(list, listcount, ocur, obar, current);
}

void
free_list(struct bbslist **list, int listcount)
{
	int i;

	for (i = 0; i < listcount; i++)
		FREE_AND_NULL(list[i]);
}

void
read_item(ini_fp_list_t *listfile, struct bbslist *entry, ini_lv_string_t *bbsname, int id, int type)
{
	char home[MAX_PATH + 1];
	str_list_t section;
	bool sys = (type == SYSTEM_BBSLIST);

	get_syncterm_filename(home, sizeof(home), SYNCTERM_DEFAULT_TRANSFER_PATH, false);
	if (bbsname != NULL) {
		size_t cpylen = bbsname->len;
		if (cpylen > LIST_NAME_MAX)
			cpylen = LIST_NAME_MAX;
		memcpy(entry->name, bbsname->str, cpylen);
		entry->name[cpylen] = 0;
	}
	section = iniGetFastParsedSectionLV(listfile, bbsname, true);
	iniGetSString(section, NULL, "Address", "", entry->addr, sizeof(entry->addr));
	entry->conn_type = iniGetEnum(section, NULL, "ConnectionType", conn_types_enum, CONN_TYPE_SSH);
	entry->flow_control = fc_from_enum(iniGetEnum(section, NULL, "FlowControl", fc_enum, 0));
	entry->port = iniGetUShortInt(section, NULL, "Port", conn_ports[entry->conn_type]);
	entry->added = iniGetDateTime(sys ? NULL : section, NULL, "Added", 0);
	entry->connected = iniGetDateTime(sys ? NULL : section, NULL, "LastConnected", 0);
	entry->calls = iniGetUInteger(sys ? NULL : section, NULL, "TotalCalls", 0);
	iniGetSString(sys ? NULL : section, NULL, "UserName", "", entry->user, sizeof(entry->user));
	iniGetSString(sys ? NULL : section, NULL, "Password", "", entry->password, sizeof(entry->password));
	iniGetSString(sys ? NULL : section, NULL, "SystemPassword", "", entry->syspass, sizeof(entry->syspass));
	if (iniGetBool(section, NULL, "BeDumb", false)) /* Legacy */
		entry->conn_type = CONN_TYPE_RAW;
	entry->screen_mode = iniGetEnum(section, NULL, "ScreenMode", screen_modes_enum, SCREEN_MODE_CURRENT);
	entry->nostatus = iniGetBool(section, NULL, "NoStatus", false);
	entry->hidepopups = iniGetBool(section, NULL, "HidePopups", false);
	entry->rip = iniGetEnum(section, NULL, "RIP", rip_versions, RIP_VERSION_NONE);
	entry->force_lcf = iniGetBool(section, NULL, "ForceLCF", false);
	entry->yellow_is_yellow = iniGetBool(section, NULL, "YellowIsYellow", false);
	iniGetSString(section, NULL, "TerminalType", "", entry->term_name, sizeof(entry->term_name));
	if (iniKeyExists(section, NULL, "SSHFingerprint")) {
		char fp[41];
		int i;
		iniGetSString(section, NULL, "SSHFingerprint", "", fp, sizeof(fp));
		for (i = 0; i < 20; i++) {
			if (!(isxdigit(fp[i * 2]) && isxdigit(fp[i * 2 + 1])))
				break;
			entry->ssh_fingerprint[i] = (HEX_CHAR_TO_INT(fp[i * 2]) * 16) + HEX_CHAR_TO_INT(fp[i * 2 + 1]);
		}
		if (i == 20)
			entry->has_fingerprint = true;
		else
			entry->has_fingerprint = false;
	}
	else
		entry->has_fingerprint = false;
	entry->sftp_public_key = iniGetBool(section, NULL, "SFTPPublicKey", false);
	iniGetSString(sys ? NULL : section, NULL, "DownloadPath", home, entry->dldir, sizeof(entry->dldir));
	iniGetSString(sys ? NULL : section, NULL, "UploadPath", home, entry->uldir, sizeof(entry->uldir));
	entry->data_bits = iniGetUShortInt(section, NULL, "DataBits", 8);
	if (entry->data_bits < 7 || entry->data_bits > 8)
		entry->data_bits = 8;
	entry->stop_bits = iniGetUShortInt(section, NULL, "StopBits", 1);
	if (entry->stop_bits < 1 || entry->stop_bits > 2)
		entry->stop_bits = 1;
	entry->parity = iniGetEnum(section, NULL, "Parity", parity_enum, SYNCTERM_PARITY_NONE);
	entry->telnet_no_binary = iniGetBool(section, NULL, "TelnetBrokenTextmode", false);
	entry->defer_telnet_negotiation = iniGetBool(section, NULL, "TelnetDeferNegotiate", false);
	// TODO: Make this not suck.
	int *pal = iniGetIntList(section, NULL, "Palette", &entry->palette_size, ",", NULL);
	if (pal == NULL)
		entry->palette_size = 0;
	if (entry->palette_size > 0) {
		unsigned lent = 0;
		for (unsigned pent = 0; pent < 16; pent++) {
			entry->palette[pent] = pal[lent++];
			if (lent == entry->palette_size)
				lent = 0;
		}
	}
	free(pal);

	/* Log Stuff */
	iniGetSString(sys ? NULL : section, NULL, "LogFile", "", entry->logfile, sizeof(entry->logfile));
	entry->append_logfile = iniGetBool(sys ? NULL : section, NULL, "AppendLogFile", true);
	entry->xfer_loglevel = iniGetEnum(sys ? NULL : section, NULL, "TransferLogLevel", log_levels, LOG_INFO);
	entry->telnet_loglevel = iniGetEnum(sys ? NULL : section, NULL, "TelnetLogLevel", log_levels, LOG_INFO);

	entry->bpsrate = iniGetInteger(section, NULL, "BPSRate", 0);
	entry->music = iniGetInteger(section, NULL, "ANSIMusic", CTERM_MUSIC_BANSI);
	entry->address_family = iniGetEnum(section, NULL, "AddressFamily", address_families, ADDRESS_FAMILY_UNSPEC);
	iniGetSString(section, NULL, "Font", "Codepage 437 English", entry->font, sizeof(entry->font));
	iniGetSString(section, NULL, "Comment", "", entry->comment, sizeof(entry->comment));
	entry->type = type;
	entry->id = id;

	/* Random junk */
	entry->sort_order = iniGetInt32(section, NULL, "SortOrder", DEFAULT_SORT_ORDER_VALUE);
}

static bool
is_reserved_bbs_name(const char *name)
{
	if (stricmp(name, "syncterm-system-cache") == 0)
		return true;
	return false;
}

static bool
prompt_password(void *cb_data, char *keybuf, size_t *sz)
{
	size_t newSz = sizeof(list_password);

	if (sz && *sz < newSz)
		newSz = *sz;
	if (list_password[0] == 0) {
		uifc.helpbuf = "`Encrypted List`\n\n"
		            "The BBS list is encrypted. Enter the password it was encrypted with.";
		int olen = uifc.input(WIN_SAV | WIN_MID, 0, 0, "Password", list_password, sizeof(list_password) - 1, K_PASSWORD);
		if (olen < 1)
			return false;
	}
	if (list_password[0]) {
		if (keybuf && sz) {
			*sz = strlcpy(keybuf, list_password, *sz);
		}
		return true;
	}
	return false;
}

str_list_t
iniReadBBSList(FILE *fp, bool userList)
{
	enum iniCryptAlgo algo = INI_CRYPT_ALGO_NONE;
	int ks;
	str_list_t inifile = iniReadEncryptedFile(fp, prompt_password, settings.keyDerivationIterations, &algo, &ks, NULL, NULL, NULL);
	if (inifile == NULL || (algo != INI_CRYPT_ALGO_NONE && !iniGetBool(inifile, NULL, "DecryptionCheck", false))) {
		uifc.msg("Failed to decrypt BBS list, exiting");
		exit(EXIT_FAILURE);
	}
	if (userList) {
		list_algo = algo;
		list_keysize = ks;
	}

	return inifile;
}

/*
 * Checks if bbsname already is listed in list
 * setting *pos to the position if not NULL.
 * optionally only if the entry is a user list
 * entry
 */
static int
list_name_check(struct bbslist **list, char *bbsname, int *pos, int useronly)
{
	int i;

	if (list == NULL) {
		FILE *listfile;
		str_list_t inifile;

		if ((listfile = fopen(settings.list_path, "rb")) != NULL) {
			inifile = iniReadBBSList(listfile, true);
			fclose(listfile);
			i = iniSectionExists(inifile, bbsname);
			strListFree(&inifile);
			return i;
		}
		return 0;
	}

	for (i = 0; list[i] != NULL; i++) {
		if (useronly && (list[i]->type != USER_BBSLIST))
			continue;
		if (stricmp(list[i]->name, bbsname) == 0) {
			if (pos)
				*pos = i;
			return 1;
		}
	}
	return 0;
}

static const struct bbslist **fip_last_visited;
static int fip_last_ret;

static int
fip_bsearch_cmp(const void *key_ptr, const void *elem_ptr)
{
	const struct bbslist **elem = (const struct bbslist**)elem_ptr;
	const ini_lv_string_t *key = key_ptr;

	fip_last_visited = elem;
	fip_last_ret = key->len == 0 ? 0 : strnicmp(key->str, elem[0]->name, key->len);
	if (fip_last_ret == 0) {
		if (elem[0]->name[key->len])
			fip_last_ret = 1;
	}
	return fip_last_ret;
}

static size_t
find_insert_point(struct bbslist **list, ini_lv_string_t *bbsname, int sz)
{
	fip_last_visited = NULL;
	fip_last_ret = 0;
	const struct bbslist **clist = (const struct bbslist**)list;

	void *item = bsearch(bbsname, list, sz, sizeof(*list), fip_bsearch_cmp);
	size_t ret = 0;
	if (item == NULL) {
		if (fip_last_visited) {
			ret = fip_last_visited - clist;
			if (fip_last_ret > 0)
				ret++;
		}
	}
	else
		ret = SIZE_MAX;
	return ret;
}

/*
 * Reads in a BBS list from listpath using *i as the counter into bbslist
 * first BBS read goes into list[i]
 */
void
read_list(char *listpath, struct bbslist **list, struct bbslist *defaults, int *i, int type)
{
	FILE *listfile;
	ini_lv_string_t **bbses;
	size_t bbs_cnt;
	size_t j;
	str_list_t inilines;
	ini_fp_list_t *nlines;

	if ((listfile = fopen(listpath, "rb")) != NULL) {
		inilines = iniReadBBSList(listfile, type == USER_BBSLIST);
		fclose(listfile);
		nlines = iniFastParseSections(inilines, false);
		if ((defaults != NULL) && (type == USER_BBSLIST))
			read_item(nlines, defaults, NULL, -1, type);
		bbses = iniGetFastParsedSectionList(nlines, NULL, &bbs_cnt);
		for (j = 0; j < bbs_cnt; j++) {
			size_t ip = find_insert_point(list, bbses[j], *i);
			if (ip < SIZE_MAX) {
				if (ip < *i) {
					memmove(&list[ip + 1], &list[ip], (*i - ip) * sizeof(*list));
				}
				if ((list[ip] = (struct bbslist *)malloc(sizeof(struct bbslist))) == NULL) {
					fputs("Out of memory at in read_list()\r\n", stderr);
					break;
				}
				read_item(nlines, list[ip], bbses[j], *i, type);
				(*i)++;
			}
			if (*i == MAX_OPTS - 1) {
				fprintf(stderr, "Reading too many entries (more than %d)!\r\n", MAX_OPTS);
				break;
			}
		}
		iniFastParsedSectionListFree(bbses);
		iniFreeFastParse(nlines);
		strListFree(&inilines);
	}
	else {
		if ((defaults != NULL) && (type == USER_BBSLIST))
			read_item(NULL, defaults, NULL, -1, type);
	}

#if 0 /*
         * This isn't necessary (NULL is a sufficient)
         * Add terminator
         */
	list[*i] = (struct bbslist *)"";
#endif
}

static void
fc_str(char *str, int fc)
{
	sprintf(str, "Flow Control      %s", fc_names[fc_to_enum(fc)]);
}

/*
 * Terminates a path with "..." if it's too long.
 * Format must contain only a single '%s'.
 */
static void
printf_trunc(char *dst, size_t dstsz, char *fmt, char *path)
{
	char *mangled;
	size_t fmt_len = strlen(fmt) - 2;
	size_t remain_len = dstsz - fmt_len;
	size_t full_len = fmt_len + strlen(path);

	if (full_len >= dstsz) {
		mangled = strdup(path);
		if (mangled) {
			mangled[remain_len - 1] = '\0';
			mangled[remain_len - 2] = '.';
			mangled[remain_len - 3] = '.';
			mangled[remain_len - 4] = '.';
			sprintf(dst, fmt, mangled);
			free(mangled);
		}
		else
			sprintf(dst, fmt, "<Long>");
	}
	else
		sprintf(dst, fmt, path);
}

static void
configure_log(struct bbslist *item, const char *itemname, str_list_t inifile, int *changed)
{
	char opt[4][69];
	char *opts[(sizeof(opt) / sizeof(opt[0])) + 1];
	int o;
	int i;
	int copt = 0;

	for (o = 0; o < sizeof(opt) / sizeof(opt[0]); o++)
		opts[o] = opt[o];
	opts[o] = NULL;

	for (;;) {
		o = 0;
		printf_trunc(opt[o], sizeof(opt[o]), "Log Filename             %s", item->logfile);
		o++;
		sprintf(opt[o++], "File Transfer Log Level  %s", log_level_desc[item->xfer_loglevel]);
		sprintf(opt[o++], "Telnet Command Log Level %s", log_level_desc[item->telnet_loglevel]);
		sprintf(opt[o++], "Append Log File          %s", item->append_logfile ? "Yes" : "No");

		uifc.helpbuf =
		        "`Log Configuration`\n\n"
		        "~ Log File ~\n"
		        "        Log file name when logging is enabled\n\n"
		        "~ File Transfer Log Level ~\n"
		        "        Selects the transfer log level.\n\n"
		        "~ Telnet Command Log Level ~\n"
		        "        Selects the telnet command log level.\n\n"
		        "~ Append Log File ~\n"
		        "        Append log file (instead of overwrite) on each connection\n\n";
		switch (uifc.list(WIN_SAV | WIN_ACT, 0, 0, 0, &copt, NULL, "Log Configuration", opts)) {
			case -1:
				return;
			case 0:
				uifc.helpbuf =
				        "`Log Filename`\n\n"
				        "Enter the path to the optional log file.";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Log File", item->logfile, MAX_PATH,
				               K_EDIT) >= 0) {
					iniSetString(&inifile, itemname, "LogFile", item->logfile, &ini_style);
					*changed = 1;
				}
				else
					check_exit(false);
				break;
			case 1:
				uifc.helpbuf = "`File Transfer Log Level`\n\n"
				               "Select the varbosity level for logging.\n"
				               "The lower in the list the item, the more berbose the log.\n"
				               "Each level includes all messages in levels above it.";
				i = item->xfer_loglevel;
				switch (uifc.list(WIN_SAV | WIN_BOT | WIN_RHT, 0, 0, 0, &(item->xfer_loglevel), NULL,
				                  "File Transfer Log Level", log_level_desc)) {
					case -1:
						item->xfer_loglevel = i;
						check_exit(false);
						break;
					default:
						if (item->xfer_loglevel != i) {
							iniSetEnum(&inifile,
							           itemname,
							           "TransferLogLevel",
							           log_levels,
							           item->xfer_loglevel,
							           &ini_style);
							*changed = 1;
						}
				}
				break;
			case 2:
				uifc.helpbuf = "`Telnet Command Log Level`\n\n"
				               "Select the varbosity level for logging.\n"
				               "The lower in the list the item, the more berbose the log.\n"
				               "Each level includes all messages in levels above it.";
				i = item->telnet_loglevel;
				switch (uifc.list(WIN_SAV | WIN_BOT | WIN_RHT, 0, 0, 0, &(item->telnet_loglevel), NULL,
				                  "Telnet Command Log Level", log_level_desc)) {
					case -1:
						item->telnet_loglevel = i;
						check_exit(false);
						break;
					default:
						if (item->telnet_loglevel != i) {
							iniSetEnum(&inifile,
							           itemname,
							           "TransferLogLevel",
							           log_levels,
							           item->telnet_loglevel,
							           &ini_style);
							*changed = 1;
						}
						break;
				}
				break;
			case 3:
				item->append_logfile = !item->append_logfile;
				*changed = 1;
				iniSetBool(&inifile, itemname, "AppendLogFile", item->append_logfile, &ini_style);
				break;
		}
	}
}

static int
get_rip_version(int oldver, int *changed)
{
	int cur = oldver;

	uifc.helpbuf = "`RIP Version`\n\n"
	               "RIP v1 requires EGA mode while RIP v3\n"
	               "works in any screen mode.";
	switch (uifc.list(WIN_SAV, 0, 0, 0, &cur, NULL, "RIP Mode", rip_versions)) {
		case -1:
			check_exit(false);
			break;
		case RIP_VERSION_NONE:
		case RIP_VERSION_1:
		case RIP_VERSION_3:
			if (cur != oldver)
				*changed = 1;
	}
	return cur;
}

static bool
edit_name(char *itemname, struct bbslist **list, str_list_t inifile, bool edit_to_add)
{
	char tmp[LIST_NAME_MAX + 1] = {0};

	do {
		uifc.helpbuf = "`Directory Entry Name`\n\n"
		               "Enter the name of the entry as it is to appear in the directory.";
		if (itemname)
			strlcpy(tmp, itemname, sizeof(tmp));
		if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Name", tmp, LIST_NAME_MAX, K_EDIT) == -1)
			return false;
		check_exit(false);
		if (quitting)
			return false;
		if ((edit_to_add || (itemname != NULL && stricmp(tmp, itemname))) && list_name_check(list, tmp, NULL, false)) {
			uifc.helpbuf = "`Entry Name Already Exists`\n\n"
			               "An entry with that name already exists in the directory.\n"
			               "Please choose a unique name.\n";
			uifc.msg("Entry Name Already Exists!");
			check_exit(false);
		}
		else if(is_reserved_bbs_name(tmp)) {
			uifc.helpbuf = "`Reserved Name`\n\n"
			               "The name you entered is reserved for internal use\n";
			uifc.msg("Reserved Name!");
			check_exit(false);
		}
		else {
			if (tmp[0] == 0) {
				uifc.helpbuf = "`Can Not Use an Empty Name`\n\n"
				               "Entry names can not be empty.  Please enter an entry name.\n";
				uifc.msg("Can not use an empty name");
				check_exit(false);
			}
			else {
				if (itemname) {
					// TODO: Rename cache
					if (!edit_to_add)
						iniRenameSection(&inifile, itemname, tmp);
					strcpy(itemname, tmp);
				}
				return true;
			}
		}
	} while (tmp[0] == 0 && !quitting);
	return false;
}

enum {
	BBSLIST_FIELD_NONE,
	BBSLIST_FIELD_NAME,
	BBSLIST_FIELD_ADDR,
	BBSLIST_FIELD_PORT,
	BBSLIST_FIELD_ADDED,
	BBSLIST_FIELD_CONNECTED,
	BBSLIST_FIELD_CALLS,
	BBSLIST_FIELD_USER,
	BBSLIST_FIELD_PASSWORD,
	BBSLIST_FIELD_SYSPASS,
	BBSLIST_FIELD_TYPE,
	BBSLIST_FIELD_CONN_TYPE,
	BBSLIST_FIELD_ID,
	BBSLIST_FIELD_SCREEN_MODE,
	BBSLIST_FIELD_NOSTATUS,
	BBSLIST_FIELD_DLDIR,
	BBSLIST_FIELD_ULDIR,
	BBSLIST_FIELD_LOGFILE,
	BBSLIST_FIELD_APPEND_LOGFILE,
	BBSLIST_FIELD_XFER_LOGLEVEL,
	BBSLIST_FIELD_TELNET_LOGLEVEL,
	BBSLIST_FIELD_BPSRATE,
	BBSLIST_FIELD_MUSIC,
	BBSLIST_FIELD_ADDRESS_FAMILY,
	BBSLIST_FIELD_FONT,
	BBSLIST_FIELD_HIDEPOPUPS,
	BBSLIST_FIELD_GHOST_PROGRAM,
	BBSLIST_FIELD_RIP,
	BBSLIST_FIELD_FLOW_CONTROL,
	BBSLIST_FIELD_COMMENT,
	BBSLIST_FIELD_FORCE_LCF,
	BBSLIST_FIELD_YELLOW_IS_YELLOW,
	BBSLIST_FIELD_HAS_FINGERPRINT,
	BBSLIST_FIELD_SSH_FINGERPRINT,
	BBSLIST_FIELD_SFTP_PUBLIC_KEY,
	BBSLIST_FIELD_STOP_BITS,
	BBSLIST_FIELD_DATA_BITS,
	BBSLIST_FIELD_PARITY,
	BBSLIST_FIELD_TELNET_NO_BINARY,
	BBSLIST_FIELD_TELNET_DEFERRED_NEGOTIATION,
	BBSLIST_FIELD_PALETTE,
	BBSLIST_FIELD_TERMINAL_TYPE,
};

static void
build_edit_list(struct bbslist *item, char opt[][69], int *optmap, char **opts, int isdefault, char *itemname)
{
	int i = 0;
	char str[64];
	bool is_ansi = true;
	bool is_serial = ((item->conn_type == CONN_TYPE_MODEM) || (item->conn_type == CONN_TYPE_SERIAL)
	|| (item->conn_type == CONN_TYPE_SERIAL_NORTS));
	bool is_c128_80 = item->screen_mode == SCREEN_MODE_C128_80;

	if (!isdefault) {
		optmap[i] = BBSLIST_FIELD_NAME;
		sprintf(opt[i++], "Name              %s", itemname);
		optmap[i] = BBSLIST_FIELD_ADDR;
		switch (item->conn_type) {
			case CONN_TYPE_MODEM:
				sprintf(opt[i++], "Phone Number      %s", item->addr);
				break;
			case CONN_TYPE_SERIAL:
			case CONN_TYPE_SERIAL_NORTS:
				sprintf(opt[i++], "Device Name       %s", item->addr);
				break;
			case CONN_TYPE_SHELL:
				sprintf(opt[i++], "Command           %s", item->addr);
				break;
			default:
				sprintf(opt[i++], "Address           %s", item->addr);
				break;
		}
	}
	optmap[i] = BBSLIST_FIELD_CONN_TYPE;
	sprintf(opt[i++], "Connection Type   %s", conn_types[item->conn_type]);
	if (IS_NETWORK_CONN(item->conn_type)) {
		optmap[i] = BBSLIST_FIELD_ADDRESS_FAMILY;
		sprintf(opt[i++], "Address Family    %s", address_family_names[item->address_family]);
	}
	if (is_serial) {
		optmap[i] = BBSLIST_FIELD_FLOW_CONTROL;
		fc_str(opt[i++], item->flow_control);
		if (item->bpsrate)
			sprintf(str, "%ubps", item->bpsrate);
		else
			strcpy(str, "Current");
		optmap[i] = BBSLIST_FIELD_BPSRATE;
		sprintf(opt[i++], "Comm Rate         %s", str);
		optmap[i] = BBSLIST_FIELD_STOP_BITS;
		sprintf(opt[i++], "Stop Bits         %hu", item->stop_bits);
		optmap[i] = BBSLIST_FIELD_DATA_BITS;
		sprintf(opt[i++], "Data Bits         %hu", item->data_bits);
		optmap[i] = BBSLIST_FIELD_PARITY;
		sprintf(opt[i++], "Parity            %s", parity_enum[item->parity]);
	}
	else if (item->conn_type != CONN_TYPE_SHELL) {
		optmap[i] = BBSLIST_FIELD_PORT;
		sprintf(opt[i++], "TCP Port          %hu", item->port);
	}
	if (item->conn_type == CONN_TYPE_MBBS_GHOST) {
		optmap[i] = BBSLIST_FIELD_USER;
		printf_trunc(opt[i], sizeof(opt[i]), "Username          %s", item->user);
		i++;
		optmap[i] = BBSLIST_FIELD_PASSWORD;
		sprintf(opt[i++], "GHost Program     %s", item->password);
		optmap[i] = BBSLIST_FIELD_SYSPASS;
		sprintf(opt[i++], "System Password   %s", item->syspass[0] ? "********" : "<none>");
	}
	else if (item->conn_type == CONN_TYPE_SSHNA) {
		optmap[i] = BBSLIST_FIELD_USER;
		printf_trunc(opt[i], sizeof(opt[i]), "SSH Username      %s", item->user);
		i++;
		optmap[i] = BBSLIST_FIELD_PASSWORD;
		sprintf(opt[i++], "BBS Username      %s", item->password);
		optmap[i] = BBSLIST_FIELD_SYSPASS;
		sprintf(opt[i++], "BBS Password      %s", item->syspass[0] ? "********" : "<none>");
	}
	else {
		optmap[i] = BBSLIST_FIELD_USER;
		printf_trunc(opt[i], sizeof(opt[i]), "Username          %s", item->user);
		i++;
		optmap[i] = BBSLIST_FIELD_PASSWORD;
		sprintf(opt[i++], "Password          %s", item->password[0] ? "********" : "<none>");
		optmap[i] = BBSLIST_FIELD_SYSPASS;
		sprintf(opt[i++], "System Password   %s", item->syspass[0] ? "********" : "<none>");
	}
	if (item->conn_type == CONN_TYPE_SSH || item->conn_type == CONN_TYPE_SSHNA) {
		optmap[i] = BBSLIST_FIELD_SFTP_PUBLIC_KEY;
		sprintf(opt[i++], "SFTP Public Key   %s", item->sftp_public_key ? "Yes" : "No");
	}
	if (item->conn_type == CONN_TYPE_TELNET || item->conn_type == CONN_TYPE_TELNETS) {
		optmap[i] = BBSLIST_FIELD_TELNET_NO_BINARY;
		sprintf(opt[i++], "Binmode Broken    %s", item->telnet_no_binary ? "Yes" : "No");
	}
	if (item->conn_type == CONN_TYPE_TELNET || item->conn_type == CONN_TYPE_TELNETS) {
		optmap[i] = BBSLIST_FIELD_TELNET_DEFERRED_NEGOTIATION;
		sprintf(opt[i++], "Defer Negotiate   %s", item->defer_telnet_negotiation ? "Yes" : "No");
	}
	optmap[i] = BBSLIST_FIELD_SCREEN_MODE;
	sprintf(opt[i++], "Screen Mode       %s", screen_modes[item->screen_mode]);
	if (item->conn_type == CONN_TYPE_SSH || item->conn_type == CONN_TYPE_SSHNA
	    || item->conn_type == CONN_TYPE_TELNET || item->conn_type == CONN_TYPE_TELNETS
	    || item->conn_type == CONN_TYPE_RLOGIN || item->conn_type == CONN_TYPE_RLOGIN_REVERSED
	    || item->conn_type == CONN_TYPE_SHELL) {
		optmap[i] = BBSLIST_FIELD_TERMINAL_TYPE;
		sprintf(opt[i++], "Terminal Type     %s", item->term_name[0] ? item->term_name : "<Automatic>");
	}
	optmap[i] = BBSLIST_FIELD_FONT;
	sprintf(opt[i++], "Font              %s", item->font);
	if (get_emulation(item) != CTERM_EMULATION_ANSI_BBS)
		is_ansi = false;
	if (is_ansi) {
		optmap[i] = BBSLIST_FIELD_MUSIC;
		sprintf(opt[i++], "ANSI Music        %s", music_names[item->music]);
		optmap[i] = BBSLIST_FIELD_RIP;
		sprintf(opt[i++], "RIP               %s", rip_versions[item->rip]);
		optmap[i] = BBSLIST_FIELD_FORCE_LCF;
		sprintf(opt[i++], "Force LCF Mode    %s", item->force_lcf ? "Yes" : "No");
	}
	if (is_ansi || is_c128_80) {
		optmap[i] = BBSLIST_FIELD_YELLOW_IS_YELLOW;
		sprintf(opt[i++], "Yellow is Yellow  %s", item->yellow_is_yellow ? "Yes" : "No");
	}
	optmap[i] = BBSLIST_FIELD_NOSTATUS;
	sprintf(opt[i++], "Hide Status Line  %s", item->nostatus ? "Yes" : "No");
	optmap[i] = BBSLIST_FIELD_DLDIR;
	printf_trunc(opt[i], sizeof(opt[i]), "Download Path     %s", item->dldir);
	i++;
	optmap[i] = BBSLIST_FIELD_ULDIR;
	printf_trunc(opt[i], sizeof(opt[i]), "Upload Path       %s", item->uldir);
	i++;
	optmap[i] = BBSLIST_FIELD_LOGFILE;
	strcpy(opt[i++], "Log Configuration");
	if (!is_serial) {
		if (item->bpsrate)
			sprintf(str, "%ubps", item->bpsrate);
		else
			strcpy(str, "Current");
		optmap[i] = BBSLIST_FIELD_BPSRATE;
		sprintf(opt[i++], "Fake Comm Rate    %s", str);
	}
	optmap[i] = BBSLIST_FIELD_HIDEPOPUPS;
	sprintf(opt[i++], "Hide Popups       %s", item->hidepopups ? "Yes" : "No");
	optmap[i] = BBSLIST_FIELD_PALETTE;
	strcpy(opt[i++], "Edit Palette");
	opt[i][0] = 0;
}

static void
build_edit_help(struct bbslist *item, int isdefault, char *helpbuf, size_t hbsz)
{
	size_t hblen = 0;
	bool is_ansi = true;
	bool is_serial = ((item->conn_type == CONN_TYPE_MODEM) || (item->conn_type == CONN_TYPE_SERIAL)
	|| (item->conn_type == CONN_TYPE_SERIAL_NORTS));

	if (get_emulation(item) != CTERM_EMULATION_ANSI_BBS)
		is_ansi = false;

	/*
	 * Not using strncpy() here because memset()ing the whole thing
	 * to NUL here is stupid.
	 */
	if (isdefault)
		hblen = strlcpy(helpbuf, "`Edit Default Connection`\n\n", hbsz);
	else
		hblen = strlcpy(helpbuf, "`Edit Directory Entry`\n\n~ CTRL-S ~ To Edit Explicit Sort Index\n\n", hbsz);

	hblen += strlcat(helpbuf + hblen, "Select item to edit.\n\n", hbsz - hblen);

	if (!isdefault) {
		hblen += strlcat(helpbuf + hblen, "~ Name ~\n"
		                                  "        The name of the BBS entry\n\n", hbsz - hblen);
		switch (item->conn_type) {
			case CONN_TYPE_MODEM:
				hblen += strlcat(helpbuf + hblen, "~ Phone Number ~\n"
				                                  "        Phone number to dial\n\n", hbsz - hblen);
				break;
			case CONN_TYPE_SERIAL:
			case CONN_TYPE_SERIAL_NORTS:
				hblen += strlcat(helpbuf + hblen, "~ Device Name ~\n"
				                                  "        Name of the COM port device to open\n\n", hbsz - hblen);
				break;
			case CONN_TYPE_SHELL:
				hblen += strlcat(helpbuf + hblen, "~ Command ~\n"
				                                  "        Command to run in the terminal\n\n", hbsz - hblen);
				break;
			default:
				hblen += strlcat(helpbuf + hblen, "~ Address ~\n"
				                                  "        Network address to connect to\n\n", hbsz - hblen);
				break;
		}
	}
	hblen += strlcat(helpbuf + hblen, "~ Conection Type ~\n"
	                                  "        Type of connection\n\n", hbsz - hblen);
	if (IS_NETWORK_CONN(item->conn_type)) {
		hblen += strlcat(helpbuf + hblen, "~ Address Family ~\n"
		                                  "       IPv4 or IPv6\n\n", hbsz - hblen);
	}
	if (is_serial) {
		hblen += strlcat(helpbuf + hblen, "~ Comm Rate ~\n"
		                                  "        Port speed\n\n"
		                                  "~ Flow Control ~\n"
		                                  "        Type of flow control to use\n\n"
		                                  "~ Stop Bits ~\n"
		                                  "        Number of stop bits for each byte\n\n"
		                                  "~ Stop Bits ~\n"
		                                  "        Number of stop bits to send for each byte\n\n"
		                                  "~ Parity ~\n"
		                                  "        Type of parity for each byte\n\n", hbsz - hblen);
	}
	else if (item->conn_type != CONN_TYPE_SHELL) {
		hblen += strlcat(helpbuf + hblen, "~ TCP Port ~\n"
		                                  "        IP port to connect to\n\n", hbsz - hblen);
	}
	if (item->conn_type == CONN_TYPE_MBBS_GHOST) {
		hblen += strlcat(helpbuf + hblen, "~ Username ~\n"
		                                  "        Username sent by the GHost protocol\n\n"
		                                  "~ GHost Program ~\n"
		                                  "        Program name for GHost connection\n\n"
		                                  "~ System Password ~\n"
		                                  "        System Password sent by auto-login command (not securely stored)\n\n", hbsz - hblen);
	}
	else if (item->conn_type == CONN_TYPE_SSHNA) {
		hblen += strlcat(helpbuf + hblen, "~ SSH Username ~\n"
		                                  "        Username sent by the SSH protocol\n\n"
		                                  "~ BBS Username ~\n"
		                                  "        Username sent by auto-login\n\n"
		                                  "~ BBS Password ~\n"
		                                  "        Password sent by auto-login command (not securely stored)\n\n", hbsz - hblen);
	}
	else {
		hblen += strlcat(helpbuf + hblen, "~ Username ~\n"
		                                  "        Username sent by auto-login\n\n"
		                                  "~ Password ~\n"
		                                  "        Password sent by auto-login command (not securely stored)\n\n"
		                                  "~ System Password ~\n"
		                                  "        System Password sent by auto-login command (not securely stored)\n\n", hbsz - hblen);
	}
	if (item->conn_type == CONN_TYPE_SSH || item->conn_type == CONN_TYPE_SSHNA) {
		hblen += strlcat(helpbuf + hblen, "~ SFTP Public Key ~\n"
		                                  "        Open an SFTP channel and transfer the public key to\n"
		                                  "        .ssh/authorized_keys\n\n", hbsz - hblen);
	}
	if (item->conn_type == CONN_TYPE_TELNET || item->conn_type == CONN_TYPE_TELNETS) {
		hblen += strlcat(helpbuf + hblen, "~ Binmode Broken ~\n"
		                                  "        Telnet binary mode is broken on the remote system, do not\n"
		                                  "        enable it when connecting\n\n", hbsz - hblen);
		hblen += strlcat(helpbuf + hblen, "~ Defer Negotiate ~\n"
		                                  "        Some systems have a mailer or other program running on the\n"
		                                  "        initial connection, and will either disconnect or just ignore\n"
		                                  "        telnet negotiations at the start of the session.  When this\n"
		                                  "        option is enabled, SyncTERM will wait until it receives a telnet\n"
		                                  "        command from the remote before starting telnet negotiation.\n\n", hbsz - hblen);
	}
	hblen += strlcat(helpbuf + hblen, "~ Screen Mode ~\n"
	                                  "        Display mode to use\n\n", hbsz - hblen);
	if (item->conn_type == CONN_TYPE_SSH || item->conn_type == CONN_TYPE_SSHNA
	    || item->conn_type == CONN_TYPE_TELNET || item->conn_type == CONN_TYPE_TELNETS
	    || item->conn_type == CONN_TYPE_RLOGIN || item->conn_type == CONN_TYPE_RLOGIN_REVERSED
	    || item->conn_type == CONN_TYPE_SHELL) {
		hblen += strlcat(helpbuf + hblen, "~Terminal Type~\n"
		                                 "        Type of terminal to advertise to remote\n\n", hbsz - hblen);
	}

	hblen += strlcat(helpbuf + hblen, "~ Font ~\n"
	                                  "        Select font to use for the entry\n\n"
	                                  "~ Hide Popups ~\n"
	                                  "        Hide all popup dialogs (i.e., Connecting, Disconnected, etc.)\n\n", hbsz - hblen);
	if (is_ansi) {
		hblen += strlcat(helpbuf + hblen, "~ ANSI Music ~\n"
		                                  "        ANSI music type selection\n\n"
		                                  "~ RIP ~\n"
		                                  "        Enable/Disable RIP modes\n\n"
		                                  "~ Force LCF Mode ~\n"
		                                  "        Force Last Column Flag mode as used in VT terminals\n\n"
		                                  "~ Yellow Is Yellow ~\n"
		                                  "        Make the dark yellow colour actually yellow instead of the brown\n"
		                                  "        used in IBM CGA monitors\n\n", hbsz - hblen);
	}
	hblen += strlcat(helpbuf + hblen, "~ Hide Status Line ~\n"
	                                  "        Selects if the status line should be hidden, giving an extra\n"
	                                  "        display row\n\n"
	                                  "~ Download Path ~\n"
	                                  "        Default path to store downloaded files\n\n"
	                                  "~ Upload Path ~\n"
	                                  "        Default path for uploads\n\n"
	                                  "~ Log Configuration ~\n"
	                                  "        Configure logging settings\n\n", hbsz - hblen);
	if (!is_serial) {
		hblen += strlcat(helpbuf + hblen, "~ Fake Comm Rate ~\n"
		                                  "        Display speed\n\n", hbsz - hblen);
	}
	hblen += strlcat(helpbuf + hblen, "~ Palette ~\n"
	                                  "        Edit colour palette for this entry\n\n", hbsz - hblen);
}

static uint32_t
get_default_palette_value(int palette, size_t entry)
{
	uint32_t pe = palettes[palette][entry];
	return (uint32_t)dac_default[pe].red << 16 | (uint32_t)dac_default[pe].green << 8 | (uint32_t)dac_default[pe].blue;
}

static void
bl_kbwait(void)
{
	kbwait(1000);
}

#define COLORBOX_WIDTH  15
#define COLORBOX_HEIGHT  5
static void
update_colourbox(uint32_t colour, uint32_t fg_colour, struct vmem_cell *new)
{
	char nattr = uifc.hclr | (uifc.bclr << 4);
	char iattr = uifc.lclr | (uifc.cclr << 4);
	char str[COLORBOX_WIDTH + 1];
	struct vmem_cell *ptr = new;
	size_t i;

	uint8_t attr = RED | GREEN << 4;
	if (cio_api.options & CONIO_OPT_PALETTE_SETTING) {
		setpalette(1, (fg_colour >> 16 & 0xff) | (fg_colour >> 8 & 0xff00)
		    , (fg_colour >> 8 & 0xff) | (fg_colour & 0xff00)
		    , (fg_colour & 0xff) | (fg_colour << 8 & 0xff00));
		setpalette(2, (colour >> 16 & 0xff) | (colour >> 8 & 0xff00)
		    , (colour >> 8 & 0xff) | (colour & 0xff00)
		    , (colour & 0xff) | (colour << 8 & 0xff00));
		strlcpy(str, "   Color   ", sizeof(str));
	}
	else {
		attr = nattr;
		snprintf(str, sizeof(str), "  #%06" PRIx32 "   ", colour);
	}
	const char *p;
	for (p = str, ptr = &new[COLORBOX_WIDTH * 1 + 2]; *p; p++, ptr++)
		set_vmem(ptr, *p, attr, 0);

	snprintf(str, sizeof(str), "%-3d %-3d %-3d", colour >> 16 & 0xff, colour >> 8 & 0xff, colour & 0xff);
	for (i = 0, ptr = &new[COLORBOX_WIDTH * 2 + 2]; str[i]; i++, ptr++)
		set_vmem(ptr, str[i], i % 4 == 3 ? nattr : iattr, 0);

	const char *bline = "Red Grn Blu";
	for (p = bline, ptr = &new[COLORBOX_WIDTH * (COLORBOX_HEIGHT - 2) + 2]; *p; p++, ptr++)
		ptr->ch = *p;
}

static uint32_t
edit_colour(uint32_t colour, uint32_t reset_value, uint32_t palette[16])
{
	struct vmem_cell old[COLORBOX_WIDTH * COLORBOX_HEIGHT]; // MSVC doesn't allow VLAs
	struct vmem_cell new[COLORBOX_WIDTH * COLORBOX_HEIGHT]; // MSVC doesn't allow VLAs
	int left, top;
	int x, y;
	struct text_info ti;
	char nattr = uifc.hclr | (uifc.bclr << 4);
	uint32_t fg_attr = 7;
	int field = 0;

	gettextinfo(&ti);
	left = (ti.screenwidth - COLORBOX_WIDTH) / 2;
	top = (ti.screenheight - COLORBOX_HEIGHT) / 2;
	vmem_gettext(left, top, left + COLORBOX_WIDTH - 1, top + COLORBOX_HEIGHT - 1, old);

	for (y = 0; y < COLORBOX_HEIGHT; y++) {
		for (x = 0; x < COLORBOX_WIDTH; x++) {
			set_vmem(&new[y * COLORBOX_WIDTH + x], 0, nattr, 0);
		}
	}

	struct vmem_cell *tptr = new;
	struct vmem_cell *bptr = &new[(COLORBOX_HEIGHT - 1) * COLORBOX_WIDTH];
	(tptr++)->ch = uifc.chars->input_top_left;
	(bptr++)->ch = uifc.chars->input_bottom_left;
	for (size_t i = 0; i < COLORBOX_WIDTH - 2; i++) {
		if (uifc.mode & UIFC_MOUSE) {
			switch (i) {
				case 0:
					(tptr++)->ch = uifc.chars->button_left;
					break;
				case 1:
					set_vmem(tptr++, uifc.chars->close_char, uifc.lclr | (uifc.bclr << 4), 0);
					break;
				case 2:
					(tptr++)->ch = uifc.chars->button_right;
					break;
				case 3:
					(tptr++)->ch = uifc.chars->button_left;
					break;
				case 4:
					set_vmem(tptr++, uifc.chars->help_char, uifc.lclr | (uifc.bclr << 4), 0);
					break;
				case 5:
					(tptr++)->ch = uifc.chars->button_right;
					break;
				default:
					(tptr++)->ch = uifc.chars->input_top;
					break;
			}
		}
		else
			(tptr++)->ch = uifc.chars->input_top;
		(bptr++)->ch = uifc.chars->input_bottom;
	}
	tptr->ch = uifc.chars->input_top_right;
	bptr->ch = uifc.chars->input_bottom_right;
	for (size_t i = 1; i < (COLORBOX_HEIGHT - 1); i++) {
		new[COLORBOX_WIDTH * i].ch = uifc.chars->input_left;
		new[COLORBOX_WIDTH * (i + 1) - 1].ch = uifc.chars->input_right;
	}

	uifc.helpbuf = "`Edit Palette Entry`\n\n"
		       "~TAB/Backtab~ switches between Red, Green, and Blue.\n"
		       "~CR~          saves the current colour component.\n"
		       "~UP/DOWN~     changes the example foreground colour\n"
		       "~%~           resets to default value\n"
		       "Each value should be a number between 0 and 255, indicating the\n"
		       "relative brightness of the named colour channel\n"
		       "(Red, Green, and Blue).";
	for (;;) {
		char nstr[4];
		int last;
		uint8_t cval;
		update_colourbox(colour, palette[fg_attr], new);
		vmem_puttext(left, top, left + COLORBOX_WIDTH - 1, top + COLORBOX_HEIGHT - 1, new);
		if (field < 0 || field > 2)
			field = 0;
		switch (field) {
			case 0:
				cval = colour >> 16 & 0xff;
				break;
			case 1:
				cval = colour >> 8 & 0xff;
				break;
			case 2:
				cval = colour & 0xff;
				break;
		}
		snprintf(nstr, sizeof(nstr), "%d", cval);
		uifc.exitstart = left + 1;
		uifc.exitend = left + 3;
		uifc.helpstart = left + 4;
		uifc.helpend = left + 6;
		uifc.buttony = top;
		uifc.getstrxy(left + 2 + field * 4, top + 2, 3, nstr, 3, K_SCANNING | K_NUMBER | K_EDIT | K_NOCRLF | K_DEUCEEXIT | K_TABEXIT, &last);
		switch (last) {
			uint32_t nval;
			case ESC:
				goto done;
			case CIO_KEY_UP:
				if (fg_attr)
					fg_attr--;
				else
					fg_attr = 15;
				break;
			case CIO_KEY_DOWN:
				if (fg_attr == 15)
					fg_attr = 0;
				else
					fg_attr++;
				break;
			case '%':
				if ((reset_value & 0xFF000000) == 0)
					colour = reset_value;
				break;
			case '\r':
				nval = strtoul(nstr, NULL, 10);
				switch (field) {
					case 0:
						colour &= 0xffff;
						colour |= nval << 16;
						break;
					case 1:
						colour &= 0xff00ff;
						colour |= nval << 8;
						break;
					case 2:
						colour &= 0xffff00;
						colour |= nval;
						break;
				}
				// Fallthrough
			case '\t':
				field++;
				if (field == 3)
					field = 0;
				break;
			case 3840: // Backtab
				if (field)
					field--;
				else
					field = 2;
				break;
			default:
				fprintf(stderr, "Char: %d\n", last);
				break;
		}

		if (last == ESC)
			break;
	}

done:
	setpalette(1,
		   dac_default[4].red << 8 | dac_default[1].red,
		   dac_default[4].green << 8 | dac_default[1].green,
		   dac_default[4].blue << 8 | dac_default[1].blue);
	setpalette(2,
		   dac_default[2].red << 8 | dac_default[2].red,
		   dac_default[2].green << 8 | dac_default[2].green,
		   dac_default[2].blue << 8 | dac_default[2].blue);
	vmem_puttext(left, top, left + COLORBOX_WIDTH - 1, top + COLORBOX_HEIGHT - 1, old);
	return colour;
}

static bool
edit_palette(struct bbslist *item)
{
	char opt[17][69];
	char *opts[(sizeof(opt) / sizeof(opt[0])) + 1];
	int dflt = 0;
	int bar = 0;
	int vmode;
	int tmode;
	int palette;
	uint32_t oldp[16];
	unsigned old_size = item->palette_size;
	unsigned min_palette_sz = 16;

	memcpy(oldp, item->palette, sizeof(oldp));
	tmode = screen_to_ciolib(item->screen_mode);
	vmode = find_vmode(tmode);
	if (vmode == -1) {
		char errstr[128];
		snprintf(errstr, sizeof(errstr), "Failed to map text mode %d to video mode", tmode);
		uifcmsg(errstr, NULL);
		return false;
	}
	palette = vparams[vmode].palette;

	for (size_t i = 0; i < sizeof(opt) / sizeof(opt[0]); i++)
		opts[i] = opt[i];
	switch(palette) {
		case PRESTEL_PALETTE:
			min_palette_sz = 8;
			break;
		case ATARI_PALETTE_4:
			min_palette_sz = 4;
			break;
		case ATARI_PALETTE_2:
			min_palette_sz = 2;
			break;
		default:
			min_palette_sz = 16;
			break;
	}
	for (;item->palette_size < min_palette_sz; item->palette_size++) {
		item->palette[item->palette_size] = get_default_palette_value(palette, item->palette_size);
	}
	for (;;) {
		opts[0] = 0;
		for (size_t i = 0; i < item->palette_size; i++) {
			snprintf(opt[i], sizeof(opt[i]), "Colour %2zd (#%06x)", i, item->palette[i]);
			opts[i] = opt[i];
			opts[i + 1] = 0;
		}
		uifc_winmode_t mode = WIN_SAV | WIN_ACT | WIN_INSACT| WIN_DELACT | WIN_EDIT;
		if (item->palette_size > min_palette_sz)
			mode |= WIN_DEL;
		if (item->palette_size < 16)
			mode |= WIN_INS | WIN_XTR;
		uifc.helpbuf = "`Edit Palette`\n\n"
		               "If there are fewer than sixteen entries in the palette, they will be\n"
		               "repeated to fill when whole palette.";
		int status = uifc.list(mode, 0, 0, 0, &dflt, &bar, "Edit Palette Entries", opts);
		if (status == -1)
			break;
		if ((status & MSK_ON) == MSK_EDIT)
			status &= MSK_OFF;
		if ((status & MSK_ON) == MSK_INS && item->palette_size < 16) {
			item->palette[item->palette_size] = get_default_palette_value(palette, item->palette_size);
			item->palette_size++;
		}
		else if ((status & MSK_ON) == MSK_DEL && item->palette_size > 1) {
			item->palette_size--;
		}
		else if (status == (status & MSK_OFF)) {
			item->palette[status] = edit_colour(item->palette[status], get_default_palette_value(palette, status), item->palette);
		}
	}
	if (item->palette_size == min_palette_sz) {
		unsigned i;
		for (i = 0; i < min_palette_sz; i++) {
			if (item->palette[i] != get_default_palette_value(palette, i))
				break;
		}
		if (i == min_palette_sz)
			item->palette_size = 0;
	}
	if (item->palette_size != old_size || memcmp(oldp, item->palette, sizeof(oldp)))
		return true;
	return false;
}

bool
edit_sort_order(str_list_t inifile, char *itemname, struct bbslist *item)
{
	char val[12];

	uifc.helpbuf = "`Explicit Sort Value`\n\n"
		       "This number is to allow manual overriding of sort order, it is not used\n"
		       "for any other purpose. The default value is 0.\n";

	// NOTE: No way to enter negative values
	snprintf(val, sizeof(val), "%" PRId32, item->sort_order);
	int32_t old_order = item->sort_order;
	if (uifc.input(WIN_MID, 0, 0, "Explicit Sort Value", val, 11, K_NUMBER | K_NEGATIVE | K_EDIT) >= 0) {
		item->sort_order = atoi(val);
		iniSetInt32(&inifile, itemname, "SortOrder", item->sort_order, &ini_style);
	}
	return old_order != item->sort_order;
}

int
edit_list(struct bbslist **list, struct bbslist *item, char *listpath, int isdefault)
{
#define EDIT_LIST_MAX 41
	char opt[EDIT_LIST_MAX + 1][69]; /* EDIT_LIST_MAX=Holds number of menu items, 69=Number of columns */
	char optname[69];
	int optmap[EDIT_LIST_MAX + 1];
#undef EDIT_LIST_MAX
	char *opts[(sizeof(opt) / sizeof(opt[0])) + 1];
	int changed = 0;
	int copt = 0, i, j;
	int bar = 0;
	char str[64];
	FILE *listfile;
	str_list_t inifile;
	char *itemname;
	char *tmpptr;
	char helpbuf[8192];

	for (i = 0; i < sizeof(opt) / sizeof(opt[0]); i++)
		opts[i] = opt[i];
	if (item->type == SYSTEM_BBSLIST) {
		uifc.helpbuf = "`Copy from system directory`\n\n"
		               "This entry was loaded from the system directory.  In order to edit it, it\n"
		               "must be copied into your personal directory.\n";
		i = 0;
		if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Copy from system directory?", YesNo) != 0) {
			check_exit(false);
			return 0;
		}
		item->type = USER_BBSLIST;
		add_bbs(listpath, item, true);
	}
	if ((listfile = fopen(listpath, "rb")) != NULL) {
		inifile = iniReadBBSList(listfile, true);
		fclose(listfile);
	}
	else
		inifile = strListInit();

	if (isdefault)
		itemname = NULL;
	else
		itemname = item->name;
	build_edit_help(item, isdefault, helpbuf, sizeof(helpbuf));
	for (; !quitting;) {
		memset(optmap, 0, sizeof(optmap));
		build_edit_list(item, opt, optmap, opts, isdefault, itemname);
		uifc.changes = 0;
		uifc.helpbuf = helpbuf;
		i = uifc.list(WIN_EXTKEYS | WIN_MID | WIN_SAV | WIN_ACT, 0, 0, 0, &copt, &bar,
		              isdefault ? "Edit Default Connection" : "Edit Directory Entry",
		              opts);
		if (i == -2 - CTRL_S && !isdefault) {
			if (edit_sort_order(inifile, itemname, item))
				changed = 1;
		}
		if (i < -1)
			continue;
		// Remember, i gets converted to (unsigned) size_t in comparison
		if (i > 0 && i >= (sizeof(optmap) / sizeof(optmap[0])))
			continue;
		if (i >= 0) {
			if (optmap[i] == BBSLIST_FIELD_NONE)
				continue;
			strcpy(optname, opt[i]);
			i = optmap[i];
			for (tmpptr = optname; *tmpptr; tmpptr++) {
				if (tmpptr[0] == 0)
					break;
				if (tmpptr[0] == ' ') {
					if (tmpptr[1] == ' ' || tmpptr[1] == 0) {
						*tmpptr = 0;
						break;
					}
				}
			}
		}
		switch (i) {
			case -1:
				check_exit(false);
				if ((!isdefault) && (itemname != NULL) && (itemname[0] == 0)) {
					uifc.helpbuf = "`Cancel Save`\n\n"
					               "This entry has no name and can therefore not be saved.\n"
					               "Selecting `No` will return to editing mode.\n";
					i = 0;
					if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Cancel Save?",
					              YesNo) != 0) {
						quitting = false;
						check_exit(false);
						break;
					}
					strListFree(&inifile);
					return 0;
				}
				if (!safe_mode) {
					if ((listfile = fopen(listpath, "wb")) != NULL) {
						iniWriteEncryptedFile(listfile, inifile, list_algo, list_keysize, settings.keyDerivationIterations, list_password, NULL);
						fclose(listfile);
					}
				}
				strListFree(&inifile);
				return changed;
			case BBSLIST_FIELD_NAME:
				edit_name(itemname, list, inifile, false);
				break;
			case BBSLIST_FIELD_ADDR:
				uifc.helpbuf = address_help;
				uifc.input(WIN_MID | WIN_SAV,
				           0, 0, optname, item->addr, LIST_ADDR_MAX, K_EDIT);
				check_exit(false);
				iniSetString(&inifile, itemname, "Address", item->addr, &ini_style);
				break;
			case BBSLIST_FIELD_CONN_TYPE:
				i = item->conn_type;
				item->conn_type--;
				uifc.helpbuf = conn_type_help;
				switch (uifc.list(WIN_SAV, 0, 0, 0, &(item->conn_type), NULL, optname,
				                  &(conn_types[1]))) {
					case -1:
						check_exit(false);
						item->conn_type = i;
						break;
					default:
						item->conn_type++;
						iniSetEnum(&inifile,
						           itemname,
						           "ConnectionType",
						           conn_types_enum,
						           item->conn_type,
						           &ini_style);

						// TODO: NOTE: This is destructive!  Beware!  Ooooooo....
						if ((i == CONN_TYPE_SSHNA) && (item->conn_type != CONN_TYPE_SSHNA)) {
							SAFECOPY(item->user, item->password);
							SAFECOPY(item->password, item->syspass);
							item->syspass[0] = 0;
						}
						if ((i != CONN_TYPE_SSHNA) && (item->conn_type == CONN_TYPE_SSHNA)) {
							SAFECOPY(item->syspass, item->password);
							SAFECOPY(item->password, item->user);
							item->user[0] = 0;
						}

						if ((item->conn_type != CONN_TYPE_MODEM)
						    && (item->conn_type != CONN_TYPE_SERIAL)
						    && (item->conn_type != CONN_TYPE_SERIAL_NORTS)
						    && (item->conn_type != CONN_TYPE_SHELL)) {
							/* Set the port too */
							j = conn_ports[item->conn_type];
							if ((j < 1) || (j > 65535))
								j = item->port;
							item->port = j;
							iniSetShortInt(&inifile,
							               itemname,
							               "Port",
							               item->port,
							               &ini_style);
						}

						changed = 1;
						break;
				}
				memset(optmap, 0, sizeof(optmap));
				build_edit_help(item, isdefault, helpbuf, sizeof(helpbuf));
				break;
			case BBSLIST_FIELD_FLOW_CONTROL:
				uifc.helpbuf = "`Flow Control`\n\n"
				               "Select the desired flow control type.\n"
				               "This should usually be left as \"RTS/CTS\".\n";
				i = fc_to_enum(item->flow_control);
				j = i;
				switch (uifc.list(WIN_SAV, 0, 0, 0, &j, NULL, optname, fc_names)) {
					case -1:
						check_exit(false);
						break;
					default:
						item->flow_control = fc_from_enum(j);
						if (j != i) {
							iniSetEnum(&inifile,
							           itemname,
							           "FlowControl",
							           fc_enum,
							           j,
							           &ini_style);
							uifc.changes = 1;
						}
						break;
				}
				break;
			case BBSLIST_FIELD_STOP_BITS:
				switch (item->stop_bits) {
					case 1:
						item->stop_bits = 2;
						break;
					default:
						item->stop_bits = 1;
						break;
				}
				iniSetUShortInt(&inifile,
				                itemname,
				                "StopBits",
				                item->stop_bits,
				                &ini_style);
				uifc.changes = 1;
				break;
			case BBSLIST_FIELD_DATA_BITS:
				switch (item->data_bits) {
					case 8:
						item->data_bits = 7;
						break;
					default:
						item->data_bits = 8;
						break;
				}
				iniSetUShortInt(&inifile,
				                itemname,
				                "DataBits",
				                item->data_bits,
				                &ini_style);
				uifc.changes = 1;
				break;
			case BBSLIST_FIELD_PARITY:
				uifc.helpbuf = "`Parity`\n\n"
				               "Select the parity setting.";
				i = item->parity;
				switch (uifc.list(WIN_SAV, 0, 0, 0, &i, NULL, "Parity", parity_enum)) {
					case -1:
						check_exit(false);
						break;
					default:
						item->parity = i;
						iniSetEnum(&inifile, itemname, "Parity", parity_enum, item->parity, &ini_style);
						changed = 1;
				}
				break;
			case BBSLIST_FIELD_PORT:
				i = item->port;
				sprintf(str, "%hu", item->port);
				uifc.helpbuf = "`TCP Port`\n\n"
				               "Enter the TCP port number that the server is listening to on the remote system.\n"
				               "Telnet is generally port 23, RLogin is generally 513 and SSH is\n"
				               "generally 22\n";
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "TCP Port", str, 5, K_EDIT | K_NUMBER);
				check_exit(false);
				j = atoi(str);
				if ((j < 1) || (j > 65535))
					j = conn_ports[item->conn_type];
				item->port = j;
				iniSetUShortInt(&inifile, itemname, "Port", item->port, &ini_style);
				if (i != j)
					uifc.changes = 1;
				else
					uifc.changes = 0;
				break;
			case BBSLIST_FIELD_USER:
				if (item->conn_type == CONN_TYPE_SSHNA) {
					uifc.helpbuf = "`SSH Username`\n\n"
					               "Enter the username for passwordless SSH authentication.";
				}
				else {
					uifc.helpbuf = "`Username`\n\n"
					               "Enter the username to attempt auto-login to the remote with.\n"
					               "For SSH, this must be the SSH user name.";
				}
				uifc.input(WIN_MID | WIN_SAV, 0, 0, optname, item->user, MAX_USER_LEN, K_EDIT);
				check_exit(false);
				iniSetString(&inifile, itemname, "UserName", item->user, &ini_style);
				break;
			case BBSLIST_FIELD_PASSWORD:
				if (item->conn_type == CONN_TYPE_MBBS_GHOST) {
					uifc.helpbuf = "`GHost Program`\n\n"
					               "Enter the program name to be sent.";
				}
				else if (item->conn_type == CONN_TYPE_SSHNA) {
					uifc.helpbuf = "`BBS Username`\n\n"
					               "Enter the username to be sent for auto-login (ALT-L).";
				}
				else {
					uifc.helpbuf = "`Password`\n\n"
					               "Enter your password for auto-login.\n"
					               "For SSH, this must be the SSH password if it exists.\n";
				}
				uifc.input(WIN_MID | WIN_SAV, 0, 0, optname, item->password, MAX_PASSWD_LEN, K_EDIT);
				check_exit(false);
				iniSetString(&inifile, itemname, "Password", item->password, &ini_style);
				break;
			case BBSLIST_FIELD_SYSPASS:
				if (item->conn_type == CONN_TYPE_SSHNA) {
					uifc.helpbuf = "`BBS Password`\n\n"
					               "Enter your password for auto-login. (ALT-L)\n";
				}
				else {
					uifc.helpbuf = "`System Password`\n\n"
					               "Enter your System password for auto-login.\n"
					               "This password is sent after the username and password, so for non-\n"
					               "Synchronet, or non-sysop accounts, this can be used for simple\n"
					               "scripting.";
				}
				uifc.input(WIN_MID | WIN_SAV,
				           0,
				           0,
				           optname,
				           item->syspass,
				           MAX_SYSPASS_LEN,
				           K_EDIT);
				check_exit(false);
				iniSetString(&inifile, itemname, "SystemPassword", item->syspass, &ini_style);
				break;
			case BBSLIST_FIELD_SCREEN_MODE:
				i = item->screen_mode;
				uifc.helpbuf = "`Screen Mode`\n\n"
				               "Select the screen size for this connection\n";
				j = i;
				switch (uifc.list(WIN_SAV, 0, 0, 0, &(item->screen_mode), &j, "Screen Mode",
				                  screen_modes)) {
					case -1:
						check_exit(false);
						item->screen_mode = i;
						break;
					default:
						iniSetEnum(&inifile,
						           itemname,
						           "ScreenMode",
						           screen_modes_enum,
						           item->screen_mode,
						           &ini_style);
						if ((item->rip == RIP_VERSION_1)
						    && (item->screen_mode != SCREEN_MODE_EGA_80X25)
						    && (item->screen_mode != SCREEN_MODE_80X43)) {
							item->rip = RIP_VERSION_3;
							iniSetEnum(&inifile,
							           itemname,
							           "RIP",
							           rip_versions,
							           item->rip,
							           &ini_style);
						}
						if (item->screen_mode == SCREEN_MODE_C64) {
							SAFECOPY(item->font, font_names[33]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,
							           itemname,
							           "NoStatus",
							           item->nostatus,
							           &ini_style);
						}
						else if ((item->screen_mode == SCREEN_MODE_C128_40)
						|| (item->screen_mode == SCREEN_MODE_C128_80)) {
							SAFECOPY(item->font, font_names[35]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,
							           itemname,
							           "NoStatus",
							           item->nostatus,
							           &ini_style);
						}
						else if (item->screen_mode == SCREEN_MODE_ATARI) {
							SAFECOPY(item->font, font_names[36]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,
							           itemname,
							           "NoStatus",
							           item->nostatus,
							           &ini_style);
						}
						else if (item->screen_mode == SCREEN_MODE_ATARI_XEP80) {
							SAFECOPY(item->font, font_names[36]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,
							           itemname,
							           "NoStatus",
							           item->nostatus,
							           &ini_style);
						}
						else if (item->screen_mode == SCREEN_MODE_PRESTEL || item->screen_mode == SCREEN_MODE_BEEB) {
							SAFECOPY(item->font, font_names[43]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
						}
						else if (item->screen_mode == SCREEN_MODE_ATARIST_40X25
						    || item->screen_mode == SCREEN_MODE_ATARIST_80X25
						    || item->screen_mode == SCREEN_MODE_ATARIST_80X25_MONO) {
							SAFECOPY(item->font, font_names[44]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
						}
						else if (i == SCREEN_MODE_ATARIST_40X25
						    || i == SCREEN_MODE_ATARIST_80X25
						    || i == SCREEN_MODE_ATARIST_80X25_MONO) {
							SAFECOPY(item->font, font_names[0]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
						}
						else if ((i == SCREEN_MODE_C64) || (i == SCREEN_MODE_C128_40)
						    || (i == SCREEN_MODE_C128_80) || (i == SCREEN_MODE_ATARI)
						    || (i == SCREEN_MODE_ATARI_XEP80) || (i == SCREEN_MODE_PRESTEL)
						    || (i == SCREEN_MODE_BEEB)) {
							SAFECOPY(item->font, font_names[0]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							item->nostatus = 0;
							iniSetBool(&inifile,
							           itemname,
							           "NoStatus",
							           item->nostatus,
							           &ini_style);
						}
						changed = 1;
						break;
				}
				break;
			case BBSLIST_FIELD_NOSTATUS:
				item->nostatus = !item->nostatus;
				changed = 1;
				iniSetBool(&inifile, itemname, "NoStatus", item->nostatus, &ini_style);
				break;
			case BBSLIST_FIELD_DLDIR:
				uifc.helpbuf = "`Download Path`\n\n"
				               "Enter the path where downloads will be placed.";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Download Path", item->dldir, MAX_PATH,
				               K_EDIT) >= 0)
					iniSetString(&inifile, itemname, "DownloadPath", item->dldir, &ini_style);
				else
					check_exit(false);
				break;
			case BBSLIST_FIELD_ULDIR:
				uifc.helpbuf = "`Upload Path`\n\n"
				               "Enter the path where uploads will be browsed from.";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Upload Path", item->uldir, MAX_PATH,
				               K_EDIT) >= 0)
					iniSetString(&inifile, itemname, "UploadPath", item->uldir, &ini_style);
				else
					check_exit(false);
				break;
			case BBSLIST_FIELD_LOGFILE:
				configure_log(item, itemname, inifile, &changed);
				break;
			case BBSLIST_FIELD_BPSRATE:
				uifc.helpbuf = "`Comm Rate (in bits-per-second)`\n\n"
				               "`For TCP connections:`\n"
				               "Select the rate which received characters will be displayed.\n\n"
				               "This allows animated ANSI and some games to work as intended.\n\n"
				               "`For Modem/Direct COM port connections:`\n"
				               "Select the `DTE Rate` to use."
				               ;
				i = get_rate_num(item->bpsrate);
				switch (uifc.list(WIN_SAV, 0, 0, 0, &i, NULL, "Comm Rate (BPS)", rate_names)) {
					case -1:
						check_exit(false);
						break;
					default:
						item->bpsrate = rates[i];
						iniSetInteger(&inifile, itemname, "BPSRate", item->bpsrate, &ini_style);
						changed = 1;
				}
				break;
			case BBSLIST_FIELD_MUSIC:
				uifc.helpbuf = music_helpbuf;
				i = item->music;
				if (uifc.list(WIN_SAV, 0, 0, 0, &i, NULL, "ANSI Music Setup", music_names) != -1) {
					item->music = i;
					iniSetInteger(&inifile, itemname, "ANSIMusic", item->music, &ini_style);
					changed = 1;
				}
				else
					check_exit(false);
				break;
			case BBSLIST_FIELD_ADDRESS_FAMILY:
				uifc.helpbuf = address_family_help;
				i = item->address_family;
				if (uifc.list(WIN_SAV, 0, 0, 0, &i, NULL, "Address Family",
				              address_family_names) != -1) {
					item->address_family = i;
					iniSetEnum(&inifile,
					           itemname,
					           "AddressFamily",
					           address_families,
					           item->address_family,
					           &ini_style);
					changed = 1;
				}
				else
					check_exit(false);
				break;
			case BBSLIST_FIELD_FONT:
				uifc.helpbuf = "`Font`\n\n"
				               "Select the desired font for this connection.\n\n"
				               "Some fonts do not allow some modes.  When this is the case, an\n"
				               "appropriate mode will be forced.\n";
				i = j = find_font_id(item->font);
				switch (uifc.list(WIN_SAV, 0, 0, 0, &i, &j, "Font", font_names)) {
					case -1:
						check_exit(false);
						break;
					default:
						if (i != find_font_id(item->font)) {
							SAFECOPY(item->font, font_names[i]);
							iniSetString(&inifile, itemname, "Font", item->font,
							             &ini_style);
							changed = 1;
						}
				}
				break;
			case BBSLIST_FIELD_HIDEPOPUPS:
				item->hidepopups = !item->hidepopups;
				changed = 1;
				iniSetBool(&inifile, itemname, "HidePopups", item->hidepopups, &ini_style);
				break;
			case BBSLIST_FIELD_RIP:
				item->rip = get_rip_version(item->rip, &changed);
				if (item->rip == RIP_VERSION_1) {
					item->screen_mode = SCREEN_MODE_80X43;
					iniSetEnum(&inifile,
					           itemname,
					           "ScreenMode",
					           screen_modes_enum,
					           item->screen_mode,
					           &ini_style);
				}
				iniSetEnum(&inifile, itemname, "RIP", rip_versions, item->rip, &ini_style);
				break;
			case BBSLIST_FIELD_FORCE_LCF:
				item->force_lcf = !item->force_lcf;
				changed = 1;
				iniSetBool(&inifile, itemname, "ForceLCF", item->force_lcf, &ini_style);
				break;
			case BBSLIST_FIELD_YELLOW_IS_YELLOW:
				item->yellow_is_yellow = !item->yellow_is_yellow;
				changed = 1;
				iniSetBool(&inifile, itemname, "YellowIsYellow", item->yellow_is_yellow, &ini_style);
				break;
			case BBSLIST_FIELD_SFTP_PUBLIC_KEY:
				item->sftp_public_key = !item->sftp_public_key;
				changed = 1;
				iniSetBool(&inifile, itemname, "SFTPPublicKey", item->sftp_public_key, &ini_style);
				break;
			case BBSLIST_FIELD_TELNET_NO_BINARY:
				item->telnet_no_binary = !item->telnet_no_binary;
				changed = 1;
				iniSetBool(&inifile, itemname, "TelnetBrokenTextmode", item->telnet_no_binary, &ini_style);
				break;
			case BBSLIST_FIELD_TELNET_DEFERRED_NEGOTIATION:
				item->defer_telnet_negotiation = !item->defer_telnet_negotiation;
				changed = 1;
				iniSetBool(&inifile, itemname, "TelnetDeferNegotiate", item->defer_telnet_negotiation, &ini_style);
				break;
			case BBSLIST_FIELD_PALETTE:
				if (edit_palette(item)) {
					if (item->palette_size == 0)
						iniRemoveKey(&inifile, itemname, "Palette");
					else
						iniSetIntList(&inifile, itemname, "Palette", ",", (int*)item->palette, item->palette_size, &ini_style);
					changed = 1;
				}
				break;
			case BBSLIST_FIELD_TERMINAL_TYPE:
				uifc.helpbuf = "`Terminal Type`\n\n"
					       "Sent to the remote to allow them to know what type of emulation is\n"
					       "supported.\n\n"
					       "Leave blank to use the correct value based on the screen mode.";
				uifc.input(WIN_MID | WIN_SAV,
				           0,
				           0,
				           optname,
				           item->term_name,
				           sizeof(item->term_name) - 1,
				           K_EDIT);
				check_exit(false);
				iniSetString(&inifile, itemname, "TerminalType", item->term_name, &ini_style);
				break;
		}
		if (uifc.changes)
			changed = 1;
	}
	strListFree(&inifile);
	return changed;
}

void
add_bbs(char *listpath, struct bbslist *bbs, bool new_entry)
{
	FILE *listfile;
	str_list_t inifile;

	if (safe_mode)
		return;
	if ((listfile = fopen(listpath, "rb")) != NULL) {
		inifile = iniReadBBSList(listfile, true);
		fclose(listfile);
	}
	else
		inifile = strListInit();

	/*
	 * Redundant:
	 * iniAddSection(&inifile,bbs->name,NULL);
	 */
	if (new_entry) {
		bbs->connected = 0;
		bbs->calls = 0;
	}
	iniSetString(&inifile, bbs->name, "Address", bbs->addr, &ini_style);
	iniSetShortInt(&inifile, bbs->name, "Port", bbs->port, &ini_style);
	iniSetDateTime(&inifile, bbs->name, "Added", /* include time */ true, time(NULL), &ini_style);
	iniSetDateTime(&inifile, bbs->name, "LastConnected", /* include time */ true, bbs->connected, &ini_style);
	iniSetInteger(&inifile, bbs->name, "TotalCalls", bbs->calls, &ini_style);
	iniSetString(&inifile, bbs->name, "UserName", bbs->user, &ini_style);
	iniSetString(&inifile, bbs->name, "Password", bbs->password, &ini_style);
	iniSetString(&inifile, bbs->name, "SystemPassword", bbs->syspass, &ini_style);
	iniSetEnum(&inifile, bbs->name, "ConnectionType", conn_types_enum, bbs->conn_type, &ini_style);
	iniSetEnum(&inifile, bbs->name, "FlowControl", fc_enum, fc_to_enum(bbs->flow_control), &ini_style);
	iniSetEnum(&inifile, bbs->name, "ScreenMode", screen_modes_enum, bbs->screen_mode, &ini_style);
	iniSetBool(&inifile, bbs->name, "NoStatus", bbs->nostatus, &ini_style);
	iniSetString(&inifile, bbs->name, "DownloadPath", bbs->dldir, &ini_style);
	iniSetString(&inifile, bbs->name, "UploadPath", bbs->uldir, &ini_style);
	iniSetString(&inifile, bbs->name, "LogFile", bbs->logfile, &ini_style);
	iniSetEnum(&inifile, bbs->name, "TransferLogLevel", log_levels, bbs->xfer_loglevel, &ini_style);
	iniSetEnum(&inifile, bbs->name, "TelnetLogLevel", log_levels, bbs->telnet_loglevel, &ini_style);
	iniSetBool(&inifile, bbs->name, "AppendLogFile", bbs->append_logfile, &ini_style);
	iniSetInteger(&inifile, bbs->name, "BPSRate", bbs->bpsrate, &ini_style);
	iniSetInteger(&inifile, bbs->name, "ANSIMusic", bbs->music, &ini_style);
	iniSetEnum(&inifile, bbs->name, "AddressFamily", address_families, bbs->address_family, &ini_style);
	iniSetString(&inifile, bbs->name, "Font", bbs->font, &ini_style);
	iniSetBool(&inifile, bbs->name, "HidePopups", bbs->hidepopups, &ini_style);
	iniSetEnum(&inifile, bbs->name, "RIP", rip_versions, bbs->rip, &ini_style);
	iniSetString(&inifile, bbs->name, "Comment", bbs->comment, &ini_style);
	iniSetBool(&inifile, bbs->name, "ForceLCF", bbs->force_lcf, &ini_style);
	iniSetBool(&inifile, bbs->name, "YellowIsYellow", bbs->yellow_is_yellow, &ini_style);
	if (bbs->term_name[0]) {
		iniSetString(&inifile, bbs->name, "TerminalType", bbs->term_name, &ini_style);
	}
	iniSetBool(&inifile, bbs->name, "TelnetBrokenTextmode", bbs->telnet_no_binary, &ini_style);
	iniSetBool(&inifile, bbs->name, "TelnetDeferNegotiate", bbs->defer_telnet_negotiation, &ini_style);
	if (bbs->has_fingerprint) {
		char fp[41];
		fp[0] = 0;
		for (int i = 0; i < 20; i++)
			sprintf(&fp[i * 2], "%02x", bbs->ssh_fingerprint[i]);
		fp[sizeof(fp) -1] = 0;
		iniSetString(&inifile, bbs->name, "SSHFingerprint", fp, &ini_style);
	}
	iniSetBool(&inifile, bbs->name, "SFTPPublicKey", bbs->sftp_public_key, &ini_style);
	iniSetUShortInt(&inifile, bbs->name, "StopBits", bbs->stop_bits, &ini_style);
	iniSetUShortInt(&inifile, bbs->name, "DataBits", bbs->data_bits, &ini_style);
	iniSetEnum(&inifile, bbs->name, "Parity", parity_enum, bbs->parity, &ini_style);
	static_assert(sizeof(int) == sizeof(uint32_t), "int must be four bytes");
	if (bbs->palette_size > 0)
		iniSetIntList(&inifile, bbs->name, "Palette", ",", (int*)bbs->palette, bbs->palette_size, &ini_style);
	if (bbs->sort_order != DEFAULT_SORT_ORDER_VALUE)
		iniSetInt32(&inifile, bbs->name, "SortOrder", bbs->sort_order, &ini_style);

	if ((listfile = fopen(listpath, "wb")) != NULL) {
		iniWriteEncryptedFile(listfile, inifile, list_algo, list_keysize, settings.keyDerivationIterations, list_password, NULL);
		fclose(listfile);
	}
	strListFree(&inifile);
}

static void
del_bbs(char *listpath, struct bbslist *bbs)
{
	FILE *listfile;
	str_list_t inifile;

	if (safe_mode)
		return;
	if ((listfile = fopen(listpath, "r+b")) != NULL) {
		inifile = iniReadBBSList(listfile, bbs->type == USER_BBSLIST);
		iniRemoveSection(&inifile, bbs->name);
		iniWriteEncryptedFile(listfile, inifile, list_algo, list_keysize, settings.keyDerivationIterations, list_password, NULL);
		fclose(listfile);
		strListFree(&inifile);
	}
}

static int
settings_list(int *opt, int *bar, char **opts, uifc_winmode_t extra_opts)
{
	uifc_winmode_t wm = WIN_T2B | WIN_RHT | WIN_EXTKEYS | WIN_DYN | WIN_ACT | extra_opts;
	return uifc.list(wm, 0, 0, 0, opt, bar, "SyncTERM Settings", opts);
}

/*
 * This is pretty sketchy...
 * These are pointers to automatic variables in show_bbslist() which are
 * used to redraw the entire menu set if the custom mode is current
 * and is changed.
 */
static int *glob_sopt;
static int *glob_sbar;
static char **glob_settings_menu;

static int *glob_listcount;
static int *glob_opt;
static int *glob_bar;
static char *glob_list_title;
static struct bbslist ***glob_list;

/*
 * This uses the above variables and therefore *must* be called from
 * show_bbslist().
 *
 * If show_bbslist() is not on the stack, this will do insane things.
 */
static void
custom_mode_adjusted(int *cur, char **opt)
{
	struct text_info ti;
	int cvmode;

	gettextinfo(&ti);
	if (ti.currmode != CIOLIB_MODE_CUSTOM) {
		cvmode = find_vmode(CIOLIB_MODE_CUSTOM);
		if (cvmode >= 0) {
			vparams[cvmode].cols = settings.custom_cols;
			vparams[cvmode].rows = settings.custom_rows;
			vparams[cvmode].charheight = settings.custom_fontheight;
			vparams[cvmode].aspect_width = settings.custom_aw;
			vparams[cvmode].aspect_height = settings.custom_ah;
		}
		return;
	}

	uifcbail();
	textmode(0);
	set_default_cursor();
	cvmode = find_vmode(CIOLIB_MODE_CUSTOM);
	if (cvmode >= 0) {
		vparams[cvmode].cols = settings.custom_cols;
		vparams[cvmode].rows = settings.custom_rows;
		vparams[cvmode].charheight = settings.custom_fontheight;
		vparams[cvmode].aspect_width = settings.custom_aw;
		vparams[cvmode].aspect_height = settings.custom_ah;
		textmode(ti.currmode);
		set_default_cursor();
	}
	init_uifc(true, true);

	// Draw BBS List
	uifc.list((*glob_listcount < MAX_OPTS ? WIN_XTR : 0)
	          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
	          | WIN_T2B | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
	          | WIN_SEL | WIN_INACT,
	          0, 0, 0, glob_opt, glob_bar, glob_list_title, (char **)*glob_list);

	// Draw settings menu
	settings_list(glob_sopt, glob_sbar, glob_settings_menu, WIN_INACT);

	// Draw program settings
	uifc.list(WIN_MID | WIN_SAV | WIN_ACT | WIN_DYN | WIN_SEL | WIN_INACT,
	          0,
	          0,
	          0,
	          cur,
	          NULL,
	          "Program Settings",
	          opt);
}

static int
settings_to_scale(void)
{
	int i = 0;

	if (!settings.blocky)
		i |= 1;
	if (settings.extern_scale)
		i |= 2;
	if (i > 2)
		i = 2;
	return i;
}

static void
scale_to_settings(int i)
{
	settings.blocky = (i & 1) ? false : true;
	settings.extern_scale = (i & 2) ? true : false;
}

static void
edit_audio_mode(str_list_t *inicontents)
{
	char opts[10][80];
	char *opt[11];
	int i = 0;
	int j = 0;

	uifc.helpbuf = "`Audio Output Mode`\n\n"
		       "        Options are tried in the order listed in the menu\n";

	while (i != -1) {
		for (i = 0; audio_output_types[i].name != NULL; i++) {
			SAFEPRINTF2(opts[i], "%-10.10s %s", audio_output_types[i].name, (xpbeep_sound_devices_enabled & audio_output_types[i].bit) ? "Yes" : "No");
			opt[i] = opts[i];
		}
		opt[i] = NULL;

		switch (i = uifc.list(WIN_SAV, 0, 0, 0, &j, NULL, "Audio Output Mode", opt)) {
			case -1:
				check_exit(false);
				continue;
			default:
				xpbeep_sound_devices_enabled ^= audio_output_types[j].bit;
				iniSetBitField(inicontents, "SyncTERM", "AudioModes", audio_output_bits, 
					   xpbeep_sound_devices_enabled, &ini_style);
				break;
		}
	}
}

static int
pick_colour(int cur, const char *name, bool bg)
{
	return uifc.list(WIN_SAV | WIN_RHT | WIN_BOT, 0, 0, 0, &cur, NULL, name, (char**)(bg ? bg_colour_names : colour_names));
}

/*
 * TODO: This function is gross and I feel bad for writing it.
 */
static void
edit_uifc_colours(str_list_t *inicontents)
{
	char opts[6][64] = {0};
	char *opt[7] = {
		opts[0],
		opts[1],
		opts[2],
		opts[3],
		opts[4],
		opts[5],
		NULL
	};
	const char *const key[6] = {
		"FrameColour",
		"TextColour",
		"BackgroundColour",
		"InverseColour",
		"LightbarColour",
		"LightbarBackgroundColour",
	};
	unsigned *set[6] = {
		&settings.uifc_hclr,
		&settings.uifc_lclr,
		&settings.uifc_bclr,
		&settings.uifc_cclr,
		&settings.uifc_lbclr,
		&settings.uifc_lbbclr
	};
	unsigned char *uifcp[5] = {
		&uifc.hclr,
		&uifc.lclr,
		&uifc.bclr,
		&uifc.cclr,
		&uifc.lbclr
	};
	unsigned char dflts[6] = {
		YELLOW,
		WHITE,
		BLUE,
		CYAN,
		BLUE,
		LIGHTGRAY,
	};
	int i = 0;
	int j = 0;
	int ret;

	uifc.helpbuf = "`UIFC Colours`\n\n"
		       "        Change UIFC Colours\n";

	while (i != -1) {
		bool bg;
		sprintf(opts[0], "Frame Colour               %s", colour_names[settings.uifc_hclr]);
		sprintf(opts[1], "Text Colour                %s", colour_names[settings.uifc_lclr]);
		sprintf(opts[2], "Background Colour          %s", bg_colour_names[settings.uifc_bclr]);
		sprintf(opts[3], "Inverse Colour             %s", bg_colour_names[settings.uifc_cclr]);
		sprintf(opts[4], "Lightbar Colour            %s", colour_names[settings.uifc_lbclr]);
		sprintf(opts[5], "Lightbar Background Colour %s", bg_colour_names[settings.uifc_lbbclr]);

		switch (i = uifc.list(WIN_SAV | WIN_ACT, 0, 0, 0, &j, NULL, "UIFC Colours", (char**)opt)) {
			case -1:
				check_exit(false);
				continue;
			default:
				bg = (i == 2 || i == 3 || i == 5);
				ret = pick_colour(*set[i], opt[i], bg);
				if (ret != -1) {
					uifc.changes = 1;
					unsigned v = ret;
					if (bg) {
						if (ret == 8)
							v = dflts[i];
					}
					else {
						if (ret == 16)
							v = dflts[i];
					}
					iniSetEnum(inicontents, "UIFC", key[i], (char **)(bg ? bg_colour_enum : colour_enum), ret, &ini_style);
					switch(i) {
						default:
							*set[i] = ret;
							*uifcp[i] = v;
							break;
						case 4: // Lightbar
							*set[i] = ret;
							*uifcp[i] = (*uifcp[i] & 0x70) | (v);
							break;
						case 5: // Lightbar background
							*set[i] = ret;
							*uifcp[i - 1] = (*uifcp[i - 1] & 0x0f) | (v << 4);
							break;
					}
				}
				break;
		}
	}
}

static void
change_settings(int connected)
{
	char inipath[MAX_PATH + 1];
	FILE *inifile;
	str_list_t inicontents;
	char opts[18][1049];
	char *opt[19];
	char *subopts[10];
	char audio_opts[1024];
	int i, j, k, l;
	char str[64];
	int cur = 0;
	int bar = 0;

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) != NULL) {
		inicontents = iniReadFile(inifile);
		fclose(inifile);
	}
	else
		inicontents = strListInit();
	const size_t opt_size = sizeof(opts) / sizeof(opts[0]);
	for (i = 0; i < opt_size; i++)
		opt[i] = opts[i];
	opt[i] = NULL;

	for (; !quitting;) {
		uifc.helpbuf = "`Program Settings Menu`\n\n"
		               "~ Confirm Program Exit ~\n"
		               "        Prompt the user before exiting.\n\n"
		               "~ Prompt to Save ~\n"
		               "        Prompt to save new URIs on before exiting\n\n"
		               "~ Startup Screen Mode ~\n"
		               "        Set the initial screen screen mode/size.\n\n"
		               "~ Video Output Mode ~\n"
		               "        Set video output mode (used during startup).\n\n"
		               "~ Default Cursor Style ~\n"
		               "        Set the default cursor style\n\n"
		               "~ Audio Output Mode ~\n"
		               "        Set audio output modes attempted.\n\n"
		               "~ Scrollback Buffer Lines ~\n"
		               "        The number of lines in the scrollback buffer.\n\n"
		               "~ Modem/Comm Device ~\n"
		               "        The device name of the modem's communications port.\n\n"
		               "~ Modem/Comm Rate ~\n"
		               "        The DTE rate of the modem's communications port.\n\n"
		               "~ Modem Init String ~\n"
		               "        The command string to use to initialize the modem.\n\n"
		               "~ Modem Dial String ~\n"
		               "        The command string to use to dial the modem.\n\n"
		               "~ List Path ~\n"
		               "        The complete path to the user's BBS list.\n\n"
		               "~ TERM For Shell ~\n"
		               "        The value to set the TERM envirnonment variable to goes here.\n\n"
		               "~ Scaling ~\n"
		               "        Cycle scaling type.\n\n"
		               "~ Key Derivation Iterations ~\n"
		               "        Change the number of iterations in the Key Derivation Function.\n\n"
		               "~ UIFC Colours ~\n"
		               "        Configure the colours used by the UIFC interface.\n\n"
		               "~ Custom Screen Mode ~\n"
		               "        Configure the Custom screen mode.\n\n";
		SAFEPRINTF(opts[0], "Confirm Program Exit    %s", settings.confirm_close ? "Yes" : "No");
		SAFEPRINTF(opts[1], "Prompt to Save          %s", settings.prompt_save ? "Yes" : "No");
		SAFEPRINTF(opts[2], "Startup Screen Mode     %s", screen_modes[settings.startup_mode]);
		SAFEPRINTF(opts[3], "Video Output Mode       %s", output_descrs[settings.output_mode]);
		SAFEPRINTF(opts[4], "Default Cursor Style    %s", cursor_descrs[settings.defaultCursor]);
		audio_opts[0] = 0;
		for (j = 0; audio_output_types[j].name != NULL; j++) {
			if (xpbeep_sound_devices_enabled & audio_output_types[j].bit) {
				if (audio_opts[0])
					strlcat(audio_opts, ", ", sizeof(audio_opts));
				strlcat(audio_opts, audio_output_types[j].name, sizeof(audio_opts));
			}
		}
		if (!audio_opts[0])
			strcpy(audio_opts, "<None>");
		SAFEPRINTF(opts[5], "Audio Output Mode       %s", audio_opts);
		SAFEPRINTF(opts[6], "Scrollback Buffer Lines %d", settings.backlines);
		SAFEPRINTF(opts[7], "Modem/Comm Device       %s", settings.mdm.device_name);
		if (settings.mdm.com_rate)
			sprintf(str, "%lubps", settings.mdm.com_rate);
		else
			strcpy(str, "Current");
		SAFEPRINTF(opts[8], "Modem/Comm Rate         %s", str);
		SAFEPRINTF(opts[9], "Modem Init String       %s", settings.mdm.init_string);
		SAFEPRINTF(opts[10], "Modem Dial String       %s", settings.mdm.dial_string);
		SAFEPRINTF(opts[11], "List Path               %s", settings.stored_list_path);
		SAFEPRINTF(opts[12], "TERM For Shell          %s", settings.TERM);
		sprintf(opts[13], "Scaling                 %s", scaling_names[settings_to_scale()]);
		sprintf(opts[14], "Invert Mouse Wheel      %s", settings.invert_wheel ? "Yes" : "No");
		sprintf(opts[15], "Key Derivation Iters.   %d", settings.keyDerivationIterations);
		sprintf(opts[16], "UIFC Colours");
		if (connected)
			opt[opt_size - 1] = NULL;
		else {
			sprintf(opts[opt_size - 1], "Custom Screen Mode");
			opt[opt_size - 1] = opts[opt_size - 1];
		}
		opt[opt_size] = NULL;
		switch (uifc.list(WIN_MID | WIN_SAV | WIN_ACT, 0, 0, 0, &cur, &bar, "Program Settings", opt)) {
			case -1:
				check_exit(false);
				goto write_ini;
			case 0:
				settings.confirm_close = !settings.confirm_close;
				iniSetBool(&inicontents, "SyncTERM", "ConfirmClose", settings.confirm_close,
				           &ini_style);
				break;
			case 1:
				settings.prompt_save = !settings.prompt_save;
				iniSetBool(&inicontents, "SyncTERM", "PromptSave", settings.prompt_save, &ini_style);
				break;
			case 2:
				j = settings.startup_mode;
				uifc.helpbuf = "`Startup Screen Mode`\n\n"
				               "Select the initial screen mode/size to use at startup.\n";
				i = sizeof(screen_modes) / sizeof(screen_modes[0]);
				switch (i = uifc.list(WIN_SAV, 0, 0, 0, &j, &i, "Startup Screen Mode", screen_modes)) {
					case -1:
						check_exit(false);
						continue;
					default:
						settings.startup_mode = j;
						iniSetEnum(&inicontents,
						           "SyncTERM",
						           "ScreenMode",
						           screen_modes_enum,
						           settings.startup_mode,
						           &ini_style);
						break;
				}
				break;
			case 3:
				for (j = 0; output_types[j] != NULL; j++)
					if (output_map[j] == settings.output_mode)
						break;


				if (output_types[j] == NULL)
					j = 0;
				uifc.helpbuf = "`Video Output Mode`\n\n"
				               "~ Autodetect ~\n"
				               "        Attempt to use the \"best\" display mode possible.  The order\n"
				               "        these are attempted is:"
#ifdef __unix__
#ifdef NO_X
				" SDL, Curses, then ANSI\n\n"
#else
				" X11, SDL, Curses, then ANSI\n\n"
#endif
#else
				" GDI, SDL, Windows Console, then ANSI\n\n"
#endif
#ifdef __unix__
				"~ Curses ~\n"
				"        Use text output using the Curses library.  This mode should work\n"
				"        from any terminal, however, high and low ASCII may not work\n"
				"        correctly.\n\n"
				"~ Curses on cp437 Device ~\n"
				"        As above, but assumes that the current terminal is configured to\n"
				"        display CodePage 437 correctly\n\n"
#endif
				"~ ANSI ~\n"
				"        Writes ANSI on CodePage 437 on stdout and reads input from\n"
				"        stdin. ANSI must be supported on the current terminal for this\n"
				"        mode to work.  This mode is intended to be used to run SyncTERM\n"
				"        as a BBS door\n\n"
#if defined(__unix__) && !defined(NO_X)
				"~ X11 ~\n"
				"        Uses the Xlib library directly for graphical output.  This is\n"
				"        the graphical mode most likely to work when using X11.\n\n"
				"~ X11 Fullscreen ~\n"
				"        As above, but starts in full-screen mode rather than a window\n\n"
#endif
#ifdef _WIN32
				"~ Win32 Console ~\n"
				"        Uses the windows console for display.  The font setting will\n"
				"        affect the look of the output and some low ASCII characters are\n"
				"        not displayable.  When in a window, blinking text is displayed\n"
				"        with a high-intensity background rather than blinking.  In\n"
				"        full-screen mode (where available), blinking works correctly.\n\n"
#endif
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
				"~ SDL ~\n"
				"        Makes use of the SDL graphics library for graphical output.\n"
				"~ SDL Fullscreen ~\n"
				"        As above, but starts in full-screen mode rather than a window\n\n"
#endif
#if defined(WITH_GDI)
				"~ GDI ~\n"
				"        Native Windows graphics library for graphical output.\n"
				"~ GDI Fullscreen ~\n"
				"        As above, but starts in full-screen mode rather than a window\n\n"
#endif
				               ;
				switch (i = uifc.list(WIN_SAV, 0, 0, 0, &j, NULL, "Video Output Mode", output_types)) {
					case -1:
						check_exit(false);
						continue;
					default:
						settings.output_mode = output_map[j];
						iniSetEnum(&inicontents,
						           "SyncTERM",
						           "OutputMode",
						           output_enum,
						           settings.output_mode,
						           &ini_style);
						break;
				}
				break;
			case 4:
				j = settings.defaultCursor;
				if (j < 0 || j > ST_CT_SOLID_BLK)
					j = 0;
				uifc.helpbuf = "`Default Cursor Style`\n\n"
				               "The style the cursor is normally displayed with\n";
				switch (i = uifc.list(WIN_SAV, 0, 0, 0, &j, NULL, "Default Cursor Style", cursor_descrs)) {
					case -1:
						check_exit(false);
						continue;
					default:
						settings.defaultCursor = j;
						iniSetEnum(&inicontents,
						           "SyncTERM",
						           "DefaultCursor",
						           cursor_enum,
						           settings.defaultCursor,
						           &ini_style);
						break;
				}
				break;
			case 5:
				edit_audio_mode(&inicontents);
				break;
			case 6:
				uifc.helpbuf = "`Scrollback Buffer Lines`\n\n"
				               "        The number of lines in the scrollback buffer.\n"
				               "        This value MUST be greater than zero\n";
				sprintf(str, "%d", settings.backlines);
				if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Scrollback Lines", str, 9,
				               K_NUMBER | K_EDIT) != -1) {
					struct vmem_cell *tmpscroll;

					j = atoi(str);
					if (j < 1) {
						uifc.helpbuf =
						        "There must be at least one line in the scrollback buffer.";
						uifc.msg("Cannot set lines to less than one.");
						check_exit(false);
					}
					else {
						tmpscroll = realloc(scrollback_buf, 80 * sizeof(*scrollback_buf) * j);
						scrollback_buf = tmpscroll ? tmpscroll : scrollback_buf;
						if (tmpscroll == NULL) {
							uifc.helpbuf = "The selected scrollback size is too large.\n"
							               "Please reduce the number of lines.";
							uifc.msg("Cannot allocate space for scrollback.");
							check_exit(false);
						}
						else {
							if (scrollback_lines > (unsigned)j)
								scrollback_lines = j;
							settings.backlines = j;
							iniSetInteger(&inicontents,
							              "SyncTERM",
							              "ScrollBackLines",
							              settings.backlines,
							              &ini_style);
						}
					}
				}
				else
					check_exit(false);
				break;
			case 7:
				uifc.helpbuf = "`Modem/Comm Device`\n\n"
				               "Enter the name of the device used to communicate with the modem.\n\n"
				               "Example: \"`"
				               DEFAULT_MODEM_DEV
				"`\"";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Modem/Comm Device", settings.mdm.device_name,
				               INI_MAX_VALUE_LEN, K_EDIT) >= 0) {
					iniSetString(&inicontents,
					             "SyncTERM",
					             "ModemDevice",
					             settings.mdm.device_name,
					             &ini_style);
				}
				else
					check_exit(false);
				break;
			case 8:
				uifc.helpbuf = "`Modem/Comm Rate`\n\n"
				               "Enter the rate (in `bits-per-second`) used to communicate with the modem.\n"
				               "Use the highest `DTE Rate` supported by your communication port and modem.\n\n"
				               "Examples: `38400`, `57600`, `115200`\n\n"
				               "This rate is sometimes (incorrectly) referred to as the `baud rate`.\n\n"
				               "Enter `0` to use the current or default rate of the communication port";
				sprintf(str, "%lu", settings.mdm.com_rate);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Modem/Comm Rate", str, 12, K_EDIT) >= 0) {
					settings.mdm.com_rate = strtol(str, NULL, 10);
					iniSetLongInt(&inicontents,
					              "SyncTERM",
					              "ModemComRate",
					              settings.mdm.com_rate,
					              &ini_style);
				}
				else
					check_exit(false);
				break;

			case 9:
				uifc.helpbuf = "`Modem Init String`\n\n"
				               "Your modem initialization string goes here.\n\n"
				               "Example:\n"
				               "\"`AT&F`\" will load a Hayes compatible modem's factory default settings.\n\n"
				               "~For reference, here are the expected Hayes-compatible settings:~\n\n"
				               "State                      Command\n"
				               "----------------------------------\n"
				               "Echo on                    E1\n"
				               "Verbal result codes        Q0V1\n"
				               "Normal CD Handling         &C1\n"
				               "Normal DTR                 &D2\n"
				               "\n\n"
				               "~For reference, here are the expected USRobotics-compatible settings:~\n\n"
				               "State                      Command\n"
				               "----------------------------------\n"
				               "Include connection speed   &X4\n"
				               "Locked speed               &B1\n"
				               "CTS/RTS Flow Control       &H1&R2\n"
				               "Disable Software Flow      &I0\n";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Modem Init String", settings.mdm.init_string,
				               INI_MAX_VALUE_LEN - 1, K_EDIT) >= 0) {
					iniSetString(&inicontents,
					             "SyncTERM",
					             "ModemInit",
					             settings.mdm.init_string,
					             &ini_style);
				}
				else
					check_exit(false);
				break;
			case 10:
				uifc.helpbuf = "`Modem Dial String`\n\n"
				               "The command string to dial the modem goes here.\n\n"
				               "Example: \"`ATDT`\" will dial a Hayes-compatible modem in touch-tone mode.";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Modem Dial String", settings.mdm.dial_string,
				               INI_MAX_VALUE_LEN - 1, K_EDIT) >= 0) {
					iniSetString(&inicontents,
					             "SyncTERM",
					             "ModemDial",
					             settings.mdm.dial_string,
					             &ini_style);
				}
				else
					check_exit(false);
				break;
			case 11:
				uifc.helpbuf = "`List Path`\n\n"
				               "The complete path to the BBS list goes here.\n";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "List Path", settings.stored_list_path, MAX_PATH,
				               K_EDIT) >= 0) {
					iniSetString(&inicontents,
					             "SyncTERM",
					             "ListPath",
					             settings.stored_list_path,
					             &ini_style);
					if (list_override == NULL)
						SAFECOPY(settings.list_path, settings.stored_list_path);
				}
				else
					check_exit(false);
				break;
			case 12:
				uifc.helpbuf = "`TERM For Shell`\n\n"
				               "The value to set the TERM envirnonment variable to goes here.\n\n"
				               "Example: \"`ansi`\" will select a dumb ANSI mode.";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "TERM", settings.TERM, LIST_NAME_MAX,
				               K_EDIT) >= 0)
					iniSetString(&inicontents, "SyncTERM", "TERM", settings.TERM, &ini_style);
				else
					check_exit(false);
				break;
			case 13:
				i = settings_to_scale();
				i++;
				if (i == 3)
					i = 0;
				scale_to_settings(i);

				iniSetBool(&inicontents, "SyncTERM", "BlockyScaling", settings.blocky, &ini_style);
				iniSetBool(&inicontents, "SyncTERM", "ExternalScaling", settings.extern_scale, &ini_style);
				if (settings.blocky)
					cio_api.options |= CONIO_OPT_BLOCKY_SCALING;
				else
					cio_api.options &= ~CONIO_OPT_BLOCKY_SCALING;
				setscaling_type(settings.extern_scale ? CIOLIB_SCALING_EXTERNAL : CIOLIB_SCALING_INTERNAL);
				break;
			case 14:
				settings.invert_wheel = !settings.invert_wheel;
				ciolib_swap_mouse_butt45 = settings.invert_wheel;
				iniSetBool(&inicontents, "SyncTERM", "InvertMouseWheel", settings.invert_wheel, &ini_style);
				break;
			case 15:
				{
					uifc.helpbuf = "`Key Derivation Function Iterations`\n\n"
						       "Number of iterations to run the Key Derivation Function when creating an\n"
						       "encryption key from a password. Using more iterations makes offline\n"
						       "attacks harder by making dictionary attacks more difficult, ideally\n"
						       "making it more difficult than simple random key brute forcing.\n\n"
						       "The default value is 50,000 which takes about 0.15 seconds on my current\n"
						       "computer. This means that a normal read/modify/write of an entry ends up\n"
						       "taking 0.3 seconds longer due to KDF. Lowering this value makes offline\n"
						       "attacks easier, but can noticably speed up encrypted file access.\n\n"
						       "Minimum value is 1, maximum value is 2147483647. You should choose the\n"
						       "highest value you can put up with. NIST reccomends a minimum of 10,000.\n";
					char value[11];
					snprintf(value, sizeof(value), "%d", settings.keyDerivationIterations);
					if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Iterations", value, sizeof(value) - 1, K_NUMBER | K_EDIT) > 0) {
						long nval = strtol(value, NULL, 10);
						if (nval > 0) {
							FILE *listfile;

							if ((listfile = fopen(settings.list_path, "r+b")) != NULL) {
								str_list_t inifile = iniReadBBSList(listfile, true);
								settings.keyDerivationIterations = nval;
								iniSetInteger(&inicontents, "SyncTERM", "KeyDerivationIterations", settings.keyDerivationIterations, &ini_style);
								if (list_algo != INI_CRYPT_ALGO_NONE)
									iniWriteEncryptedFile(listfile, inifile, list_algo, list_keysize, settings.keyDerivationIterations, list_password, NULL);
								fclose(listfile);
								iniFreeStringList(inifile);
							}
							else {
								uifc.msg("Failed to open list file");
								settings.keyDerivationIterations = nval;
								iniSetInteger(&inicontents, "SyncTERM", "KeyDerivationIterations", settings.keyDerivationIterations, &ini_style);
							}
						}
					}
				}
				break;
			case 16:
				{
					edit_uifc_colours(&inicontents);
				}
				break;
			case 17:
				uifc.helpbuf = "`Custom Screen Mode`\n\n"
				               "~ Rows ~\n"
				               "        Sets the number of rows in the custom screen mode\n"
				               "~ Columns ~\n"
				               "        Sets the number of columns in the custom screen mode\n"
				               "~ Font Size ~\n"
				               "        Chooses the font size used by the custom screen mode\n";
				j = 0;
				for (k = 0; k == 0;) {
					// Beware case 2 below if adding things
					asprintf(&subopts[0], "Rows                (%d)", settings.custom_rows);
					asprintf(&subopts[1], "Columns             (%d)", settings.custom_cols);
					asprintf(&subopts[2],
					         "Font Size           (%s)",
					settings.custom_fontheight == 8 ? "8x8" : settings.custom_fontheight
					== 14 ? "8x14" : "8x16");
					asprintf(&subopts[3], "Aspect Ratio Width  (%d)", settings.custom_aw);
					asprintf(&subopts[4], "Aspect Ratio Height (%d)", settings.custom_ah);
					subopts[5] = NULL;
					switch (uifc.list(WIN_SAV, 0, 0, 0, &j, NULL, "Video Output Mode", subopts)) {
						case -1:
							check_exit(false);
							k = 1;
							break;
						case 0:
							uifc.helpbuf = "`Rows`\n\n"
							               "Number of rows on the custom screen.  Must be between 14 and 255";
							sprintf(str, "%d", settings.custom_rows);
							if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Custom Rows", str, 3,
							               K_NUMBER | K_EDIT) != -1) {
								l = atoi(str);
								if ((l < 14) || (l > 255)) {
									uifc.msg("Rows must be between 14 and 255.");
									check_exit(false);
								}
								else {
									settings.custom_rows = l;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomRows",
									              settings.custom_rows,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
						case 1:
							uifc.helpbuf = "`Columns`\n\n"
							               "Number of columns on the custom screen.  Must be between 40 and 255\n"
							               "Note that values other than 40, 80, and 132 are not supported.";
							sprintf(str, "%d", settings.custom_cols);
							if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Custom Columns", str,
							               3, K_NUMBER | K_EDIT) != -1) {
								l = atoi(str);
								if ((l < 40) || (l > 255)) {
									uifc.msg("Columns must be between 40 and 255.");
									check_exit(false);
								}
								else {
									settings.custom_cols = l;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomColumns",
									              settings.custom_cols,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
						case 2:
							uifc.helpbuf = "`Font Size`\n\n"
							               "Choose the font size for the custom mode.";
							subopts[6] = "8x8";
							subopts[7] = "8x14";
							subopts[8] = "8x16";
							subopts[9] = NULL;
							switch (settings.custom_fontheight) {
								case 8:
									l = 0;
									break;
								case 14:
									l = 1;
									break;
								default:
									l = 2;
									break;
							}
							switch (uifc.list(WIN_SAV, 0, 0, 0, &l, NULL, "Font Size",
							                  &subopts[6])) {
								case -1:
									check_exit(false);
									break;
								case 0:
									settings.custom_fontheight = 8;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomFontHeight",
									              settings.custom_fontheight,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
								case 1:
									settings.custom_fontheight = 14;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomFontHeight",
									              settings.custom_fontheight,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
								case 2:
									settings.custom_fontheight = 16;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomFontHeight",
									              settings.custom_fontheight,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
							}
							break;
						case 3:
							uifc.helpbuf = "`Aspect Ratio Width`\n\n"
							               "Width part of the aspect ratio.  Historically, this has been 4";
							sprintf(str, "%d", settings.custom_aw);
							if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Aspect Ratio Width",
							               str, 9, K_NUMBER | K_EDIT) != -1) {
								l = atoi(str);
								if (l <= 0) {
									uifc.msg(
									        "Aspec Ratio Width must be greater than zero");
									check_exit(false);
								}
								else {
									settings.custom_aw = l;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomAspectWidth",
									              settings.custom_aw,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
						case 4:
							uifc.helpbuf = "`Aspect Ratio Height`\n\n"
							               "Height part of the aspect ratio.  Historically, this has been 3";
							sprintf(str, "%d", settings.custom_ah);
							if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Aspect Ratio Height",
							               str, 9, K_NUMBER | K_EDIT) != -1) {
								l = atoi(str);
								if (l <= 0) {
									uifc.msg(
									        "Aspec Ratio Height must be greater than zero");
									check_exit(false);
								}
								else {
									settings.custom_ah = l;
									iniSetInteger(&inicontents,
									              "SyncTERM",
									              "CustomAspectHeight",
									              settings.custom_ah,
									              &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
					}
					free(subopts[0]);
					free(subopts[1]);
					free(subopts[2]);
					free(subopts[3]);
					free(subopts[4]);
				}
		}
	}
write_ini:
	if (!safe_mode) {
		if ((inifile = fopen(inipath, "w")) != NULL) {
			iniWriteFile(inifile, inicontents);
			fclose(inifile);
		}
	}
	strListFree(&inicontents);
}

static void
load_bbslist(struct bbslist **list,
             size_t listsize,
             struct bbslist *defaults,
             char *listpath,
             size_t listpathsize,
             char *shared_list,
             size_t shared_listsize,
             int *listcount,
             int *cur,
             int *bar,
             char *current)
{
	free_list(&list[0], *listcount);
	*listcount = 0;

	memset(list, 0, listsize);
	if (defaults)
		memset(defaults, 0, sizeof(struct bbslist));

	read_list(listpath, list, defaults, listcount, USER_BBSLIST);

	/* System BBS List */
	if (stricmp(shared_list, listpath)) /* don't read the same list twice */
		read_list(shared_list, list, NULL, listcount, SYSTEM_BBSLIST);
	/* Web lists */
	if (settings.webgets) {
		char cache_path[MAX_PATH + 1];
		if (get_syncterm_filename(cache_path, sizeof(cache_path), SYNCTERM_PATH_SYSTEM_CACHE, false)) {
			backslash(cache_path);
			for (size_t i = 0; settings.webgets[i]; i++) {
				char *lpath;
				int len = asprintf(&lpath, "%s%s.lst", cache_path, settings.webgets[i]->name);
				if (len < 1) {
					free(lpath);
				}
				else {
					read_list(lpath, list, NULL, listcount, SYSTEM_BBSLIST);
					free(lpath);
				}
			}
		}
	}
	sort_list(list, listcount, cur, bar, current);
	if (current)
		free(current);
}

/*
 * Note that any time it's drawn, it's inactive...
 */
static void
draw_comment(struct bbslist *list)
{
	int lpad;
	int rpad;
	int clen;
	int remain;
	char *comment;

	if (list == NULL)
		comment = "";
	else
		comment = list->comment;
	gotoxy(1, uifc.scrn_len);
	textattr(uifc.lclr | (uifc.cclr << 4));

	// Calculator how to centre.
	clen = strlen(comment);
	if (clen > uifc.scrn_width - 4) {
		lpad = 0;
		rpad = 0;
	}
	else {
		remain = uifc.scrn_width - 4 - clen;
		rpad = remain / 2 + (remain % 2);
		lpad = remain - rpad;
	}
	cprintf("  %*s%-.*s%*s  ", lpad, "", uifc.scrn_width - 4, comment, rpad, "");
}

/*
 * Return value indicates if focus should return to list (true) or move
 * to settings (false)
 *
 * TODO: ESC in edit box doesn't exit program... good or bad?
 */
static bool
edit_comment(struct bbslist *list, char *listpath)
{
	FILE *listfile;
	str_list_t inifile = NULL;
	int ch;
	bool ret = false;
	char *old = NULL;
	int i;

	if (list == NULL)
		goto done;
	if (safe_mode)
		goto done;

	// Open with write permissions so it fails if you can't edit.
	if ((listfile = fopen(listpath, "r+b")) != NULL) {
		inifile = iniReadBBSList(listfile, true);
		fclose(listfile);
	}
	else
		goto done;

	old = strdup(list->comment);
	if (!old)
		goto done;
	textattr(uifc.lclr | (uifc.bclr << 4));
	gotoxy(1, uifc.scrn_len);
	clreol();
	uifc.getstrxy(3,
	              uifc.scrn_len,
	              uifc.scrn_width - 4,
	              list->comment,
	              sizeof(list->comment),
	              K_LINE | K_EDIT | K_NOCRLF | K_TABEXIT | K_MOUSEEXIT | K_TABEXIT,
	              &ch);
	switch (ch) {
		case '\x1b':
			SAFECOPY(list->comment, old);
			ret = true;
			goto done;
		case '\t':
			ret = false;
			break;
		default:
			ret = true;
			break;
	}

	if (strcmp(old, list->comment)) {
		iniSetString(&inifile, list->name, "Comment", list->comment, &ini_style);
		if (list->type == SYSTEM_BBSLIST) {
			uifc.helpbuf = "`Copy from system directory`\n\n"
			               "This entry was loaded from the system directory.  In order to edit it, it\n"
			               "must be copied into your personal directory.\n";
			i = 0;
			if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Copy from system directory?", YesNo) != 0)
				goto done;
			list->type = USER_BBSLIST;
			add_bbs(listpath, list, true);
		}
	}

done:
	free(old);
	if (inifile != NULL) {
		if ((listfile = fopen(listpath, "wb")) != NULL) {
			iniWriteEncryptedFile(listfile, inifile, list_algo, list_keysize, settings.keyDerivationIterations, list_password, NULL);
			fclose(listfile);
		}
		strListFree(&inifile);
	}
	draw_comment(list);
	return ret;
}

static void
write_webgets(void)
{
	FILE      *inifile;
	char       inipath[MAX_PATH + 1];
	str_list_t ini_file;

	if (safe_mode)
		return;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, false);
	if ((inifile = fopen(inipath, "r")) != NULL) {
		ini_file = iniReadFile(inifile);
		fclose(inifile);
	}
	else {
		ini_file = strListInit();
	}

	iniRemoveSection(&ini_file, "WebLists");
	iniAppendSectionWithNamedStrings(&ini_file, "WebLists", (const named_string_t**)settings.webgets, &ini_style);

	if ((inifile = fopen(inipath, "w")) != NULL) {
		iniWriteFile(inifile, ini_file);
		fclose(inifile);
	}
	else {
		uifc.helpbuf = "There was an error writing the INI file.\nCheck permissions and try again.\n";
		uifc.msg("Cannot write to the .ini file!");
		check_exit(false);
	}

	strListFree(&ini_file);
}

static bool
edit_web_lists(void)
{
	static int cur = 0;
	static int bar = 0;
	size_t alloced_size = 0;
	char **list = NULL;
	bool changed = false;

	while (!quitting) {
		size_t count;
		COUNT_LIST_ITEMS(settings.webgets, count);
		if ((count + 1) > alloced_size) {
			char **newlist = realloc(list, (count + 1) * sizeof(char *));
			if (newlist == NULL) {
				free(list);
				break;
			}
			list = newlist;
			alloced_size = count + 1;
		}
		for (size_t i = 0; i < count; i++) {
			list[i] = settings.webgets[i]->name;
		}
		list[count] = NULL;

		uifc.helpbuf = "`Web Lists`\n\n"
		    "Add and remove dialing directories available on the web (ie: via HTTP\n"
		    "or HTTPS).  Each entry must have a unique name (which is used as a\n"
		    "filename in the cache) and a URI that indicates where to download the\n"
		    "directory list from.\n"
		    "\n"
		    "The SyncTERM author hosts two BBS lists, the Synchronet BBS list at:\n"
		    "http://syncterm.bbsdev.net/syncterm.lst\n"
		    "and the Telnet BBS Guide BBS list at:\n"
		    "http://syncterm.bbsdev.net/telnetbbsguide.lst\n"
		    "for easy use and configuration.";
		int i = uifc.list(WIN_SAV | WIN_INS | WIN_INSACT | WIN_DEL | WIN_XTR | WIN_ACT,
		        0, 0, 0, &cur, &bar, "Web Lists", list);
		if (i == -1) {
			check_exit(false);
			break;
		}
		if (i & MSK_DEL) {
			namedStrListDelete(&settings.webgets, i & MSK_OFF);
			changed = true;
		}
		else if (i & MSK_INS) {
			char tmpn[INI_MAX_VALUE_LEN + 1];
			char tmpv[INI_MAX_VALUE_LEN + 1];
			if (count == 0)
				strlcpy(tmpn, "SyncTERM BBS List", sizeof(tmpn));
			else
				tmpn[0] = 0;
			while (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Web List Name", tmpn, sizeof(tmpn) - 1, K_EDIT) != -1
			    && tmpn[0]) {
				if (stricmp(tmpn, "System List") == 0) {
					uifc.msg("Invalid Name");
					continue;
				}
				if (settings.webgets != NULL && namedStrListFindName(settings.webgets, tmpn)) {
					uifc.msg("Duplicate Name");
					continue;
				}
				else {
					if (count == 0)
						strlcpy(tmpv, "http://syncterm.bbsdev.net/syncterm.lst", sizeof(tmpn));
					else
						tmpv[0] = 0;
					if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Web List URI", tmpv, sizeof(tmpv) - 1, K_EDIT) != -1
					    && tmpv[0]) {
						char cache_path[MAX_PATH + 1];
						if (get_syncterm_filename(cache_path, sizeof(cache_path), SYNCTERM_PATH_SYSTEM_CACHE, false)) {
							struct webget_request req;
							if (!init_webget_req(&req, cache_path, tmpn, tmpv) || !iniReadHttp(&req)) {
								char temp[1024];
								snprintf(temp, sizeof(temp), "Can't fetch URI: %s", req.msg);
								uifc.msg(temp);
							}
							else {
								namedStrListInsert(&settings.webgets, tmpn, tmpv, i & MSK_OFF);
								changed = true;
							}
						}
						else {
							namedStrListInsert(&settings.webgets, tmpn, tmpv, i & MSK_OFF);
							changed = true;
						}
					}
					break;
				}
			}
		}
		else if (i >= 0 && i <= count) {
			char tmp[INI_MAX_VALUE_LEN + 1];
			strlcpy(tmp, settings.webgets[i]->value, sizeof(tmp));
			uifc.changes = false;
			if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "Web List URI", tmp, sizeof(tmp) - 1, K_EDIT) == -1) {
				check_exit(false);
			}
			else {
				if (uifc.changes) {
					free(settings.webgets[i]->value);
					settings.webgets[i]->value = strdup(tmp);
					changed = true;
				}
			}
		}
	}
	if (changed) {
		write_webgets();
	}
	return changed;
}

#if (defined(WITH_CRYPTLIB) && !defined(WITHOUT_CRYPTLIB))
static void
changeAlgo(const char *listpath, enum iniCryptAlgo algo, int keySize, const char *newpass)
{
	FILE *listfile;

	if (safe_mode)
		return;
	if (newpass == NULL && algo != INI_CRYPT_ALGO_NONE && !list_password[0]) {
		if (!prompt_password(NULL, NULL, NULL))
			return;
	}
	if ((listfile = fopen(listpath, "r+b")) != NULL) {
		str_list_t inifile = iniReadBBSList(listfile, true);
		if (algo == INI_CRYPT_ALGO_NONE)
			iniRemoveKey(&inifile, NULL, "DecryptionCheck");
		else
			iniSetBool(&inifile, NULL, "DecryptionCheck", true, &ini_style);
		if (newpass)
			strlcpy(list_password, newpass, sizeof(list_password));
		iniWriteEncryptedFile(listfile, inifile, algo, keySize, settings.keyDerivationIterations, list_password, NULL);
		fclose(listfile);
		list_algo = algo;
		list_keysize = keySize;
		iniFreeStringList(inifile);
	}
}

static void
encryption_menu(const char *listpath)
{
	char *encryption[] = {
		"Change Password",
		"Encrypt Using ChaCha20",              // 2008
		"Encrypt Using AES-128",               // 1998
		"Encrypt Using AES-256",               // 1998
		"Encrypt Using CAST-128",              // 1996
		"Encrypt Using IDEA",                  // 1991
		"Encrypt Using RC2",                   // 1987
		"Encrypt Using RC4 (Insecure)",        // 1987
		"Encrypt Using 3DES (Insecure)",       // 1981
		"Decrypt",
		NULL
	};
	int dflt = 0;
	int bar = 0;
	char title[80];
	char newpass[sizeof(list_password)];

	if (list_algo == INI_CRYPT_ALGO_NONE)
		strlcpy(title, "Not Encrypted", sizeof(title));
	else {
		if (list_keysize)
			snprintf(title, sizeof(title), "Currently %s (%d)", iniCryptGetAlgoName(list_algo), list_keysize);
		else
			snprintf(title, sizeof(title), "Currently %s", iniCryptGetAlgoName(list_algo));
	}

	uifc.helpbuf = "`Encryption`\n\n"
	               "Choose the encryption type you would like to convert the list to.";
	int val = uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, &dflt, &bar, title, encryption);
	switch(val) {
		case 0:
			if (uifc.input(WIN_SAV | WIN_MID, 0, 0, "New Password", newpass, sizeof(newpass), K_PASSWORD) > 0)
				changeAlgo(listpath, list_algo, list_keysize, newpass);
			break;
		case 1:
			changeAlgo(listpath, INI_CRYPT_ALGO_CHACHA20, 0, NULL);
			break;
		case 2:
			changeAlgo(listpath, INI_CRYPT_ALGO_AES, 128, NULL);
			break;
		case 3:
			changeAlgo(listpath, INI_CRYPT_ALGO_AES, 256, NULL);
			break;
		case 4:
			changeAlgo(listpath, INI_CRYPT_ALGO_CAST, 0, NULL);
			break;
		case 5:
			changeAlgo(listpath, INI_CRYPT_ALGO_IDEA, 0, NULL);
			break;
		case 6:
			changeAlgo(listpath, INI_CRYPT_ALGO_RC2, 0, NULL);
			break;
		case 7:
			changeAlgo(listpath, INI_CRYPT_ALGO_RC4, 0, NULL);
			break;
		case 8:
			changeAlgo(listpath, INI_CRYPT_ALGO_3DES, 0, NULL);
			break;
		case 9:
			changeAlgo(listpath, INI_CRYPT_ALGO_NONE, 0, NULL);
			break;
	}
}
#endif

/*
 * Displays the BBS list and allows edits to user BBS list
 * Mode is one of BBSLIST_SELECT or BBSLIST_EDIT
 */
struct bbslist *
show_bbslist(char *current, int connected)
{
#define BBSLIST_SIZE ((MAX_OPTS + 1) * sizeof(struct bbslist *))
	struct bbslist **list;
	int i, j;
	static int opt = 0, bar = 0;
	int oldopt = -1;
	int sopt = 0, sbar = 0;
	static struct bbslist retlist;
	int val;
	int listcount = 0;
	char str[128];
	char title[1024];
	char *p;
	char addy[LIST_ADDR_MAX + 1];
	char *settings_menu[] = {
		"Web Lists",
		"Default Connection Settings",
		"Current Screen Mode",
		"Font Management",
		"Program Settings",
		"File Locations",
		"Build Options",
#if (defined(WITH_CRYPTLIB) && !defined(WITHOUT_CRYPTLIB))
		"List Encryption",
#endif
		NULL
	};
	char *connected_settings_menu[] = {
		"Web Lists",
		"Default Connection Settings",
		"Font Management",
		"Program Settings",
		"File Locations",
		"Build Options",
		NULL
	};
	int at_settings = 0;
	struct mouse_event mevent;
	struct bbslist defaults;
	char shared_list[MAX_PATH + 1];
	char personal_list[MAX_PATH + 1];
	char setting_file[MAX_PATH + 1];
	char default_download[MAX_PATH + 1];
	char cache_path[MAX_PATH + 1];
	char keys_path[MAX_PATH + 1];
	char list_title[30];
	int redraw = 0;
	bool nowait = true;
	int last_mode;
	struct bbslist *copied = NULL;

	glob_sbar = &sbar;
	glob_sopt = &sopt;
	glob_settings_menu = settings_menu;
	glob_listcount = &listcount;
	glob_opt = &opt;
	glob_bar = &bar;
	glob_list_title = list_title;
	glob_list = &list;

	if (init_uifc(connected ? false : true, true))
		return NULL;

	get_syncterm_filename(shared_list, sizeof(shared_list), SYNCTERM_PATH_LIST, true);
	list = malloc(BBSLIST_SIZE);
	if (list == NULL)
		return NULL;
	load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list,
	             sizeof(shared_list), &listcount, &opt, &bar, current ? strdup(current) : NULL);

	uifc.helpbuf = "Help Button Hack";
	settings_list(&sopt, &sbar, connected ? connected_settings_menu : settings_menu, WIN_INACT);
	last_mode = cio_api.mode;
	for (;;) {
		if (quitting) {
			free(list);
			free(copied);
			return NULL;
		}
		if (!at_settings) {
			for (; !at_settings;) {
				sprintf(list_title, "Directory (%d items)", listcount);
				if (quitting) {
					free(list);
					free(copied);
					return NULL;
				}
				if (last_mode != cio_api.mode) {
					set_uifc_title();
					last_mode = cio_api.mode;
				}
				if (connected) {
					uifc.helpbuf = "`SyncTERM Directory`\n\n"
					               "Commands:\n\n"
					               "~ CTRL-E ~ to edit the selected entry\n"
					               "~ CTRL-S ~ to modify the sort order\n"
					               "~ TAB ~ to modify the selected entry comment or SyncTERM Settings\n"
					               "~ ENTER ~ to edit to the selected entry"
					               "`Conio Keys` (may not work in some modes)\n\n"
					               "~ " ALT_KEY_NAMEP
					               "-Left ~ Snap window size to next smaller horizontal size\n"
					               "~ " ALT_KEY_NAMEP
					               "-Right ~ Snap window size to next larger horizontal size\n"
					               "~ " ALT_KEY_NAMEP "-Enter ~ Toggle fullscreen mode when available\n\n"
					               "`UIFC List Keys`\n\n"
					               "~ CTRL-F ~ find text in current menu options\n"
					               "~ CTRL-G ~ repeat last search\n";
				}
				else {
					uifc.helpbuf = "`SyncTERM Directory`\n\n"
					               "Commands:\n\n"
					               "~ CTRL-D ~ Quick-connect to a URL\n"
					               "~ CTRL-E ~ to edit the selected entry\n"
					               "~ CTRL-S ~ to modify the sort order\n"
					               "~ " ALT_KEY_NAMEP "-B ~ View scrollback of last session\n"
					               "~ TAB ~ to modify the selected entry comment or SyncTERM Settings\n"
					               "~ ENTER ~ to connect to the selected entry\n\n"
					               "`Conio Keys` (may not work in some modes)\n\n"
					               "~ " ALT_KEY_NAMEP
					               "-Left ~ Snap window size to next smaller horizontal size\n"
					               "~ " ALT_KEY_NAMEP
					               "-Right ~ Snap window size to next larger horizontal size\n"
					               "~ " ALT_KEY_NAMEP "-Enter ~ Toggle fullscreen mode when available\n\n"
					               "`UIFC List Keys`\n\n"
					               "~ CTRL-F ~ find text in current menu options\n"
					               "~ CTRL-G ~ repeat last search\n";
				}
				if (opt != oldopt) {
					if ((list[opt] != NULL) && list[opt]->name[0]) {
						sprintf(title,
						        "%s - %s (%d calls / Last: %s",
						        syncterm_version,
						        (char *)(list[opt]),
						        list[opt]->calls,
						        list[opt]->connected ? ctime(&list[opt]->connected) : "Never\n");
						p = strrchr(title, '\n');
						if (p != NULL)
							*p = ')';
					}
					else
						SAFECOPY(title, syncterm_version);
					settitle(title);
				}
				oldopt = opt;
				uifc.list_height = listcount + 5;
				if (uifc.list_height > (uifc.scrn_len - 4))
					uifc.list_height = uifc.scrn_len - 4;
				if (!nowait) {
					bl_kbwait();
					nowait = true;
				}
				val = uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
				                | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_UNGETMOUSE | WIN_SAV | WIN_ESC
				                | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN | WIN_FIXEDHEIGHT
				                | (redraw ? WIN_NODRAW : 0) | WIN_COPY
				| (copied == NULL ? 0 : (WIN_PASTE | WIN_PASTEXTR))
				                ,
				                0,
				                (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
				                0,
				                &opt,
				                &bar,
				                list_title,
				                (char **)list);
				redraw = 0;
				if (val == listcount)
					val = listcount | MSK_INS;
				if (val == -7) { /* CTRL-E */
					uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
					          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
					          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
					          | WIN_SEL | WIN_FIXEDHEIGHT
					          ,
					          0,
					          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
					          0,
					          &opt,
					          &bar,
					          list_title,
					          (char **)list);
					if (opt < (listcount))
						val = opt | MSK_EDIT;
					else
						val = opt | MSK_INS;
				}
				draw_comment(list[opt]);
				if (val < 0) {
					switch (val) {
						case -2 - 0x13: /* CTRL-S - Sort */
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_FIXEDHEIGHT
							          ,
							          0,
							          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
							          0,
							          &opt,
							          &bar,
							          list_title,
							          (char **)list);
							edit_sorting(list,
							             &listcount,
							             &opt,
							             &bar,
							             list[opt] ? list[opt]->name : NULL);
							break;
						case -2 - 0x3000: /* ALT-B - Scrollback */
							if (!connected) {
								viewofflinescroll();
								settings_list(&sopt, &sbar, settings_menu, WIN_INACT);
							}
							break;
						case -11: /* TAB */
							if (val == -11) {
								uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
								          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
								          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
								          | WIN_SEL | WIN_FIXEDHEIGHT
								          ,
								          0,
								          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
								          0,
								          &opt,
								          &bar,
								          list_title,
								          (char **)list);
								if (edit_comment(list[opt], settings.list_path)) {
									redraw = 1;
									break;
								}
								at_settings = !at_settings;
								break;
							}

						/* Fall-through */
						case -2 - CIO_KEY_MOUSE: /* Clicked outside of window... */
							if (mouse_pending()) {
								do {
									getmouse(&mevent);
								} while (mouse_pending());
								if (mevent.event == CIOLIB_BUTTON_1_CLICK
								&& mevent.starty == cio_textinfo.screenheight - 1) {
									if (edit_comment(list[opt], settings.list_path))
										redraw = 1;
									break;
								}
							}

						/* Fall-through */
						case -2 - 0x0f00: /* Backtab */
						case -2 - 0x4b00: /* Left Arrow */
						case -2 - 0x4d00: /* Right Arrow */
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_FIXEDHEIGHT
							          ,
							          0,
							          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
							          0,
							          &opt,
							          &bar,
							          list_title,
							          (char **)list);
							at_settings = !at_settings;
							break;
						case -6: /* CTRL-D */
							if (!connected) {
								if (safe_mode) {
									uifc.helpbuf =
									        "`Cannot Quick-Connect in safe mode`\n\n"
									        "SyncTERM is currently running in safe mode.  This means you cannot use the\n"
									        "Quick-Connect feature.";
									uifc.msg("Cannot edit list in safe mode");
									break;
								}
								uifc.changes = 0;
								uifc.helpbuf = "`SyncTERM Quick-Connect`\n\n"
								               "Enter a URL in the format:\n"
								               "[(rlogin|telnet|ssh)://][user[:password]@]domainname[:port]\n";
								uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
								          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
								          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
								          | WIN_SEL | WIN_FIXEDHEIGHT
								          ,
								          0,
								          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
								          0,
								          &opt,
								          &bar,
								          list_title,
								          (char **)list);
								uifc.input(WIN_MID | WIN_SAV,
								           0,
								           0,
								           "Address",
								           addy,
								           LIST_ADDR_MAX,
								           0);
								memcpy(&retlist, &defaults, sizeof(defaults));
								if (uifc.changes) {
									parse_url(addy,
									          &retlist,
									          defaults.conn_type,
									          false);
									free_list(&list[0], listcount);
									free(list);
									free(copied);
									return &retlist;
								}
							}
							break;
						case -1: /* ESC */
							if (!connected)
								if (!check_exit(true))
									continue;


							free_list(&list[0], listcount);
							free(list);
							free(copied);
							return NULL;
						case -2:
							nowait = false;
							break;
					}
				}
				else if (val & MSK_ON) {
					char tmp[LIST_NAME_MAX + 1];

					switch (val & MSK_ON) {
						case MSK_INS:
							if (listcount >= MAX_OPTS) {
								uifc.helpbuf = "`Max List size reached`\n\n"
								               "The total combined size of loaded entries is currently the highest\n"
								               "supported size.  You must delete entries before adding more.";
								uifc.msg("Max List size reached!");
								check_exit(false);
								break;
							}
							if (safe_mode) {
								uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
								               "SyncTERM is currently running in safe mode.  This means you cannot add to the\n"
								               "directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(false);
								break;
							}
							tmp[0] = 0;
							uifc.changes = 0;
							uifc.helpbuf = "`Name`\n\n"
							               "Enter the name of the entry as it is to appear in the directory.";
							if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Name", tmp,
							LIST_NAME_MAX, K_EDIT) == -1) {
								if (check_exit(false))
									break;
							}
							if (!uifc.changes)
								break;
							if (list_name_check(list, tmp, NULL, true)) {
								uifc.helpbuf = "`Entry Name Already Exists`\n\n"
								               "An entry with that name already exists in the directory.\n"
								               "Please choose a unique name.\n";
								uifc.msg("Entry Name Already Exists!");
								check_exit(false);
								break;
							}
							if(is_reserved_bbs_name(tmp)) {
								uifc.helpbuf = "`Reserved Name`\n\n"
									       "The name you entered is reserved for internal use\n";
								uifc.msg("Reserved Name!");
								check_exit(false);
								break;
							}
							listcount++;
							list[listcount] = list[listcount - 1];
							list[listcount
							     - 1] = (struct bbslist *)malloc(sizeof(struct bbslist));
							memcpy(list[listcount - 1], &defaults, sizeof(struct bbslist));
							list[listcount - 1]->id = listcount - 1;
							strcpy(list[listcount - 1]->name, tmp);

							uifc.changes = 0;
							list[listcount - 1]->conn_type--;
							uifc.helpbuf = conn_type_help;
							if (uifc.list(WIN_SAV, 0, 0, 0,
							              &(list[listcount - 1]->conn_type), NULL, "Connection Type",
							              &(conn_types[1])) >= 0) {
								list[listcount - 1]->conn_type++;
								if (IS_NETWORK_CONN(list[listcount - 1]->conn_type)) {
									/* Set the port too */
									j = conn_ports[list[listcount - 1]->conn_type];
									if ((j < 1) || (j > 65535))
										j = list[listcount - 1]->port;
									list[listcount - 1]->port = j;
								}
								uifc.changes = 1;
							}
							else {
								if (check_exit(false))
									break;
							}

							if (uifc.changes) {
								uifc.changes = 0;
								uifc.helpbuf = address_help;
								if (uifc.input(WIN_MID | WIN_SAV,
								               0,
								               0
								               ,
								list[listcount - 1]->conn_type == CONN_TYPE_MODEM ? "Phone Number"
								                                  : list[
								listcount - 1]->conn_type == CONN_TYPE_SERIAL ? "Device Name"
								                                  : list[
								listcount - 1]->conn_type == CONN_TYPE_SERIAL_NORTS ? "Device Name"
								                                  : list[
								listcount - 1]->conn_type == CONN_TYPE_SHELL ? "Command"
								                                  : "Address"
								                                  ,
								               list[listcount - 1]->addr,
								               LIST_ADDR_MAX,
								               K_EDIT) >= 1)
									uifc.changes = 1;
								// Parse TCP port from address, if specified
								switch (list[listcount - 1]->conn_type) {
									case CONN_TYPE_MODEM:
									case CONN_TYPE_SERIAL:
									case CONN_TYPE_SERIAL_NORTS:
									case CONN_TYPE_SHELL:
										break;
									default: {
										char *p = strrchr(list[listcount - 1]->addr + 1, ':');
										if (p != NULL && p == strchr(list[listcount - 1]->addr, ':')) {
											char *tp;
											long port = strtol(p + 1, &tp, 10);
											if (*tp == '\0' && port > 0 && port <= 65535) {
												list[listcount - 1]->port = port;
												*p = '\0';
											}
										}
										break;
									}
								}
								check_exit(false);
							}
							if (quitting || !uifc.changes) {
								FREE_AND_NULL(list[listcount - 1]);
								list[listcount - 1] = list[listcount];
								listcount--;
							}
							else {
								add_bbs(settings.list_path, list[listcount - 1], true);
								load_bbslist(list,
								             BBSLIST_SIZE,
								             &defaults,
								             settings.list_path,
								             sizeof(settings.list_path),
								             shared_list,
								             sizeof(shared_list),
								             &listcount,
								             &opt,
								             &bar,
								             strdup(list[listcount - 1]->name));
								oldopt = -1;
							}
							break;
						case MSK_DEL:
							if ((list[opt] == NULL) || !list[opt]->name[0]) {
								uifc.helpbuf = "`Calming down`\n\n"
								               "~ Some handy tips on calming down ~\n"
								               "Close your eyes, imagine yourself alone on a brilliant white beach...\n"
								               "Picture the palm trees up towards the small town...\n"
								               "Glory in the deep blue of the perfectly clean ocean...\n"
								               "Feel the plush comfort of your beach towel...\n"
								               "Enjoy the shade of your satellite internet feed which envelops\n"
								               "your head, keeping you cool...\n"
								               "Set your TEMPEST rated laptop aside on the beach, knowing it's\n"
								               "completely impervious to anything on the beach...\n"
								               "Reach over to your fridge, grab a cold one...\n"
								               "Watch the seagulls in their dance...\n";
								uifc.msg("It's gone, calm down man!");
								check_exit(false);
								break;
							}
							if (safe_mode) {
								uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
								               "SyncTERM is currently running in safe mode.  This means you cannot remove from the\n"
								               "directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(false);
								break;
							}
							if (list[opt]->type == SYSTEM_BBSLIST) {
								uifc.helpbuf = "`Cannot delete from system list`\n\n"
								               "This entry was loaded from the system-wide list and cannot be deleted.";
								uifc.msg("Cannot delete system list entries");
								check_exit(false);
								break;
							}
							sprintf(str, "Delete %s?", list[opt]->name);
							i = 0;
							if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, str,
							              YesNo) != 0)
								break;
							del_bbs(settings.list_path, list[opt]);
							load_bbslist(list,
							             BBSLIST_SIZE,
							             &defaults,
							             settings.list_path,
							             sizeof(settings.list_path),
							             shared_list,
							             sizeof(shared_list),
							             &listcount,
							             NULL,
							             NULL,
							             NULL);
							oldopt = -1;
							break;
						case MSK_EDIT:
							if (safe_mode) {
								uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
								               "SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
								               "directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(false);
								break;
							}
							if (edit_list(list, list[opt], settings.list_path, false)) {
								load_bbslist(list,
								             BBSLIST_SIZE,
								             &defaults,
								             settings.list_path,
								             sizeof(settings.list_path),
								             shared_list,
								             sizeof(shared_list),
								             &listcount,
								             &opt,
								             &bar,
								             strdup(list[opt]->name));
								oldopt = -1;
							}
							break;
						case MSK_COPY:
							if (safe_mode) {
								uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
								               "SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
								               "directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(false);
								break;
							}
							if (copied == NULL)
								copied = malloc(sizeof(struct bbslist));
							if (copied == NULL) {
								uifc.msg("Cannot allocate memory to copy");
								check_exit(false);
								break;
							}
							memcpy(copied, list[opt], sizeof(struct bbslist));
							break;
						case MSK_PASTE:
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_FIXEDHEIGHT
							          ,
							          0,
							          (uifc.scrn_len - (uifc.list_height) + 1) / 2 - 4,
							          0,
							          &opt,
							          &bar,
							          list_title,
							          (char **)list);
							if (safe_mode) {
								uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
								               "SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
								               "directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(false);
								break;
							}
							if (copied == NULL)
								break;
							if (copied->type != SYSTEM_BBSLIST) {
								if (!edit_name(copied->name, list, NULL, true))
									break;
							}
							add_bbs(settings.list_path, copied, true);
							edit_list(list, copied, settings.list_path, false);
							load_bbslist(list,
							             BBSLIST_SIZE,
							             &defaults,
							             settings.list_path,
							             sizeof(settings.list_path),
							             shared_list,
							             sizeof(shared_list),
							             &listcount,
							             &opt,
							             &bar,
							             strdup(copied->name));
							oldopt = -1;
							free(copied);
							copied = NULL;
							break;
					}
				}
				else {
					if (connected) {
						if (safe_mode) {
							uifc.helpbuf = "`Cannot edit list in safe mode`\n\n"
							               "SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
							               "directory.";
							uifc.msg("Cannot edit list in safe mode");
							check_exit(false);
						}
						else if (edit_list(list, list[opt], settings.list_path, false)) {
							load_bbslist(list,
							             BBSLIST_SIZE,
							             &defaults,
							             settings.list_path,
							             sizeof(settings.list_path),
							             shared_list,
							             sizeof(shared_list),
							             &listcount,
							             &opt,
							             &bar,
							             strdup(list[opt]->name));
							oldopt = -1;
						}
					}
					else {
						memcpy(&retlist, list[val], sizeof(struct bbslist));
						free_list(&list[0], listcount);
						free(list);
						free(copied);
						return &retlist;
					}
				}
			}
		}
		else {
			for (; at_settings && !quitting;) {
				if (last_mode != cio_api.mode) {
					set_uifc_title();
					last_mode = cio_api.mode;
				}
				uifc.helpbuf = "`SyncTERM Settings Menu`\n\n"
				               "~ Web Lists ~\n"
				               "        Modify the list of dialing directoies fetch from the web\n\n"
				               "~ Default Connection Settings ~\n"
				               "        Modify the settings that are used by default for new entries\n\n"
				               "~ Current Screen Mode ~\n"
				               "        Set the current screen size/mode\n\n"
				               "~ Font Management ~\n"
				               "        Configure additional font files\n\n"
				               "~ Program Settings ~\n"
				               "        Modify hardware and screen/video settings\n\n"
				               "~ File Locations ~\n"
				               "        Display location for config and directory files\n\n"
				               "~ Build Options ~\n"
				               "        Display compile options selected at build time\n\n"
				               "~ " ALT_KEY_NAMEP "-B ~\n"
				               "        View scrollback of last session\n";
				if (oldopt != -2)
					settitle(syncterm_version);
				oldopt = -2;
				if (!nowait) {
					bl_kbwait();
					nowait = true;
				}
				val = settings_list(&sopt, &sbar, connected ? connected_settings_menu : settings_menu, WIN_UNGETMOUSE | WIN_ESC);
				if (connected && (val >= 2))
					val++;
				switch (val) {
					case -2 - 0x3000: /* ALT-B - Scrollback */
						if (!connected) {
							viewofflinescroll();
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_INACT | WIN_FIXEDHEIGHT,
							          0, 0, 0, &opt, &bar, list_title, (char **)list);
							draw_comment(list[opt]);
						}
						break;
					case -2 - CIO_KEY_MOUSE:
						if (mouse_pending()) {
							do {
								getmouse(&mevent);
							} while (mouse_pending());
							if (mevent.event == CIOLIB_BUTTON_1_CLICK
							&& mevent.starty == cio_textinfo.screenheight - 1) {
								if (edit_comment(list[opt], settings.list_path))
									redraw = 1;
								break;
							}
						}

					/* Fall-through */
					case -2 - 0x0f00: /* Backtab */
						if (val == -2 - 0x0f00) {
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_INACT | WIN_FIXEDHEIGHT,
							          0, 0, 0, &opt, &bar, list_title, (char **)list);
							if (!edit_comment(list[opt], settings.list_path))
								break;
						}

					/* Fall-through */
					case -2 - 0x4b00: /* Left Arrow */
					case -2 - 0x4d00: /* Right Arrow */
					case -11: /* TAB */
						settings_list(&sopt, &sbar, connected ? connected_settings_menu : settings_menu, WIN_SEL);
						at_settings = !at_settings;
						break;
					case -2:
						nowait = false;
						break;
					case -1: /* ESC */
						if (!connected)
							if (!check_exit(true))
								continue;


						free_list(&list[0], listcount);
						free(list);
						free(copied);
						return NULL;
					case 0: /* Edit Web Lists */
						if (edit_web_lists())
							load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list,
							    sizeof(shared_list), &listcount, &opt, &bar, current ? strdup(current) : NULL);
						break;
					case 1: /* Edit default connection settings */
						edit_list(NULL, &defaults, settings.list_path, true);
						break;
					case 2: { /* Screen Mode */
						struct text_info ti;
						gettextinfo(&ti);

						uifc.helpbuf = "`Current Screen Mode`\n\n"
						               "Change the current screen size/mode.\n"
						               "\n"
						               "To change the initial screen mode, set Program Settings->Startup Screen\n"
						               "Mode instead.";
						i = ti.currmode;
						i = ciolib_to_screen(ti.currmode);
						i--;
						if (i < 0)
							i = 0;
						j = i;
						i =
						        uifc.list(WIN_SAV, 0, 0, 0, &i, &j, "Screen Mode",
						                  screen_modes + 1);
						if (i >= 0) {
							i++;
							uifcbail();
							textmode(screen_to_ciolib(i));
							set_default_cursor();
							init_uifc(true, true);
							uifc.list_height = listcount + 5;
							if (uifc.list_height > (uifc.scrn_len - 4))
								uifc.list_height = uifc.scrn_len - 4;
							uifc.list((listcount < MAX_OPTS ? WIN_XTR : 0)
							          | WIN_ACT | WIN_INSACT | WIN_DELACT | WIN_SAV | WIN_ESC
							          | WIN_INS | WIN_DEL | WIN_EDIT | WIN_EXTKEYS | WIN_DYN
							          | WIN_SEL | WIN_INACT | WIN_FIXEDHEIGHT | WIN_NODRAW,
							          0, 0, 0, &opt, &bar, list_title, (char **)list);
							draw_comment(list[opt]);
						}
						else if (check_exit(false)) {
							free_list(&list[0], listcount);
							free(list);
							free(copied);
							return NULL;
						}
					}
					break;
					case 3: /* Font management */
						if (!safe_mode) {
							font_management();
							load_font_files();
						}
						break;
					case 4: /* Program settings */
						change_settings(connected);
						load_bbslist(list,
						             BBSLIST_SIZE,
						             &defaults,
						             settings.list_path,
						             sizeof(settings.list_path),
						             shared_list,
						             sizeof(shared_list),
						             &listcount,
						             &opt,
						             &bar,
						             list[opt] ? strdup(list[opt]->name) : NULL);
						oldopt = -1;
						break;
					case 5: /* File Locations */
						strcpy(personal_list, settings.list_path);
						get_syncterm_filename(setting_file,
						                      sizeof(setting_file),
						                      SYNCTERM_PATH_INI,
						                      false);
						get_syncterm_filename(default_download,
						                      sizeof(default_download),
						                      SYNCTERM_DEFAULT_TRANSFER_PATH,
						                      false);
						get_syncterm_filename(cache_path,
						                      sizeof(cache_path),
						                      SYNCTERM_PATH_CACHE,
						                      false);
						get_syncterm_filename(keys_path,
						                      sizeof(keys_path),
						                      SYNCTERM_PATH_KEYS,
						                      false);
						asprintf(&p,
						         "`SyncTERM File Locations`\n\n"
						         "~ Global Dialing Directory (Read-Only) ~\n"
						         "  %s\n"
						         "~ Personal Dialing Directory ~\n"
						         "  %s\n"
						         "~ Configuration File ~\n"
						         "  %s\n"
						         "~ Default download Directory ~\n"
						         "  %s\n"
						         "~ Cache Directory ~\n"
						         "  %s\n"
						         "~ SSH Keys File ~\n"
						         "  %s\n",
						         shared_list,
						         personal_list,
						         setting_file,
						         default_download,
						         cache_path,
						         keys_path);
						uifc.showbuf(WIN_MID | WIN_SAV | WIN_HLP,
						             0,
						             0,
						             78,
						             20,
						             "File Locations",
						             p,
						             NULL,
						             NULL);
						break;
					case 6: // Build Options
						asprintf(&p,
						         "`SyncTERM Build Options`\n\n"
						         "%s Cryptlib (ie: SSH and TLS)\n"
						         "%s Operation Overkill ][ Terminal\n"
						         "%s JPEG XL support\n\n"
						         "Video\n"
						         "    %s SDL\n"
						         "    %s X11 Support\n"
						         "        %s Xinerama\n"
						         "        %s XRandR\n"
						         "        %s XRender\n"
						         "    %s GDI\n\n"
						         "Audio\n"
						         "    %s OSS\n"
						         "    %s SDL\n"
						         "    %s ALSA\n"
						         "    %s WaveOut\n"
						         "    %s PortAudio\n"
						         "    %s PulseAudio\n",
#if (defined(WITHOUT_CRYPTLIB) || !defined(WITH_CRYPTLIB))
						         "[ ]",
#else
						         "[`\xFB`]",
#endif
#ifdef WITHOUT_OOII
						         "[ ]",
#else
						         "[`\xFB`]",
#endif
#ifdef WITH_JPEG_XL
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_SDL
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef DISABLE_X11
						         "[ ]",
#else
						         "[`\xFB`]",
#endif
#ifdef WITH_XINERAMA
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_XRANDR
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_XRENDER
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_GDI
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#if (defined SOUNDCARD_H_IN) && (SOUNDCARD_H_IN > 0) && (defined AFMT_U8)
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_SDL_AUDIO
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef USE_ALSA_SOUND
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef _WIN32
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_PORTAUDIO
						         "[`\xFB`]",
#else
						         "[ ]",
#endif
#ifdef WITH_PULSEAUDIO
						         "[`\xFB`]"
#else
						         "[ ]"
#endif
						        );
						uifc.showbuf(WIN_MID | WIN_SAV | WIN_HLP,
						             0,
						             0,
						             60,
						             21,
						             "Build Options",
						             p,
						             NULL,
						             NULL);
						break;
#if (defined(WITH_CRYPTLIB) && !defined(WITHOUT_CRYPTLIB))
					case 7:	// Encryption!
						encryption_menu(settings.list_path);
						break;
#endif
				}
			}
		}
	}
}

cterm_emulation_t
get_emulation(struct bbslist *bbs)
{
	if (bbs == NULL)
		return CTERM_EMULATION_ANSI_BBS;

	switch (bbs->screen_mode) {
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			return CTERM_EMULATION_PETASCII;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			return CTERM_EMULATION_ATASCII;
		case SCREEN_MODE_PRESTEL:
			return CTERM_EMULATION_PRESTEL;
		case SCREEN_MODE_BEEB:
			return CTERM_EMULATION_BEEB;
		case SCREEN_MODE_ATARIST_40X25:
		case SCREEN_MODE_ATARIST_80X25:
		case SCREEN_MODE_ATARIST_80X25_MONO:
			return CTERM_EMULATION_ATARIST_VT52;
		default:
			return CTERM_EMULATION_ANSI_BBS;
	}
}

const char *
get_emulation_str(struct bbslist *bbs)
{
	if (bbs->term_name[0])
		return bbs->term_name;
	
	switch (get_emulation(bbs)) {
		case CTERM_EMULATION_ANSI_BBS:
			return "syncterm";
		case CTERM_EMULATION_PETASCII:
			return "PETSCII";
		case CTERM_EMULATION_ATASCII:
			return "ATASCII";
		case CTERM_EMULATION_PRESTEL:
			return "Prestel";
		case CTERM_EMULATION_BEEB:
			return "Beeb7";
		case CTERM_EMULATION_ATARIST_VT52:
			return "AtariST+VT52";
	}
	return "none";
}

void
get_term_size(struct bbslist *bbs, int *cols, int *rows)
{
	int cmode = find_vmode(screen_to_ciolib(bbs->screen_mode));

	if (cmode < 0) {
		// This shouldn't happen, but if it does, make something up.
		*cols = 80;
		*rows = 24;
		return;
	}
	if (vparams[cmode].cols < 80) {
		if (cols)
			*cols = 40;
	}
	else {
		if (vparams[cmode].cols < 132) {
			if (cols)
				*cols = 80;
		}
		else {
			if (cols)
				*cols = 132;
		}
	}
	if (rows) {
		*rows = vparams[cmode].rows;
		if (!bbs->nostatus)
			(*rows)--;
	}
}
