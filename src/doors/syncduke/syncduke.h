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
void syncduke_stats_toggle(void);                      /* Ctrl-S: toggle the live stats overlay */
void syncduke_depth_cycle(void);                       /* Ctrl-T: cycle the pipeline depth */
void syncduke_tier_cycle(void);                        /* F4: cycle the graphics tier (jxl/sixel) */
void syncduke_hangup(const char *why);                 /* client gone: log + exit (free the node) */

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
/* Image-tier placement: centered cell origin (*row,*col, 1-based) for the sixel/JXL
 * cursor, plus the image's center column and half-width in cells (for mouse steering).
 * Centers the out_w x out_h image in the probed pixel canvas using the real cell size;
 * falls back to the top-left when the cell size is unknown.  Any out-ptr may be NULL. */
void syncduke_image_geometry(int *row, int *col, int *center_col, int *half_cols);
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

/* --- provided by syncduke_log.c (optional file debug log; disabled unless a path
 * is set via env SYNCDUKE_LOG or syncduke.ini [debug] log) --- */
void syncduke_log(const char *fmt, ...);        /* printf-style; no-op when disabled */
void syncduke_log_set_path(const char *path);   /* configure the log file (from the ini) */
void syncduke_log_init(void);                   /* install the crash handler + atexit marker */

#endif /* SYNCDUKE_H_ */
