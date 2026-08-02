/**
 * Normalized input events. Synchronet already translates cursor keys into
 * KEY_* control chars (key_defs.js); what it does NOT translate arrives as
 * raw ESC sequences: mouse reports (X10 and SGR, passed through because of
 * CON_MOUSE_*_PASSTHRU) and function keys. This parser is a superset of
 * exec/load/mouse_getkey.js: same two mouse grammars, plus F1-F12.
 *
 * Wheel events arrive as SGR button 64/65 and are normalized to wheel +/-1.
 */

import { InputEvent, MouseEvent } from './types';

/** ms to wait for ESC-sequence continuation bytes. */
var ESC_CONT_TIMEOUT = 60;

/** Set true (via jsexec/debug patching or a temporary edit) to trace every
 * parsed mouse event to the node debug log — for diagnosing terminal mouse
 * encoding quirks (coordinates, spurious wheel/motion). */
export var MOUSE_TRACE = false;

function mouseEvent(button: number, x: number, y: number, press: boolean, motion: boolean): MouseEvent {
  var wheel = 0;
  if (button === 64) wheel = -1;
  else if (button === 65) wheel = 1;
  var ev: MouseEvent = {
    type: 'mouse',
    x: x,
    y: y,
    button: wheel !== 0 ? 0 : (button & 0x03),
    press: wheel !== 0 ? false : press,
    release: wheel !== 0 ? false : !press,
    motion: motion,
    wheel: wheel
  };
  if (MOUSE_TRACE) {
    debugLog('mouse raw=' + button + ' x=' + x + ' y=' + y +
      (ev.press ? ' press' : '') + (ev.release ? ' release' : '') +
      (ev.motion ? ' motion' : '') + (ev.wheel !== 0 ? ' wheel=' + ev.wheel : ''));
  }
  return ev;
}

var FKEY_TILDE: { [num: string]: string } = {
  '11': 'F1', '12': 'F2', '13': 'F3', '14': 'F4', '15': 'F5',
  '17': 'F6', '18': 'F7', '19': 'F8', '20': 'F9', '21': 'F10',
  '23': 'F11', '24': 'F12'
};

var FKEY_SS3: { [ch: string]: string } = {
  // Standard SS3 function keys.
  P: 'F1', Q: 'F2', R: 'F3', S: 'F4',
  // VT100+/HP extension (PuTTY "VT100+" mode and several BBS clients
  // continue the run past F4): SS3 T..Z for F5-F11.
  T: 'F5', U: 'F6', V: 'F7', W: 'F8', X: 'F9', Y: 'F10', Z: 'F11'
};

/**
 * Best-effort debug logging of input sequences we could not decode, so a
 * misbehaving terminal's F-keys can be identified from the node log
 * (sbbs LOG_DEBUG) instead of guessed at. No-op outside Synchronet.
 */
function debugLog(msg: string): void {
  try {
    log(LOG_DEBUG, 'future_edit input: ' + msg);
  } catch (e) {
    /* headless/test host: no logger */
  }
}

/**
 * Synchronet's KEY_* control chars (key_defs.js). Synchronet normally
 * translates cursor/edit keys into these before the app sees them, but some
 * terminal paths pass the raw ANSI sequence through — notably the Delete key
 * (CSI 3~) — so we decode them here too. Values must match key_defs.js so the
 * controller's KEY_* comparisons still fire.
 */
var KEY_UP = '\x1e';
var KEY_DOWN = '\x0a';
var KEY_RIGHT = '\x06';
var KEY_LEFT = '\x1d';
var KEY_HOME = '\x02';
var KEY_END = '\x05';
var KEY_INSERT = '\x16';
var KEY_DEL = '\x7f';
var KEY_PAGEUP = '\x10';
var KEY_PAGEDN = '\x0e';

/** CSI `<n>~` edit/navigation keys. */
var NAV_TILDE: { [num: string]: string } = {
  '1': KEY_HOME, '2': KEY_INSERT, '3': KEY_DEL, '4': KEY_END,
  '5': KEY_PAGEUP, '6': KEY_PAGEDN, '7': KEY_HOME, '8': KEY_END
};

/** CSI cursor keys with a single letter terminator (no `~`). */
var NAV_LETTER: { [ch: string]: string } = {
  A: KEY_UP, B: KEY_DOWN, C: KEY_RIGHT, D: KEY_LEFT, H: KEY_HOME, F: KEY_END
};

/**
 * Read one normalized event. Returns {type:'none'} on timeout.
 * `timeoutMs` bounds the wait for the FIRST byte only.
 */
export function readInput(timeoutMs: number): InputEvent {
  var key = console.inkey(K_NONE, timeoutMs);
  if (key === '' || key === null || key === undefined) return { type: 'none' };
  if (key !== '\x1b') return { type: 'key', key: key };

  // ESC: gather a possible sequence; a lone ESC is the ESC key.
  var seq = '';
  var next = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
  if (next === '' || next === undefined || next === null) return { type: 'key', key: '\x1b' };

  if (next === 'O') {
    var fin = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    var name = FKEY_SS3[fin];
    if (name !== undefined) return { type: 'key', key: name };
    if (fin === '' || fin === undefined || fin === null) return { type: 'key', key: '\x1b' };
    // Unknown SS3: swallow it. Returning ESC here would fire the Esc action
    // (the menu) on every function key the map above misses — the classic
    // "F-key opens the menu and types a stray letter" failure.
    debugLog('unknown SS3 key: ESC O ' + fin);
    return { type: 'none' };
  }

  if (next !== '[') {
    console.ungetstr(next);
    return { type: 'key', key: '\x1b' };
  }

  // CSI sequence
  var c = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
  if (c === 'M') {
    // X10 mouse: CSI M b x y  (all offset by 32)
    var b = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    var xc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    var yc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    if (b === '' || xc === '' || yc === '') return { type: 'none' };
    var bv = ascii(b) - 32;
    var x = ascii(xc) - 32;
    var y = ascii(yc) - 32;
    var motion = (bv & 0x20) !== 0;
    var btn = bv & 0xc3;
    if (btn === 3) return mouseEvent(0, x, y, false, motion);
    return mouseEvent(btn, x, y, true, motion);
  }

  seq = c;
  // Collect until a terminator (letter or ~)
  var guard = 0;
  while (guard++ < 24) {
    var last = seq.charAt(seq.length - 1);
    if (seq.length > 0 && (last === '~' || (last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z'))) break;
    var nc = console.inkey(K_NONE, ESC_CONT_TIMEOUT);
    if (nc === '' || nc === undefined || nc === null) break;
    seq += nc;
  }

  var ev = classifyCsi(seq);
  if (ev.type === 'none' && seq.length > 0) debugLog('unknown CSI sequence: ESC [ ' + seq);
  return ev;
}

/**
 * Classify a collected CSI body (everything after `ESC [`) into a normalized
 * event: SGR mouse report, an edit/navigation key, a function key, or `none`
 * for terminal chatter we deliberately drop. Pure — unit-tested directly.
 */
export function classifyCsi(seq: string): InputEvent {
  // SGR mouse: CSI < b ; x ; y M|m
  var m = seq.match(/^<([0-9]+);([0-9]+);([0-9]+)([Mm])$/);
  if (m !== null) {
    var sb = parseInt(m[1] as string, 10);
    var sx = parseInt(m[2] as string, 10);
    var sy = parseInt(m[3] as string, 10);
    var sMotion = (sb & 0x20) !== 0;
    var sBtn = sb & 0xc3;
    return mouseEvent(sBtn, sx, sy, m[4] === 'M', sMotion);
  }

  // CSI <n>~ : edit/navigation keys (Delete, Insert, Home/End, PgUp/PgDn)
  // first, then function keys. Delete (3~) is the common leak-through.
  var t = seq.match(/^([0-9]+)~$/);
  if (t !== null) {
    var nav = NAV_TILDE[t[1] as string];
    if (nav !== undefined) return { type: 'key', key: nav };
    var fname = FKEY_TILDE[t[1] as string];
    if (fname !== undefined) return { type: 'key', key: fname };
  }

  // CSI <letter> : cursor keys and letter-form Home/End.
  if (seq.length === 1) {
    var letter = NAV_LETTER[seq];
    if (letter !== undefined) return { type: 'key', key: letter };
    // CSI Z: back-tab (Shift+Tab)
    if (seq === 'Z') return { type: 'key', key: 'STAB' };
  }

  // Modified printable keys, two grammars: CSI-u / fixterms (CSI code;mods u)
  // and xterm modifyOtherKeys (CSI 27;mods;code ~). Only Ctrl+, Ctrl+. and
  // Ctrl+/ matter to us — the Moebius/PabloDraw character-set keys, which
  // plain terminals cannot transmit at all; everything else stays dropped.
  var mk = seq.match(/^([0-9]+);([0-9]+)u$/);
  var xk = seq.match(/^27;([0-9]+);([0-9]+)~$/);
  var mCode = -1;
  var mMods = 0;
  if (mk !== null) {
    mCode = parseInt(mk[1] as string, 10);
    mMods = parseInt(mk[2] as string, 10);
  } else if (xk !== null) {
    mCode = parseInt(xk[2] as string, 10);
    mMods = parseInt(xk[1] as string, 10);
  }
  if (mCode >= 0 && ((mMods - 1) & 4) !== 0) { // Ctrl held
    if (mCode === 44) return { type: 'key', key: 'C-,' };
    if (mCode === 46) return { type: 'key', key: 'C-.' };
    if (mCode === 47) return { type: 'key', key: 'C-/' };
  }

  // Unrecognized CSI (terminal chatter): swallow it rather than leaking
  // garbage keystrokes into the document.
  return { type: 'none' };
}
