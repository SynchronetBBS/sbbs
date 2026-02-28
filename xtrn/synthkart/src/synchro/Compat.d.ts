/**
 * Synchronet BBS Type Declarations
 *
 * These declarations describe the Synchronet JavaScript runtime environment.
 * This is NOT Node.js. This is NOT browser JavaScript.
 */

// ============================================================
// GLOBAL FUNCTIONS (available even without console)
// ============================================================

declare function print(text: string): void;
declare function exit(code?: number): void;
declare function load(filename: string): void;
declare function log(level: number, text: string): void;
declare function require(filename: string, symbol?: string): any;

// ============================================================
// KEY CONSTANTS (from key_defs.js)
// ============================================================

declare var KEY_UP: string;
declare var KEY_DOWN: string;
declare var KEY_LEFT: string;
declare var KEY_RIGHT: string;
declare var KEY_HOME: string;
declare var KEY_END: string;
declare var KEY_INSERT: string;
declare var KEY_DELETE: string;
declare var KEY_PAGEUP: string;
declare var KEY_PAGEDN: string;
declare var KEY_ESC: string;

// ============================================================
// SYNCHRONET CONSOLE
// ============================================================

interface SynchronetConsole {
  print(text: string): void;
  writeln(text: string): void;
  clear(attribute?: number | string, autopause?: boolean): void;
  gotoxy(x: number, y: number): void;
  screen_columns: number;
  screen_rows: number;
  attributes: number;
  inkey(mode?: number, timeout?: number): string;
  input_pending: boolean;
  beep(): void;
  home(): void;
  cleartoeol(): void;
  getstr(maxlen?: number, mode?: number): string;
  pause(): void;
  center(text: string): void;
  line_counter: number;
}

declare var console: SynchronetConsole;

// ============================================================
// INPUT MODE CONSTANTS
// ============================================================

declare var K_NONE: number;
declare var K_UPPER: number;
declare var K_UPRLWR: number;
declare var K_NUMBER: number;
declare var K_WRAP: number;
declare var K_MSG: number;
declare var K_SPIN: number;
declare var K_LINE: number;
declare var K_EDIT: number;
declare var K_CHAT: number;
declare var K_NOCRLF: number;
declare var K_ALPHA: number;
declare var K_GETSTR: number;
declare var K_LOWPRIO: number;
declare var K_NOEXASC: number;
declare var K_E71DETECT: number;
declare var K_AUTODEL: number;
declare var K_COLD: number;
declare var K_NOECHO: number;
declare var K_TAB: number;

// ============================================================
// COLOR CONSTANTS - Foreground
// ============================================================

declare var BLACK: number;
declare var BLUE: number;
declare var GREEN: number;
declare var CYAN: number;
declare var RED: number;
declare var MAGENTA: number;
declare var BROWN: number;
declare var LIGHTGRAY: number;
declare var DARKGRAY: number;
declare var LIGHTBLUE: number;
declare var LIGHTGREEN: number;
declare var LIGHTCYAN: number;
declare var LIGHTRED: number;
declare var LIGHTMAGENTA: number;
declare var YELLOW: number;
declare var WHITE: number;

// ============================================================
// COLOR CONSTANTS - Background
// ============================================================

declare var BG_BLACK: number;
declare var BG_BLUE: number;
declare var BG_GREEN: number;
declare var BG_CYAN: number;
declare var BG_RED: number;
declare var BG_MAGENTA: number;
declare var BG_BROWN: number;
declare var BG_LIGHTGRAY: number;

// ============================================================
// ATTRIBUTE CONSTANTS
// ============================================================

declare var HIGH: number;
declare var BLINK: number;

// ============================================================
// SYSTEM OBJECT
// ============================================================

interface SynchronetSystem {
  timer: number;
  node_num: number;
  nodes: number;
  name: string;
  operator: string;
  qwk_id: string;
}

declare var system: SynchronetSystem;

// ============================================================
// JS OBJECT
// ============================================================

interface SynchronetJS {
  exec_dir: string;
  startup_dir: string;
  terminated: boolean;
  global: any;
}

declare var js: SynchronetJS;

// ============================================================
// BBS OBJECT
// ============================================================

interface SynchronetBBS {
  sys_status: number;
  // Add more bbs properties as needed
}

declare var bbs: SynchronetBBS;

// ============================================================
// SYSTEM STATUS CONSTANTS (from sbbsdefs.js)
// ============================================================

declare var SS_PAUSEON: number;
declare var SS_PAUSEOFF: number;

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

declare function load(filename: string): void;
declare function mswait(ms: number): void;
declare function log(level: number, message: string): void;
declare function sleep(seconds: number): void;
declare function time(): number;
declare function random(max: number): number;

// ============================================================
// FILE CLASS
// ============================================================

declare class File {
  constructor(filename: string);
  open(mode: string, shareable?: boolean, bufsize?: number): boolean;
  close(): void;
  read(count?: number): string;
  readln(): string;
  readAll(): string[];
  write(data: string): boolean;
  writeln(data: string): boolean;
  eof: boolean;
  length: number;
  position: number;
  exists: boolean;
}

// ============================================================
// FRAME CLASS (from frame.js)
// ============================================================

declare class Frame {
  constructor(x: number, y: number, width: number, height: number, attr?: number, parent?: Frame);
  open(): void;
  close(): void;
  delete(): void;
  invalidate(): void;
  draw(): void;
  cycle(): boolean;
  load(filename: string): void;
  clear(attr?: number): void;
  scroll(x: number, y: number): void;
  scrollTo(x: number, y: number): void;
  move(x: number, y: number): void;
  moveTo(x: number, y: number): void;
  end(): void;
  home(): void;
  center(text: string): void;
  gotoxy(x: number, y: number): void;
  putmsg(text: string, attr?: number): void;
  print(text: string): void;
  top(): void;
  bottom(): void;
  refresh(): void;
  attr: number;
  x: number;
  y: number;
  width: number;
  height: number;
  data_width: number;
  data_height: number;
  transparent: boolean;
  v_scroll: boolean;
  h_scroll: boolean;
  scrollbars: boolean;
  checkbounds: boolean;
  setData(x: number, y: number, char: string, attr: number): void;
  getData(x: number, y: number): { ch: string; attr: number };
  clearData(x: number, y: number): void;
  is_open: boolean;
}
