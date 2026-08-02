/**
 * Synchronet key translations (exec/load/key_defs.js): the terminal server
 * converts ANSI cursor sequences into these control characters before they
 * reach console.inkey().
 */

export var KEY_UP = '\x1e';
export var KEY_DOWN = '\x0a';
export var KEY_RIGHT = '\x06';
export var KEY_LEFT = '\x1d';
export var KEY_HOME = '\x02';
export var KEY_END = '\x05';
export var KEY_INSERT = '\x16';
export var KEY_DEL = '\x7f';
export var KEY_PAGEUP = '\x10';
export var KEY_PAGEDN = '\x0e';

export var KEY_ENTER = '\r';
export var KEY_ESC = '\x1b';
export var KEY_BACKSPACE = '\x08';
export var KEY_TAB = '\x09';

export var CTRL_A = '\x01';
export var CTRL_C = '\x03';
export var CTRL_D = '\x04';
export var CTRL_G = '\x07';
export var CTRL_K = '\x0b';
export var CTRL_L = '\x0c';
export var CTRL_O = '\x0f';
export var CTRL_Q = '\x11';
export var CTRL_R = '\x12';
export var CTRL_S = '\x13';
export var CTRL_T = '\x14';
export var CTRL_U = '\x15';
export var CTRL_W = '\x17';
export var CTRL_X = '\x18';
export var CTRL_Y = '\x19';
export var CTRL_Z = '\x1a';

/** True for a printable CP437-typable character (space..~ plus 128-255). */
export function isPrintable(key: string): boolean {
  if (key.length !== 1) return false;
  var c = key.charCodeAt(0);
  return (c >= 0x20 && c < 0x7f) || (c >= 0x80 && c <= 0xff);
}
