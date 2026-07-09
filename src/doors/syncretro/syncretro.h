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
 * BEFORE the core is loaded. Values are read back through these accessors. */
int         sr_door_setup(int argc, char **argv);   /* 0 on success */
int         sr_door_socket(void);                   /* client socket fd, or -1 (dev: stdout) */
const char *sr_door_name(void);                     /* player alias, or NULL */
const char *sr_door_home(void);                     /* -home sandbox dir, or NULL */
const char *sr_door_core_path(void);                /* -core <path> / config, or NULL */
const char *sr_door_rom_path(void);                 /* game ROM path, or NULL */
int         sr_door_should_exit(void);              /* carrier drop / time limit / quit key */
/* Strip the door's own args so a core's arg parser (if any) never sees them. */
void        sr_door_sanitize_argv(int *argc, char **argv);

/* --- syncretro_config.c: per-user sandbox + core/system/save/ROM dirs ------
 * Creates the -home sandbox and resolves the BIOS (system) + save dirs handed
 * to the core via retro_env.c's GET_SYSTEM_DIRECTORY / GET_SAVE_DIRECTORY. Run
 * after sr_door_setup(), before rc_core_load_game(). Returns 0 (non-fatal
 * problems are logged, see DESIGN.md sec 10). */
int         sr_config_apply(void);
const char *sr_config_system_dir(void);             /* BIOS dir (shared, read-only) */
const char *sr_config_save_dir(void);               /* per-user SRAM/save-state dir */

/* --- syncretro_io.c: terminal enter/probe/leave + the present path ---------
 * sr_io_init() opens on the door socket (or stdout dev fallback) and runs the
 * enter+probe handshake; sr_io_present() composes one frame through termgfx
 * (fit/center -> active tier encode -> emit, AIMD-paced); sr_io_leave()
 * restores the BBS terminal. SYNCRETRO_SIXELOUT=<path> capture mode overwrites
 * that file with one self-contained frame per present (offline verification). */
int  sr_io_init(int sockfd);
void sr_io_present(const uint8_t *rgb, int w, int h);   /* rgb = w*h*3, top-down */
void sr_io_leave(void);
/* Fed inbound terminal bytes by syncretro_input.c to capture probe replies
 * (geometry, DA/caps). No-op-safe. */
void sr_io_feed_reply(const uint8_t *buf, int len);

/* --- syncretro_input.c: BBS socket decode -> cached RetroPad state ----------
 * sr_input_pump() is the retro_input_poll callback body: a NON-BLOCKING drain
 * of the socket, updating the cached pad + routing probe replies to
 * sr_io_feed_reply(). sr_pad_get() is the retro_input_state body. */
void    sr_input_pump(void);
int16_t sr_pad_get(unsigned port, unsigned device, unsigned id);

/* --- retro_bridge.c: install the four core callbacks on a core -------------
 * Wires video/audio/input onto sr_io_present()/sr_audio_feed()/sr_input_*.
 * Call after rc_core_open(), before retro_init (rc_core_load_game does this). */
struct rc_core;                                     /* fwd (retro_core.h) */
void sr_bridge_install(struct rc_core *c);
/* Set the libretro pixel format (from retro_env.c's SET_PIXEL_FORMAT). */
void sr_bridge_set_pixfmt(int retro_pixel_format);
/* M1 audio sink: accepts + discards the core's PCM stream (DESIGN.md sec 8). */
size_t sr_audio_feed(const int16_t *pcm, size_t frames);

/* --- retro_env.c -----------------------------------------------------------
 * The retro_environment_t callback. Passed to the core via sr_bridge_install(). */
/* (declared with libretro types in retro_env.c; not needed cross-module) */

#endif /* SYNCRETRO_H_ */
