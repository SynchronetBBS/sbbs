/* syncmoo1.h -- cross-module contract for the syncmoo1 door.
 *
 * DESIGN.md §4: a SINGLE header shared by every syncmoo1_*.c module (plus
 * hw_sbbs.c, the 1oom `hw` backend), so a function that crosses a module
 * boundary has ONE declaration and a provenance comment saying which .c
 * provides it and why -- unlike syncduke/syncconquer's per-file private
 * headers. As later tasks add syncmoo1_input.c/_door.c/_config.c/_node.c,
 * their cross-module entry points get added here too; Task 5 (this file)
 * covers the terminal I/O module only.
 *
 * sm_geom_t (the image-rect type shared between the present path and the
 * input module's mouse mapper) is already defined in syncmoo1_map.h (Task 4)
 * -- pulled in below so every consumer of this header gets it too, instead
 * of a second competing definition here.
 */
#ifndef SYNCMOO1_H
#define SYNCMOO1_H

#include <stddef.h>
#include <stdint.h>

#include "syncmoo1_map.h"   /* sm_geom_t */

/* --- syncmoo1_io.c: terminal out-buffer, enter/probe/leave, sixel present --
 *
 * Consumed by hw_sbbs.c (the 1oom hw backend): hw_video_draw_buf() calls
 * sm_io_present() on every flip, hw_video_set_palette() feeds it a running
 * 768-byte RGB888 palette. Task 6's input module reads sm_io_geom() for the
 * SGR-mouse -> game-pixel mapping.
 */

/* One-time setup: adopt `sockfd` as the door's I/O descriptor (<0 falls back
 * to stdout, fd 1 -- dev/tty use), set it non-blocking, ignore SIGPIPE,
 * resolve the SYNCMOO1_SIXELOUT capture-mode override (see syncmoo1_io.c),
 * and register sm_io_leave() via atexit(). Idempotent -- safe to call more
 * than once (a later call is a no-op). Returns 0. */
int sm_io_init(int sockfd);

/* Encode `idx320x200` (320x200 8-bit palette indices, row-major) through
 * `pal768` (256 RGB888 triples) as a DECSIXEL frame centered in the terminal
 * canvas (a sane 640x400/80x25 default until a later task's probe-reply
 * parser narrows it), stage it, and flush it to the I/O descriptor. Lazily
 * runs sm_io_enter() on the first call, so a caller need not sequence that by
 * hand. The 768-byte palette is (re)defined in the sixel registers only when
 * it actually changed since the last frame (memcmp) -- SyncTERM garbles its
 * decoder if the registers are redefined every frame. M1 (sixel-tier only,
 * DESIGN.md §10): no whole-frame de-dupe and no DSR-ACK pacing -- those are a
 * later task (§5, §9); this is a straight encode+emit every call. */
void sm_io_present(const uint8_t *idx320x200, const uint8_t *pal768);

/* Terminal setup / teardown (DESIGN.md §9). Both idempotent (each runs its
 * work exactly once, however many times it's called): sm_io_present() calls
 * sm_io_enter() lazily on its first invocation; sm_io_init() registers
 * sm_io_leave() via atexit() so a normal exit restores the BBS's terminal
 * (sixel-scroll/autowrap/cursor, mouse tracking off) even if nothing else
 * calls it explicitly. */
void sm_io_enter(void);
void sm_io_leave(void);

/* Append `len` bytes to the staged, grow-only out-buffer (no I/O). */
void sm_out_put(const void *buf, size_t len);

/* Non-blocking drain of the staged out-buffer to the I/O descriptor (or, in
 * SYNCMOO1_SIXELOUT capture mode, a truncate-write of the capture file).
 * EAGAIN/EWOULDBLOCK/EINTR just leave the remainder pending for the next
 * call (a slow client, not an error); a real write error hangs up the
 * session. Returns 0. */
int sm_io_out_flush(void);

/* The image rect the last sm_io_present() drew (or the sane 640x400/80x25
 * default before any frame has been drawn) -- Task 6's sm_map_mouse() reads
 * this so a click maps against the SAME geometry the frame was drawn in.
 * Never NULL; ew/eh are always > 0. */
const sm_geom_t *sm_io_geom(void);

/* The I/O descriptor sm_io_init() adopted (the door socket, or fd 1 in
 * dev/tty use). Task 6's hw_event_handle() wiring reads this to know what to
 * hand sm_input_pump(). */
int sm_io_get_fd(void);

/* Probe-reply setters (Task 6, DESIGN.md Sec9): syncmoo1_input.c's CSI
 * handler calls these as the startup probe replies land (ESC[14t canvas px,
 * the ESC[6n->ESC[r;cR grid, DECRPM ?1016 SGR-Pixels confirmation), so
 * sm_io_geom()'s image rect + real cell size -- and therefore sm_map_mouse()
 * -- become probe-driven instead of the 640x400/80x25/8x16 default. Each
 * recomputes the image rect immediately; a malformed/zero w/h/rows/cols is
 * ignored (keeps the prior value) rather than corrupting the geometry. */
void sm_io_set_canvas(int w, int h);
void sm_io_set_grid(int rows, int cols);
void sm_io_set_pixel_mode(int on);

/* --- syncmoo1_input.c: socket read loop, ESC/CSI/APC state machine, key +
 * mouse decode, capability-probe reply parsing --
 *
 * Consumed by hw_sbbs.c: hw_event_handle() calls sm_input_pump(sm_io_get_fd())
 * on every 1oom engine poll. Drains `sockfd` non-blocking, runs the bytes
 * through the ESC/CSI/APC(DCS/OSC/PM) state machine, and injects the result
 * into 1oom's global input state: keys via kbd_add_keypress() (kbd.h), mouse
 * position/buttons/wheel via mouse_set_xy_from_hw()/mouse_set_buttons_from_hw()/
 * mouse_set_scroll_from_hw() (mouse.h). Capability-probe CSI replies (ESC[14t,
 * the grid ESC[r;cR, DECRPM ?1016 y, DA1/CTDA c, the JXL-cap n) are parsed
 * here and fed into sm_io via the setters above rather than delivered as
 * keys; APC/DCS/OSC/PM string replies (e.g. SyncTERM's C;L cache-list) are
 * swallowed to their ST terminator so they never leak stray keystrokes.
 * Returns 0 normally, <0 if the peer hung up or a real socket read error
 * occurred (the caller should treat the door session as over). */
int sm_input_pump(int sockfd);

/* --- syncmoo1_door.c: DOOR32.SYS / -s<fd> dropfile, socket resolution,
 * splash, argv sanitize, hangup (DESIGN.md §8) --
 *
 * Consumed by hw_sbbs.c's main(): sm_door_setup() resolves the client comm
 * descriptor and paints an instant splash BEFORE 1oom's own (slow, LBX-
 * scanning) init runs, so sm_io_init() gets the REAL socket fd before the
 * first present -- fixing the Task 5/6 "stdout fallback" carry-over (sm_io_
 * init() previously only ever saw -1, since nothing resolved the door
 * socket yet). hw_video_draw_buf() calls sm_door_check_time() every present
 * tick to enforce the DOOR32 minutes-left session limit.
 */

/* One-time setup, called FIRST in main() (before sm_io_init()/main_1oom()):
 * parses argv for -s<fd> / -t<seconds> / -name <alias> / a bare *door32.sys
 * path (DOOR32.SYS: line 1 comm type, line 2 socket handle, line 7 alias,
 * line 9 minutes-left), falling back to the SYNCMOO1_SOCK env var for a dev
 * run with no DOOR32.SYS, then configures the resolved socket (non-blocking,
 * TCP_NODELAY, SIGPIPE ignored) and paints an instant splash to it (or fd 1
 * in dev/tty use, when no socket resolves) -- before 1oom's slow LBX-
 * scanning init runs, so the user sees something immediately. Returns 0
 * normally; nonzero (-help/--help/-?) means main() should return without
 * starting the engine. */
int sm_door_setup(int argc, char **argv);

/* The resolved client comm descriptor (the DOOR32.SYS/-s<fd> socket), or -1
 * if none resolved (dev/tty use) -- sm_io_init()'s argument in main(). */
int sm_door_socket(void);

/* The user alias from DOOR32.SYS line 7 / -name, or "" if none given. */
const char *sm_door_alias(void);

/* The -home <dir> value (per-user config/save sandbox), or NULL if none was
 * given -- unlike sm_door_alias() above, NULL (not "") on absence, so
 * syncmoo1_config.c's sm_config_apply() can test it directly. Captured by
 * sm_door_resolve() in the SAME pass that resolves -s<fd>/-name (DESIGN.md
 * §8): sm_door_sanitize_argv() below only strips -home from argv, it does
 * not save the value, so this getter is the only way the value survives
 * past that strip. */
const char *sm_door_home(void);

/* The DOOR32.SYS/-t<seconds> session time limit in milliseconds, or 0 if
 * none was given (no limit enforced). */
uint32_t sm_door_time_limit_ms(void);

/* Checked once per present tick (hw_sbbs.c's hw_video_draw_buf()): if the
 * DOOR32 session time limit has elapsed since sm_door_setup(), logs and
 * exits cleanly (exit(), not sm_door_hangup() -- the socket is presumably
 * still live, so the atexit-registered sm_io_leave() restores the BBS
 * terminal same as any other clean quit). A no-op when no time limit was
 * given. */
void sm_door_check_time(void);

/* Strip the door's own arguments (-s<digits>, -t<digits>, -name <alias>,
 * -home <dir>, a bare *door32.sys path) from argv before 1oom's own
 * options_parse() (main.c) sees it -- an unrecognized "-" argument makes
 * options_parse() log an error and main_1oom() return early (options.c's
 * options_parse_do()), so leaving these in would abort the door before the
 * engine even starts. The -s/-t match is DIGIT-SUFFIX-ONLY, so 1oom's own
 * -sfx/-skipintro/-savequit/etc. reach the engine untouched. -home's VALUE is
 * captured separately by sm_door_resolve() (read back via sm_door_home()
 * above) before this function strips the flag+value pair from argv --
 * syncmoo1_config.c's sm_config_apply() (DESIGN.md §8) reads it from there,
 * not from argv, since by the time sm_config_apply() runs (after this strip,
 * per hw_sbbs.c's main() call order) argv no longer carries it. Compacts in
 * place, keeps argv[0], lowers *argc, and leaves any genuine 1oom option
 * untouched. Call after sm_door_setup(), before main_1oom(). */
void sm_door_sanitize_argv(int *argc, char **argv);

/* The single canonical hangup path: client socket dead (read/write error or
 * EOF). Wired to both the read side (hw_sbbs.c's hw_event_handle) and the
 * write side (syncmoo1_io.c's flush), replacing the two earlier ad-hoc
 * bare-_exit paths. Runs sm_io_leave() (bounded drain + BBS terminal-mode
 * restore) first, then _exit(0) to skip the engine's other atexit handlers
 * (which could block on the dead socket). `why` is logged to stderr (-> BBS
 * log); may be NULL. */
void sm_door_hangup(const char *why);

/* --- syncmoo1_config.c: per-user -home sandbox + shared LBX data-path
 * resolution (DESIGN.md §8 per-user sandbox, §15 assets) --
 *
 * Consumed by hw_sbbs.c's main(), called AFTER sm_door_sanitize_argv() (so
 * -home has already been captured by sm_door_resolve()/sm_door_home() above,
 * since sanitize strips it from argv) and BEFORE sm_io_init()/main_1oom() (so
 * the chdir below happens before main_1oom() ever reaches lbxfile_find_dir(),
 * 1oom/src/main.c).
 */

/* One-time setup:
 *   1. If the SYNCMOO1_LBX env var names a shared, read-only MoO1 LBX data
 *      dir, realpath() it to an ABSOLUTE path and hand it to 1oom via
 *      os_set_path_data() (os.h) -- exactly what 1oom's own "-data <dir>"
 *      option (options.c's options_set_datadir()) and lbxfile_find_dir()
 *      itself (lbx.c, once it finds fonts.lbx) do, so lbxfile_find_dir()
 *      locates the data as its first candidate regardless of the chdir in
 *      step 2. Absolutized (not left relative) specifically so that later
 *      chdir can't break it. Unset/empty SYNCMOO1_LBX -- or a path realpath()
 *      can't resolve -- leaves 1oom's own os_get_paths_data() search (cwd,
 *      XDG dirs, /usr/share/1oom, etc.) completely untouched; this is
 *      intentionally minimal, not a reimplementation of that search.
 *   2. If sm_door_home() (above) is non-NULL: mkpath() it (dirwrap.h;
 *      creates any missing path components), realpath()-absolutize it, and
 *      hand it to 1oom via os_set_path_user() (os.h) -- which is what 1oom
 *      prefixes onto its config + save file paths (cfg.c's cfg_cfgname(),
 *      game/game_save.c's slot/year fname builders), an ABSOLUTE
 *      $XDG_CONFIG_HOME/$HOME path, NOT a cwd-relative one. So this, not a
 *      chdir, is what actually isolates each player's saves/config; a chdir()
 *      into the dir is also done (harmless, contains any stray relative I/O)
 *      but is not the isolation mechanism. os_set_path_user() stores into the
 *      same var os_get_path_user() lazily caches, and this runs before
 *      main_1oom() ever calls os_get_path_user(), so it wins. No -home =>
 *      user_path is left unset, so 1oom keeps its own default XDG/HOME
 *      resolution (the pre-Task-8 single-shared-location behavior).
 * Step 1 runs before step 2 (see the absolutize note) so the data-path
 * resolution can never be affected by the sandbox chdir, regardless of
 * whether SYNCMOO1_LBX was given as a relative path.
 *
 * Returns 0 on success (including the no-op case: neither -home nor
 * SYNCMOO1_LBX given). A failed mkpath()/chdir() into -home is logged to
 * stderr and returns -1, but is not itself treated as fatal by main() --
 * matching this door's general "degrades, doesn't abort" posture elsewhere
 * (e.g. sm_door_configure_socket()'s best-effort setsockopt/fcntl calls). */
int sm_config_apply(void);

#endif /* SYNCMOO1_H */
