/**
 * Type declarations for the Synchronet BBS JavaScript runtime
 * (SpiderMonkey 1.8.5, Synchronet 3.21) — the subset future_edit uses.
 *
 * Verified against:
 *   - /sbbs/repo/src/sbbs3/js_*.c / js_console.cpp (native implementations)
 *   - /sbbs/repo/exec/load/sbbsdefs.js, key_defs.js (runtime constants)
 *
 * Application code must not touch these globals outside src/host/ adapters —
 * that layer exists so everything above it can be unit-tested with fakes.
 */

// ---------------------------------------------------------------------------
// Global functions (js_global.c)
// ---------------------------------------------------------------------------

declare function load(path: string, ...args: unknown[]): unknown;
declare function load(scope: object, path: string, ...args: unknown[]): unknown;
declare function format(fmt: string, ...args: unknown[]): string;
declare function exit(code?: number): never;
declare function mswait(ms?: number): void;
declare function log(level: number, msg: string): void;

declare function directory(pattern: string, flags?: number): string[];
declare function file_exists(path: string): boolean;
declare function file_date(path: string): number;
declare function file_remove(path: string): boolean;
/** Case-insensitive filename lookup; undefined when no match exists. */
declare function file_getcase(path: string): string | undefined;
declare function backslash(path: string): string;

declare function ascii(value: number): string;
declare function ascii(value: string): number;
declare function strip_ctrl(text: string): string;
declare function strip_ctrl_a(text: string): string;
declare function strip_ansi(text: string): string;
declare function truncsp(text: string): string;
declare function word_wrap(text: string, lineLength?: number, origLineLength?: number, handleQuotes?: boolean): string;
/** Convert CP437 text (or a single CP437 char value) to UTF-8 bytes. */
declare function utf8_encode(text: string | number): string;
declare function utf8_decode(text: string): string;
declare function str_is_utf8(text: string): boolean;

/** Command-line arguments passed to the script (e.g. the %f message path). */
declare var argv: string[];
declare var argc: number;

// ---------------------------------------------------------------------------
// js object (js_internal.c)
// ---------------------------------------------------------------------------

interface JsObject {
  readonly exec_dir: string;
  readonly startup_dir: string;
  terminated: boolean;
  auto_terminate: boolean;
  time_limit: number;
  on_exit(expression: string): void;
}
declare var js: JsObject;

// ---------------------------------------------------------------------------
// console object (js_console.cpp) — terminal server only (absent under jsexec)
// ---------------------------------------------------------------------------

interface SbbsConsole {
  screen_rows: number;
  screen_columns: number;
  /** Current output attribute (IBM CGA bits). Assignable. */
  attributes: number;
  line_counter: number;
  aborted: boolean;
  timeout: number;
  ctrlkey_passthru: number | string;
  /** Console behavior flags (CON_* from sbbsdefs.js). */
  status: number;
  mouse_mode: number | boolean;

  clear(attribute?: number, autopause?: boolean): void;
  home(): void;
  gotoxy(x: number, y: number): void;
  getxy(): { x: number; y: number };
  /** Raw write, no character translation. */
  write(text: string): void;
  print(text: string, mode?: number): void;
  putbyte(byteValue: number): void;
  crlf(count?: number): void;
  /** ANSI sequence for the given attribute (or current if omitted). */
  ansi(attribute?: number, currentAttribute?: number): string;

  getkey(mode?: number): string;
  /** Non-blocking (or timed) single-key read; '' when no input. */
  inkey(mode?: number, timeout?: number): string;
  getstr(text: string, maxlen?: number, mode?: number): string;
  ungetstr(text: string): void;
  /** Actively query the remote ANSI terminal and update screen dimensions. */
  getdimensions?(): void;
  /** Active CPR dimension probe (cursor save/restore + ESC[6n); updates
   * screen_columns/rows server-side. Returns true when answered. */
  ansi_getdims?(): boolean;

  add_hotspot(cmd: string, hungry?: boolean, min_x?: number, max_x?: number, y?: number): void;
  clear_hotspots(): void;

  term_supports(flags?: number): boolean | number;
}
declare var console: SbbsConsole;

// ---------------------------------------------------------------------------
// bbs object (js_bbs.cpp) — terminal server only
// ---------------------------------------------------------------------------

interface BbsObject {
  mods: Record<string, unknown>;
  sys_status: number;
  readonly online: number;
  log_str(text: string): void;
}
declare var bbs: BbsObject;

// ---------------------------------------------------------------------------
// user object (js_user.c)
// ---------------------------------------------------------------------------

interface UserObject {
  readonly number: number;
  alias: string;
  name: string;
  screen_rows: number;
  screen_columns: number;
}
declare var user: UserObject;

// ---------------------------------------------------------------------------
// system object (js_system.c)
// ---------------------------------------------------------------------------

interface SystemObject {
  readonly name: string;
  readonly node_dir: string;
  readonly temp_dir: string;
  readonly ctrl_dir: string;
}
declare var system: SystemObject;

// ---------------------------------------------------------------------------
// File class (js_file.c)
// ---------------------------------------------------------------------------

declare class File {
  constructor(path: string);
  readonly name: string;
  readonly exists: boolean;
  readonly is_open: boolean;
  readonly length: number;
  readonly date: number;
  readonly error: number;
  position: number;

  open(mode?: string, shareable?: boolean, bufferLength?: number): boolean;
  close(): void;
  read(maxlen?: number): string | null;
  readln(maxlen?: number): string | null;
  readAll(maxlen?: number): string[];
  write(text: string, len?: number): boolean;
  writeln(text?: string): boolean;
}

// ---------------------------------------------------------------------------
// Runtime constants — defined by load('sbbsdefs.js') at startup.
// ---------------------------------------------------------------------------

declare var K_UPPER: number;
declare var K_NONE: number;
declare var K_NOSPIN: number;
declare var P_UTF8: number;
declare var USER_UTF8: number;
declare var CON_MOUSE_CLK_PASSTHRU: number;
declare var CON_MOUSE_REL_PASSTHRU: number;
declare var CON_MOUSE_SCROLL: number;
declare var SS_PAUSEON: number;
declare var SS_PAUSEOFF: number;
declare var LOG_INFO: number;
declare var LOG_ERR: number;
declare var LOG_DEBUG: number;
