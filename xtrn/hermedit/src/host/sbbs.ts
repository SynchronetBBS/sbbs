/**
 * SynchronetHostAdapter: the only file (besides input.ts) that touches
 * Synchronet globals. Implements the lifecycle recorded in
 * SYNCHRONET_CONTRACT.md:
 *
 *   read drop files -> edit -> on save: write body bytes + RESULT.ED, exit 0
 *                              on abort: leave body alone, exit non-zero
 *
 * Terminal state (mouse reporting, pause, ctrlkey passthrough) is restored on
 * every exit path; js.on_exit() strings are registered as a backstop for
 * exceptions and disconnects.
 */

import { SessionMeta, emptyMeta, parseEditorInf, parseMsgInf, buildResultEd } from '../core/dropfiles';
import { utf8ToCp437 } from '../core/cp437';
import { parseTdf, TdfFont } from '../core/tdf';
import { MessageSession, TerminalCaps, FontMeta, FontProvider } from './types';

export var EDITOR_IDENT = 'HERMedIT v0.1';

function readAllLines(path: string): string[] | null {
  var f = new File(path);
  if (!f.open('r')) return null;
  var lines = f.readAll();
  f.close();
  return lines;
}

function readRaw(path: string): string | null {
  var f = new File(path);
  if (!f.open('rb')) return null;
  var data = f.read();
  f.close();
  return data === null ? '' : data;
}

export function loadSession(): MessageSession {
  var bodyPath = argc > 0 ? String(argv[0]) : system.temp_dir + 'INPUT.MSG';
  var meta: SessionMeta = emptyMeta();

  // Prefer MSGINF (declares the charset); fall back to EDITOR.INF.
  var msginfPath = file_getcase(system.node_dir + 'msginf');
  if (msginfPath !== undefined) {
    var ml = readAllLines(msginfPath);
    if (ml !== null) meta = parseMsgInf(ml);
  } else {
    // FSEditor precedent: consume (and remove) editor.inf, newest first.
    var infPath;
    while ((infPath = file_getcase(system.node_dir + 'editor.inf')) !== undefined) {
      var il = readAllLines(infPath);
      if (il !== null) {
        var parsed = parseEditorInf(il);
        parsed.subject = strip_ctrl(parsed.subject);
        parsed.to = strip_ctrl(parsed.to);
        parsed.from = strip_ctrl(parsed.from);
        meta = parsed;
      }
      if (!file_remove(infPath)) break;
    }
  }

  var utf8 = sessionIsUtf8(meta);

  var quoteLines: string[] = [];
  var quotesPath = file_getcase(system.node_dir + 'quotes.txt');
  if (quotesPath !== undefined) {
    var q = readRaw(quotesPath);
    if (q !== null && q.length > 0) {
      if (utf8) q = utf8ToCp437(q).text;
      // Quote as PLAIN text: strip any raw ANSI escape sequences (an ANSI-art
      // post would otherwise dump escape codes into the reply) and any Ctrl-A
      // color codes. Quotes get their own `> ` styling on insert.
      q = strip_ansi(q);
      q = strip_ctrl_a(q);
      q = q.replace(/\r\n/g, '\n').replace(/\r/g, '\n');
      quoteLines = q.split('\n');
      while (quoteLines.length > 0 && quoteLines[quoteLines.length - 1] === '') quoteLines.pop();
    }
  }

  // Only pre-load the %f body when this is NOT a reply. When QUOTES.TXT exists
  // (a reply), Synchronet may seed %f with a quote header ("By: X to Y on ..");
  // that belongs to the quote, not the fresh body, so the body starts empty and
  // the quote source feeds only the picker (^R). This matches FSEditor. When
  // there is no QUOTES.TXT, %f is the message being edited/drafted — load it.
  var sourceText = '';
  if (quotesPath === undefined && file_exists(bodyPath)) {
    var src = readRaw(bodyPath);
    if (src !== null && src.length > 0) {
      if (utf8) src = utf8ToCp437(src).text;
      sourceText = strip_ctrl_a(strip_ansi(src));
    }
  }

  return {
    bodyPath: bodyPath,
    meta: meta,
    sourceText: sourceText,
    quoteLines: quoteLines,
    utf8: utf8
  };
}

function sessionIsUtf8(meta: SessionMeta): boolean {
  if (meta.charset === 'UTF-8') return true;
  if (meta.charset === 'CP437') return false;
  // EDITOR.INF declares nothing: fall back to the terminal capability,
  // matching fseditor.js (console.term_supports(USER_UTF8)).
  return Boolean(console.term_supports(USER_UTF8));
}

/**
 * Write the message body and RESULT.ED.
 *
 * The body is written as RAW CP437 bytes — one byte per glyph — NOT transcoded
 * to UTF-8. Our content is entirely CP437 (art glyphs, box-drawing, blocks),
 * and Synchronet detects the stored charset from the body content itself
 * (writemsg.cpp: `utf8 = !str_is_ascii(buf) && utf8_str_is_valid(buf)`), so
 * raw CP437 is stored as CP437 and display-translated for UTF-8 readers.
 * Transcoding the glyphs to multi-byte UTF-8 would corrupt ANSI-art byte
 * alignment and the CP437 art convention.
 */
export function saveMessage(session: MessageSession, bodyCp437: string, subject: string): boolean {
  var f = new File(session.bodyPath);
  if (!f.open('wb')) {
    log(LOG_ERR, 'future_edit: cannot open body file ' + session.bodyPath);
    return false;
  }
  f.write(bodyCp437);
  f.close();

  // Remove any stale result.ed (any case), then write ours with CRLF.
  var stale;
  while ((stale = file_getcase(system.node_dir + 'result.ed')) !== undefined) {
    if (!file_remove(stale)) break;
  }
  // Subject is CP437 too; Synchronet detects/validates its charset separately.
  var r = new File(system.node_dir + 'result.ed');
  if (r.open('wb')) {
    r.write(buildResultEd(subject, EDITOR_IDENT));
    r.close();
  }
  return true;
}

// ---------------------------------------------------------------------------
// Terminal state
// ---------------------------------------------------------------------------

var savedSysStatus = 0;
var savedCtrlkey: number | string = 0;
var savedStatus = 0;
var savedMouseMode: number | boolean = 0;

export function initTerminal(): TerminalCaps {
  savedSysStatus = bbs.sys_status;
  savedCtrlkey = console.ctrlkey_passthru;
  savedStatus = console.status;
  savedMouseMode = console.mouse_mode;

  // Backstop for exceptions/disconnects; explicit restoreTerminal() is the
  // primary path.
  js.on_exit('bbs.sys_status = ' + bbs.sys_status);
  js.on_exit('console.ctrlkey_passthru = ' + Number(console.ctrlkey_passthru));
  js.on_exit('console.status = ' + console.status);
  js.on_exit('console.mouse_mode = false');
  js.on_exit("console.write('\\x1b[0m')");

  bbs.sys_status &= ~SS_PAUSEON;
  bbs.sys_status |= SS_PAUSEOFF;
  // Receive the control keys the editor binds. Primary Save/Abort are ^O/^A
  // (collision-free); ^S/^Q stay in the set as muscle-memory secondaries but
  // XON/XOFF flow control below Synchronet may swallow them — which is exactly
  // why they are NOT the primaries.
  console.ctrlkey_passthru = '+ACDGKLNOPQRSTUVWXYZ_';

  if (typeof console.getdimensions === 'function') console.getdimensions();

  var mouse = false;
  // Button-event tracking (press/drag/release) with SGR coordinates.
  console.status |= (CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_REL_PASSTHRU);
  console.mouse_mode = (MOUSE_MODE_BTN | MOUSE_MODE_EXT);
  mouse = true;

  return {
    cols: console.screen_columns,
    rows: console.screen_rows,
    utf8: Boolean(console.term_supports(USER_UTF8)),
    mouse: mouse
  };
}

var lastDimsProbe = 0;

/**
 * Current terminal size, with live-resize detection.
 *
 * screen_columns/rows alone NEVER change after logon: a mid-session NAWS
 * report only lands in sbbs->telnet_cols (src/sbbs3/main.cpp) and is copied
 * to the terminal at answer time (src/sbbs3/answer.cpp) — so passively
 * polling them detects nothing. Live detection requires the active probe
 * console.ansi_getdims(): a CPR query (cursor save/restore, so visually
 * inert) whose reply updates screen_columns server-side. This is the same
 * mechanism future_shell uses (shelllib.js _checkConsoleResize).
 *
 * Throttled to one probe per 3s; the caller additionally only invokes this
 * when the input loop is idle, because the probe's blocking reply-read can
 * swallow a keystroke that races the CPR response.
 */
export function terminalSize(): { cols: number; rows: number } {
  var now = new Date().getTime();
  if (now - lastDimsProbe >= 3000) {
    lastDimsProbe = now;
    try {
      if (typeof console.ansi_getdims === 'function') console.ansi_getdims();
    } catch (e) {
      /* non-ANSI terminal: keep the logon dimensions */
    }
  }
  return { cols: console.screen_columns, rows: console.screen_rows };
}

export function restoreTerminal(): void {
  console.mouse_mode = savedMouseMode;
  console.status = savedStatus;
  console.ctrlkey_passthru = savedCtrlkey;
  bbs.sys_status = savedSysStatus;
  console.write('\x1b[0m');
  console.attributes = 7;
}

declare var MOUSE_MODE_BTN: number;
declare var MOUSE_MODE_EXT: number;

// ---------------------------------------------------------------------------
// TheDraw fonts. The .tdf files ship with Synchronet (ctrl/tdfonts/, ~1071
// fonts) and are loaded from there — never redistributed. The repo carries
// only the small prebuilt picker index (fonts/tdfont_index.json).
// ---------------------------------------------------------------------------

function fontsDir(): string {
  return js.exec_dir + 'fonts/';
}

/**
 * FontProvider: the prebuilt index (fonts/tdfont_index.json) names the
 * fonts; each .tdf loads from Synchronet's ctrl/tdfonts/. Parsed fonts are
 * cached. Degrades to an empty list if the index is missing; a font listed
 * in the index but missing on disk loads as null (picker shows it inert).
 */
export function createSbbsFontProvider(): FontProvider {
  var cache: { [name: string]: TdfFont | null } = {};
  var indexCache: FontMeta[] | null = null;
  return {
    list: function (): FontMeta[] {
      if (indexCache !== null) return indexCache;
      indexCache = [];
      var raw = readRaw(fontsDir() + 'tdfont_index.json');
      if (raw === null) {
        log(LOG_ERR, 'future_edit: font index not found in ' + fontsDir());
        return indexCache;
      }
      try {
        var arr = JSON.parse(raw) as FontMeta[];
        if (Object.prototype.toString.call(arr) === '[object Array]') indexCache = arr;
      } catch (e) {
        log(LOG_ERR, 'future_edit: bad font index: ' + String(e));
      }
      return indexCache;
    },
    load: function (name: string): TdfFont | null {
      if (Object.prototype.hasOwnProperty.call(cache, name)) return cache[name] as TdfFont | null;
      var safe = String(name).replace(/[^a-zA-Z0-9_\-!#]/g, '');
      var data = readRaw(system.ctrl_dir + 'tdfonts/' + safe + '.tdf');
      var font = data === null ? null : parseTdf(data);
      cache[name] = font;
      return font;
    }
  };
}
