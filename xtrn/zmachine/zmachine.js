// JSZM Synchronet door — Z-machine v3 interactive-fiction player.
//
// Run as an xtrn / door under the TERMINAL SERVER, where `console` is the
// connected user's live terminal. Invoke with a FULL path so it is found (a JS
// module run via ;exec/xtrn resolves bare names against exec/ or mods/, not
// xtrn/). With no argument it shows a chooser (console.uselect) of the v3 .z3
// story files curated in the games/ subdirectory; or pass a story file to play
// it directly:
//     ;exec ?/sbbs/xtrn/zmachine/zmachine.js                     (choose from games/)
//     ;exec ?/sbbs/xtrn/zmachine/zmachine.js /path/to/story.z3   (play directly)
//
// Two render modes (ANSI terminals), switchable in-game:
//   * Line scrolling (default): console output with the status folded into the
//     prompt; preserves the terminal's native scrollback.
//   * ANSI full-screen (frame.js): pinned status line + scrolling output window
//     + (v3) SPLIT/SCREEN upper window (e.g. Seastalker's sonar). Auto-engaged
//     when a game splits a window; or forced with #ansi.
//   In-game commands:  #display (toggle)   #ansi   #scroll
//   Diagnostics:        #split / #unsplit  (force a test upper window)
//   PETSCII / non-ANSI terminals always use line scrolling.
//
// `console` is unbound under bare jsexec, so all side effects live inside
// jszm_door_main(), gated by JSZM_DOOR_NO_MAIN so the file can be syntax-checked
// headless:  jsexec -r 'var JSZM_DOOR_NO_MAIN=true; load("<abs>/zmachine.js"); print("ok");'

// Auto-resume persistence, decided by the try/finally around game.run():
//   * interrupted -- disconnect / idle-hangup / operator-terminate, or a crash --
//     flush the checkpoint so the next visit resumes where you left off;
//   * natural game-end while still connected -- QUIT / death / victory --
//     discard the slot so the next visit starts a fresh game (no resuming into a
//     quit/death/win prompt).
// js.on_exit is only a BACKSTOP for an operator-terminate that skips finally; it's
// registered at the bottom of this file (top-level == the door's js_scope) because
// js.on_exit evaluates its string OUTSIDE jszm_door_main()'s scope -- a handler
// nested in that function would be unreachable at exit and silently persist
// nothing. So the handler and its state live at top level.
var jszm_resume_game = null;     // JSZM instance whose .checkpoint we persist
var jszm_resume_path = null;     // destination per-user/story .resume file
var jszm_resume_screen = "";     // narrative text of the current prompt (reprinted on resume)
var jszm_resume_upper = "";      // upper-window (status bar) model at the current prompt (repainted on resume)
var jszm_resume_pics = "";       // v6 draw_picture calls of the current screen (replayed on resume to redraw graphics)
var jszm_resume_windows = "";    // v6 window geometry (game.windows) -- not carried by quetzal, restored on resume
var jszm_resume_done = false;    // game ended naturally -> the on_exit backstop must NOT re-create the slot
var jszm_quetzal = null;         // quetzal codec, captured from jszm_door_main's scope: load() puts `quetzal`
                                 // in that function's scope, so this top-level flush can't see it otherwise
                                 // (a ReferenceError there silently lost every disconnect's resume slot).
function jszm_flush_resume() {
  if (jszm_resume_done) return;
  if (!jszm_resume_game || !jszm_resume_game.checkpoint || !jszm_resume_path || !jszm_quetzal) return;
  try {
    // checkpoint is already a Quetzal FORM; splice the screen recap into an Rcap chunk,
    // and the upper-window (status bar) model into a Ucap chunk (both ignored by deserialize).
    var screen = jszm_resume_screen || "";
    var rcap = [], i;
    for (i = 0; i < screen.length; i++) rcap.push(screen.charCodeAt(i) & 255);
    var blob = jszm_quetzal.addChunk([].slice.call(jszm_resume_game.checkpoint), "Rcap", rcap);
    var upper = jszm_resume_upper || "";
    if (upper.length) {
      var ucap = [];
      for (i = 0; i < upper.length; i++) ucap.push(upper.charCodeAt(i) & 255);
      blob = jszm_quetzal.addChunk(blob, "Ucap", ucap);
    }
    var pics = jszm_resume_pics || "";                 // v6 picture calls -> replayed on resume to redraw the graphics
    if (pics.length) {
      var pcap = [];
      for (i = 0; i < pics.length; i++) pcap.push(pics.charCodeAt(i) & 255);
      blob = jszm_quetzal.addChunk(blob, "Pcap", pcap);
    }
    var wins = jszm_resume_windows || "";              // v6 window geometry -> restored on resume (text inset + origins)
    if (wins.length) {
      var wcap = [];
      for (i = 0; i < wins.length; i++) wcap.push(wins.charCodeAt(i) & 255);
      blob = jszm_quetzal.addChunk(blob, "Wcap", wcap);
    }
    var f = new File(jszm_resume_path);
    if (f.open("wb")) { f.writeBin(blob, 1); f.close(); }
  } catch (e) {}
}
function jszm_remove_resume() {                   // natural game-end -> next visit starts fresh
  if (!jszm_resume_path) return;
  try { if (file_exists(jszm_resume_path)) file_remove(jszm_resume_path); } catch (e) {}
}

// ----- Colour / text-style translation (pure; unit-tested in test/colour.js) -----
// Z colour (2..9) -> Synchronet Ctrl-A foreground letter / background digit.
// 0 = "current" (no change), 1 = "default" (terminal normal); handled by the caller.
var JSZM_ZFG = { 2: "k", 3: "r", 4: "g", 5: "y", 6: "b", 7: "m", 8: "c", 9: "w" };
var JSZM_ZBG = { 2: "0", 3: "1", 4: "2", 5: "3", 6: "4", 7: "5", 8: "6", 9: "7" };
var JSZM_ITALIC_FG = "c";    // cyan tint approximates italic (no Ctrl-A italic code)

// Translate an attribute state {fg,bg,bold,reverse,italic} to a Ctrl-A prefix
// string. fg/bg are Z colour numbers (0 = terminal default, 2..9 = palette).
function jszm_attr_prefix(a) {
  if (a.fg === 0 && a.bg === 0 && !a.bold && !a.reverse && !a.italic) return "\x01n";
  var fgC = a.italic ? JSZM_ITALIC_FG : (JSZM_ZFG[a.fg] || "w");   // default fg -> lightgray
  var bgN = JSZM_ZBG[a.bg] || "0";                                  // default bg -> black
  if (a.reverse) {                       // no inverse Ctrl-A: swap fg/bg ourselves
    // reverse wins over the italic tint: both want the fg slot, can't show both.
    fgC = JSZM_ZFG[a.bg] || "k";         // old bg colour becomes fg (default bg black -> k)
    bgN = JSZM_ZBG[a.fg] || "7";         // old fg colour becomes bg (default fg white -> 7)
  }
  var s = "\x01n\x01" + bgN;             // normal reset, then background digit
  if (a.bold) s += "\x01h";              // high intensity = the only source of brightness
  s += "\x01" + fgC;                     // foreground letter last
  return s;
}

// Apply a set_colour(fg,bg) to an attr state. Per Z-spec 8.3.1: 0 = "current"
// (leave channel unchanged), 1 = "default" (our 0 = terminal normal), 2..9 = palette.
function jszm_apply_colour(a, fg, bg) {
  if (fg === 1) a.fg = 0; else if (fg >= 2) a.fg = fg;
  if (bg === 1) a.bg = 0; else if (bg >= 2) a.bg = bg;
}

// Apply a set_text_style(style) to an attr state. 0 = Roman (clear all); otherwise
// OR the requested style bits in (1=reverse, 2=bold, 4=italic; 8=fixed-pitch ignored,
// the terminal is already monospace). Z-spec 8.7.1: later settings combine; Roman cancels all.
function jszm_apply_style(a, style) {
  if (style === 0) { a.bold = a.reverse = a.italic = false; return; }
  if (style & 1) a.reverse = true;
  if (style & 2) a.bold = true;
  if (style & 4) a.italic = true;
}

// ----- Timed-input support (top-level so the idle policy is unit-testable) -----
var JSZM_TIMEOUT = -1;                 // readTimedKey timeout sentinel (negative; engine tests k < 0)
var jszm_last_input_activity = 0;      // Unix time of the last real keypress (timed or untimed)
var jszm_idle_warned = false;          // emitted the "are you there" warning this idle span?

// Pure idle policy: given seconds idle and the thresholds, decide whether to warn
// and/or hang up. Mirrors C getkey()'s inactivity handling (which inkey does NOT do).
function jszm_idle_decision(idle, warn, maxi, warned) {
  return { warn: (warn > 0 && !warned && idle >= warn), hangup: (maxi > 0 && idle >= maxi) };
}

// Reset the idle clock on real input. Also syncs Synchronet's own getkey clock so
// any later blocking read starts fresh. Runtime-only (console/time); never called headless.
function jszm_mark_activity() {
  jszm_last_input_activity = time();
  jszm_idle_warned = false;
  try { console.last_getkey_activity = jszm_last_input_activity; } catch (e) {}
}

// ---- game-title resolution (real titles for the chooser, instead of prettified
// filenames). A Z-machine file has no title field and its runtime banner has no
// consistent structure to scrape, but the file DOES carry its Treaty-of-Babel
// identity, and IFDB (ifdb.org) is the authoritative database keyed by that IFID.
// The door computes each game's IFID, looks the title up once via IFDB's public
// iFiction API, and caches it in games/titles.ini; offline / not-found -> prettyName.
// The functions below are the pure (network-free, testable) pieces. ----

// Candidate IFIDs for a Z-code story, tried in order (first IFDB hit wins). Newer
// Inform games embed their own UUID IFID, which IFDB indexes instead of the computed
// form; Infocom/older games use ZCODE-release-serial, with OR without the -checksum
// suffix (IFDB accepts both). Non-alphanumeric serial bytes become '-' per the Treaty.
function jszm_zcodeIfids(release, serial, csumHex, uuid) {
  var ser = "", i, c;
  for (i = 0; i < serial.length; i++) { c = serial.charAt(i); ser += /[0-9A-Za-z]/.test(c) ? c : "-"; }
  var ids = [];
  if (uuid) ids.push(String(uuid).toUpperCase());
  ids.push("ZCODE-" + release + "-" + ser + "-" + csumHex);
  ids.push("ZCODE-" + release + "-" + ser);
  return ids;
}

// Decode the few XML entities iFiction uses, and collapse whitespace.
function jszm_xmlText(s) {
  s = String(s).replace(/&#(\d+);/g, function (m, n) { return String.fromCharCode(parseInt(n, 10)); })
               .replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"')
               .replace(/&apos;/g, "'").replace(/&amp;/g, "&");
  return s.replace(/\s+/g, " ").replace(/^\s+|\s+$/g, "");
}

// The bibliographic <title> from an IFDB iFiction XML record ("" if absent). Anchored
// on <bibliographic> so it ignores the <format><title>Story File</title> sub-element.
function jszm_parseIfdbTitle(xml) {
  if (!xml) return "";
  var m = String(xml).match(/<bibliographic>[\s\S]*?<title>([\s\S]*?)<\/title>/i);
  return m ? jszm_xmlText(m[1]) : "";
}

// Parse the title cache ("<filename> = <title>" lines; '#'/';' comments) -> { name: title }.
// An empty value is a negative cache entry (looked up, IFDB had nothing -> use prettyName,
// don't re-query); a sysop can fill it in to override.
function jszm_parseTitleCache(text) {
  var out = {}, lines = String(text).split(/\r?\n/), i, line, eq, k, v;
  for (i = 0; i < lines.length; i++) {
    line = lines[i].replace(/^\s+/, "");
    if (!line.length || line.charAt(0) == "#" || line.charAt(0) == ";") continue;
    eq = line.indexOf("=");
    if (eq < 0) continue;
    k = line.substring(0, eq).replace(/\s+$/, "");
    v = line.substring(eq + 1).replace(/^\s+|\s+$/g, "");
    if (k.length) out[k] = v;
  }
  return out;
}

// Build chooser display labels "<title> (v<ver>[, release <rel>[ / <serial>]])". The
// release (and the serial, only if two files also share a release) is appended just to
// the entries whose title collides, so unique titles stay clean. Mutates each entry with
// a .label; returns the list sorted by title, then release, then serial.
function jszm_labelStories(list) {
  var tc = {}, trc = {}, i, e, rk;
  for (i = 0; i < list.length; i++) {
    e = list[i];
    tc[e.title] = (tc[e.title] || 0) + 1;
    rk = e.title + "\x00" + e.rel; trc[rk] = (trc[rk] || 0) + 1;
  }
  for (i = 0; i < list.length; i++) {
    e = list[i];
    var extra = "";
    if (tc[e.title] > 1) {
      extra = ", release " + e.rel;
      if (trc[e.title + "\x00" + e.rel] > 1) extra += " / " + String(e.serial).replace(/[^\x20-\x7e]/g, "");
    }
    e.label = e.title + " (v" + e.ver + extra + ")";
  }
  list.sort(function (a, b) {
    var an = a.title.toLowerCase(), bn = b.title.toLowerCase();
    if (an < bn) return -1;
    if (an > bn) return 1;
    if (a.rel != b.rel) return a.rel - b.rel;
    return a.serial < b.serial ? -1 : a.serial > b.serial ? 1 : 0;
  });
  return list;
}

function jszm_door_main() {
  load("sbbsdefs.js");           // K_NONE
  require("userdefs.js", "USER_ANSI"); // USER_ANSI, USER_PETSCII -- require() (idempotent), NOT load():
                                       // sbbsdefs.js already require()s userdefs.js, so a 2nd load() re-executes
                                       // it -> USER_DELETED redeclaration error on stricter engines (harmless on SM1.8.5)
  load("cga_defs.js");           // BG_BLUE, WHITE, LIGHTGRAY
  load("key_defs.js");           // CTRL_R (forced-redraw key) and friends
  load(js.exec_dir + "quetzal.js"); // Quetzal codec (must precede jszm.js)
  jszm_quetzal = quetzal;            // expose to the top-level jszm_flush_resume (out of this scope)
  load(js.exec_dir + "jszm.js");    // JSZM
  load(js.exec_dir + "v6pics.js"); // v6 picture runtime (manifest loader + SyncTERM blit bridge)
  load(js.exec_dir + "viewport.js"); // v6 viewport helper (integer-scale + centering)
  load(js.exec_dir + "zsound.js"); // @sound_effect player (Blorb Snd + SyncTERM audio APCs / BEL)

  console.clear();
  if (file_exists(js.exec_dir + "intro.msg"))     // optional sysop splash (cf. utopia.js)
    console.printfile(js.exec_dir + "intro.msg", P_NOPAUSE);   // render as-is; no pause mid-display
  // ---- attribution (always shown). No explicit pause: the intro + credits fill the screen, so the
  //      chooser's console.clear() does a read-before-clear pause on its own (one [Hit a key]) at the
  //      cursor -- i.e. just below the credits, the player's chosen placement on every terminal. ----
  console.print("\r\n\x01n\x01cZ-Machine: \x01h\x01wInfocom\x01n\x01c & \x01h\x01wGraham Nelson\x01n\x01c | JSZM: \x01h\x01wzzo38\x01n\x01c | Quetzal: \x01h\x01wMartin Frost\x01n\r\n");
  console.print("\x01n\x01cSynchronet door/ES5 port: \x01h\x01wRob Swindell\x01n\x01c (Digital Man) | Games (c) their authors\x01n\r\n");

  // Game selection and per-game play happen in the category/game loop near the bottom of
  // this function (story id + story load are computed per game there). A single file
  // argument bypasses the menus and plays directly.

  function readStory(p) {
    var f = new File(p);
    if (!f.open("rb")) { writeln("cannot open " + p); exit(1); }
    var bytes = f.readBin(1, f.length);
    f.close();
    return new Uint8Array(bytes);
  }
  // Optional per-game intro/help splash: a "<storyfile>.msg" sidecar beside the story (extension
  // swapped, e.g. fantasy/arthur-r74-s890714.z6 -> fantasy/arthur-r74-s890714.msg), mirroring the
  // ".gfx" picture-cache convention. Shown on a clean screen with a pause AFTER the game is chosen
  // and BEFORE the engine takes over -- the place to surface game-specific key bindings (e.g. the
  // graphical v6 function keys) that BBS players can't learn from the boxed manual they don't have.
  // Sysop adds the file to enable it; absent -> nothing shown.
  function showGameIntro(path) {
    var p = path.replace(/\.[zZ]\d$/, "") + ".msg";
    if (!file_exists(p)) return false;
    // Clear the prior screen (splash/credits at launch, or the game on a Ctrl-K recall). console.clear()'s
    // read-before-clear auto-pause holds it with one [Hit a key] AT THE CURSOR first. This MUST come before the
    // geometry reset below: that reset ends in DECSTBM (\x1b[r), which homes the real terminal cursor -- but as raw
    // console.write() output it leaves Synchronet's row tracker stale, so doing it first made the read-before-clear
    // pause print [Hit a key] at the screen top (row 1), over the art, on non-SyncTERM terminals (cterm stays in sync).
    console.clear();
    // Now drop any leftover v6 terminal geometry on the cleared screen so the intro paints full-width: clear() wipes
    // text but NOT the DECSLRM left/right margins, the DECSTBM scroll region, or autowrap-off that v6 play leaves set
    // -- so a Ctrl-K recall mid-game rendered the splash inside the game's inset (top rows shifted). Reset
    // margins+region+autowrap (no-op at launch, where nothing is set). The follow-up forceRedraw() re-arms the region.
    if (console.term_supports(USER_ANSI))
      console.write("\x1b[?69h\x1b[1;" + (console.screen_columns || 80) + "s\x1b[?69l\x1b[?7h\x1b[r");
    console.printfile(p);   // default flags: pages a longer-than-screen intro on its own
    console.newline();
    console.pause(false);   // one [Hit a key] so the player reads it before play; set_abort=false so a Quit/Abort
                            // key (e.g. 'N') at THIS prompt doesn't set console.aborted.
    // A multi-page intro is paged by console.printfile()/console.clear()'s own [Hit a key] prompts, and a
    // Quit/Abort key ('N', 'Q', Ctrl-C) at one of THOSE sets console.aborted -- which then aborts the game's
    // first read/output (the screen "displays nothing" before the restore prompt). Clear it so the abort from
    // reading the splash never leaks into play.
    console.aborted = false;
    return true;            // displayed -> the on-demand recall (Ctrl-K) knows to redraw the game after
  }
  // Turn a story-file path into a menu title, e.g. ".../sea_stalker.z3" -> "Sea Stalker".
  function prettyName(path) {
    var n = file_getname(path).replace(/\.[^.]*$/, "");
    n = n.replace(/[_\-]+/g, " ").replace(/\s+/g, " ");
    return n.replace(/\b[a-z]/g, function(c) { return c.toUpperCase(); });
  }
  // ---- title cache + IFDB lookup (door-runtime glue around the pure helpers above) ----
  var jszmHTTP = null;                                   // captured HTTPRequest ctor (load() scopes it here)
  function jszm_httpClass() { if (!jszmHTTP) { load("http.js"); jszmHTTP = HTTPRequest; } return jszmHTTP; }
  function jszm_titlesPath(dir) { return dir + "titles.ini"; }
  function jszm_loadTitleCache(dir) {
    var f = new File(jszm_titlesPath(dir)), txt = "";
    if (f.open("rb")) { txt = f.read(); f.close(); }
    return jszm_parseTitleCache(txt);
  }
  function jszm_appendTitle(dir, name, title) {          // persist one resolved (or negative) entry
    var p = jszm_titlesPath(dir), fresh = !file_exists(p), f = new File(p);
    if (!f.open(fresh ? "wb" : "ab")) return;
    if (fresh) f.write("# Z-machine game titles (filename = title), resolved from IFDB by IFID.\n" +
                       "# Delete a line to re-query; edit a value to override the IFDB title.\n");
    f.write(name + " = " + title + "\n");
    f.close();
  }
  function jszm_ifdbTitle(ifid) {                        // one IFDB lookup; throws on a network error
    var H = jszm_httpClass(), h = new H(undefined, undefined, undefined, 10000);   // 10s recv timeout
    return jszm_parseIfdbTitle(h.Get("https://ifdb.org/viewgame?ifiction&ifid=" + ifid));
  }
  // Resolve a title for the already-open story file `f` via IFDB ("" if no record). Reads
  // the release/serial/checksum header fields and scans for an embedded UUID, then tries
  // each candidate IFID. Throws (network error) -> caller marks IFDB unreachable this run.
  function jszm_lookupTitle(f) {
    var n = f.length, i;
    f.position = 0x02; var rel = (f.readBin(1, 1) << 8) | f.readBin(1, 1);          // release word
    f.position = 0x12; var sb = f.readBin(1, 6), ser = "", s;                       // serial code
    for (s = 0; s < sb.length; s++) ser += String.fromCharCode(sb[s] & 255);
    f.position = 0x1c; var cs = (f.readBin(1, 1) << 8) | f.readBin(1, 1);           // checksum word
    var csh = ("000" + cs.toString(16).toUpperCase()).slice(-4);
    f.position = 0; var b = f.readBin(1, n), parts = [];                            // whole file -> scan for UUID
    for (i = 0; i < n; i += 8192) parts.push(String.fromCharCode.apply(null, b.slice(i, i + 8192)));
    var um = parts.join("").match(/([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12})/);
    var ids = jszm_zcodeIfids(rel, ser, csh, um ? um[1] : null), k, t;
    for (k = 0; k < ids.length; k++) { t = jszm_ifdbTitle(ids[k]); if (t) return t; }
    return "";
  }
  // Story files in `dir` our engine can run, with their version byte and real title (IFDB,
  // cached in titles.ini; prettyName fallback). Returns [{ path, ver, title }], sorted by title.
  function jszm_storyList(dir) {
    var exts = ["*.z3", "*.z4", "*.z5", "*.z6", "*.z8"], out = [], i, j;
    var cache = jszm_loadTitleCache(dir), ifdbDown = false, announced = false;
    for (i = 0; i < exts.length; i++) {
      var files = directory(dir + exts[i]);
      for (j = 0; j < files.length; j++) {
        var path = files[j], name = file_getname(path);
        var f = new File(path);
        if (!f.open("rb")) continue;
        var ver = f.readBin(1, 1);                       // header byte 0: Z-machine version
        if (ver != 3 && ver != 4 && ver != 5 && ver != 6 && ver != 8) { f.close(); continue; }
        f.position = 0x02; var rel = (f.readBin(1, 1) << 8) | f.readBin(1, 1);   // release word (disambiguates dup titles)
        f.position = 0x12; var sb2 = f.readBin(1, 6), serial = "", s2;           // serial code
        for (s2 = 0; s2 < sb2.length; s2++) serial += String.fromCharCode(sb2[s2] & 255);
        var title = cache.hasOwnProperty(name) ? cache[name] : undefined;
        if (title === undefined && !ifdbDown) {          // not cached -> resolve via IFDB (once)
          if (!announced) { console.line_counter = 0; console.print("\x01n\r\nLooking up titles for newly-added games from IFDB (cached once found)...\r\n"); announced = true; }
          try { title = jszm_lookupTitle(f); } catch (e) { ifdbDown = true; }
          if (title !== undefined) { cache[name] = title; jszm_appendTitle(dir, name, title); }
        }
        f.close();
        if (!title) title = prettyName(path);            // empty/negative/offline -> prettified filename
        out.push({ path: path, ver: ver, title: title, rel: rel, serial: serial });
      }
    }
    return jszm_labelStories(out);                        // sort + disambiguate same-title entries by release
  }
  var jszm_lastStory = null;             // last game played -> default selection on return to the menu
  var jszm_lastCat = null;               // last category used -> default selection in the category menu
  // Game chooser for one category directory: list its v3/4/5/8 story files via uselect.
  // `catName` (when categories are in use) is appended to the menu title. NOTE: uselect
  // prepends "Select " to the title, so we pass "a Game[...]", not "Select a Game".
  function chooseGame(dir, catName) {
    var games = jszm_storyList(dir);
    if (!games.length) {
      console.print("\r\nNo Z-machine story files found in " + dir + "\r\n");
      return null;
    }
    console.clear();                     // read-before-clear pause shows the prior screen (intro/credits) if full
    var title = "a Game" + (catName ? " - " + catName : "");
    var dflt = 0;
    for (var i = 0; i < games.length; i++) {
      console.uselect(i, title, games[i].label);
      if (games[i].path === jszm_lastStory) dflt = i;   // pre-select the last game played (uselect default)
    }
    var sel = console.uselect(dflt);
    if (js.terminated || !bbs.online) return null;
    return (sel >= 0 && sel < games.length) ? games[sel].path : null;
  }
  function countGames(dir) {             // cheap count for the category menu (no header reads / IFDB)
    var exts = ["*.z3", "*.z4", "*.z5", "*.z6", "*.z8"], n = 0, i;
    for (i = 0; i < exts.length; i++) n += directory(dir + exts[i]).length;
    return n;
  }
  function prettyDir(dir) {              // ".../infocom-classics/" -> "Infocom Classics"
    var n = String(dir).replace(/[\/\\]+$/, "").replace(/^.*[\/\\]/, "");
    n = n.replace(/[_\-]+/g, " ").replace(/\s+/g, " ");
    n = n.replace(/\b[a-z]/g, function (c) { return c.toUpperCase(); });
    return n.length ? n : String(dir);
  }
  function chooseCategory(cats) {        // first-level menu (multiple game dirs); null = Quit -> exit
    console.clear();                     // read-before-clear pause shows the intro/credits screen if full
    var dflt = 0;
    for (var i = 0; i < cats.length; i++) {
      console.uselect(i, "a Category",   // uselect prepends "Select " -> "Select a Category:"
                      cats[i].name + " (" + cats[i].count + " game" + (cats[i].count == 1 ? "" : "s") + ")");
      if (cats[i].dir === jszm_lastCat) dflt = i;        // pre-select the last category used
    }
    var sel = console.uselect(dflt);
    if (js.terminated || !bbs.online) return null;
    return (sel >= 0 && sel < cats.length) ? cats[sel] : null;
  }
  function pad(s, n) {                 // SpiderMonkey 1.8.5 has no String.repeat
    while (s.length < n) s += " ";
    return s.substr(0, n);
  }
  function trim(s) { return (s == null ? "" : String(s)).replace(/^\s+|\s+$/g, ""); }
  // Wrap text to `width` into visual lines using the global word_wrap() built-in,
  // honoring the starting column. Returns [{t:line, nl:newline-follows?}, ...]
  // (nl is false only for a trailing partial line, so callers don't add a stray break).
  function wrapLines(text, width, startCol) {
    var res = [], paras = String(text).split("\n");
    if (!(startCol > 0)) startCol = 0;
    for (var p = 0; p < paras.length; p++) {
      var col = (p == 0) ? startCol : 0;
      var wr = word_wrap((col > 0 ? pad("", col) : "") + paras[p], width);
      if (wr.length && wr.charAt(wr.length - 1) == "\n") wr = wr.substr(0, wr.length - 1);
      var vl = wr.split("\n");
      if (col > 0 && vl.length) vl[0] = vl[0].substr(col);
      for (var k = 0; k < vl.length; k++)
        res.push({ t: vl[k], nl: !(k == vl.length - 1 && p == paras.length - 1) });
    }
    return res;
  }
  function statusRight(v18, v17) {
    return game.statusType
      ? ("Time: " + v18 + ":" + (v17 < 10 ? "0" : "") + v17)
      : ("Score: " + v18 + "  Moves: " + v17);
  }
  // UTF-8-encode a string of Unicode CODE POINTS. The engine produces real code points
  // (Tasks 1-3), so each char must be encoded as Unicode -- NOT via utf8_encode(string),
  // which treats an all-<=0xFF string as CP437 (e.g. U+00E9 'e' would mis-encode to the
  // CP437 glyph at 0xE9). utf8_encode(NUMBER) always encodes a true code point, so encode
  // per char and concatenate; ASCII passes through 1:1.
  function jszm_utf8(text) {
    var out = "", i, c;
    for (i = 0; i < text.length; i++) {
      c = text.charCodeAt(i);
      out += (c < 128) ? text.charAt(i) : utf8_encode(c);
    }
    return out;
  }
  // Print game text to the console, choosing the encoding: ASCII-only text goes out plain
  // (CP437 default, unchanged); text with any code point > 127 is UTF-8-encoded and tagged
  // P_UTF8 so Synchronet down-converts to the user's charset ('?' for unmappable). `extra`
  // is OR'd into the print mode (callers pass nothing or additional P_* flags).
  function jszm_emit(text, extra) {
    var hi = false, i;
    for (i = 0; i < text.length; i++) { if (text.charCodeAt(i) > 127) { hi = true; break; } }
    if (hi) console.print(jszm_utf8(text), P_UTF8 | (extra || 0));
    else console.print(text, extra || 0);
  }

  // Wrap a narrative run in the current Ctrl-A attribute prefix and a trailing
  // normal reset (so a coloured background never smears past the run / line end).
  function styled(text) { return jszm_attr_prefix(attr) + text + "\x01n"; }

  // ---- arg parsing: each directory argument is a category; a single file plays directly.
  // Relative arguments resolve against the door's directory (so an xtrn command line like
  // "zmachine.js games ifiction" works regardless of the node's cwd). ----
  function jszm_resolve(p) { return /^([\/\\]|[A-Za-z]:)/.test(p) ? p : (js.exec_dir + p); }
  var jszm_args = [], jszm_ai;
  for (jszm_ai = 0; jszm_ai < argv.length; jszm_ai++)
    if (argv[jszm_ai] && String(argv[jszm_ai]).length) jszm_args.push(jszm_resolve(String(argv[jszm_ai])));
  var jszm_directFile = (jszm_args.length === 1 && file_exists(jszm_args[0]) && !file_isdir(jszm_args[0]))
                          ? jszm_args[0] : null;
  var jszm_cats = [];
  if (!jszm_directFile) {
    var jszm_dirs = [], jszm_di;
    for (jszm_di = 0; jszm_di < jszm_args.length; jszm_di++)
      if (file_isdir(jszm_args[jszm_di])) jszm_dirs.push(String(jszm_args[jszm_di]).replace(/[\/\\]*$/, "") + "/");
    if (!jszm_dirs.length) {
      // No directory arguments -> auto-discover: every subdirectory of the door's own dir
      // becomes a candidate category (those without z-code files are dropped by the count
      // below). Lets a sysop just drop game dirs next to the door with no command-line config.
      var jszm_ents = directory(js.exec_dir + "*"), jszm_ei;
      for (jszm_ei = 0; jszm_ei < jszm_ents.length; jszm_ei++)
        if (file_isdir(jszm_ents[jszm_ei])) jszm_dirs.push(String(jszm_ents[jszm_ei]).replace(/[\/\\]*$/, "") + "/");
      jszm_dirs.sort();   // stable, alphabetical category order (explicit args keep their given order)
    }
    var jszm_namedCats = true;   // categories always come from directory names now
    for (jszm_di = 0; jszm_di < jszm_dirs.length; jszm_di++) {
      var jszm_gc = countGames(jszm_dirs[jszm_di]);
      if (jszm_gc > 0) jszm_cats.push({ dir: jszm_dirs[jszm_di], name: prettyDir(jszm_dirs[jszm_di]), count: jszm_gc });
    }
    if (!jszm_cats.length) { console.print("\r\nNo Z-machine story files found.\r\n"); return; }
  }

  // ---- play loop: category -> game -> play. A natural game-end returns to the game menu;
  // Quit there returns to the category menu (or exits the door when there's one category). A
  // direct file argument plays once. A disconnect mid-game exits (the resume slot is kept). ----
  var jszm_cat = null;
  for (;;) {
    var storyPath;
    if (jszm_directFile) {
      storyPath = jszm_directFile;
    } else {
      if (!jszm_cat) {
        jszm_cat = (jszm_cats.length > 1) ? chooseCategory(jszm_cats) : jszm_cats[0];
        if (!jszm_cat) break;
        jszm_lastCat = jszm_cat.dir;     // remember the category now -- even if no game is played
      }
      storyPath = chooseGame(jszm_cat.dir, jszm_namedCats ? jszm_cat.name : null);
      if (!storyPath) { if (jszm_cats.length <= 1) break; jszm_cat = null; continue; }   // Quit: up a level
      jszm_lastStory = storyPath;        // remember the last game for the game-menu default
    }
    var storyId = file_getname(storyPath).replace(/\.[^.]*$/, "")
                    .toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
    if (!storyId.length) storyId = "story";
    showGameIntro(storyPath);   // optional per-game key/help splash before the engine takes the screen
    var game = new JSZM(readStory(storyPath));
    game.screenRows = console.screen_rows || 24;
    game.screenCols = console.screen_columns || 80;
    var ansiCapable = console.term_supports(USER_ANSI) && !console.term_supports(USER_PETSCII);
    if (game.version === 6) {
      // v6 lays out windows in PIXELS. For TEXT play we do NOT query the terminal for exact pixel
      // geometry: cterm_lib's query_fontdims/query_graphicsdim each block up to 3s waiting for a
      // response some terminals never send -> a multi-second startup stall. Derive the cell size from
      // the row count (the charheight() fallback table) + a standard 8px graphics cell width; the
      // renderer quantizes back with the SAME cell size, so the window layout stays self-consistent.
      // Plan B (pictures) can do the precise query_graphicsdim() probe lazily, only when graphics are on.
      // EXCEPTION: a non-cterm Sixel terminal is probed up front (DA1 attribute 4) and, if Sixel-capable,
      // asked for its real text-cell pixel size (CSI 16t). Sixel pictures must scale UNIFORMLY (or they
      // distort), so the logical cell-height is derived from the terminal's actual cell ASPECT rather than
      // the row count. cterm/SyncTERM (own cell model) is not probed here -> no stall.
      var v6cterm = load({}, "cterm_lib.js");
      var v6sixelOk = false, v6devCW = 0, v6devCH = 0;
      if (ansiCapable && !console.cterm_version && typeof v6cterm.query === "function") {
        var v6da0 = v6cterm.query("\x1b[c");                          // Primary Device Attributes
        if (v6da0 && /^\x1b\[\?/.test(v6da0)) {
          var v6dp = v6da0.replace(/^\x1b\[\?/, "").replace(/c$/, "").split(";"), v6i0;
          for (v6i0 = 0; v6i0 < v6dp.length; v6i0++) if (v6dp[v6i0] === "4") { v6sixelOk = true; break; }
        }
        if (v6sixelOk) {                                              // only a Sixel terminal gets the cell-size query
          var v6cqm = /\x1b\[6;(\d+);(\d+)t/.exec(v6cterm.query("\x1b[16t") || "");   // CSI 16t -> CSI 6;height;width t
          if (v6cqm) { v6devCH = parseInt(v6cqm[1], 10); v6devCW = parseInt(v6cqm[2], 10); }
        }
      }
      var v6r = game.screenRows, v6cw = 8;
      var v6ch = (v6sixelOk && v6devCW > 0 && v6devCH > 0) ? Math.round(v6cw * v6devCH / v6devCW)
                 : ((v6r === 27 || v6r === 28 || v6r === 33 || v6r === 34) ? 14 : (v6r <= 30 ? 16 : 8));
      game.fontWidth = v6cw; game.fontHeight = v6ch;
      // Scaled viewport: derive native width/height from the widest/tallest picture, pick an integer
      // scale that fills the terminal (2x on 80-col SyncTERM), and report that scaled screen so Arthur
      // lays out at scale and full-width. v6vp.offsetPx centres it; assets/picture_data are scaled by S.
      var v6termPx = game.screenCols * v6cw;
      var v6natW = 0, v6natH = 0, v6mf0 = JSZM_loadManifest(storyPath.replace(/\.[zZ]\d$/, "") + ".gfx");
      if (v6mf0 && v6mf0.pics) { for (var v6pn in v6mf0.pics) { var pw = v6mf0.pics[v6pn].w || 0, ph = v6mf0.pics[v6pn].h || 0; if (pw > v6natW) v6natW = pw; if (ph > v6natH) v6natH = ph; } }
      var v6vp = JSZM_v6Viewport(v6termPx, game.screenCols, v6cw, v6r, v6ch, v6natW, v6natH);
      var v6scale = v6vp.scale, v6offsetPx = v6vp.offsetPx, v6offsetCols = v6vp.offsetCols;
      game.screenCols = v6vp.cols;
      game.screenWidthPx = v6vp.widthPx; game.screenHeightPx = v6r * v6ch;
      // v6 games are authored for a fixed NATIVE screen (MCGA 320x200; Arthur's art is 320x196). If we report
      // the full terminal height, a terminal taller than the scaled-native height (e.g. SyncTERM 80x30 = 480px
      // vs Arthur's 2x-native 400px) makes Arthur lay out for the extra rows -- it re-centres/repositions graphics
      // and leaks window-0's room text into the now-uncovered top. Clamp the reported screen to the scaled-native
      // height (letterbox the extra rows at the bottom) so the layout matches the validated 80x25 case.
      // Applies to BOTH cterm/SyncTERM and the Sixel/WT path: the Sixel path previously ran unclamped, which on a
      // tall window made Arthur spread the F2 map (native y=1, top) and the room scene (mid-screen) into separate
      // bands -- toggling to the map then left the cleared room band as a black void below the parchment.
      // 200 is the standard v6 native height; move to a per-game manifest tunable when a 2nd v6 game is tuned.
      var v6nativeScreenH = 200, v6clampH = v6scale * v6nativeScreenH;
      if (v6clampH < game.screenHeightPx) {
        game.screenHeightPx = v6clampH;
        game.screenRows = Math.floor(v6clampH / v6ch);   // keep header lines (0x20) consistent with the clamped pixel height (0x24)
      }
      // Plan B: locate the pre-baked picture cache (<game>.gfx beside the story) and load its manifest.
      // The blit bridge + engine hooks are wired in Change C below, after the renderer is created.
      var v6dir = storyPath.replace(/\.[zZ]\d$/, "") + ".gfx";
      var v6manifest = JSZM_loadManifest(v6dir);
      // Position scale (native-authored games): Arthur lays out in the full device space (posScale 1); a
      // native-authored game (Zork Zero) lays out in 320-space and needs its picture positions + window
      // origins scaled up to fill the device. authoredWidth comes from the re-bake-safe <game>.gfx/layout
      // sidecar (absent -> device width -> posScale 1, Arthur unchanged).
      var v6layout = JSZM_v6LoadLayout(v6dir);
      var v6authoredW = v6layout.authoredWidth > 0 ? v6layout.authoredWidth : game.screenWidthPx;
      var v6posScale = Math.max(1, Math.round(game.screenWidthPx / (v6authoredW > 0 ? v6authoredW : game.screenWidthPx)));
      var v6borderFrame = v6layout.borderFrame;   // OPT IN per game; Arthur (no sidecar) never enters border-frame mode
      var v6windowedText = v6layout.windowedText;   // OPT IN per game: mirror the game's multi-window text layout (Journey)
      var v6tag = "zmachine/" + storyId;        // per-game client-side cache path prefix for C;S uploads
      game.graphicsAvailable = function () { return false; };   // text-safe default; the picture-bridge block below replaces it (and this also covers a bridge-build failure)
    }
    var rows = console.screen_rows || 24;
    var cols = console.screen_columns || 80;
    // ---- sound (@sound_effect) ----
    // Bleeps (sounds 1/2 -- Arthur, Zork Zero, Journey, Beyond Zork) work on ANY
    // terminal (SyncTERM Synth tone, else ASCII BEL); sampled sounds (The Lurking
    // Horror, Sherlock) additionally need SyncTERM with libsndfile and the game's
    // .blb beside the story (provisioned by getgames.js). The probe inside
    // JSZM_makeZSound only queries cterm/SyncTERM clients -- no stall elsewhere.
    var zsnd = JSZM_makeZSound({
      storyPath: storyPath,
      tag: "zmachine/" + storyId,
      cterm: (typeof v6cterm != "undefined" && v6cterm) ? v6cterm : null
    });
    game.sound = zsnd.sound;
    // ansiCapable computed earlier (before the v6 cell-aspect probe)
    jszm_last_input_activity = time();   // seed the idle clock for the timed-input path
    jszm_idle_warned = false;            // fresh idle span per game (loop reuses top-level state)

    // ---- save/restore (shared) ----
    // Synchronet per-user data file convention: data/user/<NNNN>.<ext> (user
    // number zero-padded to 4 digits via format("%04u", ...)).
    function savePath() { return system.data_dir + "user/" + format("%04u", user.number) + ".jszm." + storyId + ".save"; }
    game.save = function(buf) {
      try {
        var f = new File(savePath()); if (!f.open("wb")) return false;
        f.writeBin([].slice.call(buf), 1); f.close();
        game.print("\n[Saved]\n");
        return true;
      } catch (e) { return false; }
    };
    game.restore = function() {
      try {
        var f = new File(savePath()); if (!f.open("rb")) return null;
        var bytes = f.readBin(1, f.length); f.close();
        return new Uint8Array(bytes);
      } catch (e) { return null; }
    };

    // ---- auto-resume slot (last screen + checkpoint, per user/story) ----
    function resumePath() {
      return system.data_dir + "user/" + format("%04u", user.number) + ".jszm." + storyId + ".resume";
    }
    var screenBuf = "";             // narrative printed since the last prompt (the player's current screen)
    var attr = { fg: 0, bg: 0, bold: false, reverse: false, italic: false };   // current text attributes
    var gameWindow = 0;             // Z-machine window the game is writing to (0 = main/lower)
    var pendingResumeScreen = null; // screen text to reprint on the first prompt after a resume
    var pendingResumeUpper = null;  // upper-window (status bar) model to repaint on the first prompt after a resume
    var pendingResumePics = null;   // v6 picture calls to replay on the first prompt after a resume (redraw graphics)
    var pendingResumeWindows = null; // v6 window geometry (game.windows) to restore on the first prompt after a resume
    var v6replaying = false;        // true while replaying captured pictures (resume/forceRedraw): defer the per-
                                    // picture relayout so the text inset never lands at an intermediate (one-pole,
                                    // full-width) state that would clear over the side poles -- sync once at the end
    // Hand the engine + destination path to the global flush handler. The actual
    // write happens in the try/finally around game.run() (primary) and via the
    // top-level js.on_exit backstop -- see the header comment.
    jszm_resume_game = game;
    jszm_resume_path = resumePath();
    jszm_resume_done = false;   // fresh per game (the play loop reuses this top-level state)
    // Load any existing slot so jszm auto-resumes (deserialize rejects a foreign one);
    // stash its screen text to reprint at the first prompt so the player sees context.
    (function () {
      var f = new File(resumePath());
      if (!f.open("rb")) return;
      var raw = f.readBin(1, f.length); f.close();
      var bytes = new Uint8Array(raw);
      game.resumeData = bytes;                          // deserialize() ignores the Rcap chunk
      var rcap = quetzal.getChunk(raw, "Rcap");         // pull the screen recap back out
      if (rcap) {
        var s = "";
        for (var i = 0; i < rcap.length; i++) s += String.fromCharCode(rcap[i] & 255);
        pendingResumeScreen = s;
      }
      var ucap = quetzal.getChunk(raw, "Ucap");         // ...and the upper-window (status bar) model
      if (ucap) {
        var u = "";
        for (var j = 0; j < ucap.length; j++) u += String.fromCharCode(ucap[j] & 255);
        pendingResumeUpper = u;
      }
      var pcap = quetzal.getChunk(raw, "Pcap");         // ...and the v6 picture calls to replay
      if (pcap) {
        var p = "";
        for (var k = 0; k < pcap.length; k++) p += String.fromCharCode(pcap[k] & 255);
        pendingResumePics = p;
      }
      var wcap = quetzal.getChunk(raw, "Wcap");         // ...and the v6 window geometry to restore
      if (wcap) {
        var wn = "";
        for (var wi2 = 0; wi2 < wcap.length; wi2++) wn += String.fromCharCode(wcap[wi2] & 255);
        pendingResumeWindows = wn;
      }
    })();

    // ---- renderers + current-mode dispatch ----
    var selfManagedScreen = game.version >= 4;     // v4+ games draw their own status (no interpreter bar)
    // v4+ (self-managed screen) ANSI games use the status-scroll renderer (native scroll +
    // redrawn status bar); v3 upper-window games (Seastalker sonar) use the full-screen Frame.
    var frameR = ansiCapable
      ? (game.version === 6 ? makeV6Renderer()
         : selfManagedScreen ? makeStatusScrollRenderer()
         : makeFrameRenderer(false))
      : null;
    var scrollR = makeScrollRenderer();
    var cur = null;
    var curIsFrame = false;
    var lastStatusArgs = null;         // last [text, v18, v17] so a new mode can paint it

    if (game.version === 6) {
      // Plan B: turn the engine's picture hooks into real SyncTERM blits when the client supports
      // pixel graphics AND a pre-baked cache exists; otherwise fall back to text [picture #N] markers.
      // (v6cterm + the Sixel probe v6sixelOk/v6devCW/v6devCH were established earlier, before the viewport.)
      var v6pixops = false;
      // console.cterm_version is an instant property; query_ctda (which blocks for a reply) is only
      // reached on real SyncTERM, so non-SyncTERM clients incur no startup stall.
      if (console.cterm_version >= v6cterm.cterm_version_supports_copy_buffers)
        v6pixops = !!v6cterm.query_ctda(v6cterm.cterm_device_attributes.pixelops_supported);
      var v6blackPath = v6dir + "/.black.ppm";    // generated once; black source for the remnant black-blit
      var v6blackName = v6tag + "/.black.ppm";     // client-cache path
      function v6ensureBlackFile() {
        var bf = new File(v6blackPath);
        if (bf.exists && bf.length > 0) return true;
        if (!file_isdir(v6dir)) return false;
        if (!bf.open("wb")) return false;
        bf.write("P6\n" + game.screenWidthPx + " " + game.screenHeightPx + "\n255\n");
        var zeros = "\x00\x00\x00\x00\x00\x00\x00\x00", n = game.screenWidthPx * game.screenHeightPx * 3, w8;
        for (w8 = 0; w8 + 8 <= n; w8 += 8) bf.write(zeros);
        while (w8 < n) { bf.write("\x00"); w8++; }
        bf.close();
        return true;
      }
      var v6im = null;                             // ImageMagick command, detected once (output redirected so it
      function v6imCmd() {                          // never reaches the player). Prefer IM7 "magick" -- also avoids
        if (v6im === null) {                        // Windows's convert.exe disk-tool shadowing ImageMagick on PATH.
          var pr = v6dir + "/.im_probe";
          v6im = (system.exec('magick -version >"' + pr + '" 2>&1') === 0) ? "magick" : "convert";
          file_remove(pr);
        }
        return v6im;
      }
      function v6scaledFile(fn) {                  // native at 1x; nearest-neighbor S× cached in <game>.gfx/<S>x/
        var p;
        if (v6scale <= 1) p = v6dir + "/" + fn;
        else {
          var sdir = v6dir + "/" + v6scale + "x", sf = sdir + "/" + fn;
          var ex = new File(sf);
          if (ex.exists && ex.length > 0) p = sf;
          else if (!file_isdir(sdir) && !mkdir(sdir)) p = v6dir + "/" + fn;
          else {
            system.exec(v6imCmd() + ' "' + v6dir + '/' + fn + '" -sample ' + (v6scale * 100) + '% "' + sf + '"');  // -sample = nearest neighbor
            var g = new File(sf); p = (g.exists && g.length > 0) ? sf : (v6dir + "/" + fn);
          }
        }
        v6makePpmSafe(p);                            // forward-interop: make first raster byte safe for un-patched SyncTERM (see v6makePpmSafe)
        return p;
      }
      function v6ppmWs(c) { return c === " " || c === "\t" || c === "\r" || c === "\n"; }
      var v6safeChecked = {};                        // scaled-file path -> true once its first raster byte is verified/made safe this session
      // Forward-interop mitigation for SyncTERM clients WITHOUT the read_pbm fix: that
      // parser runs a greedy whitespace/'#'-comment skip on the byte right after a raw
      // P6 maxval, so a first-raster byte of 0x09/0x0a/0x0d/0x20 (whitespace) or 0x23
      // ('#') is mis-consumed -> a valid image reads short and fails to load (black
      // inset). Nudge that one colour byte by +/-1 (imperceptible, still spec-correct
      // PPM) so the file renders on un-patched clients too. Idempotent: already-safe
      // files stay byte-identical (no MD5 change -> no needless re-upload), and a
      // previously-uploaded un-nudged file self-heals (MD5 now differs -> one re-upload
      // of the safe copy). P6 only -- a P4 mask byte spans 8 transparency bits (a flip
      // would be visible) and no uploaded mask trips the bug, so masks rely on the
      // proper client-side fix.
      function v6makePpmSafe(path) {
        if (v6safeChecked[path]) return;
        var BAD = { 9: 1, 10: 1, 13: 1, 32: 1, 35: 1 };   // whitespace bytes + '#'
        var f = new File(path);
        if (!f.open("rb")) return;
        var hdr;
        try { hdr = f.read(64); } finally { f.close(); }
        if (!hdr || hdr.charAt(0) !== "P" || hdr.charAt(1) !== "6") { v6safeChecked[path] = true; return; }   // raw colour only
        var i = 2, n = hdr.length, got = 0;
        while (got < 3) {                            // skip width, height, maxval
          while (i < n && v6ppmWs(hdr.charAt(i))) i++;
          if (i >= n) return;                        // header longer than our peek -> retry next call
          while (i < n && hdr.charAt(i) >= "0" && hdr.charAt(i) <= "9") i++;
          got++;
        }
        if (i >= n || !v6ppmWs(hdr.charAt(i))) return;   // exactly one separator precedes the raster
        var off = i + 1;
        if (off >= n) return;
        var b = hdr.charCodeAt(off);
        if (!BAD[b]) { v6safeChecked[path] = true; return; }   // first raster byte already safe
        var nb = b + 1; if (BAD[nb] || nb > 255) nb = b - 1;
        var w = new File(path);
        if (!w.open("r+b")) return;
        try { w.position = off; w.writeBin(nb, 1); } finally { w.close(); }
        v6safeChecked[path] = true;
        log(LOG_DEBUG, "Z-machine v6: " + path + " - nudged first raster byte 0x" + b.toString(16) + " -> 0x" + nb.toString(16) + " for un-patched SyncTERM PNM compat");
      }
      function v6readB64(fn) { var f = new File(v6scaledFile(fn)); if (!f.open("rb")) return ""; try { f.base64 = true; return f.read() || ""; } finally { f.close(); } }
      // Client-cache skip: SyncTERM persists C;S'd files on disk per BBS (term.c get_cache_fn_base, never
      // deleted on disconnect), so ask (APC C;L) which of THIS game's pictures it already holds + their MD5.
      // drawPicture then skips re-uploading any whose MD5 still matches our local copy (a re-bake changes the
      // hash -> re-upload, never serving stale pixels). First connect uploads once; later connects go straight
      // to LoadPPM. The trailing DSR makes the read terminate even when the client sends no list (older
      // SyncTERM ignores C;L): its cursor-position reply always ends the read, so no startup stall.
      function v6cacheList(glob) {
        var map = {}, oldctrl = console.ctrlkey_passthru, buf = "", ch;
        console.ctrlkey_passthru = -1;
        try {
          console.clearkeybuffer();
          console.write("\x1b_SyncTERM:C;L;" + glob + "\x1b\\\x1b[6n");   // list files + DSR terminator
          while (true) {
            ch = console.inkey(0, 2000);
            if (ch === "" || ch === undefined || ch === null) break;      // timeout -> give up
            buf += ch;
            if (/\x1b\[[0-9]+;[0-9]+R/.test(buf)) break;                   // DSR cursor report arrived -> done
          }
        } catch (e) {} finally { console.ctrlkey_passthru = oldctrl; }
        var m = buf.match(/\x1b_SyncTERM:C;L\n([\s\S]*?)\x1b\\/);
        if (!m) return map;
        var lines = m[1].split("\n"), i, t, base;
        for (i = 0; i < lines.length; i++) {
          if (!lines[i]) continue;
          t = lines[i].split("\t");
          if (t.length < 2) continue;
          base = t[0].replace(/^.*\//, "");                               // basename (glob scopes to this game)
          map[base] = ("" + t[1]).replace(/[^0-9a-fA-F]/g, "").toLowerCase();
        }
        return map;
      }
      var v6cacheMap = v6pixops ? v6cacheList(v6tag + "/*") : {};
      if (v6pixops) { var v6cn = 0, v6ck; for (v6ck in v6cacheMap) v6cn++; log(LOG_INFO, "Z-machine v6: SyncTERM client-cache (C;L) reports " + v6cn + " file(s) already cached for this game"); }
      function v6isCached(fn) {                       // client already holds fn with a matching MD5 -> skip C;S
        var cm = v6cacheMap[fn]; if (!cm) { log(LOG_DEBUG, "Z-machine v6: " + fn + " - not on client, uploading"); return false; }
        var f = new File(v6scaledFile(fn)); if (!f.open("rb")) return false;
        var h = f.md5_hex; f.close();
        var hit = !!h && ("" + h).toLowerCase() === cm;
        log(LOG_DEBUG, "Z-machine v6: " + fn + (hit ? " - cached on client (MD5 match), skipping upload" : " - MD5 changed, re-uploading"));
        return hit;
      }
      // Sixel surface module: loaded for all v6 sessions; JSZM_makeSixelSurface is only called when DA1
      // reports attribute 4 (Sixel Graphics) on a non-SyncTERM ANSI client.
      load(js.exec_dir + "v6sixel.js");
      // Select the pixel surface tier: SyncTERM APC (v6pixops) > Sixel (v6sixelOk) > text-only (null).
      // The bridge's graphicsAvailable() returns false when surface is null or capable is false, so the
      // text [picture #N] marker path is taken unchanged for clients with no pixel tier.
      var v6surface = null;
      if (v6pixops) {
        // The SyncTERM-APC pixel surface: owns the protocol emit (C;S / LoadPPM / LoadPBM / Paste) and the
        // upload-once bookkeeping. The bridge (geometry/layout/drawnPix) drives it via blit()/fillBlack(). Built
        // from the same I/O closures the bridge used before the extraction -> byte-identical wire output.
        v6surface = JSZM_makeApcSurface({
          write: function (s) {                       // APC must pass through untouched (it contains ctrl bytes)
            var ck = console.ctrlkey_passthru; console.ctrlkey_passthru = true;
            console.write(s); console.ctrlkey_passthru = ck;
          },
          clientPrefix: v6tag,
          isCached: v6isCached,                         // client already has fn (MD5-matched) -> skip the C;S upload
          readPpmBase64: function (fn) { return v6readB64(fn); },
          readPbmBase64: function (fn) { return v6readB64(fn); },
          blackName: v6blackName,
          readBlackBase64: function () {              // base64 of the screen-sized black PPM (generated on first use)
            if (!v6ensureBlackFile()) return "";
            var f = new File(v6blackPath); if (!f.open("rb")) return "";
            try { f.base64 = true; return f.read() || ""; } finally { f.close(); }
          }
        });
      } else if (v6sixelOk) {
        // Sixel pixel surface: for non-SyncTERM ANSI clients that reported DA1 attribute 4. The terminal's
        // real text-cell pixel size (v6devCW x v6devCH from CSI 16t) was read up front; the logical cell
        // (game.fontWidth/Height) was already chosen to MATCH that aspect, so each picture scales uniformly
        // (devCell / logical-cell) to the terminal's real cell grid and lines up with the cell-positioned text.
        v6surface = JSZM_makeSixelSurface({
          write: function (s) { console.write(s); },
          cellW: game.fontWidth, cellH: game.fontHeight,
          devCellW: v6devCW || game.fontWidth, devCellH: v6devCH || game.fontHeight,
          loadPPM: function (file) { var f = new File(v6scaledFile(file)); if (!f.open("rb")) return null; var b = new Uint8Array(f.readBin(1, f.length)); f.close(); return JSZM_sixelParsePPM(b); },
          loadPBM: function (mask) { var f = new File(v6scaledFile(mask)); if (!f.open("rb")) return null; var b = new Uint8Array(f.readBin(1, f.length)); f.close(); return JSZM_sixelParsePBM(b); }
        });
      }
      var v6bridge = JSZM_makePictureBridge({
        surface: v6surface,
        capable: ansiCapable && (v6pixops || v6sixelOk),
        manifest: v6manifest,
        scale: v6scale,
        screenWidthPx: game.screenWidthPx,            // scaled screen width -> distinguishes full-width backgrounds from small overlays
        screenHeightPx: game.screenHeightPx,          // scaled screen height -> clip pastes to the bottom edge
        clipRightPx: v6offsetPx + game.screenWidthPx,   // RIGHT edge of the centred content in dx-space (dx includes the offset). Arthur draws every picture within [offset, offset+content]; a full-width bg (the map) that lands a pixel past this is trimmed here so the scene/pole clears (which reach this edge) fully cover it -> no remnant sliver.
        clipBottomPx: game.screenHeightPx,            // no vertical centring offset -> same as the content height

        originOf: function (win) {                  // PURE native window origin (§15); the bridge applies posScale + the centring offset
          var o = (frameR && frameR.windowOrigin) ? frameR.windowOrigin(win) : { x: 0, y: 0 };
          return { x: (o.x || 0), y: (o.y || 0) };
        },
        posScale: v6posScale,                          // 1 for scaled-authored (Arthur); >1 scales native-authored positions (Zork Zero)
        offsetPx: v6offsetPx,                          // horizontal centring offset (device space), applied after posScale
        fontHeight: game.fontHeight,
        fontWidth: game.fontWidth                      // v6 cell width (8) -> screenFrame cell math
      });
      // pictureInfo is data-only (valid even without graphics) so text-mode picture_data still answers.
      game.pictureInfo = v6bridge.pictureInfo;
      game.graphicsAvailable = v6bridge.graphicsAvailable;
      game.erasePicture = v6bridge.erasePicture;
      function v6textArea() {   // the lower text window's pixel strip, from the game's OWN window-0 geometry
        var w = game.windows && game.windows[0];
        if (!w) return null;
        var x = w[1] & 0xffff, ww = w[3] & 0xffff;
        if (!(ww > 0) || x + ww > game.screenWidthPx || (x === 0 && ww >= game.screenWidthPx)) return null;
        return { x: x, w: ww };   // a real inset (Arthur sets x=29,w=584 -> the strip between the side poles)
      }
      var v6cursorHidden = false;
      function v6cursor(vis) {   // DECTCEM (CSI ?25h/l): the text cursor is parked noise over a graphics-only
        vis = !!vis;             // cutscene (no input pending) -- hide it there, show it again in framed/text.
        if (vis === !v6cursorHidden) return;   // already in the desired state
        v6cursorHidden = !vis;
        console.write(vis ? "\x1b[?25h" : "\x1b[?25l");
      }
      function v6applyWindowedLayout() {   // Journey: mirror the game's win0 narrative inset + menu side-panel from game.windows
        if (!frameR) return;
        var fw = game.fontWidth || 8, ws = game.windows;
        var w0 = ws && ws[0];
        if (w0 && (w0[3] & 0xffff) > 0) {   // win0 declared width -> inset the narrative to its columns
          var m0 = JSZM_v6WindowCols((w0[1] & 0xffff) * v6posScale, (w0[3] & 0xffff) * v6posScale, fw);
          if (frameR.setTextInset) frameR.setTextInset({ x: (m0.col1 - 1) * fw, w: m0.cols * fw });
        }
        // The MENU window (Journey win1): a positioned reverse-video grid pinned to the BOTTOM. Place the grid at
        // win1's screen rectangle -- columns from its declared width, the top ROW from its declared Y origin -- so
        // set_cursor positions the 5-col x ~3-row command grid where Journey draws it.
        var mw = ws && ws[1];
        if (mw && frameR.setPanelRegion) {
          var ch = game.fontHeight || 16;
          var mx = (mw[1] & 0xffff) * v6posScale, mwid = (mw[3] & 0xffff) * v6posScale, my = (mw[0] & 0xffff) * v6posScale;
          var mm = JSZM_v6WindowCols(mx, mwid > 0 ? mwid : (game.screenWidthPx || 0), fw);
          // Top screen row of the menu. Journey declares win1 with NO origin/size (it positions the bottom menu
          // by absolute set_cursor), so my is 0 -> fall back to the row just below the narrative window (win0's
          // bottom edge): the menu sits under the narrative. (Live: win0 Y=1,H=336 -> row 22.)
          var mtop = (my > 0) ? (Math.floor(my / ch) + 1) : 1;
          if (mtop <= 1 && w0) { var w0bot = ((w0[0] & 0xffff) + (w0[2] & 0xffff)) * v6posScale; if (w0bot > 0) mtop = Math.floor(w0bot / ch) + 1; }
          if (mtop < 1) mtop = 1; if (mtop > rows) mtop = rows;
          frameR.setPanelRegion(mm.col1, mm.cols, mtop);
        }
      }
      function v6syncBand() {
        if (!frameR || !frameR.setGraphicsBand) return "text";
        var band = v6bridge.bandRows ? v6bridge.bandRows() : 0;
        var frame = (v6borderFrame && v6bridge.screenFrame) ? v6bridge.screenFrame() : null;   // border-frame is opt-in per game (sidecar); Arthur never detects one
        var mode = JSZM_v6Mode(band, rows, v6bridge.graphicsAvailable(), frame);
        if (mode === "border-frame") {
          // Enclosing border (Zork Zero): inner text rectangle from the frame + window 0's declared origin (scaled px).
          var w0 = game.windows && game.windows[0];
          var x0 = w0 ? (w0[1] & 0xffff) * v6posScale : 0, y0 = w0 ? (w0[0] & 0xffff) * v6posScale : 0;
          var r = JSZM_v6FrameRect(frame, x0, y0, rows, game.screenCols, game.fontWidth || 8, game.fontHeight || 16);
          frameR.setGraphicsBand(r.topRows);                                  // top border (+status strip) reserved
          if (frameR.setBottomInset) frameR.setBottomInset(r.bottomRows);     // bottom border reserved
          // Pass the inner left/width as PIXELS aligned to the rect's cells so setTextInset's floor() reproduces them.
          if (frameR.setTextInset) frameR.setTextInset({ x: (r.vpCol1 - 1) * (game.fontWidth || 8), w: r.vpCols * (game.fontWidth || 8) });
          v6cursor(!(frameR.inOverlay && frameR.inOverlay()));
          return mode;
        }
        // --- unchanged Arthur path (top band + side poles, no bottom border) ---
        frameR.setGraphicsBand(mode === "framed" ? band : 0);   // cutscene/text -> no text band
        if (v6windowedText) {
          v6applyWindowedLayout();                              // Journey: inset narrative + place the menu panel from game.windows
        } else if (frameR.setTextInset) {
          // Inset story text + status to window 0's game-declared strip (between the side poles); full-width otherwise.
          frameR.setTextInset(mode === "framed" ? v6textArea() : null);
        }
        if (frameR.setBottomInset) frameR.setBottomInset(0);    // clear any border-frame bottom reservation
        v6cursor((mode !== "cutscene") && !(frameR.inOverlay && frameR.inOverlay()));   // hide over a graphics-only cutscene AND over the key-driven hint-menu overlay; show it in framed/text
        return mode;
      }
      var v6lastPlaceholder = -1;
      var v6cutsceneActive = false, v6lastFrameMs = 0, V6_FRAME_MS = 500;   // min on-screen time per cutscene frame
      game.drawPicture = function (pic, x, y, win) {
        if (cur && cur.endOverlay) cur.endOverlay();   // a picture = back to the graphical room; leave any text overlay first
        if (v6bridge.graphicsAvailable()) {
          // Cutscene animation pacing: full-screen frames (the intro's empty churchyard #2 -> Merlin #3) are drawn
          // back-to-back with no @read between. Pre-cache, the per-frame PPM upload latency made each briefly
          // visible; with client-caching they blit instantly so the intermediate frame flashes invisibly. Hold the
          // previous cutscene frame on screen for a minimum time before overwriting it. Only in cutscene (full-
          // screen) mode; the elapsed-time test means a real @read pause between frames adds no delay.
          if (!v6replaying && v6cutsceneActive && v6lastFrameMs) {
            var dt = (system.timer * 1000) - v6lastFrameMs;
            if (dt > 0 && dt < V6_FRAME_MS) mswait(Math.round(V6_FRAME_MS - dt));
          }
          v6bridge.drawPicture(pic, x, y, win);   // the bridge's drawnPix is the single source of truth for replay/resume
          if (!v6replaying) { v6cutsceneActive = (v6syncBand() === "cutscene"); v6lastFrameMs = system.timer * 1000; }   // relayout (deferred during a replay), track cutscene pacing
          return;
        }
        pic = pic & 0xffff;                          // text fallback: a [picture #N] marker, repeats collapsed
        if (pic === v6lastPlaceholder) return;
        pic = pic & 0xffff;                          // text fallback: a [picture #N] marker, repeats collapsed
        if (pic === v6lastPlaceholder) return;
        v6lastPlaceholder = pic;
        cur.print("\x01n\x01h\x01k[picture #" + pic + "]\x01n\r\n");
      };
      // v6 window geometry (game.windows: 8 windows x 18 props) is interpreter state, NOT carried by quetzal, and
      // jszm init() zeroes it -- so capture it into the resume slot and restore it on resume, or the text inset
      // (window 0) and picture/pole origins are wrong after a disconnect/resume. Serialised as rows of comma-
      // separated props, windows separated by ";".
      function v6captureWindows() {
        var ws = game.windows, out = [], i;
        if (!ws || !ws.length) return "";
        for (i = 0; i < ws.length; i++) out.push((ws[i] || []).join(","));
        return out.join(";");
      }
      function v6restoreWindows(s) {
        var ws = game.windows; if (!s || !ws) return;
        var rows = ("" + s).split(";"), i, j, vals, w;
        for (i = 0; i < rows.length && i < ws.length; i++) {
          if (!rows[i]) continue;
          vals = rows[i].split(","); w = ws[i];
          for (j = 0; j < vals.length && j < w.length; j++) w[j] = parseInt(vals[j], 10) || 0;
        }
      }
      // Re-blit every picture on THIS screen on top of the current text, from the captured draw_picture calls
      // (uploads are skipped -- the client still holds them cached). Pictures and the lower text window share one
      // screen: a side pole is a full-height picture occupying pixel columns that also fall in the story rows'
      // boundary cells, so a text cell-clear (clearRow) erases those pole pixels. Re-asserting the pictures AFTER
      // the text repaint restores them. Relayout is deferred to one final sync so the inset never visits an
      // intermediate (one-pole) state. Geometry (band/inset) is unchanged here, so that sync early-returns.
      function v6reblitPictures() {
        if (!v6bridge.graphicsAvailable()) return;
        var saved = (v6bridge.screenPicCalls ? v6bridge.screenPicCalls() : []), i, pp;   // snapshot BEFORE forgetScreen
        if (v6bridge.forgetScreen) v6bridge.forgetScreen();     // drop stale rects; the replay rebuilds them clean
        v6replaying = true;
        for (i = 0; i < saved.length; i++) {
          pp = saved[i].split(",");
          if (pp.length >= 4) game.drawPicture(+pp[0], +pp[1], +pp[2], +pp[3]);
        }
        v6replaying = false;
        v6syncBand();
      }
      // Forced full redraw (Ctrl-R / dropped-byte recovery): clear the screen, repaint status + narrative + the
      // half-typed input from the renderer's own buffers, THEN re-blit the pictures on top so the side poles
      // aren't clipped by the text cell-clears. No engine round-trip -- the door holds the whole visible state,
      // so this also recovers a screen corrupted by leaked bytes. Geometry is the current scene's, unchanged.
      game.forceRedraw = function () {
        if (!cur) return;
        console.clear(LIGHTGRAY, false);                 // autopause=false: no [Hit a key] before clearing
        if (cur.redraw) cur.redraw();                    // text first (status + story + in-progress input)
        else if (cur.print && screenBuf) cur.print(screenBuf);   // non-framed fallback: reprint the narrative
        v6reblitPictures();                              // pictures last -> poles/scene sit on top, not clipped
      };
      // Debug key (Ctrl-P): like forceRedraw, but FIRST forget the per-session upload tracking and force a fresh
      // C;S re-send of every image (bypassing the client cache) -- diagnoses a missing/corrupt client cache file
      // (a black inset = read_pbm() returned NULL for the scene or its mask on the client).
      game.forceReupload = function () {
        if (!v6bridge || !v6bridge.forgetUploads) { if (game.forceRedraw) game.forceRedraw(); return; }
        v6bridge.setForceUpload(true);
        v6bridge.forgetUploads();
        if (game.forceRedraw) game.forceRedraw();
        v6bridge.setForceUpload(false);
      };
      // On-demand help (Ctrl-K): redisplay the per-game key/help splash mid-game -- for the player who
      // forgets the graphical function-key bindings -- then rebuild the screen exactly like a forced redraw.
      // Reuses showGameIntro (the same "<storyfile>.msg" sidecar shown at launch); no-op (key ignored) if the
      // game has no help file, so nothing flickers.
      game.showHelp = function () {
        if (showGameIntro(storyPath) && game.forceRedraw) game.forceRedraw();
      };
    }

    function setMode(useFrames) {
      var want = (useFrames && frameR) ? frameR : scrollR;
      if (want === cur) return;
      if (cur) cur.exit();
      cur = want;
      curIsFrame = (cur === frameR);
      cur.enter();
      // a freshly-entered renderer has no status yet; repaint the last known one
      if (lastStatusArgs) cur.updateStatusLine(lastStatusArgs[0], lastStatusArgs[1], lastStatusArgs[2]);
    }

    game.print = function(text) {
      if (gameWindow == 0) screenBuf += text;   // buffer main-window narrative for resume reprint
      cur.print(text);
    };
    game.checkUnicode = function(cp) {
      // bit0 = printable, bit1 = receivable. On a UTF-8 terminal anything in the BMP prints;
      // otherwise rely on Synchronet's down-conversion for Latin-1 and report higher code
      // points as not-cleanly-printable. Input of arbitrary Unicode isn't supported -> not receivable.
      var utf8 = (typeof console.charset === "string") && console.charset.toUpperCase().indexOf("UTF") >= 0;
      if (utf8) return 1;                 // printable on a UTF-8 client (not receivable)
      return cp < 0x100 ? 1 : 0;          // Latin-1 down-converts; above that, unreliable
    };
    // Flush any buffered narrative word BEFORE changing the attribute, so a word built across genPrint fragments
    // keeps the style it was printed with instead of inheriting the new one (e.g. "[...points.]" staying bold).
    game.setColour    = function (fg, bg) { if (cur && cur.flushWord) cur.flushWord(); jszm_apply_colour(attr, fg, bg); };
    game.setTextStyle = function (s)      { if (cur && cur.flushWord) cur.flushWord(); jszm_apply_style(attr, s); };
    // Timed single-key read (Z-spec 15: read/read_char time+routine). Built on
    // console.inkey(K_NUL, ms): K_NUL returns null on timeout, "\0" for a NUL key.
    // Returns ZSCII code (>=0) on a key, JSZM_TIMEOUT (<0) on a slice timeout, or
    // null on disconnect/idle-hangup. inkey does NOT enforce inactivity, so we do.
    game.readTimedKey = function (tenths) {
      if (js.terminated || !bbs.online) return null;
      var k = console.inkey(K_NUL, tenths * 100);
      if (k === CTRL_R) {                                // Ctrl-R: global forced redraw, then resume the read as
        if (game.forceRedraw) game.forceRedraw();        // if a time-slice elapsed (clock routine + status repaint)
        jszm_mark_activity();
        return JSZM_TIMEOUT;
      }
      if (k === CTRL_P) {                                // Ctrl-P (debug): forget client cache + force fresh C;S re-upload + redraw
        if (game.forceReupload) game.forceReupload();
        jszm_mark_activity();
        return JSZM_TIMEOUT;
      }
      if (k === null) {                                  // null = slice timeout OR a dropped connection
        if (js.terminated || !bbs.online) return null;   // disconnect: unwind NOW so the try/finally can persist
                                                         // the resume slot before the node thread is torn down
        var idle = time() - (console.last_getkey_activity > jszm_last_input_activity
                             ? console.last_getkey_activity : jszm_last_input_activity);
        var d = jszm_idle_decision(idle, console.getkey_inactivity_warning,
                                   console.max_getkey_inactivity, jszm_idle_warned);
        if (d.warn) { jszm_idle_warned = true; console.print("\r\n\x01n\x01h\x01yAre you still there?\x01n\r\n"); }
        if (d.hangup) { log(LOG_WARNING, "Z-machine: disconnecting inactive user " + user.alias); bbs.hangup(); return null; }
        return JSZM_TIMEOUT;
      }
      if (js.terminated || !bbs.online) return null;
      jszm_mark_activity();                              // real key -> reset idle clock
      return k.charCodeAt(0);
    };
    // Echo for the engine's timed line editor: number = a typed key (8 = erase one
    // char), string = re-show after an interrupt printed. Writes straight to the
    // terminal (like getstr's echo), not into screenBuf.
    game.echoInput = function (x) {
      if (cur && cur.echo) { cur.echo(x); return; }   // v6 scroll-region renderer echoes into the tracked story cursor
      if (typeof x === "number") {
        if (x === 8) console.print("\b \b");
        else console.print(String.fromCharCode(x));
      } else {
        console.print(String(x));
      }
    };
    // Timed read/read_char prompt prep: the timed path bypasses cur.read()/cur.readKey(),
    // so the engine calls this once per prompt to do what they do (reset the [MORE] pager,
    // repaint the status bar) before input begins.
    game.readPrompt = function () {
      beginInputPrompt();           // resume reprint + capture persisted screen (timed-path parity with game.read)
      screenBuf = "";               // next turn's narrative starts fresh (untimed game.read resets after the command)
      if (cur.notePrompt) cur.notePrompt();
    };
    // After a timed-read interrupt routine printed: live status refresh, or re-show the
    // in-progress input if the interrupt scrolled the lower (narrative) window.
    game.afterInterrupt = function (typed) { if (cur.afterInterrupt) cur.afterInterrupt(typed); };
    game.updateStatusLine = function(t, v18, v17) {
      lastStatusArgs = [t, v18, v17];
      cur.updateStatusLine(t, v18, v17);
    };
    if (ansiCapable) {                  // upper-window games (e.g. Seastalker) need cursor addressing
      game.split = function(h) {
        if (h > 0 && cur !== frameR) setMode(true);   // auto-engage the frame for the upper window
        cur.split(h);
      };
      game.screen = function(w) { gameWindow = w; cur.screen(w); };   // track window so we buffer only narrative
      game.setCursor   = function(r, c) { cur.setCursor(r, c); };
      game.getCursor   = function()     { return cur.getCursor(); };
      game.eraseWindow = function(w) {
        if (w < -2) return;   // erase_window only defines -1/-2 (and 0-7); Arthur emits a non-standard -3
                              // after an empty command -> a real interpreter ignores it (no clear/collapse)
        if (v6bridge && v6bridge.clearRegion) v6bridge.clearRegion(w);   // pixel black-blit clears leftover picture pixels (remnant fallback)
        // @erase_window clears a window's RECTANGLE: also wipe cross-window picture pixels in this window's
        // pixel columns (clearRegion above only clears SAME-tagged pictures). Arthur's full-width map background
        // is tagged window 7 but the dismiss erases windows 2/5/6 -- which together span the band -- so without
        // this the map background lingered behind the (smaller) room scene as a coloured remnant. The bridge's
        // drawnPix (which clearBandStrip prunes) is the single source of truth, so a Ctrl-R/resume replay never
        // re-draws the cleared map background.
        // EXCLUDE window 3 (Arthur's parser-reply / text-overlay window): erasing it clears TEXT in its rectangle,
        // it must NOT cross-window-evict the persistent banner (win 7) and room scene (win 2) drawn BEHIND it.
        // ("dive" at the lake sets window 3 to a real band-spanning rect [y=129 h=256 x=29 w=584] and erases it;
        // clearBandStrip then black-blitted + dropped pics 54+43 from drawnPix, leaving only the side poles -- so
        // the banner+scene went black and Ctrl-R, which replays drawnPix, couldn't bring them back. Only F1's fresh
        // draw_picture restored them.) The earlier Y-bound only neutralised window 3 when it happened to be
        // zero-height ("I beg your pardon?"); window 3 can carry real height, so skip it outright. clearRegion(3)
        // above still clears any window-3-TAGGED picture, so genuine win-3 graphics are unaffected.
        if (v6bridge && v6bridge.clearBandStrip && w >= 0 && w !== 3 && game.windows && game.windows[w]) {
          var ew = game.windows[w], exLo = (ew[1] & 0xffff) + v6offsetPx, exWid = ew[3] & 0xffff;
          var exTop = ew[0] & 0xffff, exHt = ew[2] & 0xffff;   // window's pixel rectangle (Y bound also guards a
          // zero-height band-spanning window from black-blitting the scene).
          if (exWid > 0 && exHt > 0) v6bridge.clearBandStrip(exLo, exLo + exWid, exTop, exTop + exHt);
        }
        if (typeof v6syncBand === "function") v6syncBand();
        cur.eraseWindow(w);
      };
      game.eraseLine   = function(v)    { cur.eraseLine(v); };
      game.bufferMode  = function(f)    { cur.bufferMode(f); };
      game.windowChanged = function(w) {
        if (w === 0 && frameR && frameR.setTextInset) frameR.setTextInset(v6textArea());   // re-inset on resize/move
        if (cur.windowChanged) cur.windowChanged(w);
      };
      game.scrollWindow  = function(w, px) { if (cur.scrollWindow) cur.scrollWindow(w, px); };
    }
    // Resume-reprint + align the persisted screen with the checkpoint. Shared by the
    // untimed read (below) AND the timed path (game.readPrompt), so a timed game like
    // Border Zone gets the same auto-resume reprint instead of the generic nudge.
    function beginInputPrompt() {
      if (pendingResumeScreen != null) {                   // first prompt after an auto-resume
        // The Z-machine snapshot restores memory+stack but no SCREEN state, and we resume AT the
        // @read -- so the game's split/status-bar draw (run pre-disconnect) is gone. Repaint the
        // upper window from the captured model first, then redisplay the lower (narrative) recap.
        // v3 starts in the scroll renderer (no upper window); if the saved screen had a split
        // (e.g. Seastalker's sonar), switch into the frame renderer so there's one to repaint.
        // v6 GRAPHICS: the game doesn't redraw on restore, so replay the captured draw_picture calls to
        // rebuild banner/scene/poles (uploads are skipped if the client still has them cached). This
        // re-establishes the graphics band + text inset BEFORE the status and narrative are repainted.
        // Restore the v6 window geometry FIRST: quetzal restores memory+stack but NOT self.windows (jszm init()
        // zeroes them), and we resume AT the @read so the game never re-runs its window setup. Without this the
        // text inset (window 0) collapses to full width and picture/pole origins are wrong.
        if (pendingResumeWindows) {
          v6restoreWindows(pendingResumeWindows); pendingResumeWindows = null;
          // Apply window-0's text inset NOW: in live play windowChanged(0) narrows the story strip to the
          // columns BETWEEN the side poles, so the text + its clearRow never touch the pole pixels. On resume the
          // game doesn't re-run its window setup, so without this the recap text runs full-width and clears the
          // lower part of the side poles (the left pole came back ~half missing).
          if (frameR && frameR.setTextInset && typeof v6textArea === "function") frameR.setTextInset(v6textArea());
        }
        if (pendingResumePics && pendingResumePics.length && game.graphicsAvailable && game.graphicsAvailable()) {
          if (v6bridge && v6bridge.forgetScreen) v6bridge.forgetScreen();   // clean slate -> replay rebuilds drawnPix
          var _pl = pendingResumePics.split(";"), _pi, _pp;
          v6replaying = true;                              // defer relayout: replay all pics, THEN sync once, so the
          for (_pi = 0; _pi < _pl.length; _pi++) {         // inset/band never lands at an intermediate full-width state
            if (!_pl[_pi]) continue;
            _pp = _pl[_pi].split(",");
            if (_pp.length >= 4) game.drawPicture(+_pp[0], +_pp[1], +_pp[2], +_pp[3]);
          }
          v6replaying = false;
          if (typeof v6syncBand === "function") v6syncBand();   // single relayout at the final pole configuration
        }
        pendingResumePics = null;
        if (pendingResumeUpper && pendingResumeUpper.length && !cur.restoreUpper && frameR) setMode(true);
        if (cur.restoreUpper) cur.restoreUpper(pendingResumeUpper);
        if (cur.armRegion) cur.armRegion();                // ensure the DECSTBM scroll region is set before the recap
                                                           // print scrolls -- so resumed narrative can't drag the image off
        // The buffered screen ends with the game's own prompt (e.g. "...\n>"); drop
        // that trailing prompt punctuation so a dead ">" doesn't show above the live prompt.
        var recap = pendingResumeScreen.replace(/[\s>]+$/, "");
        var hadPrompt = />\s*$/.test(pendingResumeScreen);  // game was waiting at a ">" line read
        if (recap.length) {                                // we have the player's last screen -> reprint it
          cur.print(recap);
          cur.print("\n\x01n\x01h\x01g[ Resumed your saved game ]\x01n\n");
        } else {                                            // nothing to redisplay (legacy/empty slot) -> nudge
          cur.print("\x01n\x01h\x01g[ Resumed your saved game -- type LOOK to look around ]\x01n\n");
        }
        // We restore AT the @read, so the game's own prompt (printed pre-disconnect) is never
        // re-emitted. Only the scroll renderer driving a v3 status line folds a ">" into its
        // own read(); every other case (v4+ status-scroll, the frame renderer incl. v3 sonar,
        // and the status-less v4+ scroll renderer) draws no prompt, so re-show one there.
        if (hadPrompt && !(cur === scrollR && game.version < 4)) cur.print("\x01n>");
        screenBuf = pendingResumeScreen;                   // keep the full screen as the current (persisted) state
        pendingResumeScreen = null;
        pendingResumeUpper = null;
      }
      jszm_resume_screen = screenBuf;                      // keep the persisted screen aligned with game.checkpoint
      jszm_resume_upper = (cur && cur.captureUpper) ? cur.captureUpper() : "";   // ...and the status bar
      jszm_resume_pics = (v6bridge && v6bridge.screenPicCalls) ? v6bridge.screenPicCalls().join(";") : "";   // on-screen pictures (bridge = source of truth)
      // ...and the v6 window geometry (not carried by quetzal). v6captureWindows is declared INSIDE the
      // `if (game.version === 6)` block, so for v4/5/8 games it's never bound (SpiderMonkey conditional function
      // definition) -- calling it unconditionally threw "ReferenceError: v6captureWindows is not defined" here on
      // every input prompt (beginInputPrompt runs for all versions), breaking disconnect-capture for non-v6 games.
      // Guard with typeof, matching the v6syncBand/v6textArea guards used elsewhere in this function.
      jszm_resume_windows = (typeof v6captureWindows === "function") ? v6captureWindows() : "";
    }
    game.read = function(maxlen) {
      beginInputPrompt();
      if (maxlen == 1) {
        // @read_char: a SINGLE keypress (e.g. "[Please press SPACE.]"), not a line. Don't
        // use getstr (it waits for ENTER) and don't run the #-command handling -- just read
        // one key. (The line read passes the text-buffer size, always > 1.)
        if (js.terminated || !bbs.online) return null;
        var k = cur.readKey ? cur.readKey() : console.getkey(K_NONE);   // renderer draws any upper-window menu first
        if (js.terminated || !bbs.online) return null;
        screenBuf = "";
        return (typeof k == "number") ? String.fromCharCode(k) : (k == null ? "" : String(k));
      }
      for (;;) {
        if (js.terminated || !bbs.online) return null;     // pre-check (already offline)
        var s = cur.read(maxlen);
        if (js.terminated || !bbs.online) return null;     // disconnect/terminate during getstr
        var c = trim(s).toLowerCase();
        if (frameR && (c == "#display" || c == "#ansi" || c == "#scroll")) {
          setMode(c == "#ansi" ? true : c == "#scroll" ? false : !curIsFrame);
          cur.print("[display: " + (curIsFrame ? "ANSI full-screen" : "line scrolling") + "]\n");
          continue;                     // don't pass the command to the game
        }
        if (frameR && c == "#split") {  // diagnostic: exercise the upper-window code path
          setMode(true);                // upper window needs the frame renderer
          cur.split(6);                 // open a 6-line upper window
          cur.screen(1);                // select it and paint a fake "sonar" panel
          cur.print(" SONAR (upper window)\n" +
                    " LAT 0042   LON 0118   DEPTH 240 ft\n" +
                    " contact bearing 030, range 5\n" +
                    " temp 39F   speed slow\n" +
                    " --- this row stays fixed while text scrolls below ---");
          cur.screen(0);                // back to the scrolling lower window
          cur.print("[upper window open; type #unsplit to close]\n");
          continue;
        }
        if (frameR && c == "#unsplit") {
          cur.split(0);
          cur.print("[upper window closed]\n");
          continue;
        }
        screenBuf = "";   // accepted a real game command -> the next turn's narrative starts fresh
        return s;
      }
    };

    // v4+ games manage their own screen (status bar + cursor addressing), so start them
    // directly in the ANSI frame renderer. Otherwise the banner/room scrolls in line mode
    // and is orphaned the moment the game first splits a window (status line) -- the screen
    // flips to the frame with a blank lower window and no visible prompt. v3 games (and any
    // non-ANSI terminal) keep the line-scrolling default with status-in-prompt.
    setMode(selfManagedScreen && frameR ? true : false);
    if (frameR && !curIsFrame) cur.print("[ status shows in the prompt; #ansi forces full-screen, #scroll returns ]\n");

    var endedNormally = false;
    // SS_MOFF: suppress Synchronet's automatic node-message/telegram display while a pinned pixel image is
    // on screen. Those are printed straight to the console (CRLFs and all) by the BBS core -- bypassing the
    // door renderer -- and scroll the graphics off. Engage only when graphics are actually in use; restore
    // on the way out (clear just our bit so other flags set during play survive).
    var v6moff = !!(v6pixops && !(bbs.sys_status & SS_MOFF));
    if (v6moff) bbs.sys_status |= SS_MOFF;
    try {
      game.run();
      endedNormally = true;   // run() returned without throwing
    } finally {
      // ALWAYS fully restore the terminal: re-enable DECLRMM briefly so "CSI 1;cols s" widens the margins back to
      // full (DECLRMM is OFF during play), disable DECLRMM, restore autowrap, clear the DECSTBM region. A leaked
      // region OR stuck left/right margins would confine the BBS menu after the door (gaps/corruption). Safe even
      // if the rectangle was never set (?69h/?69l no-ops; the "CSI 1;cols s" degrades to a harmless save-cursor).
      console.write("\x1b[?69h\x1b[1;" + cols + "s\x1b[?69l\x1b[?7h\x1b[r");
      zsnd.stop();                              // silence any playing/looping sound (exception-safe no-op off-tier)
      if (v6moff) bbs.sys_status &= ~SS_MOFF;   // restore automatic node-message display
      // Natural game-end while still connected (QUIT/death/victory) -> discard the
      // slot so the next visit starts fresh. Interruption (disconnect/idle/terminate)
      // or a crash -> keep the checkpoint for resume.
      if (endedNormally && bbs.online && !js.terminated) {
        jszm_resume_done = true;   // stop the on_exit backstop from re-creating the slot
        jszm_remove_resume();
      } else {
        jszm_flush_resume();
      }
      console.attributes = LIGHTGRAY;
      console.gotoxy(1, rows);
      console.crlf();
    }
    if (jszm_directFile || !bbs.online || js.terminated) break;   // direct play / disconnect -> leave
  }   // end play loop

  // ===================== ANSI renderer (frame.js) =====================
  function makeFrameRenderer(selfManaged) {
    load("frame.js");
    var root = null, statusFrame = null, outputFrame = null, upperFrame = null;
    var curWindow = 0, linesSincePause = 0;

    function build() {
      if (root) return;
      // One shared Display (root); status/output/upper are CHILD frames of it so
      // they coordinate on one canvas and never blank each other.
      root = new Frame(1, 1, cols, rows, LIGHTGRAY);
      if (selfManaged) {
        // v4+: no interpreter status bar; the whole screen is the output window
        // (upper window, when split, occupies the top rows including row 1).
        statusFrame = null;
        outputFrame = new Frame(1, 1, cols, rows, LIGHTGRAY, root);
      } else {
        statusFrame = new Frame(1, 1, cols, 1, BG_BLUE | WHITE, root);
        outputFrame = new Frame(1, 2, cols, rows - 1, LIGHTGRAY, root);
      }
      outputFrame.v_scroll = true;
    }
    function morePrompt() {
      if (js.terminated || !bbs.online) return;   // session ending -> stop paging
      // Transient overlay child frame; close() restores the content beneath it,
      // so no raw console writes and no manual status redraw are needed.
      var pf = new Frame(1, rows, cols, 1, BG_BLUE | WHITE, root);
      pf.open();
      pf.home();
      pf.putmsg(" Press a key for more... ");
      pf.cycle();
      console.getkey(K_NONE);
      pf.close();
      pf.delete();
    }

    return {
      enter: function() {
        build();
        root.open();
        if (statusFrame) statusFrame.open();
        outputFrame.open();
        root.cycle();
      },
      exit: function() {               // leaving frame mode: tear down cleanly
        if (upperFrame) { upperFrame.close(); upperFrame = null; outputFrame.moveTo(1, selfManaged ? 1 : 2); outputFrame.height = rows - (selfManaged ? 0 : 1); }
        root.close();                  // closes the whole frame tree
        console.attributes = LIGHTGRAY;
        console.clear();
      },
      print: function(text) {
        if (curWindow == 1 && upperFrame) {        // upper window: positioned, no paging
          upperFrame.putmsg(lfexpand(text));
          upperFrame.cycle();
          return;
        }
        // Lower (narrative) window: append text, NO interpreter-driven pagination.
        // The game owns pacing -- it prompts after each turn -- so a "more" pager here
        // would interrupt before the game's own ">" prompt (and isn't wanted mid-game).
        var lines = wrapLines(text, outputFrame.width, outputFrame.getxy().x - 1);
        for (var i = 0; i < lines.length; i++) {
          if (lines[i].t.length) outputFrame.putmsg(lines[i].t);
          if (lines[i].nl) outputFrame.putmsg("\r\n");
        }
        outputFrame.cycle();
      },
      read: function(maxlen) {
        curWindow = 0;
        linesSincePause = 0;
        outputFrame.cycle();
        var c = outputFrame.getxy();   // 1-based, frame-relative
        console.attributes = outputFrame.attr;   // don't echo input in the status blue
        console.gotoxy(outputFrame.x + c.x - 1, outputFrame.y + c.y - 1);
        // K_NOCRLF: don't emit a newline that would physically scroll the screen
        // (and drag the status row off); advance the line via the frame instead.
        var s = console.getstr(maxlen, K_NONE | K_NOCRLF);
        outputFrame.putmsg(lfexpand((s || "") + "\n"));
        outputFrame.cycle();
        return s;
      },
      updateStatusLine: function(text, v18, v17) {
        if (!statusFrame) return;      // v4+ games draw their own status (no interpreter bar)
        var right = statusRight(v18, v17);
        var line = pad(" " + text, cols - right.length - 1) + right + " ";
        statusFrame.clear(BG_BLUE | WHITE);
        statusFrame.home();
        statusFrame.putmsg(line.substr(0, cols), BG_BLUE | WHITE);
        statusFrame.cycle();
      },
      split: function(height) {
        var top = selfManaged ? 1 : 2;            // v4+ upper window includes row 1
        if (height > 0) {
          if (!upperFrame) {
            upperFrame = new Frame(1, top, cols, height, LIGHTGRAY, root);
            upperFrame.v_scroll = false;
            upperFrame.open();
          } else {
            upperFrame.moveTo(1, top);
            upperFrame.height = height;
          }
          upperFrame.clear(LIGHTGRAY);
          outputFrame.moveTo(1, top + height);
          outputFrame.height = rows - (top - 1) - height;
        } else if (upperFrame) {
          upperFrame.close();
          upperFrame = null;
          outputFrame.moveTo(1, top);
          outputFrame.height = rows - (top - 1);
        }
        root.cycle();
      },
      screen: function(win) {
        curWindow = win;
        if (win == 1 && upperFrame) upperFrame.home();
      },
      setCursor: function(row, col) {
        if (curWindow == 1 && upperFrame) { upperFrame.gotoxy(col, row); upperFrame.cycle(); }
        // lower-window cursor moves are rare; ignore (output window is scrolling)
      },
      getCursor: function() {
        var f = (curWindow == 1 && upperFrame) ? upperFrame : outputFrame;
        var c = f.getxy();                         // {x:col, y:row}, 1-based, frame-relative
        return { row: c.y, col: c.x };
      },
      eraseWindow: function(win) {
        if (win == -1 || win == -2) {              // whole screen
          if (upperFrame) upperFrame.clear(LIGHTGRAY);
          outputFrame.clear(LIGHTGRAY);
          if (win == -1 && upperFrame) {           // -1 also unsplits; -2 keeps the split
            upperFrame.close(); upperFrame = null;
            outputFrame.moveTo(1, selfManaged ? 1 : 2);
            outputFrame.height = rows - (selfManaged ? 0 : 1);
          }
          root.cycle();
        } else if (win == 1 && upperFrame) { upperFrame.clear(LIGHTGRAY); upperFrame.cycle(); }
        else if (win == 0) { outputFrame.clear(LIGHTGRAY); outputFrame.cycle(); }
      },
      eraseLine: function(val) {
        if (val != 1) return;                       // only "erase to end of line" is defined
        var f = (curWindow == 1 && upperFrame) ? upperFrame : outputFrame;
        f.clearline(); f.cycle();
      },
      bufferMode: function() {},                    // word-wrap is always on in this renderer; no-op
      // Auto-resume: capture/restore the upper window (a v3 split, e.g. Seastalker's
      // sonar) by reading/writing the Frame's cells. The Z-machine snapshot has no
      // screen state and we resume at the @read, so the door persists this on disconnect
      // and repaints it on resume. Encoding matches the status-scroll renderer:
      // "<height>\n<row0>\n<row1>...". (For v3 the door also switches back into the frame
      // renderer before calling this -- see beginInputPrompt.)
      captureUpper: function() {
        if (!upperFrame) return "";
        var h = upperFrame.height, w = upperFrame.width, out = String(h), r, c, line, d;
        for (r = 0; r < h; r++) {
          line = "";
          for (c = 0; c < w; c++) { d = upperFrame.getData(c, r, false); line += (d && d.ch != undefined) ? d.ch : " "; }
          out += "\n" + line.replace(/\s+$/, "");
        }
        return out;
      },
      restoreUpper: function(s) {
        if (!s) return;
        var nl = s.indexOf("\n");
        var h = parseInt(nl < 0 ? s : s.substr(0, nl), 10);
        if (!(h > 0)) return;
        var body = nl < 0 ? [] : s.substr(nl + 1).split("\n"), r;
        this.split(h);                              // (re)create the upper frame + reflow the output window
        if (!upperFrame) return;
        upperFrame.clear(LIGHTGRAY);
        for (r = 0; r < h; r++) { upperFrame.gotoxy(1, r + 1); upperFrame.putmsg((body[r] || "").substr(0, upperFrame.width)); }
        upperFrame.cycle();
        curWindow = 0;                              // resumed read happens in the lower window
      }
    };
  }

  // ============== Line-scrolling renderer (any terminal) ==============
  function makeScrollRenderer() {
    var lastStatus = "";
    return {
      enter: function() { console.attributes = LIGHTGRAY; },
      exit: function() {},
      print: function(text) {
        // word-wrap explicitly (don't rely on console auto-wrap), honoring the
        // current column; emit CR+LF between visual lines. Wrap to cols-1 so a
        // full-width line never hits the console's column-80 auto-wrap, which
        // would combine with our CR+LF into a spurious blank line.
        var lines = wrapLines(text, cols - 1, console.current_column);
        var out = "";
        for (var i = 0; i < lines.length; i++) {
          if (lines[i].t.length) out += styled(lines[i].t);
          if (lines[i].nl) out += "\r\n";
        }
        jszm_emit(out);
      },
      read: function(maxlen) {
        // Only fold the status into the prompt when the game's prompt is the
        // short ">" (cursor still near column 0). For longer in-line prompts
        // like "Do you wish to leave the game? (Y is affirmative): " leave the
        // line intact so we don't erase the question.
        if (lastStatus.length && console.current_column <= 3) {
          if (ansiCapable) {
            console.creturn();
            console.cleartoeol();
            console.print(lastStatus + " \x01h\x01g>\x01n ");
          } else {
            console.print(" " + lastStatus + " ");
          }
        }
        return console.getstr(maxlen, K_NONE);
      },
      readKey: function() { return console.getkey(K_NONE); },   // @read_char: single keypress
      afterInterrupt: function(typed) {                  // no status window: any interrupt print scrolled the input
        if (typed && typed.length) console.print(typed);
      },
      updateStatusLine: function(text, v18, v17) {
        // Ctrl-A color codes; console translates them per the user's terminal.
        lastStatus = "\x01n\x01c[ \x01h\x01w" + text +
                     "\x01n\x01c  \x01h\x01y" + statusRight(v18, v17) +
                     "\x01n\x01c ]\x01n";
      },
      split: function() {},          // upper-window games auto-switch to the frame renderer
      screen: function() {},
      setCursor: function() {},        // no cursor addressing on a scrolling terminal
      getCursor: function() { return { row: 1, col: 1 }; },
      eraseWindow: function() {},      // best-effort: don't blank a scrollback terminal
      eraseLine: function() {},
      bufferMode: function() {}
    };
  }

  // ----- v4+ status-scroll renderer ("Option B"): the narrative (lower window)
  // scrolls NATIVELY in the terminal (hardware scroll -> light on bytes, real
  // scrollback, the prompt is always on-screen), and the small upper window /
  // status bar is REDRAWN at the top after each scroll. Uses only CUP + cursor
  // save/restore via console.current_row/column -- NO scroll regions -- so it
  // works on any ANSI.SYS terminal (QModem/Procomm/Telix/Telemate). Best for a
  // status bar; rich multi-row upper windows (v3 Seastalker sonar) still use the
  // Frame. Colour/text-styles are deferred (the bar shows in a default attribute). -----
  function makeStatusScrollRenderer() {
    var upperH = 0;                 // upper-window height in rows (0 = none)
    var upper = [];                 // upperH rows, each an array of `cols` chars
    var upRow = 1, upCol = 1;       // cursor within the upper window (1-based)
    var curWindow = 0;              // 0 = lower (scrolling), 1 = upper
    // Synchronet Ctrl-A colour codes (the console translates them per terminal): normal,
    // blue background (\x014), high-intensity white foreground. The leading \x01n makes
    // putmsg honour OUR colour instead of resetting to the normal attribute first.
    var BARCLR = "\x01n\x014\x01h\x01w";
    var linesSinceInput = 0;        // narrative lines printed since the last input prompt ([MORE] pacing)
    var lowerDirty = false;         // did the lower (narrative) window print since the last prompt/interrupt?

    function blankRow() { var a = [], i; for (i = 0; i < cols; i++) a.push(" "); return a; }
    function putUpper(text) {       // apply a print into the upper-window model
      var i, ch;
      for (i = 0; i < text.length; i++) {
        ch = text.charAt(i);
        if (ch == "\r") { upCol = 1; continue; }
        if (ch == "\n") { upRow++; upCol = 1; continue; }
        if (upRow >= 1 && upRow <= upperH && upCol >= 1 && upCol <= cols)
          upper[upRow - 1][upCol - 1] = ch;
        upCol++;
      }
    }
    function redrawUpper() {         // repaint the upper window, preserving the lower cursor
      if (!upperH) return;
      var sr = console.current_row, sc = console.current_column;   // 0-based narrative cursor
      var r;
      // A SMALL upper window (<=4 rows) is a status bar: paint its CONTENT rows blue,
      // leaving all-blank rows plain (e.g. Curses' blank top line). A large upper window
      // is a help/menu/map screen -- render it all plain (readable, no blue wash).
      var isMenu = upperH > 4, rowtext, clr;
      for (r = 0; r < upperH; r++) {
        rowtext = upper[r].join("").substr(0, cols - 1);              // cols-1: never the last cell (autowrap)
        clr = (!isMenu && /\S/.test(rowtext)) ? BARCLR : "\x01n";     // content row of a status bar -> blue
        console.gotoxy(1, r + 1);
        var hi = false, hk; for (hk = 0; hk < rowtext.length; hk++) { if (rowtext.charCodeAt(hk) > 127) { hi = true; break; } }
        if (hi) console.putmsg(clr + jszm_utf8(rowtext), P_UTF8);
        else console.putmsg(clr + rowtext);
      }
      console.attributes = LIGHTGRAY;
      console.gotoxy(sc + 1, sr + 1);                              // restore the narrative cursor (gotoxy is 1-based)
    }
    function morePause() {          // interpreter pause after a screenful of unread output
      if (js.terminated || !bbs.online) return;
      console.pause();              // sysop-configured Pause prompt (text.dat #563 "Pause"); waits for a key and erases it
      if (upperH) redrawUpper();
      linesSinceInput = 0;
    }

    return {
      enter: function() {
        console.attributes = LIGHTGRAY;
        console.clear();
        upperH = 0; upper = []; curWindow = 0; upRow = 1; upCol = 1; linesSinceInput = 0;
      },
      exit: function() {
        console.attributes = LIGHTGRAY;
        console.gotoxy(1, rows);
      },
      print: function(text) {
        if (curWindow == 1) { putUpper(text); return; }   // upper window: model only (drawn at next redraw/read)
        // lower (narrative) window: native scroll, explicit word-wrap (cols-1 avoids col-80 autowrap),
        // with an interpreter [MORE] pause every screenful of unread output (reset at each input prompt).
        var lines = wrapLines(text, cols - 1, console.current_column);
        var avail = (rows - upperH) - 1, i;               // lower-window rows, less 1 for the [MORE]/prompt line
        for (i = 0; i < lines.length; i++) {
          if (lines[i].t.length) { jszm_emit(styled(lines[i].t)); lowerDirty = true; }
          if (lines[i].nl) {
            console.print("\r\n");
            lowerDirty = true;
            if (upperH) redrawUpper();                     // re-pin the status after this line scrolled
            if (++linesSinceInput >= avail) morePause();
          }
        }
      },
      read: function(maxlen) {
        linesSinceInput = 0;                              // the user is caught up at this prompt
        if (upperH) redrawUpper();                        // status current before the already-printed prompt
        return console.getstr(maxlen, K_NONE);
      },
      readKey: function() {                               // @read_char: draw any upper-window menu, then one key
        linesSinceInput = 0;
        if (upperH) redrawUpper();
        return console.getkey(K_NONE);
      },
      notePrompt: function() {                            // timed read/read_char: mirror read()/readKey() prompt prep
        linesSinceInput = 0;                              // user is caught up here -> don't carry [MORE] pacing across turns
        lowerDirty = false;
        if (upperH) redrawUpper();
      },
      afterInterrupt: function(typed) {                   // a timed-read interrupt routine just printed
        if (lowerDirty) {                                 // it scrolled the narrative -> input is gone; re-show it
          lowerDirty = false;
          if (typed && typed.length) console.print(typed);
        } else if (upperH) {
          redrawUpper();                                  // status-only update (e.g. real-time clock) -> repaint it
        }
      },
      updateStatusLine: function() {},                    // v4+ status arrives via the upper window, not this
      split: function(h) {
        linesSinceInput = 0;
        if (h > 0) {
          // split_window must NOT clear the upper window (Z-spec 8.7.2): resize the model
          // but PRESERVE existing content. A 2-line status often draws its top line once and
          // only refreshes the bottom line each turn -- clearing here lost the top line.
          while (upper.length < h) upper.push(blankRow());
          if (upper.length > h) upper.length = h;
          upperH = h; upRow = 1; upCol = 1;
          if (console.current_row < h) console.gotoxy(1, h + 1);   // keep the narrative below the status
        } else { upperH = 0; upper = []; }
      },
      // Auto-resume snapshot helpers: the upper window (split height + status text) isn't part
      // of the Z-machine save state and isn't re-drawn when we restore at the @read, so the door
      // captures it on disconnect and repaints it on resume. Encoding: "<height>\n<row0>\n<row1>...".
      captureUpper: function() {
        if (!upperH) return "";
        var out = String(upperH), r;
        for (r = 0; r < upperH; r++) out += "\n" + upper[r].join("");
        return out;
      },
      restoreUpper: function(s) {
        if (!s) return;
        var nl = s.indexOf("\n");
        var h = parseInt(nl < 0 ? s : s.substr(0, nl), 10);
        if (!(h > 0)) return;
        var body = nl < 0 ? [] : s.substr(nl + 1).split("\n"), r, c, line, a;
        upper = [];
        for (r = 0; r < h; r++) {
          a = blankRow();
          line = body[r] || "";
          for (c = 0; c < line.length && c < cols; c++) a[c] = line.charAt(c);
          upper.push(a);
        }
        upperH = h; upRow = 1; upCol = 1;
        if (console.current_row < h) console.gotoxy(1, h + 1);   // narrative below the restored status
        redrawUpper();
      },
      screen: function(w) { curWindow = w; if (w == 1) { upRow = 1; upCol = 1; } },
      setCursor: function(r, c) { if (curWindow == 1) { upRow = r; upCol = c; } },
      getCursor: function() {
        return curWindow == 1 ? { row: upRow, col: upCol }
                              : { row: console.current_row + 1, col: console.current_column + 1 };
      },
      eraseWindow: function(w) {
        var r, y;
        if (w == -1 || w == -2) {                         // whole screen (-1 also unsplits)
          console.attributes = LIGHTGRAY; console.clear();
          if (w == -1) { upperH = 0; upper = []; }
          else { for (r = 0; r < upperH; r++) upper[r] = blankRow(); console.gotoxy(1, upperH + 1); }
        } else if (w == 1) {                              // clear the upper window
          for (r = 0; r < upperH; r++) upper[r] = blankRow();
          redrawUpper();
        } else if (w == 0) {                              // clear the lower (narrative) region
          for (y = upperH + 1; y <= rows; y++) { console.gotoxy(1, y); console.cleartoeol(); }
          console.gotoxy(1, upperH + 1);
        }
      },
      eraseLine: function() { console.cleartoeol(); },
      bufferMode: function() {}
    };
  }
  function makeV6Renderer() {
    // v6 graphical: the pixel-art band + status bar stay FIXED at the top; only the story window scrolls.
    // The story window is a SELF-OWNED virtual line buffer: every visible row is repainted from the buffer
    // (gotoxy -> cleartoeol -> putmsg) and a scroll is just an array shift, so we never lean on Synchronet's
    // console text-flow or a terminal scroll region -- both desync from the pinned pixels above. (Three earlier
    // models all died on that coupling: native-scroll dragged the image off, frame.js blanked the cells, a
    // DECSTBM region garbled the text. Here we only ever paint rows BELOW the band, never disturbing it.)
    var cellW = game.fontWidth || 1, cellH = game.fontHeight || 1;
    var fullCols = (typeof v6vp !== "undefined" && v6vp.cols) ? v6vp.cols : cols;
    var fullCol1 = (typeof v6offsetCols !== "undefined") ? v6offsetCols + 1 : 1;
    var vpCols = fullCols, vpCol1 = fullCol1;   // story text width/left col -- inset to the scene picture when framed
    var graphicsBand = 0;          // pixel-art rows at the top (fixed; never scrolled)
    var committedBand = 0;         // band the status was last anchored to -- absorbs +/-1-row scene<->map jitter
    var statusH = 0;               // status rows just below the band (fixed)
    var bottomInset = 0;           // rows reserved at the screen bottom (border-frame bottom border); 0 = none (Arthur)
    var curWindow = 0;             // 0 = story (scrolls); non-0 = status/upper window
    var status = [];               // statusH rows, each a `cols`-wide char array
    var stRow = 1, stCol = 1;      // cursor within the status window (1-based)
    var panel = [];                // upper-window (win 2) text rows painted OVER the graphics band (info displays:
                                   // Arthur's stats / room-text); each a `cols`-wide char array, mirrors status[]
    var pstyle = [];               // parallel per-cell Ctrl-A style prefix for panel[] (carries reverse/bold so the
                                   // F3 inventory's reverse-video separator bars + bold headers survive to repaint)
    var panRow = 1, panCol = 1;    // cursor within the upper window (1-based cells)
    var panPending = "";           // buffered word for the panel (word-wrap flowing text across print fragments)
    var panPendPrefix = "";        // style prefix captured when the buffered word began (style is set before the word)
    var panelActive = false;       // window 2 holds content -> paint it over the band (instead of routing to story)
    var panCol1 = 0, panCols = 0;   // panel grid's terminal columns (0 = follow the win0 inset; set for a side panel, Journey win3)
    var panTop = 1;                 // first SCREEN row the panel grid paints (1 = top-anchored = Arthur; >1 = a bottom region, Journey menu)
    function panTopRow() { return panTop > 0 ? panTop : 1; }
    function panLeft()  { return panCol1 > 0 ? panCol1 : vpCol1; }   // panel's left column (defaults to the story inset = Arthur)
    function panWidth() { return panCols > 0 ? panCols : vpCols; }   // panel's width in cells
    function setPanelRegion(col1, n, top) { panCol1 = (col1 > 0) ? col1 : 0; panCols = (n > 0) ? n : 0; panTop = (top > 0) ? top : 1; }
    function isMenuWin(w) { return v6windowedText && w === 1; }   // Journey's MENU is win1 (positioned bottom grid), not the status bar
    var BARCLR = "\x01n\x014\x01h\x01w";   // normal, blue bg, high white -- the status bar colour
    var lines = [];                // completed story rows, top->bottom: { s:<styled string>, n:<visible length> }
    var partial = { s: "", n: 0 }; // the in-progress bottom story row (narrative tail / live input)
    var pendingWord = "";          // a not-yet-flushed run of non-space narrative chars -- buffered across genPrint
                                   // fragments so a word split into separate print calls (e.g. "points" then ",")
                                   // word-wraps as ONE unit and never breaks across a line (matches DOS/the z-spec
                                   // buffer_mode). Plain game text only; flushed at spaces/newlines and before input.
    var inStk = [];                // byte-lengths of input units appended to `partial` (for backspace erase)
    var linesSinceInput = 0;       // story lines since the last prompt ([MORE] pacing)
    var afterRead = false;         // true after a line read -> the next committed line is the command echo (don't count it)

    // Function-key delivery (#111). SyncTERM/Synchronet don't surface F-keys, and v6 games (Arthur) drive their
    // UI with them (F1 dismisses the map). Generic scheme: Ctrl-A..Ctrl-L stand in for F1..F12 (ZSCII 133..144);
    // skip Ctrl-H/I/J (8/9/10 = BS/TAB/LF, which keep their editing role) and Ctrl-R (our redraw). v6ctrlToFkey
    // returns the ZSCII function code for a control char, or 0. v6isTerminator checks the game's terminating-
    // characters table (z-spec 10.7, header 0x2e; 255 = any function key) so a Ctrl-letter only ends a line read
    // if that game actually binds that key.
    function v6ctrlToFkey(c) { return (c >= 1 && c <= 12 && c !== 8 && c !== 9 && c !== 10) ? (132 + c) : 0; }
    // Synchronet handles some control keys as hotkeys BEFORE getkey() returns them -- notably Ctrl-C (abort),
    // which it eats by default so Arthur's F3 (inventory) never reached us (Ctrl-A/B/D/E/F already pass through).
    // Pass THROUGH the control keys we translate to function keys (Ctrl-A..Ctrl-G, K, L per v6ctrlToFkey) plus
    // Ctrl-R (our forced redraw); restore the caller's setting after each read so the sysop's hotkeys still work
    // outside the game. Bit (code-1) per control key.
    var V6_CTRL_PASSTHRU = (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<10)|(1<<11)|(1<<16)|(1<<17);   // sbbs inkey: bit = (1<<ctrlcode); bit16 = Ctrl-P (code 16, debug re-upload)
    // Read input in K_CTRLKEYS mode: control keys AND ESC pass through RAW. Synchronet does NOT translate the
    // cursor/function escape sequences in this mode -- only K_EXTKEYS does, and that collapses the arrows onto
    // the Ctrl-letters (Right -> CTRL_F, Home -> CTRL_B, End -> CTRL_E) which collide with our Ctrl-letter->F-key
    // shortcuts. So we parse the cterm escape sequences OURSELVES, keeping the real arrows/F-keys DISTINCT from
    // the actual Ctrl-letters (Right-arrow = ESC[C, not byte 6). Arrows -> ZSCII 129-132, F1..F12 -> 133..144.
    function v6keyByte(ms) {   // ms omitted -> blocking getkey (idle-enforced); a number -> timed inkey (ESC-seq tail)
      var k = (ms === undefined) ? console.getkey(K_CTRLKEYS) : console.inkey(K_CTRLKEYS, ms);
      return (k && k.length) ? k.charCodeAt(0) : -1;
    }
    function v6escToZscii(intro, fb, params) {   // CSI '[' (0x5b) or SS3 'O' (0x4f) introducer -> ZSCII ext key, or 0
      if (intro === 0x5b) {
        if (fb === 0x41) return 129; if (fb === 0x42) return 130;     // A up / B down
        if (fb === 0x44) return 131; if (fb === 0x43) return 132;     // D left / C right
        if (fb === 0x7e) {                                            // ESC[<n>~ : F1-5=11-15, F6-10=17-21, F11-12=23-24
          var n = parseInt(params, 10);
          var FK = { 11:133,12:134,13:135,14:136,15:137,17:138,18:139,19:140,20:141,21:142,23:143,24:144 };
          if (FK[n]) return FK[n];
        }
      } else if (intro === 0x4f) {                                    // SS3: some terminals send F1-F4 / arrows as ESC O <x>
        if (fb === 0x50) return 133; if (fb === 0x51) return 134; if (fb === 0x52) return 135; if (fb === 0x53) return 136;
        if (fb === 0x41) return 129; if (fb === 0x42) return 130; if (fb === 0x44) return 131; if (fb === 0x43) return 132;
      }
      return 0;
    }
    function v6getKey() {   // one logical keystroke -> { fk: ext-key ZSCII (129-144) or 0, ch: raw byte or -1 }
      var b = v6keyByte();
      if (b !== 27) return { fk: 0, ch: b };                          // ordinary/control byte (or -1 on disconnect)
      // ESC introducer ([ or O). Use a GENEROUS timeout: the introducer follows the ESC within one keypress burst,
      // but under SSH/network jitter -- notably right after a map<->text toggle, where a large graphics blit runs
      // just before the read -- the ESC and its "[..~" body can land in separate reads. With a short window the
      // introducer read times out, we return a bare ESC, and the orphaned tail (e.g. F1's "[11~") leaks into the
      // line as literal characters (the echo seen at the > prompt). Arthur ignores a bare ESC, so waiting is free.
      var intro = v6keyByte(600);                                     // ESC: introducer (timeout -> a bare Escape key)
      if (intro !== 0x5b && intro !== 0x4f) return { fk: 0, ch: (intro < 0 ? 27 : -1) };   // bare ESC, or ESC+other -> swallow
      var params = "", nb, fb = 0;
      for (;;) { nb = v6keyByte(300); if (nb < 0) break;              // gather param/intermediate bytes up to the final byte (looser too: a split mid-sequence else swallows the key)
        if (nb >= 0x40 && nb <= 0x7e) { fb = nb; break; } params += String.fromCharCode(nb); }
      return { fk: fb ? v6escToZscii(intro, fb, params) : 0, ch: -1 };
    }
    var v6termSet = null, v6termWild = false;
    function v6isTerminator(zc) {
      if (v6termSet === null) {
        v6termSet = {};
        var mm = game.memInit, ta = (game.version >= 5 && mm) ? (((mm[0x2e] << 8) | mm[0x2f]) & 0xffff) : 0, a, b;
        if (ta) { for (a = ta; a < mm.length && (b = mm[a]); a++) { if (b === 255) v6termWild = true; else v6termSet[b] = true; } }
      }
      return !!v6termSet[zc] || (v6termWild && zc >= 129 && zc <= 154);
    }
    var v6prevStatusTop = 1, v6prevStatusBot = 0;   // last screen rows the status bar painted (bot<top = none yet)

    function winTop() { var t = graphicsBand + statusH + 1; return t > rows ? rows : t; }   // first story row
    function winH()   { var h = (rows - bottomInset) - winTop() + 1; return h > 0 ? h : 1; } // visible story rows (bottom border reserved)
    // DECSTBM scroll-region SAFETY NET: confine the terminal's scroll to the story rows [winTop..rows] so any
    // stray output we don't control (a Synchronet message that slips through, an inactivity warning, etc.)
    // scrolls WITHIN the text strip instead of dragging the pinned pixel art off the top. The virtual buffer
    // never relies on this region (it repaints absolutely, never scrolls) -- this is purely a backstop, so it
    // doesn't reintroduce the old scroll-region cursor desync.
    // rectMode: on a graphics-capable client (SyncTERM 1.2+, the SAME floor as PPM -- see provenance) we set a
    // TRUE scroll rectangle: DECSLRM left/right margins (CSI l;r s) ON TOP of DECSTBM (CSI t;b r), so a stray
    // scroll is confined on all four sides and can't disturb the side poles. DECOM stays OFF, so our absolute
    // gotoxy still addresses the whole screen (status bar above the band, pixel blits, etc.). Autowrap is disabled
    // (CSI ?7l) so writing the right-margin cell can't arm a wrap-scroll (we pre-wrap anyway).
    //
    // CRITICAL: enable DECLRMM (CSI ?69h) ONLY long enough to set the margins, then turn it back OFF (CSI ?69l).
    // cterm enforces the margins by VALUE (scroll/CR confinement) regardless of the DECLRMM flag; the flag only
    // controls whether "CSI s" means DECSLRM vs save-cursor. sbbs emits "CSI s" for cursor-save -- in
    // insert_indicator() (every getstr) and in the saveline()/restoreline() behind every Ctrl-key hotkey (incl.
    // global hotkeys). With DECLRMM left ON, that "CSI s" is read as DECSLRM-with-defaults and resets our margins
    // to full width -> the rectangle collapses and the output corrupts the pinned graphics. Leaving DECLRMM OFF
    // keeps "CSI s" = save-cursor, so the margins survive and sbbs's input + hotkeys all work normally.
    function rectMode() { return !!(game.graphicsAvailable && game.graphicsAvailable()); }
    function setRegion() {
      // The full-screen text overlay (hint menu) owns the whole width and paints absolute-positioned text up to
      // its own computed right edge. NEVER confine it to the room's narrow text inset: a stale 4..76 DECSLRM
      // right margin + autowrap-off makes a word that reaches col 77 clamp-overwrite col 76 (dropping its
      // next-to-last glyph: "the"->"te"). windowChanged(0)/relayout call setRegion mid-overlay, so guard here.
      if (v6pos) { resetRegion(); return; }
      var regBot = rows - bottomInset; if (regBot < winTop()) regBot = winTop();   // DECSTBM bottom (border-frame reserves bottom rows)
      if (rectMode())
        console.write("\x1b[?69h\x1b[" + vpCol1 + ";" + (vpCol1 + vpCols - 1) + "s\x1b[?69l\x1b[?7l\x1b[" + winTop() + ";" + regBot + "r");
      else
        console.write("\x1b[" + winTop() + ";" + regBot + "r");   // CSI top;bottom r (homes cursor)
    }
    // Full teardown, SAFE to emit unconditionally: re-enable DECLRMM briefly to widen DECSLRM back to full (so the
    // "CSI 1;cols s" is actually interpreted as set-margins, since DECLRMM is OFF during play), disable DECLRMM,
    // restore autowrap, reset the DECSTBM region. ?69l does NOT itself reset the margins (cterm keeps reading
    // left/right_margin) -- so we MUST widen them or the terminal stays stuck scrolling in a sub-rectangle and
    // corrupts the next door (cf. gitlab #1161). If the client lacks DECLRMM, ?69h/?69l are no-ops and the
    // "CSI 1;cols s" degrades to a harmless save-cursor.
    function resetRegion() { console.write("\x1b[?69h\x1b[1;" + cols + "s\x1b[?69l\x1b[?7h\x1b[r"); }
    function blankRow() { var a = [], i; for (i = 0; i < cols; i++) a.push(" "); return a; }
    function blankStyleRow() { var a = [], i; for (i = 0; i < cols; i++) a.push(""); return a; }   // "" = default (LIGHTGRAY) cell style
    function inputRow() { return winTop() + lines.length; }                                  // screen row of `partial`
    function cursorToInput() { console.gotoxy(vpCol1 + partial.n, inputRow()); }
    function blanks(n) { var s = " "; while (s.length < n) s += s; return n > 0 ? s.substr(0, n) : ""; }
    // Clear ONLY the text strip [vpCol1 .. vpCol1+vpCols) -- NOT to end-of-line, which would wipe the
    // right-hand side pole. CRUCIAL: never write the LAST terminal column: a character written there
    // arms SyncTERM's delayed line-wrap, and the next output scrolls the whole screen up one row --
    // dragging the pinned pixel art up with it (the "scene shifts up when the status draws" bug).
    function clearRow(r) {
      var n = vpCols; if (vpCol1 + n > cols) n = cols - vpCol1;
      console.gotoxy(vpCol1, r); console.attributes = LIGHTGRAY; if (n > 0) console.write(blanks(n));
    }
    // Paint a styled run at the cursor WITHOUT console.print's word-wrap/line-counting (we pre-wrap; rows
    // never reach the viewport's right edge, so nothing auto-wraps). Ctrl-A codes pass through putmsg; any
    // code point > 127 is UTF-8-encoded + tagged P_UTF8 so Synchronet down-converts to the user's charset.
    function paint(s) {
      var hi = false, i; for (i = 0; i < s.length; i++) { if (s.charCodeAt(i) > 127) { hi = true; break; } }
      // P_NOPAUSE: suppress Synchronet's own line-counter [Hit a key] -- pagination is OUR job (morePause).
      if (hi) console.putmsg(jszm_utf8(s), P_UTF8 | P_NOPAUSE); else console.putmsg(s, P_NOPAUSE);
    }
    // [MORE] pacing is LAZY: the screen is "full" once winH()-1 unread narrative lines have appeared (the bottom
    // row is reserved for input/[MORE]). We pause BEFORE drawing/committing the NEXT line rather than right after
    // the line that fills the screen -- so output that ends exactly at the bottom row (prompt next) does not throw
    // a spurious [Hit a key]. The pause still fires before any unread line scrolls off (linesSinceInput resets at
    // every prompt), so it keeps the same no-line-loss guarantee as the eager winH()-1 threshold it replaces.
    function moreFull() { var th = winH() - 1; if (th < 2) th = 2; return linesSinceInput >= th; }
    function emitSeg(seg, vis) {   // append a styled run to `partial` and draw it in place at the input cursor
      if (moreFull()) morePause();   // screen full + more narrative text arriving -> pause before drawing it
      console.gotoxy(vpCol1 + partial.n, inputRow());
      partial.s += seg; partial.n += vis;
      paint(seg);
    }
    // Usable text columns: the full inset strip, EXCEPT reserve the last column only when the strip reaches the
    // terminal's right edge (writing the terminal's last column arms SyncTERM's wrap-scroll). Arthur's strip is
    // inset (ends well short of the edge), so it gets its full width -- one more column than vpCols-1, which lets
    // a word like "points," sit at the end of the line instead of wrapping (matching DOS).
    function vpWidth() { return (vpCol1 + vpCols >= cols) ? vpCols - 1 : vpCols; }
    function flushWord() {         // commit the buffered word to `partial`, wrapping to a fresh row if it won't fit
      if (!pendingWord.length) return;
      var w = pendingWord, lim = vpWidth(); pendingWord = "";
      if (partial.n > 0 && partial.n + w.length > lim) newStoryLine();   // whole word won't fit -> wrap before it
      while (w.length > lim) {                                           // word longer than the strip -> hard-break
        emitSeg(styled(w.substr(0, lim)), lim);
        newStoryLine();
        w = w.substr(lim);
      }
      if (w.length) emitSeg(styled(w), w.length);
    }
    function repaintStory() {      // full repaint of the visible story window from the buffer
      if (v6pos) return;           // a positioned full-screen overlay (hint menu) owns the screen -- a story
                                   // repaint here clears rows 1.. (winTop=1) right over the menu's header/list
      var top = winTop(), h = winH(), all = lines.concat([partial]), i;
      for (i = 0; i < h; i++) {
        clearRow(top + i);
        if (i < all.length && all[i].s) { console.gotoxy(vpCol1, top + i); paint(all[i].s); }
      }
      cursorToInput();
    }
    function morePause() {
      if (js.terminated || !bbs.online) return;
      console.gotoxy(vpCol1, inputRow());
      console.pause();             // prints [Hit a key], waits, erases it -- no newline, so no scroll
      clearRow(inputRow());
      linesSinceInput = 0;
      cursorToInput();
    }
    function newStoryLine() {       // finish `partial`, scroll if the window is full, ready a fresh bottom row
      // [MORE] pacing: count NARRATIVE lines EMITTED since the last input (one per committed line) -- NOT lines
      // scrolled off. linesSinceInput is reset to 0 at every prompt (read/readKey/notePrompt), so it counts only
      // NEW lines; the just-read text already on screen (a room description) scrolls off harmlessly and is NOT
      // counted. The story buffer holds at most winH()-1 committed lines (the bottom row is the input/[MORE] line):
      // pause BEFORE committing another line once that many NEW lines are already on screen, so the next commit
      // can't shift an as-yet-unread line off the top and lose it. The pause is LAZY (see moreFull/emitSeg) so it
      // only triggers when a further line actually arrives -- a turn that ends exactly at the bottom row doesn't.
      // (emitSeg pauses before a TEXT line's first glyph; this catches BLANK lines, which emit no segment. The
      // earlier eager winH()+1 threshold let two new lines scroll away before the first pause -- the dropped-line
      // bug in long output, e.g. Merlin's speech. relayout()'s band-resize shifts are intentionally NOT counted.)
      if (moreFull()) morePause();
      lines.push(partial); partial = { s: "", n: 0 }; inStk = [];
      var scrolled = false;
      while (lines.length > winH() - 1) { lines.shift(); scrolled = true; }
      if (scrolled) repaintStory();              // everything moved up -> repaint the whole window
      else clearRow(inputRow());                 // no scroll -> just clear the new (blank) bottom row
      cursorToInput();
      // The command-echo line (the ">se" the player just typed) is INPUT, not scrolling output -- DOS doesn't
      // count it toward MORE pacing, so neither do we: the first committed line after a read is the echo and is
      // skipped here. Without this, the echo stole a story row and a one-screen room description (which fits in
      // DOS) tripped a spurious [Hit a key] on its trailing blank line.
      if (afterRead) afterRead = false; else linesSinceInput++;
    }
    function pushInput(ch) { var seg = "\x01n" + ch; emitSeg(seg, 1); inStk.push(seg.length); }
    function eraseInput() {         // backspace one echoed input char: drop its styled unit + blank the cell
      if (!inStk.length) return;
      var blen = inStk.pop(), col;
      partial.s = partial.s.substr(0, partial.s.length - blen); partial.n -= 1;
      col = vpCol1 + partial.n;
      console.gotoxy(col, inputRow()); console.attributes = LIGHTGRAY; console.write(" "); console.gotoxy(col, inputRow());
    }
    function repaintStatus() {     // paint the status rows in the SAME strip as the text (window 1 = window 0's columns)
      // In a graphical (FRAMED) game the band isn't set until the pictures are drawn, but Arthur sets up the
      // status window BEFORE that -- painting it then would land at row 1 and leave a stray bar above the
      // banner. Skip until the band exists; setGraphicsBand's relayout repaints it at the right row. BUT only
      // while framed (inset, vpCols < fullCols = a band is coming): in a FULL-SCREEN TEXT mode (Arthur's F6:
      // erase_window -1, window 0 full width) there is legitimately NO band -- graphicsBand stays 0 -- and the
      // status belongs at row 1, so don't suppress it then (else F6 shows a blank screen with no status bar).
      if (graphicsBand === 0 && vpCols < fullCols && game.graphicsAvailable && game.graphicsAvailable()) { cursorToInput(); return; }
      if (!statusH) { cursorToInput(); return; }
      // Anchor the status to the persistent room view. This is the settled paint point (the band has
      // finished growing picture-by-picture for this scene); a framed band within one row of the last
      // committed band is snapped back to it -- Arthur's map background is one row taller than the room
      // scene, and without this the status/text shoves down a row each time the map is toggled. Larger
      // differences (a real scene change) commit the new band.
      if (graphicsBand > 0) {
        if (committedBand > 0 && (graphicsBand - committedBand <= 1) && (committedBand - graphicsBand <= 1)) {
          if (graphicsBand !== committedBand) { graphicsBand = committedBand; relayout(); }
        } else {
          committedBand = graphicsBand;
        }
      }
      var r, line, newTop = graphicsBand + 1, newBot = graphicsBand + statusH;
      // The map<->scene toggle changes the graphics-band height by a row, so the bar moves; clear any row it
      // occupied last time but not now, or the old bar is left stranded (the duplicate status line).
      if (v6prevStatusBot >= v6prevStatusTop) {
        for (r = v6prevStatusTop; r <= v6prevStatusBot; r++) if (r < newTop || r > newBot) clearRow(r);
      }
      for (r = 0; r < statusH; r++) {
        console.gotoxy(vpCol1, newTop + r);             // window-relative model -> strip's left edge
        line = status[r].slice(0, vpCols).join("");
        while (line.length < vpCols) line += " ";        // fill the whole strip so the bar is solid (poles untouched)
        console.putmsg(BARCLR + line.substr(0, vpCols));
      }
      v6prevStatusTop = newTop; v6prevStatusBot = newBot;
      console.attributes = LIGHTGRAY;
      cursorToInput();
    }
    function putStatus(text) {     // apply a window-1 print into the status model (Arthur right-justifies the date)
      if (text.indexOf("\n") < 0 && stCol > 1) {       // right-justify within the STRIP (vpCols), not the full screen
        var vlen = text.replace(/\r/g, "").length;
        if (stCol + vlen - 1 > vpCols) stCol = (vpCols + 1 - vlen) > 1 ? (vpCols + 1 - vlen) : 1;
      }
      // Clear the existing status FIELD at the cursor before overwriting it. Arthur replaces a single field by
      // positioning here and printing the new value -- which may be SHORTER or EMPTY (reverting from a "Turtle"
      // form to no form prints ""). The full-row blank only happens on a room-change redraw, so on a light field
      // update the old field's tail would otherwise linger ("Turtle" stayed centred after `cyr human`). A field is
      // a maximal glyph run whose internal gaps are single spaces, bounded by a 2+-space separator; only clear when
      // the cursor sits ON an old field (a blank cell means a fresh write, so leave the neighbours alone).
      if (text.indexOf("\n") < 0) {
        var fr = stRow - 1;
        if (fr >= 0 && fr < statusH && status[fr] && stCol >= 1 && stCol <= vpCols && status[fr][stCol - 1] !== " ") {
          var fc = stCol, fb = 0;
          while (fc <= vpCols) { if (status[fr][fc - 1] === " ") { fb++; if (fb >= 2) break; } else fb = 0; fc++; }
          var fe = fc - fb, fcc;                          // fe = last non-blank col of the field at the cursor
          for (fcc = stCol; fcc <= fe && fcc <= vpCols; fcc++) status[fr][fcc - 1] = " ";
        }
      }
      var i, ch, rr;
      for (i = 0; i < text.length; i++) {
        ch = text.charAt(i);
        if (ch === "\r") { stCol = 1; continue; }
        if (ch === "\n") { stRow++; stCol = 1; continue; }
        rr = stRow - 1;
        if (rr >= 0 && rr < statusH && stCol >= 1 && stCol <= vpCols) status[rr][stCol - 1] = ch;
        stCol++;
      }
      // Update the MODEL only; painting is deferred to notePrompt/afterInterrupt so the status never lands at
      // an intermediate band row during a framed-scene setup (which left stale bars above the final position).
    }
    // Upper-window (window 2) INFO PANELS (Arthur's stats / room-text, driven by the function keys): Arthur
    // erases the band (our clearRegion blacks the pixels) then writes pixel-positioned text into window 2. We
    // model it like the status bar but painted OVER the band rows, and clear it back to graphics on dismiss.
    function repaintPanel() {
      if (!panelActive || !panel.length) return;
      var r, c;
      // Paint down to the FULL graphics band, not just the panel's content rows. Arthur draws the inventory's
      // vertical separator bars only to window-2's height (12 rows), but our scaled room scene makes the band a
      // row taller -- so the bars otherwise stop one row short of the status bar (a visible gap; DOS's boxes
      // reach the status). Detect each vertical bar (a reverse-styled space column) from the last content row
      // and extend it through the remaining band rows so it meets the status bar.
      var lastR = panel.length - 1, barStyle = [], hasBars = false;     // barStyle[c] = a bar column's style to carry down
      if (graphicsBand > panel.length) {
        // A column qualifies as a vertical SEPARATOR bar only if it's a reverse-styled space at BOTH the top and
        // the bottom content row -- i.e. a full-height vertical line (the inventory's two dividers). This excludes
        // F4's HORIZONTAL meter bars (a reverse run on one row), which must NOT be carried down (it filled a solid
        // inverse block above the status).
        var ls = pstyle[lastR] || [], lp = panel[lastR] || [], ts = pstyle[0] || [], tp = panel[0] || [];
        for (c = 0; c < panWidth(); c++) {
          var lbar = (c < ls.length && ls[c] && (c >= lp.length || lp[c] === " "));
          var tbar = (c < ts.length && ts[c] && (c >= tp.length || tp[c] === " "));
          if (lbar && tbar) { barStyle[c] = ls[c]; hasBars = true; }
        }
      }
      // Extend to the full band ONLY when there are vertical bars to carry down (the inventory); otherwise keep
      // the old clip so a text-only panel (room text) doesn't paint extra blank rows into the band.
      var bandH = (graphicsBand > 0) ? (hasBars ? graphicsBand : Math.min(panel.length, graphicsBand)) : panel.length;
      for (r = 0; r < bandH && (panTopRow() + r) <= rows; r++) {
        console.gotoxy(panLeft(), panTopRow() + r);
        // Group consecutive cells of equal style into one styled run (reverse bars, bold headers, plain text),
        // padding to the full strip with default-style spaces. "" cell style -> normal/LIGHTGRAY ("\x01n").
        var prow = (r < panel.length) ? panel[r] : null, srow = (r < panel.length) ? (pstyle[r] || []) : null;
        var out = "", curPre = null, run = "", pre, chh;
        for (c = 0; c < panWidth(); c++) {
          if (prow) { chh = (c < prow.length) ? prow[c] : " "; pre = (c < srow.length && srow[c]) ? srow[c] : "\x01n"; }
          else { chh = " "; pre = barStyle[c] ? barStyle[c] : "\x01n"; }   // extension row below the content: only the bar columns
          if (pre !== curPre) { if (curPre !== null) out += curPre + run + "\x01n"; curPre = pre; run = ""; }
          run += chh;
        }
        if (curPre !== null) out += curPre + run + "\x01n";
        console.attributes = LIGHTGRAY; paint(out);
      }
      console.attributes = LIGHTGRAY; cursorToInput();
    }
    function panAt(row) { while (panel.length < row) { panel.push(blankRow()); pstyle.push(blankStyleRow()); } }
    function panPut(row, col, ch, pre) {   // store a char + its style into the panel grids at a 1-based cell
      panAt(row);
      if (col >= 1 && col <= panWidth()) { panel[row - 1][col - 1] = ch; pstyle[row - 1][col - 1] = pre || ""; }
    }
    function flushPanWord() {       // commit the buffered word, wrapping to a fresh panel row if it won't fit
      if (!panPending.length) return;
      var w = panPending, lim = panWidth(), j; panPending = "";
      if (panCol > 1 && panCol + w.length - 1 > lim) { panRow++; panCol = 1; }
      for (j = 0; j < w.length; j++) {
        if (panCol > lim) { panRow++; panCol = 1; }                 // word longer than the strip -> hard break
        panPut(panRow, panCol, w.charAt(j), panPendPrefix);
        panCol++;
      }
    }
    function panelPrint(text) {     // window-2 text: word-wrap flowing text (room desc) at the strip edge; positioned
      panelActive = true;          // fields (stats) keep their explicit set_cursor columns (flushed on each set_cursor)
      var i, ch;
      panAt(panRow);
      for (i = 0; i < text.length; i++) {
        ch = text.charAt(i);
        if (ch === "\r") { flushPanWord(); panCol = 1; }
        else if (ch === "\n") { flushPanWord(); panRow++; panCol = 1; panAt(panRow); }
        // A styled space carries the current attr (Arthur's reverse-video separator bars are single reverse spaces):
        // store it WITH its style so the reverse cell paints, instead of dropping it as a plain gap.
        else if (ch === " ") { flushPanWord(); if (panCol > 1 && panCol <= panWidth()) panPut(panRow, panCol, " ", jszm_attr_prefix(attr)); if (panCol <= panWidth()) panCol++; }
        else { if (!panPending.length) panPendPrefix = jszm_attr_prefix(attr); panPending += ch; }   // capture style at word start (set before the print)
      }
      repaintPanel();
    }
    function clearPanelRow(rr) {   // blank the panel's own column span (defaults to the story inset = Arthur's clearRow)
      var c1 = panLeft(), n = panWidth(); if (c1 + n > cols) n = cols - c1;
      console.gotoxy(c1, rr); console.attributes = LIGHTGRAY; if (n > 0) console.write(blanks(n));
    }
    function clearPanel() {         // dismiss the panel: drop the model + blank the band text rows it painted
      var r, n = panel.length; if (graphicsBand > n) n = graphicsBand;   // also clear bar-extension rows below the content
      panel = []; pstyle = []; panPending = ""; panPendPrefix = ""; panelActive = false; panRow = 1; panCol = 1;
      for (r = 0; r < n && (panTopRow() + r) <= rows; r++) clearPanelRow(panTopRow() + r);
    }
    // POSITIONED full-screen overlay (Arthur's InvisiClues hint menu and similar cursor-addressed v6 UIs). The
    // game @split_windows, @erase_window -1's the screen, then cursor-addresses windows 0/1 with text at absolute
    // positions (set_cursor is window-relative: screen pixel = window.origin + cursor - 1). Our normal renderer
    // routes window 0 to the scrolling story and window 1 to the status bar, so the menu collapsed. While the
    // overlay is active we instead paint each fragment directly at its computed cell, with word-wrap for the
    // flowing hint-clue text and set_text_style honoured (the selected item's reverse highlight). Direct paint, no
    // model: the menu fully redraws itself on every keypress, and a resume @erase_window -1 + scene redraw rebuilds
    // the game screen. Entered on a non-zero @split_window (graphical), exited at the next line read() (the
    // restored command prompt). Calibrated to Arthur -- the only v6 game exercised; gate stays on graphical v6.
    var v6pos = false;             // overlay active
    var posRow = 1, posCol = 1, posPend = "", posAlign = 0;   // paint cursor (cells) + buffered word + field align (0 L,1 C,2 R)
    function posWinLeft()  { var w = game.windows && game.windows[curWindow]; return Math.floor((w ? (w[1] & 0xffff) : 0) / cellW) + 1; }
    function posWinRight() { var w = game.windows && game.windows[curWindow]; if (!w) return cols;
      var r = Math.floor((((w[1] & 0xffff) + (w[3] & 0xffff) - 1)) / cellW) + 1; return r > cols ? cols : (r < 1 ? cols : r); }
    // TOP-ANCHOR the overlay rather than honour Arthur's absolute window origins: it reuses the room's window
    // layout (status y=193, story y=209) for the menu, which drops it into the lower half AND leaves only 1 row
    // between the (3-row) header and the list. DOS shows the menu in the UPPER half, header above list. So we map
    // the header window (1) to the top rows and the content window (0/others) just below it. (Arthur-tuned: it is
    // the only v6 game with these cursor-addressed UIs.) Column placement keeps Arthur's x (left/centre/right).
    var POS_CONTENT_TOP = 4;       // content (list/clues) starts on this row; the header occupies rows 1..3 above it
    function posSet(y, x) {         // window-relative set_cursor (pixels) -> top-anchored screen cell
      posFlushWord();
      var w = game.windows && game.windows[curWindow];
      var ox = w ? (w[1] & 0xffff) : 0, ow = w ? (w[3] & 0xffff) : 0;
      var relRow = Math.floor((y > 1 ? y - 1 : 0) / cellH);
      posRow = ((curWindow === 1) ? 1 : POS_CONTENT_TOP) + relRow;
      posCol = Math.floor((ox + (x > 1 ? x - 1 : 0)) / cellW) + 1;
      // Header (win1) fields use x as a left/centre/right ANCHOR (Arthur sets 1 / width/2 / width); detect it so a
      // right-anchored field ("Return for hints.", "Q to resume story.") is right-justified instead of overflowing
      // and wrapping onto the next row. Content windows always left-flow.
      posAlign = 0;
      if (curWindow === 1 && ow > 0) {
        if (x >= ow - cellW) posAlign = 2;                          // right edge -> right-align
        else if (Math.abs(x - ow / 2) <= cellW * 2) posAlign = 1;   // ~centre -> centre
      }
      if (posRow < 1) posRow = 1; if (posCol < 1) posCol = 1;
    }
    function posFlushWord() {       // commit the buffered word, wrapping to the next row if it won't fit the window
      if (!posPend.length) return;
      var word = posPend, wl = posWinLeft(), wr = posWinRight(); posPend = "";
      if (posCol > wl && (posCol + word.length - 1) > wr) { posRow++; posCol = wl; }
      if (posRow >= 1 && posRow <= rows && posCol >= 1 && posCol <= cols) { console.gotoxy(posCol, posRow); paint(styled(word)); }
      posCol += word.length;
    }
    function posPrint(text) {       // paint a fragment at the cursor: positioned items + flowing word-wrapped clues
      if (curWindow === 1) {        // header field: a single positioned line, aligned at the anchor (no word-wrap)
        var t = text.replace(/[\r\n]/g, "");
        if (!t.length) return;
        var col = posCol;
        if (posAlign === 2) col = posCol - t.length + 1;            // right: text ENDS at the anchor
        else if (posAlign === 1) col = posCol - Math.floor(t.length / 2);   // centre on the anchor
        if (col + t.length - 1 > cols) col = cols - t.length + 1;
        if (col < 1) col = 1;
        if (posRow >= 1 && posRow <= rows) { console.gotoxy(col, posRow); paint(styled(t)); }
        posCol = col + t.length;
        return;
      }
      var i, ch, wl = posWinLeft();
      for (i = 0; i < text.length; i++) {
        ch = text.charAt(i);
        if (ch === "\r") { posFlushWord(); posCol = wl; }
        else if (ch === "\n") { posFlushWord(); posRow++; posCol = wl; }
        else if (ch === " ") { posFlushWord(); if (posRow >= 1 && posRow <= rows && posCol >= 1 && posCol <= cols) { console.gotoxy(posCol, posRow); paint(styled(" ")); } posCol++; }   // styled space -> inter-word gap keeps the reverse highlight
        else posPend += ch;
      }
      posFlushWord();
    }
    function posClearWin(w) {       // @erase_window <w>: blank the top-anchored screen rows (header vs content zone)
      var top = (w === 1) ? 1 : POS_CONTENT_TOP, bot = (w === 1) ? (POS_CONTENT_TOP - 1) : rows, r;
      for (r = top; r <= bot && r <= rows; r++) if (r >= 1) { console.gotoxy(1, r); console.attributes = LIGHTGRAY; console.cleartoeol(); }
    }
    function relayout() {          // band/status geometry changed -> re-fit the buffer + repaint the story
      setRegion();                 // re-arm the DECSTBM scroll region for the new band/status top
      while (lines.length > winH() - 1) lines.shift();
      // Deliberately do NOT repaint the status here: during a framed-scene setup the band grows picture by
      // picture (0 -> 11 -> 12), and repainting the status at each intermediate band leaves a stale bar one
      // row above the final position. The status is repainted once the layout settles (notePrompt) and on
      // live updates (putStatus/afterInterrupt).
      repaintStory();
    }
    return {
      flushWord: flushWord,   // commit the buffered word -- the door calls this before a style/colour change so a
                              // word buffered across fragments can't pick up the NEXT attribute (e.g. "points.]")
      setGraphicsBand: function(n) {
        n = (n > 0) ? n : 0; if (n > rows - 1) n = rows - 1;
        // Once anchored to a room view (committedBand set at the prompt), absorb +/-1-row fluctuations so
        // the story text doesn't bounce as a scene redraws picture-by-picture or the map (a row taller than
        // the room scene) is toggled. Keyed on the STABLE committedBand, not the live band, so the first
        // scene still grows freely (0 -> 11 -> 12) to establish the anchor; a >= 2-row change relayouts.
        if (committedBand > 0 && n > 0 && (n - committedBand <= 1) && (committedBand - n <= 1)) n = committedBand;
        if (n === graphicsBand) return;
        graphicsBand = n; relayout();
      },
      setBottomInset: function(n) {   // reserve rows at the screen bottom for a border-frame bottom border (Zork Zero)
        n = (n > 0) ? n : 0; if (n > rows - 1) n = rows - 1;
        if (n === bottomInset) return;
        bottomInset = n; relayout();   // re-fit the buffer + re-arm the scroll region for the new bottom bound
      },
      setTextInset: function(ext) {   // narrow the story text + status to the game's window-0 strip (between poles)
        var nc1, nco;
        if (ext && ext.w > 0) {
          nc1 = Math.floor(ext.x / cellW) + fullCol1;   // ext.x is in the game's (un-offset) pixel space; shift by the
                                                        // centring offset (fullCol1 = v6offsetCols+1) so the inset text
                                                        // lines up with the offset graphics. (SyncTERM offset 0 -> +1, unchanged.)
          nco = Math.floor(ext.w / cellW);
          if (nc1 < fullCol1) nc1 = fullCol1;
          if (nco < 16) nco = 16;                          // never absurdly narrow
          if (nc1 + nco - 1 > cols) nco = cols - nc1 + 1;
        } else { nc1 = fullCol1; nco = fullCols; }
        if (nc1 === vpCol1 && nco === vpCols) return;       // unchanged
        lines = []; partial = { s: "", n: 0 }; pendingWord = ""; inStk = [];   // old rows were wrapped at a different width -> drop them
        vpCol1 = nc1; vpCols = nco;
        // Re-arm the scroll rectangle for the NEW strip width. cterm enforces the DECSLRM left/right margins by
        // VALUE (see setRegion), so if the inset widens AFTER the last relayout/setRegion (e.g. Arthur's F6->F5
        // path narrows to 73 then widens back to 80 with no band change in between), the margins stay stuck at the
        // narrow extent and CLIP everything painted past them -- the right-justified status date lost its tail
        // ("St John's Day, Terce" -> "St John's Day, T") until a Ctrl-R re-armed the region. relayout() already
        // does this on a band change; the inset is the other geometry axis and must do it too.
        setRegion();
        // No screen-clear here: a scene change already console.clear()ed, and the side poles are drawn just
        // before this fires -- clearing would wipe them. repaintStory clears only the NEW strip's columns.
        // (Status paint is deferred to notePrompt -- see relayout -- so it never lands at an intermediate band.)
        repaintStory();
      },
      setPanelRegion: function(col1, n, top) { setPanelRegion(col1, n, top); },
      enter: function() {
        console.clear(LIGHTGRAY, false);   // autopause=false: no [Hit a key] before clearing
        graphicsBand = 0; committedBand = 0; statusH = 0; curWindow = 0; status = []; stRow = 1; stCol = 1;
        lines = []; partial = { s: "", n: 0 }; pendingWord = ""; inStk = []; linesSinceInput = 0;
        setRegion(); console.gotoxy(1, 1);
      },
      exit: function() { resetRegion(); console.write("\x1b[?25h"); console.attributes = LIGHTGRAY; console.gotoxy(1, rows); },   // restore the cursor (may have been hidden over a cutscene)
      print: function(text) {
        // Positioned full-screen overlay (hint menu): paint at the cursor cell, not the story/status models.
        if (v6pos) { posPrint(text); return; }
        // Only window 1 is the status bar. Arthur leaves other windows selected at times (seen: window 3 for
        // a parser reply, plus picture windows 2/7); text printed then is narrative, not status -- route
        // everything except window 1 to the story so "I beg your pardon?" never overwrites the status bar.
        if (curWindow === 1) { flushWord(); putStatus(text); return; }
        // Window 2 = the upper-window INFO PANEL (Arthur's stats / room-text). Paint it over the band instead of
        // the story scroll. Only in a graphical (framed) game -- a non-graphical v6 has no band, so keep the old
        // story-routing. In windowed-text mode, the menu window (win3) also routes to the side panel.
        if ((curWindow === 2 || isMenuWin(curWindow)) && game.graphicsAvailable && game.graphicsAvailable()) { flushWord(); panelPrint(text); return; }
        // Door-generated styled text (the ">" prompt isn't this -- the GAME prints that as plain text -- but the
        // "[ Resumed ]" markers + resume prompt do) carries Ctrl-A codes: keep it on the old per-fragment wrap so
        // the codes aren't buffered as visible word chars. Everything the game prints is plain text.
        if (text.indexOf("\x01") >= 0) {
          flushWord();
          var segs = wrapLines(text, vpWidth(), partial.n), j, t;
          for (j = 0; j < segs.length; j++) { t = segs[j].t; if (t.length) emitSeg(styled(t), t.length); if (segs[j].nl) newStoryLine(); }
          cursorToInput();
          return;
        }
        // Plain game narrative: buffer whole words (runs of non-space) across genPrint fragments so a word split
        // into separate print calls (e.g. "points" then ",") word-wraps as one unit -- break only at spaces.
        var i, ch;
        for (i = 0; i < text.length; i++) {
          ch = text.charAt(i);
          if (ch === "\n") { flushWord(); newStoryLine(); }
          else if (ch === " ") {
            flushWord();
            if (partial.n >= vpWidth()) newStoryLine();         // a space at the edge IS the wrap point -> drop it
            else if (partial.n > 0) emitSeg(styled(" "), 1);    // keep inter-word spacing; never lead a row with a space
          } else pendingWord += ch;
        }
        cursorToInput();
      },
      read: function(maxlen) {
        if (v6pos) { v6pos = false; posPend = ""; }   // a line read = the game's command prompt is back -> leave the overlay
        flushWord();   // commit any buffered narrative word before the prompt/input
        linesSinceInput = 0; afterRead = true; curWindow = 0; inStk = []; repaintStatus();   // layout settled -> ensure status is shown; next committed line = the command echo (uncounted)
        // Cap the echo so it can't reach the viewport's right edge: a wrap there would scroll the strip. Commands
        // are short. Read CHAR-BY-CHAR (not console.getstr) so Ctrl-R is intercepted BY THE DOOR (forced redraw)
        // instead of being swallowed by getstr's own line editor (which redraws the line and clips the pole). A
        // BBS hot-key (Ctrl-U etc.) is handled by sbbs inside getkey and returns empty here, so the loop just
        // continues -- hotkeys keep working. Mirrors getstr K_NOCRLF editing: Enter ends, backspace erases,
        // printables echo up to `cap`; no trailing newline (newStoryLine advances the buffer row instead).
        var room = vpWidth() - partial.n; if (room < 1) { newStoryLine(); room = vpWidth(); }
        var cap = (maxlen < room) ? maxlen : room;
        console.attributes = LIGHTGRAY; cursorToInput();
        game.readTerminator = undefined;                            // default: engine stores Return (13)
        var s = "", c, fk, termFk = false;
        var oldPT = console.ctrlkey_passthru; console.ctrlkey_passthru = oldPT | V6_CTRL_PASSTHRU;   // belt-and-suspenders alongside K_CTRLKEYS
        try {
          for (;;) {
            if (js.terminated || !bbs.online) break;
            var kk = v6getKey();                                       // K_CTRLKEYS read + our cterm ESC-seq parser
            if (kk.fk) {                                               // an arrow / function key from an escape sequence
              if (v6isTerminator(kk.fk)) { game.readTerminator = kk.fk; termFk = true; break; }   // bound terminator -> action
              continue;                                                // a key the game doesn't bind -> ignore
            }
            c = kk.ch;
            if (c < 0) { if (js.terminated || !bbs.online) break; continue; }   // disconnect / swallowed lone-ESC
            if (c === CTRL_R.charCodeAt(0)) { if (game.forceRedraw) game.forceRedraw(); cursorToInput(); continue; }
            if (c === CTRL_P.charCodeAt(0)) { if (game.forceReupload) game.forceReupload(); cursorToInput(); continue; }   // debug: re-upload current screen images
            if (c === CTRL_K.charCodeAt(0)) { if (game.showHelp) game.showHelp(); cursorToInput(); continue; }   // Ctrl-K: redisplay the key/help splash on demand (intercept before the Ctrl->F-key map)
            fk = v6ctrlToFkey(c);                                       // Ctrl-A..Ctrl-L -> F1..F12 (now distinct from the arrows)
            if (fk && v6isTerminator(fk)) { game.readTerminator = fk; termFk = true; break; }   // function-key terminator
            if (c === 13 || c === 10) break;                            // Enter -> done
            if (c === 8 || c === 127) { if (s.length) { s = s.slice(0, -1); eraseInput(); } continue; }   // backspace
            if (c >= 32 && c <= 126 && s.length < cap) { var ks = String.fromCharCode(c); s += ks; pushInput(ks); }   // printable -> echo + record
          }
        } finally { console.ctrlkey_passthru = oldPT; }
        if (!termFk) newStoryLine();   // a function-key terminator triggers an action, not a typed line -- don't advance/scroll the prompt
        return s;
      },
      readKey: function() {                               // @read_char untimed: Ctrl-R/P, cterm ESC-seq -> ZSCII, else one key
        flushWord();
        linesSinceInput = 0; curWindow = 0; repaintStatus(); cursorToInput();
        var oldPT = console.ctrlkey_passthru; console.ctrlkey_passthru = oldPT | V6_CTRL_PASSTHRU;
        try {
          for (;;) {
            if (js.terminated || !bbs.online) return "";
            var kk = v6getKey();
            if (kk.fk) return String.fromCharCode(kk.fk);   // arrow / function key -> its ZSCII ext code
            var c = kk.ch;
            if (c < 0) { if (js.terminated || !bbs.online) return ""; continue; }
            if (c === CTRL_R.charCodeAt(0)) { if (game.forceRedraw) game.forceRedraw(); cursorToInput(); continue; }
            if (c === CTRL_P.charCodeAt(0)) { if (game.forceReupload) game.forceReupload(); cursorToInput(); continue; }   // debug: re-upload current screen images
            if (c === CTRL_K.charCodeAt(0)) { if (game.showHelp) game.showHelp(); cursorToInput(); continue; }   // Ctrl-K: redisplay the key/help splash on demand (before the Ctrl->F-key map)
            var fk = v6ctrlToFkey(c);                       // Ctrl-A..Ctrl-L -> F-key (read_char takes any key)
            if (fk) return String.fromCharCode(fk);
            return String.fromCharCode(c);
          }
        } finally { console.ctrlkey_passthru = oldPT; }
      },
      echo: function(x) {            // timed-read line-editor echo (number=key, 8=erase, 10/13=newline; string=re-show)
        if (curWindow !== 0) return;
        if (typeof x === "number") {
          if (x === 8) eraseInput();
          else if (x === 10 || x === 13) newStoryLine();   // Enter -> advance the buffer line (not a literal LF)
          else { if (partial.n >= vpWidth()) newStoryLine(); pushInput(String.fromCharCode(x)); }
        } else {
          var str = String(x), i, c;
          for (i = 0; i < str.length; i++) {
            c = str.charAt(i);
            if (c === "\n") newStoryLine();                // advance the buffer line, NOT a literal LF
            else if (c === "\r") continue;                 // CR is part of the CRLF newline echo -> skip
            else { if (partial.n >= vpWidth()) newStoryLine(); pushInput(c); }
          }
        }
        cursorToInput();
      },
      updateStatusLine: function() {},
      screen: function(w) {
        if (v6pos) {                                    // overlay: window origin drives positioning
          // Entering the header (win1) from another window = a view redraw (main menu <-> a topic). Arthur only
          // clears the header rows from the far-right column, so the previous view's title/fields bleed through
          // ("RReturn for hints.", the caption overlap). Clear the whole header zone before the new fields draw.
          if (w === 1 && curWindow !== 1) posClearWin(1);
          curWindow = w; return;
        }
        if ((curWindow === 2 || isMenuWin(curWindow)) && w !== curWindow && panelActive) { flushPanWord(); repaintPanel(); }   // commit the last buffered word when leaving the panel
        curWindow = w;
        if (w === 2 || isMenuWin(w)) { panRow = 1; panCol = 1; panPending = ""; panPendPrefix = ""; }   // info panel OR Journey win1 menu: reset the grid cursor
        else if (w === 1) { stRow = 1; stCol = 1; if (statusH === 0) { statusH = 1; status = [blankRow()]; relayout(); } }
      },
      setCursor: function(y, x) {
        if (v6pos) { posSet(y, x); return; }            // overlay: window-relative pixel -> absolute screen cell
        if (curWindow === 2 || isMenuWin(curWindow)) {   // upper-window panel OR Journey menu panel: pixel -> cell
          flushPanWord();                               // commit any buffered word at the OLD position before moving
          panRow = (Math.floor((y > 0 ? y - 1 : 0) / cellH) + 1) - (panTopRow() - 1);   // absolute screen row -> grid-relative (panTop 1 = unchanged)
          if (panRow < 1) panRow = 1;
          panCol = Math.floor((x > 0 ? x - 1 : 0) / cellW) + 1;
          if (panCol < 1) panCol = 1; if (panCol > panWidth()) panCol = panWidth();
          panAt(panRow);
          return;
        }
        if (curWindow !== 1) return;
        stRow = Math.floor((y > 0 ? y - 1 : 0) / cellH) + 1;
        stCol = Math.floor((x > 0 ? x - 1 : 0) / cellW) + 1;
        while (status.length < stRow) status.push(blankRow());
        if (stRow > statusH) { statusH = stRow; relayout(); }
      },
      getCursor: function() {
        return curWindow === 1 ? { row: stRow, col: stCol } : { row: inputRow(), col: partial.n + 1 };
      },
      eraseWindow: function(w) {
        var r;
        if (v6pos && w >= 0) { posClearWin(w); return; }   // overlay redraw: blank just this window's screen rows
        if (w == -1 || w == -2) {
          console.clear(LIGHTGRAY, false);
          panel = []; pstyle = []; panPending = ""; panPendPrefix = ""; panelActive = false; panRow = 1; panCol = 1;   // full clear drops the info panel too (the console.clear wiped its cells)
          // The screen was fully cleared, so there is no stranded status bar to wipe later -- and the bar's last
          // position is now stale. Forget it (bot<top = none), or repaintStatus' "clear the row the bar used to
          // occupy" step fires on the OLD row after the next scene is drawn. Arthur's F6 text mode parks the
          // status at row 1 (full-screen text); returning to a framed scene re-homes it to the band's bottom and
          // would otherwise clearRow(1) -- black-painting the top row of the just-drawn picture (the missing top
          // border on the map; the band above the room). (Scene/map toggles use PARTIAL erases, not -1/-2, so
          // their stranded-bar cleanup is unaffected.)
          v6prevStatusTop = 1; v6prevStatusBot = 0;
          if (w == -1) { graphicsBand = 0; committedBand = 0; statusH = 0; status = []; }   // full clear -> re-anchor the band fresh
          else { for (r = 0; r < statusH; r++) status[r] = blankRow(); }
          lines = []; partial = { s: "", n: 0 }; pendingWord = ""; inStk = []; linesSinceInput = 0;
          repaintStatus(); cursorToInput();
        } else if (w === 2 || isMenuWin(w)) { clearPanel(); }   // dismiss the upper-window info panel -> back to the graphics band
        else if (w === 1) {
          for (r = 0; r < statusH; r++) status[r] = blankRow();
          repaintStatus();
        } else if (w === 0) {                            // story window: clear the text only
          lines = []; partial = { s: "", n: 0 }; pendingWord = ""; inStk = []; linesSinceInput = 0;
          repaintStory();
        }
        // other windows (graphics sub-windows) -> no text-side action; pixel clears go via clearRegion
      },
      eraseLine: function(value) {
        if (curWindow === 2 || isMenuWin(curWindow)) {                          // erase_line in the info panel
          flushPanWord(); panAt(panRow);
          // Z-spec 15 (v6): erase_line 1 = blank to the line's end; any other value = erase that many PIXELS
          // rightward from the cursor (cursor unmoved). Arthur clears each inventory column with `erase_line
          // <col-width-px>` so it stops just before the reverse-video separator bar -- honour the pixel count or
          // a full-strip clear would wipe the separators on the header/text rows.
          var last = vpCols;
          if (value && value !== 1) {
            last = Math.floor(((panCol - 1) * cellW + value - 1) / cellW) + 1;   // last cell the pixel span touches
            if (last > vpCols) last = vpCols;
          }
          var c; for (c = panCol; c <= last; c++) { panel[panRow - 1][c - 1] = " "; pstyle[panRow - 1][c - 1] = ""; }
          repaintPanel(); return;
        }
        console.cleartoeol();
      },
      windowOrigin: function(w) { var win = game.windows && game.windows[w]; return (win && typeof win[1] === "number") ? { x: win[1], y: win[0] } : { x: 0, y: 0 }; },
      bufferMode: function() {},
      notePrompt: function() { flushWord(); linesSinceInput = 0; repaintStatus(); },   // layout has settled -> paint the status now
      afterInterrupt: function() { repaintStatus(); },
      // Repaint everything this renderer owns onto a (just-cleared) screen: re-arm the scroll region, then
      // paint the status bar and the story window -- repaintStory includes `partial`, so the in-progress
      // input command comes back too. The caller re-blits the pictures before invoking this.
      redraw: function() { flushWord(); setRegion(); repaintStatus(); repaintStory(); },
      armRegion: function() { setRegion(); },            // (re)set the DECSTBM scroll region at the current band/status top
      windowChanged: function() {}, scrollWindow: function() {},
      endOverlay: function() { if (v6pos) { v6pos = false; posPend = ""; } },   // a picture means we're back in the graphical room -> leave the overlay so the band/status geometry rebuilds normally
      inOverlay: function() { return v6pos; },   // v6syncBand hides the text cursor while a key-driven overlay (hint menu) owns the screen
      split: function(h) {   // a non-zero split in a graphical v6 game = entering a cursor-addressed full-screen UI
        if (h > 0 && game.graphicsAvailable && game.graphicsAvailable()) { v6pos = true; posPend = ""; posRow = 1; posCol = 1;
          resetRegion();   // full-screen text overlay: drop the band/inset DECSTBM+DECSLRM so the top rows (header) paint
        }
      },
      captureUpper: function() {
        if (!statusH) return "";
        var out = String(statusH), r; for (r = 0; r < statusH; r++) out += "\n" + status[r].join("");
        return out;
      },
      restoreUpper: function(s) {
        if (!s) return;
        var nl = s.indexOf("\n"), h = parseInt(nl < 0 ? s : s.substr(0, nl), 10);
        if (!(h > 0)) return;
        var body = nl < 0 ? [] : s.substr(nl + 1).split("\n"), r, c, line, a;
        status = [];
        for (r = 0; r < h; r++) { a = blankRow(); line = body[r] || ""; for (c = 0; c < line.length && c < cols; c++) a[c] = line.charAt(c); status.push(a); }
        statusH = h; stRow = 1; stCol = 1; relayout();
      }
    };
  }
}

if (typeof JSZM_DOOR_NO_MAIN == "undefined") {
  js.on_exit("jszm_flush_resume()");   // top-level scope == the door's js_scope, so the handler is reachable at exit
  jszm_door_main();
}
