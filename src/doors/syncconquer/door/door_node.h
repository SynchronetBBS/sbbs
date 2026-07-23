// door_node.h -- door-native Synchronet node integration for SyncConquer:
// node.exb who's-online status, Ctrl-U who's-online overlay, Ctrl-P page
// compose. Cloned at a minimal v1 level from syncduke_node.c/.h (see that
// file's header comment for the fuller design this trims) -- Task 5's brief
// asks for exactly three things: the node.exb status string, Ctrl-U, Ctrl-P.
// Everything no-ops when sbbs_node_available() is false (no BBS context,
// e.g. a bare dev/test launch).
#ifndef DOOR_NODE_H_
#define DOOR_NODE_H_

#ifdef __cplusplus
extern "C" {
#endif

// Locate the BBS node context ($SBBSCTRL + the door's -home) and publish the
// initial node.exb status. Call once, after -home is known (door_io_init()).
void door_node_init(const char *home);

// Per-frame housekeeping: refreshes the node.exb status (only writes when it
// changes), and services a pending Ctrl-U/Ctrl-P request. Cheap when idle
// (a couple of string compares) -- call unconditionally from door_io_present().
void door_node_tick(void);

// Paint the who's-online / page-compose overlay (if one is currently showing)
// over the just-emitted frame, positioned by the CURRENT text grid size
// (cols/rows -- door_io.c's canvas/probe state). No-op when nothing is live.
void door_node_draw(int cols, int rows);

// True while the who's-online / page-compose overlay is showing. door_io.c
// checks this ahead of its whole-frame de-dupe (a static game menu wouldn't
// otherwise trigger a resend) and forces a repaint for as long as it's true,
// so the banner's countdown/typed text stays live on screen.
int door_node_overlay_active(void);

// Ctrl-U: flag the who's-online banner to (re)build on the next tick.
void door_node_userlist_request(void);

/* A one-line transient banner for `ms`, on the same overlay as the page and
 * who's-online notices. Used by the idle countdown. */
void door_node_notice(const char *text, int ms);

/* True while such a notice is on screen. It is drawn on the BOTTOM row -- the
 * row the Ctrl-S stats strip also owns -- so the strip checks this before
 * repainting, and redraws itself once the notice expires. */
int door_node_notice_active(void);

/* Retire the current notice immediately instead of waiting out its dwell. */
void door_node_notice_expire(void);

// Ctrl-P: flag the page-compose overlay to open on the next tick.
void door_node_page_request(void);

// True while the Ctrl-P compose overlay is capturing keystrokes -- door_io.c's
// input parser checks this BEFORE its normal key dispatch and routes bytes to
// door_node_compose_key() instead while it's set.
int door_node_composing(void);

// Feed one byte to the compose line while door_node_composing() is true:
// printable ASCII appends, Backspace (0x08) erases, Enter (\r) sends, Escape
// (0x1b) cancels. Anything else is ignored.
void door_node_compose_key(int c);

#ifdef __cplusplus
}
#endif

#endif // DOOR_NODE_H_
