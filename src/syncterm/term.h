/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _TERM_H_
#define _TERM_H_

#include <stdbool.h>

#include "bbslist.h"
#include "ciolib.h"

struct terminal {
	int height;
	int width;
	int x;
	int y;
	int nostatus;
};

enum mouse_modes {
	MM_OFF,
	MM_RIP = 1,
	MM_X10 = 9,
	MM_NORMAL_TRACKING = 1000,
	MM_HIGHLIGHT_TRACKING = 1001,
	MM_BUTTON_EVENT_TRACKING = 1002,
	MM_ANY_EVENT_TRACKING = 1003
};

struct mouse_state {
	uint32_t         flags;

#define MS_FLAGS_SGR (1 << 0)
#define MS_FLAGS_DISABLED (1 << 1)
#define MS_SGR_SET (1006)
	enum mouse_modes mode;
};

#define XMODEM_128B (1 << 10) /* Use 128 byte block size (ick!) */

extern struct terminal   term;
extern struct cterminal *cterm;
extern int               log_level;
extern bool              force_status_update;
void setup_mouse_events(struct mouse_state *ms);
void show_status_url(const char *url);

/* Clipboard → wire (codepage-aware, bracketed-paste-aware).  Reads
 * the system clipboard, converts UTF-8 → active codepage using the
 * current alt-font, and sends.  No-op when clipboard is empty. */
void do_paste(void);

/* Font control dialog — uifc-driven font picker for the four cterm
 * font slots.  No-op in safe mode and on text-only video backends.
 * Caller restores mouse events afterwards. */
void font_control(struct bbslist *bbs, struct cterminal *cterm);

/* Save the current screen in CP437 mode after resetting font slots
 * and the legacy bg-bright / no-blink flags — used by every uifc-
 * shaped dialog so the saved snapshot matches the cp437 attribute
 * model uifc paints in.  Caller restores via restorescreen +
 * freescreen.  Returns NULL on alloc failure. */
struct ciolib_screen *cp437_savescrn(void);

/* Save the current cterm area (NOT including the status bar) as IBM-
 * CGA / BinaryText to `fp`, optionally appending a SAUCE block.
 * Returns 0 on success, errno on write failure.  Caller is responsible
 * for closing `fp` (the consent-token model relies on the caller
 * owning the FILE*). */
#include <stdio.h>
int save_screen_binary(FILE *fp, bool with_sauce, struct bbslist *bbs);

/* Signal the doterm() main loop to skip its idle WaitForEvent so
 * the next iteration runs immediately.  Thread-safe, NULL-safe (a
 * no-op when called outside doterm()'s active session).  Wired into
 * conn_buf_put for the in-direction buffer and wren_result_push so
 * remote-byte arrivals and async-op completions wake the loop. */
void doterm_wake(void);
char *detect_url_at(struct vmem_cell *cells, int width, int total_rows, int click_col, int click_row);
void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path);
void xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch);
void zmodem_batch_upload(struct bbslist *bbs, char **paths, int npaths);
void xmodem_batch_upload(struct bbslist *bbs, char **paths, int npaths, long mode, int lastch);
void xmodem_download(struct bbslist *bbs, long mode, char *path);
void zmodem_download(struct bbslist *bbs);
void ascii_upload(FILE *fp);
void raw_upload(FILE *fp);
/* Returns the captured CET frame buffer through frame_buffer/buflen
 * (caller must free).  No-op if cterm isn't in PRESTEL emulation. */
void cet_telesoftware_download(struct bbslist *bbs, void **frame_buffer, size_t *buflen);
bool doterm(struct bbslist *);
void mousedrag(struct vmem_cell *scrollback, bool force_rect);
void get_cterm_size(int *cols, int *rows, int ns);
int get_cache_fn_base(struct bbslist *bbs, char *fn, size_t fnsz);
int get_cache_fn_subdir(struct bbslist *bbs, char *fn, size_t fnsz, const char *subdir);
void send_login(struct bbslist *bbs);

#endif // ifndef _TERM_H_
