/**
 * Line-based syntax highlighter for code blocks: pure ES5, no globals. A
 * small state machine scans one line at a time carrying only "inside block
 * comment / multiline string" state across lines, so re-highlighting a whole
 * (message-sized) block on every keystroke is cheap and always correct.
 *
 * Colors are CGA attribute bytes on a black background, chosen to survive
 * 16-color BBS readers.
 */

export var HL_DEFAULT = 0x07;  // lightgray
export var HL_KEYWORD = 0x0b;  // bright cyan
export var HL_STRING = 0x0e;   // yellow
export var HL_COMMENT = 0x08;  // dark gray
export var HL_NUMBER = 0x0a;   // bright green
export var HL_PREPROC = 0x0d;  // bright magenta

export interface LangDef {
  /** Menu label and the id stored on the region. */
  id: string;
  name: string;
  keywords: { [k: string]: boolean };
  /** Line-comment introducers ('//', '#', ...). */
  lineComments: string[];
  /** Block-comment [open, close] pairs; may nest across lines (state). */
  blockComments: [string, string][];
  /** Quote characters. `dbl`: closing by doubling ('' in Pascal) instead of \-escapes. */
  quotes: { q: string; dbl?: boolean }[];
  /** Multiline string delimiters (python triple quotes, JS backticks). */
  multiStrings: string[];
  /** Pascal-style case-insensitive keywords. */
  caseInsensitive?: boolean;
  /** '#' preprocessor lines (C/C++): colors the directive word. */
  hashPreproc?: boolean;
}

/** Carried across lines: which block comment / multiline string is open. */
export interface HlState {
  block: number;
  mstr: number;
}

export function initialHlState(): HlState {
  return { block: -1, mstr: -1 };
}

function kw(words: string): { [k: string]: boolean } {
  var out: { [k: string]: boolean } = {};
  var list = words.split(' ');
  for (var i = 0; i < list.length; i++) out[list[i] as string] = true;
  return out;
}

var JS_BASE = 'var let const function return if else for while do break continue switch case default ' +
  'new delete typeof instanceof in of class extends super this null true false undefined ' +
  'try catch finally throw void yield async await import export from static get set with debugger';

export var LANGS: LangDef[] = [
  {
    id: 'javascript', name: 'JavaScript',
    keywords: kw(JS_BASE),
    lineComments: ['//'], blockComments: [['/*', '*/']],
    quotes: [{ q: "'" }, { q: '"' }], multiStrings: ['`']
  },
  {
    id: 'typescript', name: 'TypeScript',
    keywords: kw(JS_BASE + ' interface type enum implements declare namespace module public private ' +
      'protected readonly abstract as is keyof infer never unknown any number string boolean symbol object'),
    lineComments: ['//'], blockComments: [['/*', '*/']],
    quotes: [{ q: "'" }, { q: '"' }], multiStrings: ['`']
  },
  {
    id: 'python', name: 'Python',
    keywords: kw('def class return if elif else for while break continue pass import from as with ' +
      'lambda yield global nonlocal try except finally raise assert del in is not and or ' +
      'None True False async await match case self print'),
    lineComments: ['#'], blockComments: [],
    quotes: [{ q: "'" }, { q: '"' }], multiStrings: ['"""', "'''"]
  },
  {
    id: 'cpp', name: 'C++',
    keywords: kw('int char long short float double void bool unsigned signed const static struct class ' +
      'public private protected virtual override final template typename namespace using new delete ' +
      'return if else for while do switch case default break continue goto sizeof enum union typedef ' +
      'extern inline friend operator this nullptr true false try catch throw auto constexpr mutable ' +
      'volatile register explicit noexcept decltype wchar_t size_t std'),
    lineComments: ['//'], blockComments: [['/*', '*/']],
    quotes: [{ q: "'" }, { q: '"' }], multiStrings: [],
    hashPreproc: true
  },
  {
    id: 'pascal', name: 'Pascal',
    keywords: kw('program begin end procedure function var const type uses unit interface implementation ' +
      'if then else for to downto do while repeat until case of record array set string integer real ' +
      'boolean char byte word longint shortint cardinal writeln write readln read new dispose nil not ' +
      'and or xor div mod in with goto label packed file text object constructor destructor inherited ' +
      'private public protected published property class exit break continue result true false'),
    lineComments: ['//'], blockComments: [['{', '}'], ['(*', '*)']],
    quotes: [{ q: "'", dbl: true }], multiStrings: [],
    caseInsensitive: true
  },
  {
    id: 'plain', name: 'Plain (no highlight)',
    keywords: {}, lineComments: [], blockComments: [], quotes: [], multiStrings: []
  }
];

export function langById(id: string): LangDef | null {
  for (var i = 0; i < LANGS.length; i++) {
    if ((LANGS[i] as LangDef).id === id) return LANGS[i] as LangDef;
  }
  return null;
}

function isDigit(c: string): boolean {
  return c >= '0' && c <= '9';
}

function isIdentStart(c: string): boolean {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';
}

function isIdentPart(c: string): boolean {
  return isIdentStart(c) || isDigit(c);
}

function startsAt(text: string, i: number, s: string): boolean {
  return text.substring(i, i + s.length) === s;
}

function fillAttr(attrs: number[], from: number, to: number, a: number): void {
  for (var i = from; i < to; i++) attrs[i] = a;
}

/**
 * Highlight one line: returns the attribute per character and mutates `st`
 * (the carry state for the NEXT line).
 */
export function highlightLine(text: string, st: HlState, def: LangDef): number[] {
  var len = text.length;
  var attrs: number[] = [];
  for (var p = 0; p < len; p++) attrs.push(HL_DEFAULT);
  var i = 0;

  while (i < len) {
    // Inside a carried block comment: eat until its closer.
    if (st.block >= 0) {
      var close = (def.blockComments[st.block] as [string, string])[1];
      var end = text.indexOf(close, i);
      if (end === -1) { fillAttr(attrs, i, len, HL_COMMENT); return attrs; }
      fillAttr(attrs, i, end + close.length, HL_COMMENT);
      i = end + close.length;
      st.block = -1;
      continue;
    }
    // Inside a carried multiline string: eat until its delimiter.
    if (st.mstr >= 0) {
      var mdelim = def.multiStrings[st.mstr] as string;
      var mend = text.indexOf(mdelim, i);
      if (mend === -1) { fillAttr(attrs, i, len, HL_STRING); return attrs; }
      fillAttr(attrs, i, mend + mdelim.length, HL_STRING);
      i = mend + mdelim.length;
      st.mstr = -1;
      continue;
    }

    var c = text.charAt(i);

    // Line comments.
    var lc = -1;
    for (var l = 0; l < def.lineComments.length; l++) {
      if (startsAt(text, i, def.lineComments[l] as string)) { lc = l; break; }
    }
    if (lc >= 0) { fillAttr(attrs, i, len, HL_COMMENT); return attrs; }

    // Block comment opens.
    var bc = -1;
    for (var b = 0; b < def.blockComments.length; b++) {
      if (startsAt(text, i, (def.blockComments[b] as [string, string])[0])) { bc = b; break; }
    }
    if (bc >= 0) {
      var opener = (def.blockComments[bc] as [string, string])[0];
      fillAttr(attrs, i, i + opener.length, HL_COMMENT);
      i += opener.length;
      st.block = bc;
      continue;
    }

    // Multiline string opens (checked before single quotes: ''' starts with ').
    var ms = -1;
    for (var m = 0; m < def.multiStrings.length; m++) {
      if (startsAt(text, i, def.multiStrings[m] as string)) { ms = m; break; }
    }
    if (ms >= 0) {
      var mo = def.multiStrings[ms] as string;
      fillAttr(attrs, i, i + mo.length, HL_STRING);
      i += mo.length;
      st.mstr = ms;
      continue;
    }

    // Quoted strings (single line; unterminated colors to EOL).
    var qi = -1;
    for (var q = 0; q < def.quotes.length; q++) {
      if ((def.quotes[q] as { q: string }).q === c) { qi = q; break; }
    }
    if (qi >= 0) {
      var qd = def.quotes[qi] as { q: string; dbl?: boolean };
      var j = i + 1;
      while (j < len) {
        var cj = text.charAt(j);
        if (!qd.dbl && cj === '\\') { j += 2; continue; }
        if (cj === qd.q) {
          if (qd.dbl && text.charAt(j + 1) === qd.q) { j += 2; continue; } // '' doubling
          j++;
          break;
        }
        j++;
      }
      if (j > len) j = len;
      fillAttr(attrs, i, j, HL_STRING);
      i = j;
      continue;
    }

    // C preprocessor: '#' as the line's first non-blank colors the directive.
    if (def.hashPreproc && c === '#' && text.substring(0, i).replace(/ +/g, '') === '') {
      var pj = i + 1;
      while (pj < len && isIdentPart(text.charAt(pj))) pj++;
      fillAttr(attrs, i, pj, HL_PREPROC);
      i = pj;
      continue;
    }

    // Numbers (integers, hex, decimals).
    if (isDigit(c)) {
      var nj = i + 1;
      while (nj < len) {
        var nc = text.charAt(nj);
        if (isDigit(nc) || nc === '.' || nc === 'x' || nc === 'X' ||
            (nc >= 'a' && nc <= 'f') || (nc >= 'A' && nc <= 'F')) nj++;
        else break;
      }
      fillAttr(attrs, i, nj, HL_NUMBER);
      i = nj;
      continue;
    }

    // Identifiers / keywords.
    if (isIdentStart(c)) {
      var ij = i + 1;
      while (ij < len && isIdentPart(text.charAt(ij))) ij++;
      var word = text.substring(i, ij);
      if (def.caseInsensitive) word = word.toLowerCase();
      if (def.keywords[word] === true) fillAttr(attrs, i, ij, HL_KEYWORD);
      i = ij;
      continue;
    }

    i++;
  }
  return attrs;
}

/**
 * Re-highlight a whole block in place: walks `lines` top to bottom writing
 * each line's `attr` array. The caller passes the active flow's lines.
 */
export function highlightLines(lines: { text: string; attr: number[] }[], def: LangDef): void {
  var st = initialHlState();
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i] as { text: string; attr: number[] };
    line.attr = highlightLine(line.text, st, def);
  }
}

// ---------------------------------------------------------------------------
// Markdown-style ``` fences and language auto-detection
// ---------------------------------------------------------------------------

/**
 * If `text` is a fence line (``` with an optional language tag), returns the
 * lowercased tag ('' for a bare fence); otherwise null.
 */
export function fenceTag(text: string): string | null {
  var m = /^```\s*([A-Za-z+#]*)\s*$/.exec(text);
  if (m === null) return null;
  return (m[1] as string).toLowerCase();
}

/** Common fence-tag spellings -> language ids. */
var FENCE_ALIASES: { [k: string]: string } = {
  js: 'javascript', javascript: 'javascript',
  ts: 'typescript', typescript: 'typescript',
  py: 'python', python: 'python',
  c: 'cpp', cpp: 'cpp', 'c++': 'cpp', cxx: 'cpp',
  pas: 'pascal', pascal: 'pascal', delphi: 'pascal'
};

/** Distinctive per-language markers that keyword counting alone misses. */
var DETECT_MARKERS: { [id: string]: string[] } = {
  javascript: ['=>', '===', 'function ', 'console.', 'require('],
  typescript: [': string', ': number', ': boolean', 'interface ', '=>'],
  python: ['def ', 'elif ', 'import ', 'self.', '):'],
  cpp: ['#include', '::', '->', 'std::', ');'],
  pascal: [':=', 'begin', 'end;', 'writeln', 'procedure ']
};

function countOccurrences(haystack: string, needle: string): number {
  var n = 0;
  var i = 0;
  for (;;) {
    var at = haystack.indexOf(needle, i);
    if (at === -1) return n;
    n++;
    i = at + needle.length;
  }
}

/**
 * Best-effort language guess for an untagged fence: keyword hits plus
 * distinctive-marker bonuses per language. Returns 'plain' when nothing
 * scores (so unknown text just gets no highlighting).
 */
export function detectLanguage(sample: string[]): string {
  var bestId = 'plain';
  var bestScore = 0;
  for (var li = 0; li < LANGS.length; li++) {
    var def = LANGS[li] as LangDef;
    if (def.id === 'plain') continue;
    var score = 0;
    for (var s = 0; s < sample.length; s++) {
      var line = sample[s] as string;
      var scan = def.caseInsensitive ? line.toLowerCase() : line;
      // keyword hits (word-tokenized)
      var word = '';
      for (var i = 0; i <= scan.length; i++) {
        var c = i < scan.length ? scan.charAt(i) : ' ';
        if (isIdentPart(c)) word += c;
        else {
          if (word.length > 1 && def.keywords[word] === true) score++;
          word = '';
        }
      }
      var markers = DETECT_MARKERS[def.id];
      if (markers !== undefined) {
        for (var mk = 0; mk < markers.length; mk++) {
          score += countOccurrences(scan, markers[mk] as string) * 3;
        }
      }
    }
    if (score > bestScore) { bestScore = score; bestId = def.id; }
  }
  return bestId;
}

/**
 * Resolve a fence's language: explicit tag (with alias spellings) wins; a
 * bare fence auto-detects from the block's content.
 */
export function resolveFenceLang(tag: string, sample: string[]): LangDef {
  var id = tag === '' ? detectLanguage(sample) : (FENCE_ALIASES[tag] !== undefined ? FENCE_ALIASES[tag] as string : tag);
  var def = langById(id);
  return def !== null ? def : (langById('plain') as LangDef);
}
