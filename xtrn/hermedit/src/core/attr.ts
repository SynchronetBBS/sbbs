/**
 * Textmode attribute model: one IBM CGA attribute byte per cell.
 *   bits 0-2  foreground color
 *   bit  3    high intensity (bright foreground)
 *   bits 4-6  background color
 *   bit  7    blink
 *
 * Two encoders live here, one per boundary (see SYNCHRONET_CONTRACT.md):
 *   - ctrlATransition(): Synchronet Ctrl-A codes for the POSTED MESSAGE BODY.
 *     Port of fseditor.js make_strings() so the host accepts every sequence.
 *   - ansiFromAttr(): ANSI SGR for the LIVE TERMINAL only. Never emitted into
 *     a message body.
 */

export var BLACK = 0;
export var BLUE = 1;
export var GREEN = 2;
export var CYAN = 3;
export var RED = 4;
export var MAGENTA = 5;
export var BROWN = 6;
export var LIGHTGRAY = 7;

export var HIGH = 0x08;
export var BLINK = 0x80;

export var DEFAULT_ATTR = LIGHTGRAY;

export function fgOf(attr: number): number {
  return attr & 0x07;
}

export function bgOf(attr: number): number {
  return (attr >> 4) & 0x07;
}

export function isHigh(attr: number): boolean {
  return (attr & HIGH) !== 0;
}

export function isBlink(attr: number): boolean {
  return (attr & BLINK) !== 0;
}

export function makeAttr(fg: number, bg: number, high?: boolean, blink?: boolean): number {
  var a = (fg & 0x07) | ((bg & 0x07) << 4);
  if (high) a |= HIGH;
  if (blink) a |= BLINK;
  return a;
}

/** Ctrl-A foreground code letters indexed by CGA color 0-7. */
var CTRLA_FG = ['K', 'B', 'G', 'C', 'R', 'M', 'Y', 'N'];
/** Ctrl-A background code digits indexed by CGA color 0-7. */
var CTRLA_BG = ['0', '4', '2', '6', '1', '5', '3', '7'];

/**
 * The minimum Ctrl-A string that moves a message reader from `last` to
 * `next`. Mirrors fseditor.js make_strings(): HIGH and BLINK can only be
 * cleared with a full \1N reset, which resets the working attribute to 7.
 */
export function ctrlATransition(last: number, next: number): string {
  if (last === next) return '';
  var s = '';
  var cur = last;
  if ((!(next & BLINK) && (cur & BLINK)) || (!(next & HIGH) && (cur & HIGH))) {
    cur = LIGHTGRAY;
    s += '\x01N';
  }
  if ((next & BLINK) && !(cur & BLINK)) s += '\x01I';
  if ((next & HIGH) && !(cur & HIGH)) s += '\x01H';
  if ((next & 0x07) !== (cur & 0x07)) s += '\x01' + CTRLA_FG[next & 0x07];
  if ((next & 0x70) !== (cur & 0x70)) s += '\x01' + CTRLA_BG[(next >> 4) & 0x07];
  return s;
}

/**
 * Combine a cell's existing attribute with a brush attribute, applying only
 * the chosen channel(s) — for the recolor tool, which repaints color without
 * touching the glyph. `fg` keeps the background (bits 4-7: bg + blink) and
 * takes the foreground (bits 0-3: fg + bright); `bg` is the inverse; `both`
 * takes the whole brush attribute.
 */
export function applyColorChannel(existing: number, brush: number, channel: 'fg' | 'bg' | 'both'): number {
  if (channel === 'fg') return (existing & 0xf0) | (brush & 0x0f);
  if (channel === 'bg') return (existing & 0x0f) | (brush & 0xf0);
  return brush & 0xff;
}

/** CGA color 0-7 -> ANSI SGR color digit. */
var ANSI_COLOR = [0, 4, 2, 6, 1, 5, 3, 7];

/**
 * Full ANSI SGR sequence selecting `attr` from any prior state. Used by the
 * screen renderer for batched terminal writes; deliberately stateless (always
 * begins from a reset) so a diffed partial repaint can never inherit stale
 * bright/blink state.
 */
export function ansiFromAttr(attr: number): string {
  var parts = '0';
  if (attr & HIGH) parts += ';1';
  if (attr & BLINK) parts += ';5';
  parts += ';3' + ANSI_COLOR[attr & 0x07];
  parts += ';4' + ANSI_COLOR[(attr >> 4) & 0x07];
  return '\x1b[' + parts + 'm';
}
