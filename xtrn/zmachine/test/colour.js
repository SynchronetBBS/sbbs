// Headless unit tests for the door's pure colour/style helpers. The door file
// only defines functions/vars at top level (the connected-terminal side effects
// live inside jszm_door_main, gated by JSZM_DOOR_NO_MAIN), so we can load it
// under jsexec and call the pure helpers directly.  Run: jsexec test/colour.js
var JSZM_DOOR_NO_MAIN = true;
load(js.exec_dir + "../zmachine.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }
function A(o) {                       // build an attr state with defaults
  return { fg: o.fg || 0, bg: o.bg || 0, bold: !!o.bold, reverse: !!o.reverse, italic: !!o.italic };
}

// --- jszm_attr_prefix ---
ok(jszm_attr_prefix(A({})) === "\x01n", "default state -> normal only");
ok(jszm_attr_prefix(A({ bold: true })) === "\x01n\x010\x01h\x01w", "bold only -> normal,black-bg,high,white-fg");
ok(jszm_attr_prefix(A({ reverse: true })) === "\x01n\x017\x01k", "reverse on default -> white bg, black fg");
ok(jszm_attr_prefix(A({ fg: 3, bg: 6 })) === "\x01n\x014\x01r", "red on blue -> blue bg, red fg");
ok(jszm_attr_prefix(A({ fg: 3, bg: 6, reverse: true })) === "\x01n\x011\x01b", "reverse red/blue -> red bg, blue fg");
ok(jszm_attr_prefix(A({ italic: true })) === "\x01n\x010\x01c", "italic -> cyan tint fg");
ok(jszm_attr_prefix(A({ fg: 4, bold: true })) === "\x01n\x010\x01h\x01g", "bold green -> high, green fg");

// --- jszm_apply_colour: 0=current(no change), 1=default(reset), 2..9=set ---
(function () {
  var a = A({});
  jszm_apply_colour(a, 3, 6); ok(a.fg === 3 && a.bg === 6, "apply_colour set fg=3,bg=6");
  jszm_apply_colour(a, 0, 0); ok(a.fg === 3 && a.bg === 6, "apply_colour 0/0 = current (unchanged)");
  jszm_apply_colour(a, 9, 0); ok(a.fg === 9 && a.bg === 6, "apply_colour fg=9, bg current");
  jszm_apply_colour(a, 1, 1); ok(a.fg === 0 && a.bg === 0, "apply_colour 1/1 = default (reset)");
})();

// --- jszm_apply_style: 0 resets all; nonzero ORs style bits in (cumulative) ---
(function () {
  var a = A({});
  jszm_apply_style(a, 2); ok(a.bold && !a.reverse && !a.italic, "apply_style 2 = bold");
  jszm_apply_style(a, 4); ok(a.bold && a.italic, "apply_style 4 adds italic (cumulative)");
  jszm_apply_style(a, 1); ok(a.bold && a.italic && a.reverse, "apply_style 1 adds reverse");
  jszm_apply_style(a, 0); ok(!a.bold && !a.italic && !a.reverse, "apply_style 0 = roman (all off)");
})();

if (fails) throw new Error(fails + " colour test(s) failed");
print("COLOUR OK");
