/**
 * Headless smoke test for jsexec (no terminal, no console screen I/O):
 * exercises the pure core on the real SpiderMonkey 1.8.5 engine so ES5
 * compatibility regressions surface before the editor runs on a live node.
 *
 * Run: /sbbs/exec/jsexec dist/smoke_runner.js   (exit 0 = pass)
 */

import { Document } from './core/doc';
import { ctrlATransition, makeAttr, ansiFromAttr, HIGH } from './core/attr';
import { cp437ToUtf8, utf8ToCp437, displayChar } from './core/cp437';
import { parseMsgInf, parseEditorInf, buildResultEd } from './core/dropfiles';
import { parseTdf, renderTdf } from './core/tdf';

var failures: string[] = [];

function check(name: string, ok: boolean): void {
  if (!ok) failures.push(name);
}

// --- document: wrap + reflow ---
var doc = new Document(20);
doc.loadText('The quick brown fox jumps over the lazy dog');
check('wrap produces multiple lines', doc.lines.length > 1);
var joined = '';
for (var i = 0; i < doc.lines.length; i++) joined += (doc.lines[i] as { text: string }).text;
check('wrap is lossless', joined === 'The quick brown fox jumps over the lazy dog');

// --- art stays fixed while text edits ---
doc.setArt(5, 0, { ch: 0xdb, attr: 7 });
doc.caret = { row: 0, col: 0 };
doc.insertChar(0x58);
check('art cell unmoved by insert', doc.artAt(5, 0) !== null);

// --- undo ---
check('undo restores', doc.undo() && doc.lines.length > 0);

// --- message body ---
var doc2 = new Document(79);
doc2.loadText('hello');
var body = doc2.toMessageBody(true);
check('body has CRLF', body.indexOf('\r\n') !== -1);

// --- codecs ---
check('ctrl-a transition', ctrlATransition(7, makeAttr(4, 0, true)) === '\x01H\x01R');
check('ansi attr', ansiFromAttr(7 | HIGH) === '\x1b[0;1;37;40m');
check('cp437->utf8 block', cp437ToUtf8('\xdb') === '\xe2\x96\x88');
check('utf8->cp437 roundtrip', utf8ToCp437('\xe2\x96\x88').text === '\xdb');
check('displayChar cp437', displayChar(0xb3, false) === '\xb3');

// --- drop files ---
var msginf = parseMsgInf(['Alice', 'Bob', 'Test subj', '1', 'General', 'NO', '', 'UTF-8']);
check('msginf parse', msginf.subject === 'Test subj' && msginf.charset === 'UTF-8');
var edinf = parseEditorInf(['Subj', 'Bob', '1', 'Alice', 'alice', '30']);
check('editor.inf parse', edinf.subject === 'Subj' && edinf.from === 'Alice');
check('result.ed', buildResultEd('S', 'HERMedIT') === '0\r\nS\r\nHERMedIT\r\n');

// --- TheDraw font parse + render on the real engine (binary read included) ---
var fontChecks = 0;
try {
  // Fonts ship with Synchronet: always ctrl/tdfonts/.
  var ff = new File(system.ctrl_dir + 'tdfonts/block.tdf');
  if (ff.open('rb')) {
    var fdata = ff.read();
    ff.close();
    var font = parseTdf(fdata === null ? '' : fdata);
    check('tdf parse', font !== null && font.height > 0);
    if (font !== null) {
      var r = renderTdf(font, 'HI');
      check('tdf render dims', r.height === font.height && r.width > 0 && r.rows.length === font.height);
    }
    fontChecks = 2;
  }
} catch (e) {
  failures.push('tdf pipeline threw: ' + String(e));
}

if (failures.length) {
  print('SMOKE FAIL: ' + failures.join(', '));
  exit(1);
}
print('SMOKE PASS (' + (12 + fontChecks) + ' checks)');
exit(0);

declare function print(msg: string): void;
