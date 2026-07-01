#ifndef SYNCDUKE_H_
#define SYNCDUKE_H_

/*
 * syncduke.h -- cross-module declarations for the SyncDuke door shim.
 *
 * The vendored engine owns main(); our headless layer is split across
 * syncduke_plat.c (the display.c replacement: framebuffer, palette, timer,
 * input) and syncduke_io.c (terminal out-buffer + the sixel present path).
 */

#include <stddef.h>
#include <stdint.h>

/* SyncDuke v1 renders Build "classic" mode only. */
#define SYNCDUKE_SCREEN_W 320
#define SYNCDUKE_SCREEN_H 200

/* --- provided by syncduke_plat.c --- */
const uint8_t *syncduke_fb(void);             /* SYNCDUKE_SCREEN_W*H 8-bit palette indices */
const uint8_t *syncduke_palette(void);        /* 256 * 3 bytes, 8-bit RGB            */
int            syncduke_palette_take_dirty(void); /* 1 if palette changed since last call (clears) */

/* --- provided by syncduke_io.c --- */
void syncduke_out_put(const void *buf, size_t len);   /* stage bytes for the terminal */
void syncduke_out_flush(void);                        /* push staged bytes to the sink */
void syncduke_present(void);                           /* encode + emit one frame      */
void syncduke_pace_ack(void);                          /* a per-frame DSR report came back (pacing) */
int  syncduke_pace_ready(void);                        /* pipeline has room for another frame (render gate) */
int  syncduke_pace_inflight(void);                     /* unacked frames in flight (frame-stall log) */
int  syncduke_pace_curdepth(void);                     /* current effective pipeline depth (frame-stall log) */
void syncduke_stats_toggle(void);                      /* Ctrl-S: toggle the live stats overlay */
void syncduke_depth_cycle(void);                       /* Ctrl-T: cycle the pipeline depth */
void syncduke_tier_cycle(void);                        /* F4: cycle the graphics tier (jxl/sixel) */
int  syncduke_text_tier(void);                         /* 1 if the active tier is a text/block tier (engine skips image-only screens) */
void syncduke_hangup(const char *why);                 /* client gone: log + exit (free the node) */

/* --- provided by syncduke_stubs.c --- */
void sd_music_pending_retry(void);                     /* replay title music dropped before the audio tier was known */

/* --- provided by syncduke_input.c (terminal -> Build/Duke key state) ---
 * Self-contained (no engine linkage): syncduke_map_key is a pure function; the queue
 * holds raw Build scancode bytes (low 7 bits = scancode, 0x80 = key release) the
 * way the engine's keyhandler()/_readlastkeyhit() expect. `now` is the engine's
 * totalclock (a 120Hz tick), used only to time the synthetic key releases that a
 * terminal (which has no key-up) can't send. */
int  syncduke_map_key(const char *seq, int len, int gameplay);   /* terminal byte/seq -> scancode, or -1 */
void syncduke_input_pump(int fd, int now, int gameplay);          /* read fd, enqueue key-down events */
void syncduke_input_reset(void);                                  /* clear latches on game load (crouch, holds, turn) */
void syncduke_input_expire(int now);                /* enqueue key-up for held-out keys */
int  syncduke_input_has_raw(void);                  /* a queued raw scancode byte awaits? */
int  syncduke_input_pop_raw(void);                  /* next raw scancode byte (0 if none) */
int  syncduke_input_fd(void);                       /* the input fd (resolved client socket / stdin) */
int  syncduke_is_syncterm(void);                    /* 1 if the client is SyncTERM (DA reply); cterm 2x sixel scaling */
int  syncduke_kitty_active(void);                   /* 1 if the kitty keyboard protocol negotiated (true key-up) */
int  syncduke_evdev_active(void);                   /* 1 if SyncTERM physical key (evdev) reports negotiated */
uint32_t syncduke_rtt(void);                        /* smoothed RTT (ms); drives native-vs-synthetic turn */
int  syncduke_turn_native(void);                    /* 1 if turn keys use the native hold (low-latency true-key-up) */
int  syncduke_jxl_supported(void);                  /* 1 if SyncTERM can decode JXL (CTQJS reply); -> JXL/APC tier */
int  syncduke_have_sixel(void);                     /* 1 if the terminal advertised sixel (DA1/CTDA cap 4 or SyncTERM) */
int  syncduke_probe_replied(void);                  /* 1 once the terminal answered the DA capability probe */
int  syncduke_term_px_w(void);                      /* terminal pixel-canvas width from probe, 0 if unknown */
int  syncduke_term_px_h(void);                      /* terminal pixel-canvas height from probe, 0 if unknown */
int  syncduke_term_cell_w(void);                    /* terminal cell width in px from ESC[16t, 0 if unknown */
int  syncduke_term_cell_h(void);                    /* terminal cell height in px from ESC[16t, 0 if unknown */
int  syncduke_canvas_w(void);                       /* graphics-canvas width: XTSMGRAPHICS or text-area px */
int  syncduke_canvas_h(void);                       /* graphics-canvas height: XTSMGRAPHICS or text-area px */
int  syncduke_jxl_scale_max(void);                  /* JXL fill cap (px), [video] scale_max; 0 = uncapped */
int  syncduke_sixel_max_w(void);                    /* sixel width cap (px), [video] sixel_max_width */
/* The displayed image's horizontal center column and half-width in cells, recorded by
 * present() each frame (the placement depends on tier + terminal).  Used by the mouse
 * steer to map a pointer column to a turn rate around the actual image. */
void syncduke_hsteer(int *center_col, int *half_cols);
int  syncduke_mouse_enabled(void);                  /* 1 if terminal mouse steering is on (io.c keeps SGR tracking in sync) */
void syncduke_mouse_toggle(void);                   /* Ctrl-O: toggle terminal mouse steering on/off */
int  syncduke_mouse_sens(void);                     /* steer sensitivity, 0..63 (Setup Controls slider) */
void syncduke_mouse_sens_set(int v);                /* set steer sensitivity (clamped 0..63) */
int  syncduke_kb_tap(void);                         /* KEY TAP slider 0..63 (fresh-press frames) */
void syncduke_kb_tap_set(int v);
int  syncduke_kb_hold(void);                        /* KEY HOLD slider 0..63 (repeat-byte frames) */
void syncduke_kb_hold_set(int v);
int  syncduke_kb_turn(void);                        /* TURN HOLD slider 0..63 (turn-key frames) */
void syncduke_kb_turn_set(int v);
int  syncduke_kb_fastturn(void);                    /* FAST TURN on/off (defeat Duke's turn-accel ramp) */
void syncduke_kb_fastturn_set(int v);
/* The steer/fire levels (syncduke_mouse_turn, syncduke_mouse_fire) are defined in
 * syncduke_input.c and read directly by the engine's getinput() in Game/src/player.c. */

/* --- provided by syncduke_door.c (DOOR32.SYS / -s<fd> door interface) ---
 * A constructor captures argv before the engine's main() runs and resolves the
 * BBS client socket (so the engine never has to know about door args). */
int      syncduke_door_socket(void);          /* client comm socket fd, or -1 if none (local/dev) */
uint32_t syncduke_door_time_limit_ms(void);   /* session time limit in ms, or 0 if none */
const char *syncduke_door_alias(void);        /* user's alias/handle, or "" */

/* --- provided by syncduke_game.c (engine-state queries; pulls in duke3d.h) --- */
int syncduke_in_gameplay(void);   /* 1 when actually playing (not in a menu), so the WASD/Space action layer applies */
int syncduke_player_dead(void);   /* 1 when the player is dead -- door drops the action layer so Space = Open (restart) */

/* --- text-tier legible HUD overlay (game pop-up/status quotes as real chars) ---
 * In a text/block tier the game's own quote font rasterises to unreadable blocks. When
 * syncduke_text_hud is set (door: the active tier is a text tier), the engine's
 * operatefta() (Game/src/game.c) CAPTURES the on-screen quote/chat strings here instead
 * of drawing them, and the door's text-tier present redraws them as real terminal
 * characters over the block frame.  In an image tier (sixel/JXL) the flag is 0, so the
 * game renders its font normally.  Storage lives in syncduke_game.c. */
#define SYNCDUKE_HUD_MAX 5      /* fta quote + up to MAXUSERQUOTES(4) chat lines */
#define SYNCDUKE_HUD_LEN 160    /* >= user_quote[128] and fta_quotes[64]         */
typedef struct {
	char text[SYNCDUKE_HUD_LEN];  /* NUL-terminated message text                 */
	int  y;                       /* Duke 320x200 y of the row -> proportional terminal row */
} syncduke_hud_line_t;
extern int syncduke_text_hud;                             /* door sets 1 in a text tier */
void syncduke_hud_begin(void);                            /* engine: reset the per-frame capture */
void syncduke_hud_add(const char *text, int y);           /* engine: record a captured line */
int  syncduke_hud_lines(const syncduke_hud_line_t **out); /* door: current lines; returns count */

/* --- provided by syncduke_log.c (optional file debug log; disabled unless a path
 * is set via env SYNCDUKE_LOG or syncduke.ini [debug] log) --- */
void syncduke_log(const char *fmt, ...);        /* printf-style; no-op when disabled */
void syncduke_log_set_path(const char *path);   /* configure the log file (from the ini) */
void syncduke_log_init(void);                   /* install the crash handler + atexit marker */

#endif /* SYNCDUKE_H_ */
