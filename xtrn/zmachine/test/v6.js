// Headless tests for Z-machine v6 engine support (jszm.js). Run: jsexec test/v6.js
load(js.exec_dir + "../quetzal.js");
load(js.exec_dir + "../jszm.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// --- versionParams(6): accepted, with v4/5-style object layout and scale-8 file length ---
(function () {
  var vp = JSZM.versionParams(6);
  ok(!!vp, "versionParams(6) is defined");
  ok(vp.objEntry === 14 && vp.attrBytes === 6 && vp.objNumWidth === 2 && vp.defProps === 63 &&
     vp.objectsAdj === 114, "v6 object/property layout matches v4/5");
  ok(vp.addrShift === 2 && vp.lenDiv === 8, "v6 packed-addr base shift 2, file-length scale 8");
  ok(vp.splitPacked === true, "v6 splitPacked flag set (routine/string offset)");
})();

// --- split packed addresses: v6 adds 8*R_O (routines) / 8*S_O (strings); v3/4/5/8 unchanged ---
(function () {
  function hdr(v) {                              // 64-byte header, version v, given offsets
    var m = new Uint8Array(0x40); m[0] = v;
    m[0x28] = 0x00; m[0x29] = 0x10;             // R_O = 0x0010
    m[0x2A] = 0x00; m[0x2B] = 0x20;             // S_O = 0x0020
    return m;
  }
  var g6 = new JSZM(hdr(6));
  ok(g6.unpackRoutine(0x100) === 0x100 * 4 + 8 * 0x0010, "v6 routine unpack = 4P + 8*R_O");
  ok(g6.unpackString(0x100) === 0x100 * 4 + 8 * 0x0020, "v6 string unpack = 4P + 8*S_O");
  var g5 = new JSZM(hdr(5));
  ok(g5.unpackRoutine(0x100) === (0x100 << 2) && g5.unpackString(0x100) === (0x100 << 2),
     "v5 routine/string unpack identical (no offset)");
  var g3 = new JSZM(hdr(3));
  ok(g3.unpackRoutine(0x100) === (0x100 << 1), "v3 unpack = 2P (unchanged)");
  var ms = new Uint8Array(0x40); ms[0] = 6; ms[1] = 1;   // v6, byteSwapped
  ms[0x28] = 0x10; ms[0x29] = 0x00;                       // R_O = 0x0010 (low,high swapped)
  ms[0x2A] = 0x20; ms[0x2B] = 0x00;                       // S_O = 0x0020 (low,high swapped)
  var gs = new JSZM(ms);
  ok(gs.unpackRoutine(0x100) === 0x100 * 4 + 8 * 0x0010 && gs.unpackString(0x100) === 0x100 * 4 + 8 * 0x0020,
     "v6 byteSwapped: offsets read low,high");
})();

// --- v6 main-routine entry + header geometry + Flags2 pictures bit ---
(function () {
  // main routine @0x100 (packed 0x40 with R_O=0): 0 locals; inc g16 ; quit.
  function v6story(graphics, gprops) {
    var m = new Uint8Array(0x400);
    m[0] = 6;
    m[6] = 0x00; m[7] = 0x40;                  // main = packed 0x40 -> 4*0x40 = 0x100
    m[12] = 0x01; m[13] = 0x80;                // globals @ 0x180
    m[14] = 0x02; m[15] = 0x00;                // static base @ 0x200
    m[0x28] = 0; m[0x29] = 0; m[0x2A] = 0; m[0x2B] = 0;   // R_O = S_O = 0
    m[0x100] = 0x00;                           // main: 0 locals
    m[0x101] = 0x95; m[0x102] = 0x10;          // inc g16
    m[0x103] = 0xBA;                           // quit
    var g = new JSZM(m);
    g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
    g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
    g.screen = function () {}; g.split = function () {};
    if (graphics) g.graphicsAvailable = function () { return true; };
    if (gprops) { g.screenWidthPx = 640; g.screenHeightPx = 400; g.fontWidth = 8; g.fontHeight = 16; }
    return g;
  }
  var g = v6story(true, true);
  try { g.run(); } catch (e) { ok(false, "v6 main run threw: " + e); }
  ok(g.getu(0x180) === 1, "v6 main routine entered + ran (g16 incremented)");
  ok(g.getu(0x22) === 640 && g.getu(0x24) === 400, "screen width/height (units) = pixels");
  ok(g.mem[0x26] === 8 && g.mem[0x27] === 16, "font width/height = real cell size");
  ok((g.mem[0x11] & 8) !== 0, "Flags2 pictures bit set when graphicsAvailable() true");

  var g2 = v6story(false, true);
  try { g2.run(); } catch (e) { ok(false, "v6 g2 run threw: " + e); }
  ok((g2.mem[0x11] & 8) === 0, "Flags2 pictures bit clear when graphics unavailable");
})();

// --- window model: put_wind_prop then get_wind_prop round-trips; set_window selects current ---
(function () {
  var code = [];
  // set_window 2  (VAR 0xEB) -> types 0x7F (small + 3 omit)
  code.push(0xEB, 0x7F, 0x02);
  // put_wind_prop 2,0,123  -> EXT 25, three small consts: types 0x57 = sc,sc,sc,omit
  code.push(0xBE, 0x19, 0x57, 0x02, 0x00, 0x7B);
  // get_wind_prop 2,0 -> g16(0x10) : EXT 19, two small consts (types 0x5F), store 0x10
  code.push(0xBE, 0x13, 0x5F, 0x02, 0x00, 0x10);
  // put_wind_prop -3,1,55 : window -3 = current(=2). -3 as a large const 0xFFFD.
  //   types 0x17 -> large,small,small,omit ; operands: 0xFF 0xFD, 0x01, 0x37
  code.push(0xBE, 0x19, 0x17, 0xFF, 0xFD, 0x01, 0x37);
  // get_wind_prop 2,1 -> g17(0x11)
  code.push(0xBE, 0x13, 0x5F, 0x02, 0x01, 0x11);
  code.push(0xBA);   // quit
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  var win = -1; g.screen = function (w) { win = w; }; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "window model run threw: " + e); }
  ok(g.getu(0x180) === 123, "get_wind_prop reads back put_wind_prop value (123)");
  ok(g.getu(0x182) === 55, "window -3 resolves to current window (2); prop1 = 55");
  ok(win === 2, "set_window forwarded current window (2) to the door hook");
})();

// --- window geometry opcodes update the model and fire windowChanged ---
(function () {
  var code = [];
  code.push(0xBE, 0x10, 0x57, 0x03, 0x10, 0x20);            // move_window win=3 y=16 x=32 (types 0x57=sc,sc,sc,omit)
  code.push(0xBE, 0x11, 0x53, 0x03, 0x64, 0x00, 0xC8);      // window_size win=3 y=100 x=200 (types 0x53=sc,sc,large,omit)
  code.push(0xBE, 0x08, 0x57, 0x06, 0x03, 0x04);            // set_margins left=6 right=3 window=4 (op0,op1,op2)
  code.push(0xBE, 0x13, 0x5F, 0x03, 0x00, 0x10);            // get_wind_prop 3,0 -> g16
  code.push(0xBE, 0x13, 0x5F, 0x03, 0x03, 0x11);            // get_wind_prop 3,3 -> g17
  code.push(0xBE, 0x12, 0x57, 0x05, 0x0A, 0x00);           // window_style win=5 flags=0x0A op=set -> 0x0A
  code.push(0xBE, 0x12, 0x57, 0x05, 0x04, 0x01);           // window_style win=5 flags=0x04 op=OR  -> 0x0E
  code.push(0xBE, 0x14, 0x5F, 0x03, 0x08);                 // scroll_window win=3 pixels=8
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  var changed = []; g.windowChanged = function (w) { changed.push(w); };
  var scrolled = null; g.scrollWindow = function (w, px) { scrolled = [w, px]; };
  try { g.run(); } catch (e) { ok(false, "geometry run threw: " + e); }
  ok(g.getu(0x180) === 16, "move_window set y coord (16)");
  ok(g.getu(0x182) === 200, "window_size set x size (200)");
  ok(g.windows[4][6] === 6 && g.windows[4][7] === 3, "set_margins set left/right margins");
  ok(changed.length >= 3, "windowChanged fired for each geometry change (was " + changed.length + ")");
  ok(g.windows[3][1] === 32 && g.windows[3][2] === 100, "move_window x + window_size y also set");
  ok(g.windows[5][10] === 0x0E, "window_style set then OR -> 0x0E");
  ok(scrolled && scrolled[0] === 3 && scrolled[1] === 8, "scroll_window fired scrollWindow(3, 8)");
})();

// --- picture hooks + graphics-off picture_data branch ---
(function () {
  var code = [];
  code.push(0xBE, 0x05, 0x57, 0x05, 0x0A, 0x14);            // draw_picture pic=5 y=10 x=20 (types 0x57=sc,sc,sc,omit)
  code.push(0xBE, 0x07, 0x7F, 0x05);                        // erase_picture 5 (types 0x7F = 1 small const + 3 omitted)
  // picture_data 99, 0x190 ?(label): EXT 6 ; types 0x4F = small,large,omit,omit ; pic=99 array=0x0190 ;
  // branch byte 0xC0 = branch-on-TRUE; pictureInfo(99)->null -> predicate(false) -> NOT taken -> falls through to quit.
  code.push(0xBE, 0x06, 0x4F, 0x63, 0x01, 0x90, 0xC0);
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  var drew = null, erased = null;
  g.drawPicture = function (p, x, y, w) { drew = [p, x, y]; };
  g.erasePicture = function (p) { erased = p; };
  g.pictureInfo = function (p) { return null; };   // picture 99 absent
  try { g.run(); } catch (e) { ok(false, "picture-op run threw: " + e); }
  ok(drew && drew[0] === 5 && drew[1] === 20 && drew[2] === 10, "draw_picture hook got (pic=5, x=20, y=10)");
  ok(erased === 5, "erase_picture hook got pic 5");
})();

// --- window 0 default size derives from the SAME effective dims as the header (consistency,
//     even when the door supplies no pixel props -> derived from cols/rows * font) ---
(function () {
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x100] = 0x00; m[0x101] = 0xBA;            // main: 0 locals; quit
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  g.screenRows = 25; g.screenCols = 80;        // no pixel props set -> dims derive from cols/rows * font(1px)
  try { g.run(); } catch (e) { ok(false, "win0-size run threw: " + e); }
  ok(g.windows[0][3] === g.getu(0x22) && g.windows[0][2] === g.getu(0x24),
     "window 0 size matches header screen dims when pixel props unset (w=" + g.windows[0][3] + " h=" + g.windows[0][2] + ")");
  ok(g.windows[0][3] === 80 && g.windows[0][2] === 25, "derived window 0 size = cols x rows (80x25) with 1px font");
})();

// --- user-stack push_stack/pop_stack: table word0 = free slots; values fill top slot down ---
(function () {
  var code = [];
  code.push(0xBE, 0x18, 0x4F, 0xAA, 0x01, 0x90, 0x40);   // push_stack value=0xAA stack=0x190
  code.push(0xBE, 0x18, 0x4F, 0xBB, 0x01, 0x90, 0x40);   // push_stack value=0xBB stack=0x190
  code.push(0xBE, 0x15, 0x4F, 0x01, 0x01, 0x90);         // pop_stack items=1 stack=0x190
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x190] = 0x00; m[0x191] = 0x03;            // user stack @0x190: word0 = 3 free slots (capacity 3)
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "user-stack run threw: " + e); }
  ok(g.getu(0x196) === 0xAA, "push 0xAA stored at slot stack+2*3 (0x196)");
  ok(g.getu(0x194) === 0xBB, "push 0xBB stored at slot stack+2*2 (0x194)");
  ok(g.getu(0x190) === 2, "word0: 3 -> push 2 -> push 1 -> pop(1) -> 2 free slots");
})();

// --- push_stack/pop_stack game-stack form (no stack operand) still works ---
(function () {
  var code = [];
  code.push(0xBE, 0x18, 0x7F, 0x07, 0x40);     // push_stack 0x07 (game stack) ?(branch-on-false)
  code.push(0xE9, 0xFF, 0x10);                 // pull (no operand, v6 store form) -> g16(0x10)
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "game-stack push run threw: " + e); }
  ok(g.getu(0x180) === 7, "push_stack(no stack) -> game stack; pull recovered 7");
})();

// --- push_stack onto a FULL user stack (word0 == 0) branches false (does not store) ---
(function () {
  var code = [];
  // push_stack 0xCC, 0x190 ?(branch-on-TRUE +4 -> skip the inc on success). Stack is full -> false ->
  // fall through to `inc g16`. Branch byte 0xC4 = on-true, 1-byte, offset 4 (skips inc's 2 bytes).
  code.push(0xBE, 0x18, 0x4F, 0xCC, 0x01, 0x90, 0xC4);
  code.push(0x95, 0x10);                        // inc g16 (runs only if push branched false)
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x190] = 0x00; m[0x191] = 0x00;            // user stack @0x190: word0 = 0 free slots (full)
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "full-stack push run threw: " + e); }
  ok(g.getu(0x180) === 1, "push onto full stack branched false (inc g16 ran)");
  ok(g.getu(0x190) === 0, "full stack word0 unchanged (no store)");
})();

// --- v6 pull (VAR:233) is "pull stack -> (result)": a STORE opcode that reads a store byte. (v1-5
//     pull stores INTO the operand variable with NO store byte.) Regression for the Arthur crash:
//     the engine read no store byte -> PC off by one -> derailed on the next instruction (Invalid op 0). ---
(function () {
  var code = [];
  code.push(0xE9, 0x3F, 0x01, 0x90, 0x10);   // pull 0x190 -> g16 : VAR pull, types 0x3F (op0 large const), store 0x10
  code.push(0x95, 0x11);                      // inc g17 -> proves PC stayed aligned (store byte consumed)
  code.push(0xBA);
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x190] = 0x00; m[0x191] = 0x02;           // user stack @0x190: word0 = 2 free (one slot used)
  m[0x196] = 0x00; m[0x197] = 0x42;           // the used value 0x42 sits at slot 3 (0x190 + 2*3)
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "v6 pull run threw: " + e); }
  ok(g.getu(0x180) === 0x42, "v6 pull stored the user-stack top value (0x42) via the store byte");
  ok(g.getu(0x190) === 3, "v6 pull incremented the user-stack free count (2 -> 3)");
  ok(g.getu(0x182) === 1, "PC stayed aligned: the inc after pull ran (store byte was consumed)");
})();

// --- v6 set_cursor: op0 = -1/-2 are cursor OFF/ON (visibility toggles, no position/window operands);
//     they must NOT write a position into the window cursor model nor be forwarded to the door as a
//     position. Only op0 >= 0 is a real (y, x [, window]) position. ---
(function () {
  function run(code) {
    var m = new Uint8Array(0x400);
    m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
    m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
    var g = new JSZM(m);
    g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
    g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
    g.screen = function () {}; g.split = function () {};
    g.calls = []; g.setCursor = function (y, x) { g.calls.push([y, x]); };
    try { g.run(); } catch (e) { ok(false, "set_cursor run threw: " + e); }
    return g;
  }
  // set_window 3 ; set_cursor -1 (cursor OFF: VAR 0xEF, large const 0xFFFF) ; quit
  var goff = run([0xEB, 0x7F, 0x03, 0xEF, 0x3F, 0xFF, 0xFF, 0xBA]);
  ok(goff.windows[3][4] === 0 && goff.windows[3][5] === 0, "set_cursor -1 (off) wrote NO position to the model");
  ok(goff.calls.length === 0, "set_cursor -1 (off) not forwarded to the door as a position");
  // set_window 3 ; set_cursor 16 32 (small consts) ; quit
  var gpos = run([0xEB, 0x7F, 0x03, 0xEF, 0x5F, 0x10, 0x20, 0xBA]);
  ok(gpos.windows[3][4] === 16 && gpos.windows[3][5] === 32, "set_cursor (real) updates the window cursor model");
  ok(gpos.calls.length === 1 && gpos.calls[0][0] === 16 && gpos.calls[0][1] === 32, "set_cursor (real) forwarded to the door");
})();

// --- print_form (EXT 26): word 0 = CHARACTER count, then that many chars. Regression for the Arthur
//     runaway -- reading word 0 as a ROW count and looping dumped megabytes of memory as garbage. ---
(function () {
  var code = [0xBE, 0x1A, 0x3F, 0x01, 0x90, 0xBA];   // print_form 0x190 (large const) ; quit
  var m = new Uint8Array(0x400);
  m[0] = 6; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
  m[0x190] = 0x00; m[0x191] = 0x05;                  // word0 = 5 (char count)
  var hello = "Hello", k; for (k = 0; k < hello.length; k++) m[0x192 + k] = hello.charCodeAt(k);
  m[0x193 + hello.length] = 0x00;                    // trailing garbage that a runaway would dump
  m[0x100] = 0x00; var i; for (i = 0; i < code.length; i++) m[0x101 + i] = code[i];
  var g = new JSZM(m); var out = "";
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.screen = function () {}; g.split = function () {};
  try { g.run(); } catch (e) { ok(false, "print_form run threw: " + e); }
  ok(out === "Hello", "print_form printed exactly the captured chars, no memory dump (was '" + out.substr(0, 20) + "')");
})();

// --- picture_data(0): stores catalogue (count -> array[0], release -> array[1]) and branches true ---
(function () {
  var m = new Uint8Array(0x40); m[0] = 6;
  var g = new JSZM(m);
  g.mem = g.memInit; // mimic init() base so put/getu are available before run()
  g.pictureInfo = function (pic) {
    if (pic === 0) return { count: 12, release: 7 };
    return { width: 100, height: 50 };
  };
  // Assert the catalogue word order the dispatcher must use: count -> arr[0], release -> arr[2] (byte +2).
  var arr = 0x30;
  var pinfo = g.pictureInfo(0);
  g.put(arr, pinfo.count & 0xffff); g.put((arr + 2) & 0xffff, pinfo.release & 0xffff);
  ok(g.getu(arr) === 12 && g.getu(arr + 2) === 7, "picture_data(0) catalogue: count then release");
})();

if (fails) throw new Error(fails + " v6 test(s) failed");
print("V6 OK");
