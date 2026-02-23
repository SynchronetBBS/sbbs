/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SYNCTERM_H_
#define _SYNCTERM_H_

#include <dirwrap.h>
#include <stdbool.h>

#include "bbslist.h"
#include "webget.h"

#if defined(_WIN32)
 #include <malloc.h> /* alloca() on Win32 */
#endif

enum {
	SYNCTERM_PATH_INI,
	SYNCTERM_PATH_LIST,
	SYNCTERM_DEFAULT_TRANSFER_PATH,
	SYNCTERM_PATH_CACHE,
	SYNCTERM_PATH_KEYS,
	SYNCTERM_PATH_SYSTEM_CACHE
};

enum CursorTypeEnum {
	ST_CT_DEFAULT,
	ST_CT_BLINK_UNDER,
	ST_CT_SOLID_UNDER,
	ST_CT_BLINK_BLK,
	ST_CT_SOLID_BLK,
};

/* Default modem device */
#if defined(__APPLE__) && defined(__MACH__)

/* Mac OS X */
 #define DEFAULT_MODEM_DEV "/dev/tty.modem"
#elif defined(_WIN32)
 #define DEFAULT_MODEM_DEV "COM1"
#else

/* FreeBSD */
 #define DEFAULT_MODEM_DEV "/dev/ttyd0"
#endif

/* "ALT/META" key name string */
#if defined(__APPLE__) && defined(__MACH__)
 #define ALT_KEY_NAME "OPTION"
 #define ALT_KEY_NAMEP "Option"
 #define ALT_KEY_NAME3CH "OPT"
#else
 #define ALT_KEY_NAME "ALT"
 #define ALT_KEY_NAMEP "Alt"
 #define ALT_KEY_NAME3CH "ALT"
#endif

struct modem_settings {
	char  init_string[INI_MAX_VALUE_LEN];
	char  dial_string[INI_MAX_VALUE_LEN];
	char  device_name[INI_MAX_VALUE_LEN + 1];
	ulong com_rate;
};

struct syncterm_settings {
	named_string_t      **webgets;
	int                   confirm_close;
	int                   startup_mode;
	int                   output_mode;
	int                   backlines;
	int                   prompt_save;
	struct modem_settings mdm;
	char                  TERM[INI_MAX_VALUE_LEN + 1];
	char                  list_path[MAX_PATH + 1];
	char                  stored_list_path[MAX_PATH + 1];
	double                scaling_factor;
	int                   xfer_failure_keypress_timeout; /* wait for user acknowledgement via keypress, in seconds
                                                              */
	int                   xfer_success_keypress_timeout; /* wait for user acknowledgement via keypress, in seconds
                                                              */
	int                   custom_cols;
	int                   custom_rows;
	int                   custom_fontheight;
	int                   custom_aw;
	int                   custom_ah;
	int                   window_width;
	int                   window_height;
	int                   left_just;
	int                   blocky;
	int                   extern_scale;
	uint                  audio_output_modes;
	bool                  invert_wheel;
	bool                  webgetUserList;
	int                   keyDerivationIterations;
	enum CursorTypeEnum   defaultCursor;
	unsigned              uifc_hclr;
	unsigned              uifc_lclr;
	unsigned              uifc_bclr;
	unsigned              uifc_cclr;
	unsigned              uifc_lbclr;
	unsigned              uifc_lbbclr;
};

extern ini_bitdesc_t audio_output_bits[];
extern ini_bitdesc_t audio_output_types[];

extern char                    *inpath;
extern char                    *list_override;
extern const char              *syncterm_version;
extern struct vmem_cell        *scrollback_buf;
extern unsigned int             scrollback_lines;
extern unsigned int             scrollback_mode;
extern unsigned int             scrollback_cols;
extern struct syncterm_settings settings;
extern bool                     quitting;
extern int                      default_font;
extern char                    *font_names[];
extern int                      safe_mode;
extern char                    *output_types[];
extern int                      output_map[];
extern char                    *output_descrs[];
extern char                    *output_enum[];
extern char                    *cursor_descrs[];
extern char                    *cursor_enum[];
extern int                      fake_mode;
extern const char * const colour_names[18];
extern const char * const colour_enum[18];
extern const char * const bg_colour_names[10];
extern const char * const bg_colour_enum[10];

void parse_url(char *url, struct bbslist *bbs, int dflt_conn_type, int force_defaults);
char *get_syncterm_filename(char *fn, int fnlen, int type, bool shared);
void load_settings(struct syncterm_settings *set);
int ciolib_to_screen(int screen);
int screen_to_ciolib(int ciolib);
bool check_exit(bool force);
void set_default_cursor(void);


#endif // ifndef _SYNCTERM_H_
