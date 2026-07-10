/* syncretro.h -- cross-module contract for the SyncRetro door glue.
 *
 * One header shared by every syncretro_*.c and retro_bridge.c/retro_env.c
 * (peer of retro_core.h, which wraps the libretro core ABI). Mirrors the
 * single-contract-header pattern of ../syncmoo1/syncmoo1.h. See DESIGN.md for
 * how these fit together; module ownership is noted per section below.
 */
#ifndef SYNCRETRO_H_
#define SYNCRETRO_H_

#include <stddef.h>
#include <stdint.h>

/* --- syncretro_door.c: DOOR32.SYS / -s<fd> / -name / -home / -t setup ------
 * Resolves the client socket + session limits from the drop file or flags,
 * BEFORE the core is loaded. Values are read back through these accessors.
 * sr_door_setup() returns 0 to continue, non-zero to exit (e.g. -help). */
int         sr_door_setup(int argc, char **argv);
int         sr_door_socket(void);                   /* client socket fd, or -1 (dev: stdout) */
const char *sr_door_name(void);                     /* player alias, or NULL */
const char *sr_door_home(void);                     /* -home sandbox dir, or NULL */
const char *sr_door_core_path(void);                /* -core <path> / config, or NULL */
const char *sr_door_rom_path(void);                 /* game ROM path, or NULL */
/* Strip the door's own args so a core's arg parser (if any) never sees them. */
void        sr_door_sanitize_argv(int *argc, char **argv);

/* Nonzero once the session should end: the DOOR32 time limit elapsed, the
 * client dropped carrier, or the player hit the quit key. Polled once per
 * frame by main.c's run loop, which then tears down normally. */
int         sr_door_should_exit(void);
/* Latch a carrier drop (syncretro_input.c, on socket EOF/error): the loop
 * exits at the top of the next frame rather than dying mid-callback. */
void        sr_door_carrier_lost(void);

/* The one unrecoverable path: a write error to a dead client, raised from deep
 * inside a libretro callback where there is no way to return a failure. Runs
 * the (idempotent, bounded) terminal restore and _exit()s. Never returns. */
void        sr_door_hangup(const char *why);

/* --- syncretro_config.c: per-user sandbox + core/system/save/ROM dirs ------
 * Creates the -home sandbox and resolves the BIOS (system) + save dirs handed
 * to the core via retro_env.c's GET_SYSTEM_DIRECTORY / GET_SAVE_DIRECTORY. Run
 * after sr_door_setup(), before rc_core_load_game(). Returns 0 (non-fatal
 * problems are logged, see DESIGN.md sec 10). */
int         sr_config_apply(void);
/* Diagnostic trace: every inbound byte, plus the frame-pacing decisions and a
 * once-a-second pad sample. OFF unless `keytrace.on` exists in the door's own
 * directory (or SYNCRETRO_KEYTRACE names a file); the trace lands in
 * `keytrace.log` beside the switch. A no-op -- not even a file handle -- when
 * disabled. Answers what no amount of source-reading can: what a given terminal
 * actually sends, and whether a frozen picture is a stalled door or a core that
 * has the input and draws nothing. */
void        sr_trace(const char *fmt, ...);

const char *sr_config_launch_dir(void);             /* the door's own dir (pre-chdir cwd) */
const char *sr_config_system_dir(void);             /* BIOS dir (shared, read-only) */
const char *sr_config_save_dir(void);               /* per-user SRAM/save-state dir */
/* The core .so and ROM, resolved to ABSOLUTE paths against the launch directory
 * (and, for the ROM, its roms/ sub-directory) before sr_config_apply()'s chdir
 * moves cwd into the sandbox. Use these, not the sr_door_*_path() raw values.
 * NULL when none was given. */
const char *sr_config_core_path(void);
const char *sr_config_rom_path(void);

/* syncretro.ini, [audio] -- read from the launch directory by sr_config_apply().
 * The file is optional and every key has a default, so a door directory without
 * one behaves exactly as a door with audio at its defaults. Values are CLAMPED
 * on read: a sysop must not be able to configure a state the measurements in
 * M4_AUDIO.md sec 4 call broken (e.g. chunk_ms = 17, where the Ogg headers cost
 * three times the audio). */
int    sr_config_audio_enabled(void);     /* [audio] enabled,   default true */
double sr_config_audio_quality(void);     /* quality,   0.01..1.0, default 0.15 */
int    sr_config_audio_volume(void);      /* volume,    0..100,    default 100 */
int    sr_config_audio_chunk_ms(void);    /* chunk_ms,  50..250,   default 100 */
int    sr_config_audio_prebuffer(void);   /* prebuffer, 2..8,      default 3 */

/* --- syncretro_io.c: terminal enter/probe/leave + the present path ---------
 * sr_io_init() adopts the door socket (or the stdout dev fallback) and arms the
 * enter+probe handshake; sr_io_present() composes one frame through termgfx
 * (quantize -> fit/center -> sixel encode -> emit, DSR-ACK paced);
 * sr_io_leave() restores the BBS terminal. SYNCRETRO_SIXELOUT=<path> capture
 * mode overwrites that file with one self-contained frame per present (offline
 * verification with no live terminal). */
int  sr_io_init(int sockfd);
void sr_io_present(const uint8_t *rgb, int w, int h);   /* rgb = w*h*3, top-down */
void sr_io_leave(void);

/* Staged, non-blocking output shared by every emitter in the door. */
void sr_out_put(const void *buf, size_t len);
int  sr_io_out_flush(void);
/* Bytes staged for the client but not yet written. The audio module's ONLY
 * congestion signal, and it samples it at chunk boundaries -- never as an
 * instantaneous "is the socket busy" test, which is the check SyncDOOM tried
 * for SFX and reverted (syncdoom/i_termsound.c). */
size_t sr_io_out_backlog(void);
int  sr_io_get_fd(void);                            /* the adopted fd (socket, or 1) */

/* Probe-reply setters, fed by syncretro_input.c's CSI handler: the ESC[14t
 * pixel canvas and the ESC[6n -> ESC[r;cR text grid. Each recomputes the image
 * rect, so the present path is probe-driven rather than stuck on the default. */
void sr_io_set_canvas(int w, int h);
void sr_io_set_grid(int rows, int cols);

/* DSR-ACK frame pacing. sr_io_take_grid_probe() is a one-shot armed when the
 * startup grid probe is SENT, so the first ESC[r;cR after it is the grid reply
 * and every later one acks an in-flight frame (-> sr_io_pace_ack()). */
int  sr_io_take_grid_probe(void);
void sr_io_pace_ack(void);

/* Force the next sr_io_present() to repaint in full: it bypasses the identical-
 * frame de-dupe and re-emits the sixel palette. Used after anything that wrote
 * over the game area (the pause and help screens) and after a core reset. */
void sr_io_invalidate(void);

/* --- syncretro_input.c: BBS socket decode -> cached RetroPad state ----------
 * sr_input_pump() is the retro_input_poll callback body: a NON-BLOCKING drain
 * of the socket, updating the cached pad + routing probe replies to the io
 * setters above. sr_pad_get() is the retro_input_state body. */
void    sr_input_pump(void);
/* The retro_input_state body. `index` selects the analog stick: the RIGHT stick
 * carries the Intellivision keypad digits (syncretro_keypad.c). */
int16_t sr_pad_get(unsigned port, unsigned device, unsigned index, unsigned id);
/* One-shot: the pending SR_DOOR_* action (pause / help / reset), or SR_DOOR_NONE.
 * Polled by main.c's run loop, which owns the pause and help screens. */
int     sr_input_take_action(void);
/* Drop every held button and the held keypad digit. Called on pause entry and
 * on reset, so a held disc direction cannot survive them. */
void    sr_input_release_all(void);
/* While SUSPENDED (a door screen is up), game keys are swallowed: they must not
 * reach the pad, or a key pressed behind the pause screen would still be held
 * on resume. Door actions still fire, and any bound key is remembered by
 * sr_input_take_anykey() so "press any key to return" can mean it. */
void    sr_input_set_suspended(int on);
int     sr_input_take_anykey(void);   /* one-shot: a bound key arrived */
/* Did the terminal identify itself as SyncTERM? Until a probe reply lands this
 * is 0, i.e. the SAFE assumption (re-send the sixel palette every frame). */
int     sr_input_is_syncterm(void);
/* The player pressed the quit key (Ctrl-Q). Polled via sr_door_should_exit(). */
int     sr_input_quit_requested(void);
/* Undo whatever key mode was negotiated (kitty flags / SyncTERM physical key
 * reports), so the BBS gets its terminal back as it lent it. Staged through the
 * out-buffer; called from sr_io_leave(). */
void    sr_input_restore_keys(void);

/* --- retro_bridge.c: install the four core callbacks on a core -------------
 * Wires video/audio/input onto sr_io_present()/sr_audio_feed()/sr_input_*.
 * Call after rc_core_open(), before retro_init (rc_core_load_game does this). */
struct rc_core;                                     /* fwd (retro_core.h) */
void sr_bridge_install(struct rc_core *c);
/* Set the libretro pixel format (from retro_env.c's SET_PIXEL_FORMAT). */
void sr_bridge_set_pixfmt(int retro_pixel_format);
/* The core's PCM stream: syncretro_audio.c owns it (M4). Declared there. */

/* --- retro_env.c -----------------------------------------------------------
 * The retro_environment_t callback. Passed to the core via sr_bridge_install(). */
/* (declared with libretro types in retro_env.c; not needed cross-module) */

#endif /* SYNCRETRO_H_ */
