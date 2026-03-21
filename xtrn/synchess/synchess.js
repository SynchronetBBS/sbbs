"use strict";
require("sbbsdefs.js", 'K_NOCRLF');
load("synchess_logic.js");
load("synchess_engine.js");
load("cterm_lib.js");

// ── Layout ───────────────────────────────────────────────────────────────────
var boardX    = 20;
var boardY    = 6;
var cellW     = 5;
var cellH     = 2;
var LEFT_COL  = 2;
var LEFT_W    = 16;
var RIGHT_COL = boardX + 8 * cellW + 2;  // = 62
var RIGHT_W   = 18;
var CAP_W     = 3;   // chars per captured icon
var CAP_H     = 2;   // rows per captured icon
var CAP_PER   = Math.floor(RIGHT_W / CAP_W);  // 6 per row
var STATUS_ROW = boardY + 8 * cellH + 1;
var INPUT_ROW  = STATUS_ROW + 1;

var SF_PATH      = "/usr/games/stockfish";
var useStockfish = true;
var imgDir       = js.exec_dir + "images/";

// ── ANSI color scheme (defaults, overridden by user prefs) ───────────────────
var darkBg  = "44";    // dark square bg    (ANSI 40-47)
var lightBg = "46";    // light square bg   (ANSI 40-47)
var whiteFg = "1;37";  // white piece fg
var blackFg = "1;33";  // black piece fg (bright yellow = visible on dark/cyan)

// ── Load global config (synchess.ini — admin-set Stockfish path etc.) ────────
(function() {
    var f = new File(js.exec_dir + "synchess.ini");
    if (!f.open("r")) return;
    var lines = f.readAll();
    f.close();
    var vals = {};
    for (var i = 0; i < lines.length; i++) {
        var kv = lines[i].trim().match(/^([^=;#\[]+)=(.*)$/);
        if (kv) vals[kv[1].trim()] = kv[2].trim();
    }
    if (vals['path'])          SF_PATH      = vals['path'];
    if (vals['use_stockfish'] !== undefined)
        useStockfish = (vals['use_stockfish'] !== 'false');
})();

// ── Game state ────────────────────────────────────────────────────────────────
var game = new ChessGame(user.number);
game.init();

// ── Guest detection ───────────────────────────────────────────────────────────
var isGuest = user.compare_ars("GUEST");

// ── Per-user settings & stats ─────────────────────────────────────────────────
var showLegalMoves  = true;
var allowUndo       = false;
var showOpeningName = true;
var moveDisplay     = 'all';   // 'all', 'last', 'none'
var selBg           = "43";    // selected square bg  (ANSI 40-47)
var hintBg          = "42";    // hint square bg      (ANSI 40-47)
var hideWhite       = false;   // blindfold: hide white pieces
var hideBlack       = false;   // blindfold: hide black pieces
var stats = {
    wins_checkmate:     0,
    wins_resign:        0,
    losses_checkmate:   0,
    losses_resign:      0,
    losses_abandoned:   0,
    draws_stalemate:    0,
    draws_insufficient: 0,
    draws_threefold:    0,
    draws_fifty:        0,
    draws_agreed:       0
};

// ── User INI file ─────────────────────────────────────────────────────────────
var iniPath = system.data_dir + "user/" + format("%04d.synchess.ini", user.number);

function loadIni() {
    var f = new File(iniPath);
    if (!f.open("r")) return;
    var lines = f.readAll();
    f.close();
    var section = '', vals = {};
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();
        var sm = line.match(/^\[(.+)\]$/);
        if (sm) { section = sm[1]; continue; }
        var kv = line.match(/^([^=]+)=(.*)$/);
        if (kv) vals[section + '.' + kv[1].trim()] = kv[2].trim();
    }
    if (vals['settings.level'])      game.level = parseInt(vals['settings.level'], 10) || 3;
    if (vals['settings.show_hints'] !== undefined)
        showLegalMoves = (vals['settings.show_hints'] !== 'false');
    if (vals['settings.allow_undo'] !== undefined)
        allowUndo = (vals['settings.allow_undo'] === 'true');
    if (vals['settings.show_opening'] !== undefined)
        showOpeningName = (vals['settings.show_opening'] !== 'false');
    if (vals['settings.move_display'])
        moveDisplay = vals['settings.move_display'];

    if (vals['colors.dark_bg'])   darkBg  = vals['colors.dark_bg'];
    if (vals['colors.light_bg'])  lightBg = vals['colors.light_bg'];
    if (vals['colors.white_fg'])  whiteFg = vals['colors.white_fg'];
    if (vals['colors.black_fg'])  blackFg = vals['colors.black_fg'];
    if (vals['colors.sel_bg'])    selBg     = vals['colors.sel_bg'];
    if (vals['colors.hint_bg'])   hintBg    = vals['colors.hint_bg'];
    if (vals['colors.hide_white'] !== undefined) hideWhite = (vals['colors.hide_white'] === 'true');
    if (vals['colors.hide_black'] !== undefined) hideBlack = (vals['colors.hide_black'] === 'true');
    var sv = function(k) { return parseInt(vals['stats.' + k], 10) || 0; };
    stats.wins_checkmate     = sv('wins_checkmate');
    stats.wins_resign        = sv('wins_resign');
    stats.losses_checkmate   = sv('losses_checkmate');
    stats.losses_resign      = sv('losses_resign');
    stats.losses_abandoned   = sv('losses_abandoned');
    stats.draws_stalemate    = sv('draws_stalemate');
    stats.draws_insufficient = sv('draws_insufficient');
    stats.draws_threefold    = sv('draws_threefold');
    stats.draws_fifty        = sv('draws_fifty');
    stats.draws_agreed       = sv('draws_agreed');
}

function saveIni() {
    if (isGuest) return;
    var f = new File(iniPath);
    if (!f.open("w")) return;
    f.writeln("[settings]");
    f.writeln("level="        + game.level);
    f.writeln("show_hints="    + (showLegalMoves  ? "true" : "false"));
    f.writeln("allow_undo="   + (allowUndo       ? "true" : "false"));
    f.writeln("show_opening=" + (showOpeningName ? "true" : "false"));
    f.writeln("move_display=" + moveDisplay);

    f.writeln("");
    f.writeln("[colors]");
    f.writeln("dark_bg="  + darkBg);
    f.writeln("light_bg=" + lightBg);
    f.writeln("white_fg=" + whiteFg);
    f.writeln("black_fg=" + blackFg);
    f.writeln("sel_bg="     + selBg);
    f.writeln("hint_bg="   + hintBg);
    f.writeln("hide_white=" + (hideWhite ? "true" : "false"));
    f.writeln("hide_black=" + (hideBlack ? "true" : "false"));
    f.writeln("");
    f.writeln("[stats]");
    f.writeln("wins_checkmate="     + stats.wins_checkmate);
    f.writeln("wins_resign="        + stats.wins_resign);
    f.writeln("losses_checkmate="   + stats.losses_checkmate);
    f.writeln("losses_resign="      + stats.losses_resign);
    f.writeln("losses_abandoned="   + stats.losses_abandoned);
    f.writeln("draws_stalemate="    + stats.draws_stalemate);
    f.writeln("draws_insufficient=" + stats.draws_insufficient);
    f.writeln("draws_threefold="    + stats.draws_threefold);
    f.writeln("draws_fifty="        + stats.draws_fifty);
    f.writeln("draws_agreed="       + stats.draws_agreed);
    f.close();
}

// ── Saved game ────────────────────────────────────────────────────────────────
var gameSavePath = system.data_dir + "user/" + format("%04d.synchess.save", user.number);

function saveGame() {
    if (isGuest) return false;
    var f = new File(gameSavePath);
    if (!f.open("w")) return false;
    f.writeln("player_color=" + game.playerColor);
    f.writeln("level=" + game.level);
    var ucis = [];
    for (var i = 0; i < game.history.length; i++) ucis.push(game.history[i].uci);
    f.writeln("moves=" + ucis.join(" "));
    f.close();
    return true;
}

function loadSavedGame() {
    var f = new File(gameSavePath);
    if (!f.open("r")) return false;
    var lines = f.readAll();
    f.close();
    var vals = {};
    for (var i = 0; i < lines.length; i++) {
        var kv = lines[i].match(/^([^=]+)=(.*)$/);
        if (kv) vals[kv[1].trim()] = kv[2].trim();
    }
    if (!vals['player_color']) return false;
    game.playerColor = vals['player_color'];
    if (vals['level']) game.level = parseInt(vals['level'], 10) || game.level;
    var moveStr = vals['moves'] || '';
    if (!moveStr) return true;
    var moves = moveStr.split(' ');
    for (var mi = 0; mi < moves.length; mi++) {
        var mv = moves[mi];
        if (!mv || mv.length < 4) continue;
        var fs = game.toCoords(mv.substring(0, 2));
        var ts = game.toCoords(mv.substring(2, 4));
        var pr = mv.length >= 5 ? mv[4].toUpperCase() : null;
        if (!game.isLegalMove(fs.r, fs.c, ts.r, ts.c)) { game.init(); return false; }
        game.applyMove(fs, ts, pr);
    }
    return true;
}

function deleteSavedGame() { (new File(gameSavePath)).remove(); }

// ── Char cell pixel size detection ───────────────────────────────────────────
var charPixW = 8, charPixH = 16;
(function() {
    var buf = "", ch;
    console.ctrlkey_passthru = -1;
    console.clearkeybuffer();
    console.write("\x1b[=3n");
    var deadline = 800;
    while ((ch = console.inkey(0, deadline)) !== '') {
        buf += ch; deadline = 200;
        if (ch === 'n') break;
    }
    console.ctrlkey_passthru = 0;
    var m = buf.match(/\x1b\[=3;(\d+);(\d+)n/);
    if (m) { charPixH = parseInt(m[1], 10); charPixW = parseInt(m[2], 10); }
})();

// ── JXL support ───────────────────────────────────────────────────────────────
var jxlOk = supports_jpegxl();

// ── JXL upload ────────────────────────────────────────────────────────────────
function uploadToCache(localPath, cacheName) {
    var f = new File(localPath);
    if (!f.open("rb")) return false;
    var flen = f.length;
    f.base64 = true;
    var b64 = f.read(flen);
    f.close();
    if (!b64 || b64.length === 0) return false;
    console.write("\x1b_SyncTERM:C;S;" + cacheName + ";" + b64 + "\x1b\\");
    return true;
}

var pieceNames = {
    'p':'black-pawn',   'P':'white-pawn',
    'n':'black-knight', 'N':'white-knight',
    'b':'black-bishop', 'B':'white-bishop',
    'r':'black-rook',   'R':'white-rook',
    'q':'black-queen',  'Q':'white-queen',
    'k':'black-king',   'K':'white-king'
};

// ── One-time startup: load settings + upload JXL cache (silent) ──────────────
console.clear();
if (!isGuest) loadIni();

if (jxlOk) {
    // Upload sprite sheet + animation masks, load into pixel/mask buffers
    if (uploadToCache(imgDir + "piece_sprites.jxl", "piece_sprites.jxl") &&
        uploadToCache(imgDir + "piece_mask.pbm", "piece_mask.pbm")) {
        console.write("\x1b_SyncTERM:C;LoadJXL;B=1;piece_sprites.jxl\x1b\\");
        console.write("\x1b_SyncTERM:C;LoadPBM;piece_mask.pbm\x1b\\");
    } else {
        jxlOk = false;  // silently fall back to ANSI
    }
}

// ── Mouse ────────────────────────────────────────────────────────────────────
function mouseon()  { console.write("\x1b[?1000h\x1b[?1002h\x1b[?1006h"); }
function mouseoff() { console.write("\x1b[?1000l\x1b[?1002l\x1b[?1006l"); }

// rowMap: optional object {rowNum: 'KEY'} for mouse click detection
function readOneKey(rowMap) {
    while (!console.aborted && bbs.online) {
        if (rowMap) console.ctrlkey_passthru = -1;
        var ch = console.inkey(0, 100);
        if (ch === '') continue;
        if (ch === '\x1b') {
            var seq = '\x1b';
            for (var t = 0; t < 24; t++) {
                var n = console.inkey(0, 40);
                if (n === '') break;
                seq += n;
                if (n.match(/[A-Za-z~]/)) break;
            }
            if (rowMap) {
                var mm = seq.match(/^\x1b\[<(\d+);(\d+);(\d+)([Mm])$/);
                if (mm) {
                    var cb = parseInt(mm[1], 10), my = parseInt(mm[3], 10);
                    var released = mm[4] === 'm';
                    if (!(cb & 32) && !released && rowMap[my]) return rowMap[my];
                }
            }
            continue;
        }
        return ch.toUpperCase();
    }
    return '';
}


// Print a hotspot prompt at (1, row), detect keyboard OR column-aware mouse clicks.
// str uses @`display`key@ hotspots and \x01X attr codes (no raw \x1b escapes).
function readHotspotKey(str, row) {
    console.gotoxy(1, row);
    console.print(str + "\x1b[K\x1b[0m", P_ATCODES);
    var spots = parseMenuHotspots(str);
    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        var ch = console.inkey(0, 100);
        if (ch === '') continue;
        if (ch === '\x1b') {
            var seq = '\x1b';
            for (var t = 0; t < 24; t++) {
                var n = console.inkey(0, 40);
                if (n === '') break;
                seq += n;
                if (n.match(/[A-Za-z~]/)) break;
            }
            var mm = seq.match(/^\x1b\[<(\d+);(\d+);(\d+)([Mm])$/);
            if (mm) {
                var cb = parseInt(mm[1], 10), mx = parseInt(mm[2], 10), my = parseInt(mm[3], 10);
                var released = mm[4] === 'm';
                if (!(cb & 32) && !released && my === row) {
                    for (var hi = 0; hi < spots.length; hi++) {
                        if (mx >= spots[hi].startCol && mx <= spots[hi].endCol) {
                            console.ctrlkey_passthru = 0;
                            return spots[hi].key;
                        }
                    }
                }
            }
            continue;
        }
        console.ctrlkey_passthru = 0;
        return ch.toUpperCase();
    }
    console.ctrlkey_passthru = 0;
    return '';
}

// ── Helpers ───────────────────────────────────────────────────────────────────
function padR(s, n) { s = String(s); while (s.length < n) s += ' '; return s.substring(0, n); }
function padL(s, n) { s = String(s); while (s.length < n) s = ' ' + s; return s.substring(s.length - n); }

// ── ECO opening names ────────────────────────────────────────────────────────
load(js.exec_dir + "synchess_openings.js");

function getOpeningName() {
    var line = '';
    for (var hi = 0; hi < game.history.length; hi++) {
        if (hi > 0) line += ' ';
        line += game.history[hi].uci;
    }
    var best = '';
    for (var key in OPENINGS) {
        if (line.indexOf(key) === 0 && key.length > best.length) best = key;
    }
    return best ? OPENINGS[best] : '';
}

// Convert 1-based terminal col/row → board {r,c}
function charToSquare(col, row) {
    var j = Math.floor((col - boardX) / cellW);
    var i = Math.floor((row - boardY) / cellH);
    if (j < 0 || j > 7 || i < 0 || i > 7) return null;
    var r = (game.playerColor === 'w') ? i : 7 - i;
    var c = (game.playerColor === 'w') ? j : 7 - j;
    return { r: r, c: c };
}

// Board {r,c} → terminal col/row (top-left of cell)
function squareToChar(r, c) {
    var j = (game.playerColor === 'w') ? c : 7 - c;
    var i = (game.playerColor === 'w') ? r : 7 - r;
    return { col: boardX + j * cellW, row: boardY + i * cellH };
}

// ── Status bar ────────────────────────────────────────────────────────────────
function showStatus(msg, col) {
    console.gotoxy(1, STATUS_ROW);
    console.print("\x1b[" + (col || "0;37") + "m" + padR(msg, 78) + "\x1b[0m");
}
function clearStatus() { showStatus(""); }

// ── Drawing ───────────────────────────────────────────────────────────────────
function drawPieceJXL(piece, isDark, col, row, masked) {
    if (pieceSpriteY[piece]) {
        var cmd = "\x1b_SyncTERM:P;Paste;SY=" + pieceSpriteY[piece][isDark ? 0 : 1] +
                  ";SH=" + (cellH * charPixH) +
                  ";DX=" + ((col-1)*charPixW) +
                  ";DY=" + ((row-1)*charPixH);
        if (masked)
            cmd += ";MBUF;MY=" + pieceDrawMY[piece.toUpperCase()];
        cmd += ";B=1\x1b\\";
        console.write(cmd);
    } else {
        var fname = pieceNames[piece] + '-' + (isDark ? 'dark' : 'light') + '.jxl';
        console.write("\x1b_SyncTERM:C;DrawJXL;DX=" + ((col-1)*charPixW) +
                      ";DY=" + ((row-1)*charPixH) + ";" + fname + "\x1b\\");
    }
}

function drawCapIconJXL(piece, col, row) {
    if (pieceSpriteY[piece]) {
        console.write("\x1b_SyncTERM:P;Paste;SY=" + pieceSpriteY[piece][2] +
                      ";SW=" + (CAP_W * charPixW) + ";SH=" + (CAP_H * charPixH) +
                      ";DX=" + ((col-1)*charPixW) +
                      ";DY=" + ((row-1)*charPixH) + ";B=1\x1b\\");
    } else {
        var fname = pieceNames[piece] + '-cap.jxl';
        console.write("\x1b_SyncTERM:C;DrawJXL;DX=" + ((col-1)*charPixW) +
                      ";DY=" + ((row-1)*charPixH) + ";" + fname + "\x1b\\");
    }
}

function drawSquare(r, c, highlight) {
    var isDark = (r + c) % 2 !== 0;
    var pos    = squareToChar(r, c);
    var bg     = isDark ? darkBg : lightBg;
    if (highlight === 'sel')  bg = selBg;
    if (highlight === 'hint') bg = hintBg;
    for (var h = 0; h < cellH; h++) {
        console.gotoxy(pos.col, pos.row + h);
        console.print("\x1b[" + bg + "m     ");
    }
    var piece = game.board[r][c];
    if (piece !== ' ') {
        var isWp = (piece === piece.toUpperCase());
        var hidden = (isWp && hideWhite) || (!isWp && hideBlack);
        if (!hidden) {
            if (jxlOk && pieceSpriteY[piece]) {
                // Use mask on highlighted squares so the highlight bg shows through
                drawPieceJXL(piece, isDark, pos.col, pos.row, !!highlight);
            } else {
                console.gotoxy(pos.col + 2, pos.row);
                console.print("\x1b[" + bg + ";" + (isWp ? whiteFg : blackFg) + "m" + (isWp ? piece.toUpperCase() : piece.toLowerCase()));
            }
        }
    }
}

// Draw a section of captured icons; label may contain ANSI codes
function drawCapturedSection(label, caps, startRow) {
    console.gotoxy(RIGHT_COL, startRow);
    console.print("\x1b[0;36m" + padR("", RIGHT_W) + "\x1b[0m");
    console.gotoxy(RIGHT_COL, startRow);
    console.print("\x1b[0;36m" + label + "\x1b[0m");
    var row = startRow + 1;

    if (caps.length === 0) {
        console.gotoxy(RIGHT_COL, row);
        console.print("\x1b[0;37m" + padR("-", RIGHT_W) + "\x1b[0m");
        return row + 1;
    }

    var sorted  = caps.slice().sort(function(a, b) { return game.pieceValue(b) - game.pieceValue(a); });
    var display = sorted.slice(0, CAP_PER * 3);
    var numRows = Math.ceil(display.length / CAP_PER);
    var charRows = numRows * CAP_H;

    for (var dr = 0; dr < charRows; dr++) {
        console.gotoxy(RIGHT_COL, row + dr);
        console.print("\x1b[47m" + padR("", RIGHT_W) + "\x1b[0m");
    }

    if (jxlOk) {
        for (var i = 0; i < display.length; i++) {
            var col  = RIGHT_COL + (i % CAP_PER) * CAP_W;
            var irow = row + Math.floor(i / CAP_PER) * CAP_H;
            drawCapIconJXL(display[i], col, irow);
        }
    } else {
        for (var i = 0; i < display.length && i < RIGHT_W; i++) {
            var isWp = (display[i] === display[i].toUpperCase());
            console.gotoxy(RIGHT_COL + i, row);
            // On gray bg: white pieces in dark blue, black pieces in black
            console.print("\x1b[47;" + (isWp ? "0;34" : "0;30") + "m" + display[i].toUpperCase());
        }
        charRows = 1;
    }

    return row + charRows;
}

function drawPanels() {
    // ── Left: move list ──────────────────────────────────────────────────
    var hm = game.history;
    var row = 3;
    if (moveDisplay === 'none') {
        while (row <= STATUS_ROW - 2) { console.gotoxy(LEFT_COL, row++); console.print(padR("", LEFT_W)); }
    } else if (moveDisplay === 'last') {
        console.gotoxy(LEFT_COL, row++);
        console.print("\x1b[0;36m" + padR("-- Last --", LEFT_W) + "\x1b[0m");
        if (hm.length > 0) {
            var li  = hm.length - 1;
            var num = Math.floor(li / 2) + 1;
            var wAlg = hm[li - (li % 2)]       ? hm[li - (li % 2)].alg   : '';
            var bAlg = hm[li - (li % 2) + 1]   ? hm[li - (li % 2) + 1].alg : '';
            console.gotoxy(LEFT_COL, row++);
            console.print("\x1b[0;37m" + padR(padL(num,2) + '.' + padR(wAlg,6) + padR(bAlg,6), LEFT_W) + "\x1b[0m");
        }
        while (row <= STATUS_ROW - 2) { console.gotoxy(LEFT_COL, row++); console.print(padR("", LEFT_W)); }
    } else {
        console.gotoxy(LEFT_COL, row++);
        console.print("\x1b[0;36m" + padR("-- Moves --", LEFT_W) + "\x1b[0m");
        console.gotoxy(LEFT_COL, row++);
        console.write("\x1b[0m   \x1b[1;37mW     \x1b[1;33mB\x1b[0m      ");
        var totalPairs = Math.ceil(hm.length / 2);
        var visibleRows = STATUS_ROW - 2 - row;
        var startPair = Math.max(0, totalPairs - visibleRows);
        for (var p = startPair; p < totalPairs && row <= STATUS_ROW - 2; p++) {
            var wAlg = hm[p*2]   ? hm[p*2].alg   : '';
            var bAlg = hm[p*2+1] ? hm[p*2+1].alg : '';
            console.gotoxy(LEFT_COL, row++);
            console.print("\x1b[0;37m" + padR(padL(p+1,2) + '.' + padR(wAlg,6) + padR(bAlg,6), LEFT_W) + "\x1b[0m");
        }
        while (row <= STATUS_ROW - 2) { console.gotoxy(LEFT_COL, row++); console.print(padR("", LEFT_W)); }
    }

    // ── Right: captured pieces & material ────────────────────────────────
    console.gotoxy(RIGHT_COL, 3);
    console.print("\x1b[0;36m" + padR("-- Captured --", RIGHT_W) + "\x1b[0m");

    var wCaps = game.captured.w;
    var bCaps = game.captured.b;
    var wPts = 0, bPts = 0;
    for (var i = 0; i < wCaps.length; i++) wPts += game.pieceValue(wCaps[i]);
    for (var i = 0; i < bCaps.length; i++) bPts += game.pieceValue(bCaps[i]);
    var bal = wPts - bPts;

    var wLabel = (bal > 0) ? "White: \x1b[1;37m+" + bal + "\x1b[0;36m" : "White:";
    var bLabel = (bal < 0) ? "Black: \x1b[1;37m+" + (-bal) + "\x1b[0;36m" : "Black:";

    // Opponent's captures at top, player's own captures pinned near the bottom
    var topLabel, topCaps, botLabel, botCaps;
    if (game.playerColor === 'w') {
        topLabel = bLabel; topCaps = bCaps;
        botLabel = wLabel; botCaps = wCaps;
    } else {
        topLabel = wLabel; topCaps = wCaps;
        botLabel = bLabel; botCaps = bCaps;
    }
    // Pin bottom section: label + up to 3 rows of icons (CAP_H=2 each) = 7 rows above status
    var botStart = STATUS_ROW - 7;
    drawCapturedSection(topLabel, topCaps, 4);
    // Clear middle gap
    for (var cr = 4 + 1 + Math.max(1, Math.ceil(topCaps.length / CAP_PER)) * CAP_H + 1;
             cr < botStart; cr++) {
        console.gotoxy(RIGHT_COL, cr);
        console.print(padR("", RIGHT_W));
    }
    var nextRow = drawCapturedSection(botLabel, botCaps, botStart);
    for (var cr2 = nextRow; cr2 < STATUS_ROW - 1; cr2++) {
        console.gotoxy(RIGHT_COL, cr2);
        console.print(padR("", RIGHT_W));
    }
}

function drawBoard(selSq, hintSqs) {
    console.clear();
    console.gotoxy(1, 1);
    console.print("\x1b[1;44;37m SynChess \x1b[0m" +
        " Lvl:\x1b[1;33m" + game.level + "\x1b[0m" +
        "  Playing: \x1b[" + (game.playerColor==='w' ? '1;37' : blackFg) + "m" +
        (game.playerColor==='w' ? 'White' : 'Black') + "\x1b[0m");
    drawPanels();
    for (var i = 0; i < 8; i++) {
        var r = (game.playerColor === 'w') ? i : 7 - i;
        console.gotoxy(boardX - 2, boardY + i * cellH);
        console.print("\x1b[0;37m" + (8 - r));
        for (var j = 0; j < 8; j++) {
            var c = (game.playerColor === 'w') ? j : 7 - j;
            var hl = null;
            if (selSq && selSq.r === r && selSq.c === c) hl = 'sel';
            if (hintSqs) {
                for (var hi = 0; hi < hintSqs.length; hi++)
                    if (hintSqs[hi].r === r && hintSqs[hi].c === c) { hl = 'hint'; break; }
            }
            drawSquare(r, c, hl);
        }
    }
    console.gotoxy(boardX, boardY + 8 * cellH);
    console.print("\x1b[0;37m" + ((game.playerColor==='w')
        ? " a    b    c    d    e    f    g    h"
        : " h    g    f    e    d    c    b    a") + "\x1b[0m");
    // Opening name drawn last so nothing can overwrite it
    console.gotoxy(1, 2);
    if (showOpeningName) {
        var opening = getOpeningName();
        if (opening) {
            console.write("\x1b[0;36mOpening: \x1b[1;36m" + opening + "\x1b[0m" + padR("", 40));
        } else {
            console.write("\x1b[0m" + padR("", 50));
        }
    } else {
        console.write("\x1b[0m" + padR("", 50));
    }
}

// ── ANSI Color preview (used in color config screen) ─────────────────────────
// Draws 4 squares at specified position showing all piece/square combos
function drawColorPreview(col, row, db, lb, wf, bf) {
    // 4 squares: WR/dark, BR/dark, WR/light, BR/light
    var squares = [
        { bg: db, fg: wf, label: 'WR' },
        { bg: db, fg: bf, label: 'BR' },
        { bg: lb, fg: wf, label: 'WR' },
        { bg: lb, fg: bf, label: 'BR' }
    ];
    for (var h = 0; h < 2; h++) {
        console.gotoxy(col, row + h);
        for (var s = 0; s < 4; s++) {
            var bg = squares[s].bg, fg = squares[s].fg;
            // Apply fg then bg separately: fg may contain a reset ("0;30"),
            // so we re-apply bg afterwards to ensure it isn't wiped out.
            console.print(
                "\x1b[" + bg + "m  " +
                "\x1b[" + fg + "m\x1b[" + bg + "m" +
                (h === 0 ? squares[s].label : "  ") +
                "\x1b[" + bg + "m  "
            );
        }
        console.print("\x1b[0m");
    }
    // column labels
    console.gotoxy(col, row + 2);
    console.print("\x1b[0;37m W/dark  B/dark  W/lite  B/lite\x1b[0m");
}

// ── Color configuration screen ────────────────────────────────────────────────
function showColorConfig() {
    var bgOpts  = ['40','41','42','43','44','45','46','47'];
    var bgNames = ['Black','Red','Green','Yellow','Blue','Magenta','Cyan','White'];
    var fgOpts  = ['0;30','0;31','0;32','0;33','0;34','0;35','0;36','0;37',
                   '1;30','1;31','1;32','1;33','1;34','1;35','1;36','1;37'];
    var fgNames = ['Black','Red','Green','Yellow','Blue','Magenta','Cyan','White',
                   'Br.Black','Br.Red','Br.Green','Br.Yellow',
                   'Br.Blue','Br.Magenta','Br.Cyan','Br.White'];

    function findIdx(arr, v) {
        for (var i = 0; i < arr.length; i++) if (arr[i] === v) return i;
        return 0;
    }
    var di = findIdx(bgOpts, darkBg);
    var li = findIdx(bgOpts, lightBg);
    var wi = findIdx(fgOpts, whiteFg);
    var bi = findIdx(fgOpts, blackFg);
    var si = findIdx(bgOpts, selBg);
    var gi = findIdx(bgOpts, hintBg);

    var COL = 34, W = 42;
    var bdr = "\x1b[0;37m";
    var HBAR = Array(W + 1).join('\xC4');

    function drawMenu() {
        var rows = {};
        var r = 3;

        function ln(text, attr, key) {
            console.gotoxy(COL - 1, r); console.print(bdr + "\xB3\x1b[0m");
            console.gotoxy(COL, r);
            console.print("\x1b[" + (attr || "0;37") + "m" + text + "\x1b[K\x1b[0m");
            console.gotoxy(COL + W, r); console.print(bdr + "\xB3\x1b[0m");
            if (key) rows[r] = key;
            r++;
        }

        console.gotoxy(COL - 1, 2); console.print(bdr + "\xDA" + HBAR + "\xBF\x1b[0m");

        ln("  ANSI Color Configuration", "1;37");
        ln("");
        ln("  \x01H\x01Y(D)\x01N Dark sq:  \x1b[" + bgOpts[di] + "m    \x1b[0;37m  " + bgNames[di], "0;37", "D");
        ln("  \x01H\x01Y(L)\x01N Light sq: \x1b[" + bgOpts[li] + "m    \x1b[0;37m  " + bgNames[li], "0;37", "L");
        ln("  \x01H\x01Y(S)\x01N Sel sq:   \x1b[" + bgOpts[si] + "m    \x1b[0;37m  " + bgNames[si], "0;37", "S");
        ln("  \x01H\x01Y(G)\x01N Hint sq:  \x1b[" + bgOpts[gi] + "m    \x1b[0;37m  " + bgNames[gi], "0;37", "G");
        ln("  \x01H\x01Y(W)\x01N White pc: \x1b[44;" + fgOpts[wi] + "m R \x1b[0;37m  " + fgNames[wi], "0;37", "W");
        ln("  \x01H\x01Y(V)\x01N White pc: " + (hideWhite ? "\x1b[1;31mHidden (blindfold)" : "\x1b[1;32mVisible"), "0;37", "V");
        ln("  \x01H\x01Y(B)\x01N Black pc: \x1b[44;" + fgOpts[bi] + "m r \x1b[0;37m  " + fgNames[bi], "0;37", "B");
        ln("  \x01H\x01Y(N)\x01N Black pc: " + (hideBlack ? "\x1b[1;31mHidden (blindfold)" : "\x1b[1;32mVisible"), "0;37", "N");
        ln("  (each key cycles to next option)", "0;33");
        ln("");
        ln("  Preview:", "0;36");
        drawColorPreview(COL + 2, r, bgOpts[di], bgOpts[li], fgOpts[wi], fgOpts[bi]);
        for (var pr = 0; pr < 3; pr++) {
            console.gotoxy(COL - 1, r + pr); console.print(bdr + "\xB3\x1b[0m");
            console.gotoxy(COL + W, r + pr); console.print(bdr + "\xB3\x1b[0m");
        }
        r += 3;
        ln("");
        ln("  \x01H\x01Y(R)\x01N Reset to defaults", "0;35", "R");
        ln("  \x01H\x01Y(Q)\x01N Save & close", "0;33", "Q");

        console.gotoxy(COL - 1, r); console.print(bdr + "\xC0" + HBAR + "\xD9\x1b[0m");

        return rows;
    }

    console.clear();
    mouseon();
    while (true) {
        var rows = drawMenu();
        var k = readOneKey(rows);
        if (k === 'Q' || k === '\x1b') break;
        if (k === 'D') { di = (di + 1) % bgOpts.length; darkBg  = bgOpts[di]; }
        if (k === 'L') { li = (li + 1) % bgOpts.length; lightBg = bgOpts[li]; }
        if (k === 'S') { si = (si + 1) % bgOpts.length; selBg   = bgOpts[si]; }
        if (k === 'G') { gi = (gi + 1) % bgOpts.length; hintBg  = bgOpts[gi]; }
        if (k === 'W') { wi = (wi + 1) % fgOpts.length; whiteFg = fgOpts[wi]; }
        if (k === 'V') { hideWhite = !hideWhite; }
        if (k === 'B') { bi = (bi + 1) % fgOpts.length; blackFg = fgOpts[bi]; }
        if (k === 'N') { hideBlack = !hideBlack; }
        if (k === 'R') {
            darkBg = '44'; lightBg = '46'; whiteFg = '1;37'; blackFg = '1;33';
            selBg = '43'; hintBg = '42'; hideWhite = false; hideBlack = false;
            di = findIdx(bgOpts, darkBg);  li = findIdx(bgOpts, lightBg);
            wi = findIdx(fgOpts, whiteFg); bi = findIdx(fgOpts, blackFg);
            si = findIdx(bgOpts, selBg);   gi = findIdx(bgOpts, hintBg);
        }
    }
}

// ── Promotion picker ──────────────────────────────────────────────────────────
function askPromotion(promotingColor) {
    var prefix = (promotingColor === 'w') ? 'white' : 'black';
    var pieces = ['Q', 'R', 'B', 'N'];
    var names  = ['queen', 'rook', 'bishop', 'knight'];
    var labels = ['Queen', 'Rook', 'Bishop', 'Knight'];

    for (var row = 1; row <= 5; row++) {
        console.gotoxy(1, row);
        console.print("\x1b[47;30m" + padR("", 80) + "\x1b[0m");
    }
    console.gotoxy(5, 1);
    console.print("\x1b[47;1;30m Promote pawn: click image or press Q/R/B/N \x1b[0m");

    var promoRow      = 2;
    var promoStartCol = boardX;
    var slotW         = 10;   // chars — matches label width
    var jxlPixW       = 40;   // promo JXL fixed width in pixels

    for (var i = 0; i < 4; i++) {
        var slotCol = promoStartCol + i * slotW;
        if (jxlOk) {
            // Centre the 40px image within the slotW-char slot
            var px = (slotCol - 1) * charPixW + Math.floor((slotW * charPixW - jxlPixW) / 2);
            var py = (promoRow - 1) * charPixH;
            var promoChar = (promotingColor === 'w') ? pieces[i] : pieces[i].toLowerCase();
            if (pieceSpriteY[promoChar]) {
                console.write("\x1b_SyncTERM:P;Paste;SY=" + pieceSpriteY[promoChar][3] +
                              ";SH=" + (cellH * charPixH) +
                              ";DX=" + px + ";DY=" + py + ";B=1\x1b\\");
            } else {
                console.write("\x1b_SyncTERM:C;DrawJXL;DX=" + px + ";DY=" + py + ";" +
                    prefix + '-' + names[i] + '-promo.jxl\x1b\\');
            }
        } else {
            console.gotoxy(slotCol + Math.floor(slotW / 2), promoRow);
            console.print("\x1b[47;1;30m" + pieces[i]);
        }
    }
    // Labels — one row below pieces, 10 chars per slot
    console.gotoxy(boardX, promoRow + cellH);
    console.print("\x1b[47;1;30m" +
        padR(" Q=Queen",  slotW) + padR(" R=Rook",   slotW) +
        padR(" B=Bishop", slotW) + padR(" N=Knight", slotW) +
        "\x1b[0m");

    console.ctrlkey_passthru = -1;
    while (true) {
        var ch = console.inkey(0, 100);
        if (ch === '') continue;
        if (ch === '\x1b') {
            var seq = '\x1b';
            for (var t = 0; t < 24; t++) {
                var n = console.inkey(0, 40);
                if (n === '') break;
                seq += n;
                if (n.match(/[A-Za-z~]/)) break;
            }
            var mm = seq.match(/^\x1b\[<(\d+);(\d+);(\d+)([Mm])$/);
            if (mm && mm[4] === 'm') {
                var mx = parseInt(mm[2], 10), my = parseInt(mm[3], 10);
                if (my >= promoRow && my < promoRow + cellH) {
                    for (var si = 0; si < 4; si++) {
                        var sc = promoStartCol + si * slotW;
                        if (mx >= sc && mx < sc + slotW) {
                            console.ctrlkey_passthru = 0;
                            return pieces[si];
                        }
                    }
                }
            }
            continue;
        }
        var k = ch.toUpperCase();
        if (k === 'Q' || k === 'R' || k === 'B' || k === 'N') {
            console.ctrlkey_passthru = 0;
            return k;
        }
    }
}

// ── Settings overlay ──────────────────────────────────────────────────────────
function showSettings() {
    var COL = 3;

    function drawSettingsMenu() {
        console.clear();
        var rows = {};
        var r = 2;

        function ln(text, attr, key) {
            console.gotoxy(COL, r);
            console.print("\x1b[" + (attr || "0;37") + "m" + text + "\x1b[0m");
            if (key) rows[r] = key;
            r++;
        }

        ln("SynChess Settings", "1;37");  r++;
        ln("\x01H\x01Y(+)\x01N  Level up      Level: \x01H\x01W" + game.level + "/10", "0;37", "+");
        ln("\x01H\x01Y(-)\x01N  Level down", "0;37", "-");
        ln("       Engine: " + (useStockfish ? "\x1b[1;32mStockfish" : "\x1b[1;33mBuilt-in") + " \x1b[0;37m(set in synchess.ini)", "0;37");
        ln("\x01H\x01Y(H)\x01N  Hints:   " + (showLegalMoves  ? "\x1b[1;32mON " : "\x1b[1;31mOFF"), "0;37", "H");
        ln("\x01H\x01Y(U)\x01N  Undo:    " + (allowUndo       ? "\x1b[1;32mON " : "\x1b[1;31mOFF"), "0;37", "U");
        ln("\x01H\x01Y(O)\x01N  Opening: " + (showOpeningName ? "\x1b[1;32mON " : "\x1b[1;31mOFF"), "0;37", "O");
        var mdLabel = moveDisplay === 'all' ? "\x1b[1;32mAll moves" : moveDisplay === 'last' ? "\x1b[1;33mLast only" : "\x1b[1;31mHidden";
        ln("\x01H\x01Y(M)\x01N  Moves:   " + mdLabel, "0;37", "M");
        ln("\x01H\x01Y(C)\x01N  Configure Colors", "0;37", "C");
        r++;
        if (isGuest) {
            console.gotoxy(COL, r++); console.print("\x1b[0;33m  Stats not tracked for guests.\x1b[0m");
            r++;
        } else {
        ln("Stats:", "1;37");
        var SC1 = COL, SC2 = COL + 22, SC3 = COL + 44;
        var sln = function(t1, v1, g1, t2, v2, g2, t3, v3, g3) {
            if (t1) { console.gotoxy(SC1, r); console.print("\x1b[" + g1 + "m  " + padR(t1, 12) + v1 + "\x1b[0m"); }
            if (t2) { console.gotoxy(SC2, r); console.print("\x1b[" + g2 + "m  " + padR(t2, 12) + v2 + "\x1b[0m"); }
            if (t3) { console.gotoxy(SC3, r); console.print("\x1b[" + g3 + "m  " + padR(t3, 12) + v3 + "\x1b[0m"); }
            r++;
        };
        console.gotoxy(SC1, r); console.print("\x1b[0;32m  Wins\x1b[0m");
        console.gotoxy(SC2, r); console.print("\x1b[0;31m  Losses\x1b[0m");
        console.gotoxy(SC3, r); console.print("\x1b[0;36m  Draws\x1b[0m");
        r++;
        sln("Checkmate:", stats.wins_checkmate,    "0;32", "Checkmate:", stats.losses_checkmate,  "0;31", "Stalemate:",  stats.draws_stalemate,    "0;36");
        sln("Resign:",    stats.wins_resign,        "0;32", "Resign:",    stats.losses_resign,      "0;31", "Insuff.mat:", stats.draws_insufficient, "0;36");
        sln(null,null,null,                                  "Abandoned:", stats.losses_abandoned,   "0;31", "3-fold rep:", stats.draws_threefold,    "0;36");
        sln(null,null,null,                                  null,null,null,                                  "50-move:",   stats.draws_fifty,         "0;36");
        sln(null,null,null,                                  null,null,null,                                  "Agreed:",    stats.draws_agreed,        "0;36");
        r++;
        } // end !isGuest stats block
        ln(isGuest ? "\x01H\x01Y(Q)\x01N  Close" : "\x01H\x01Y(Q)\x01N  Close & save", "0;33", "Q");

        mouseon();  // console.clear() resets mouse tracking
        return rows;
    }

    while (true) {
        var rows = drawSettingsMenu();
        var k = readOneKey(rows);
        if (k === 'Q' || k === '\x1b') break;
        if (k === '+') { game.level = Math.min(10, game.level + 1); continue; }
        if (k === '-') { game.level = Math.max(1,  game.level - 1); continue; }

        if (k === 'H') { showLegalMoves  = !showLegalMoves;  continue; }
        if (k === 'U') { allowUndo       = !allowUndo;       continue; }
        if (k === 'O') { showOpeningName = !showOpeningName; continue; }
        if (k === 'M') { moveDisplay = moveDisplay === 'all' ? 'last' : moveDisplay === 'last' ? 'none' : 'all'; continue; }
        if (k === 'C') { showColorConfig(); continue; }
    }
    saveIni();
}

// ── Instructions ──────────────────────────────────────────────────────────────
function showInstructions() {
    function ln(C, r, text, attr) {
        console.gotoxy(C, r);
        console.print("\x1b[" + (attr || "0;37") + "m" + text + "\x1b[0m");
    }
    // ── Page 1: Move entry ────────────────────────────────────────────────────
    console.clear();
    var C = 3, r = 2;
    ln(C, r++, "SynChess - Instructions  (1/3)", "1;37");  r++;
    ln(C, r++, "Move entry:", "1;36");
    ln(C, r++, "  e2e4 / E2E4   move from square to square");
    ln(C, r++, "  e4             pawn to e4");
    ln(C, r++, "  Nc3            knight to c3");
    ln(C, r++, "  Rae1           rook on a-file to e1 (disambiguate)");
    ln(C, r++, "  exd5 / bxc3   pawn captures (with or without x)");
    ln(C, r++, "  O-O  0-0       castle kingside");
    ln(C, r++, "  O-O-O          castle queenside");
    ln(C, r++, "  Kg1 / Kc1      also castle (king destination)");
    ln(C, r++, "  e8=Q           promote pawn (=Q/R/B/N)");  r++;
    ln(C, r++, "Mouse:", "1;36");
    ln(C, r++, "  Click and drag to move");
    ln(C, r++, "  Hints shown on drag if hints enabled in (S)ettings");  r++;
    ln(C, r++, "Auto-submit:", "1;36");
    ln(C, r++, "  Move is sent automatically when it is unambiguous");
    ln(C, r++, "  Add Enter to force-submit a partial move");  r++;
    var pm1 = {}; pm1[r] = ' '; ln(C, r++, "Press any key for page 2...", "0;33");
    mouseon();
    readOneKey(pm1);
    // ── Page 2: Keys & settings ───────────────────────────────────────────────
    console.clear();
    r = 2;
    ln(C, r++, "SynChess - Instructions  (2/3)", "1;37");  r++;
    ln(C, r++, "Keys:", "1;36");
    ln(C, r++, "  (S)ettings & Stats      (I)nstructions");
    ln(C, r++, "  (R)esign                (Q)uit");
    ln(C, r++, "  (O)ffer Draw            U  Undo last move");
    ln(C, r++, "  P  Export PGN          +/- Level up/down");  r++;
    ln(C, r++, "Difficulty levels:", "1;36");
    ln(C, r++, "  1-10, press ? at level select for ELO guide");  r++;
    ln(C, r++, "Settings (S):", "1;36");
    ln(C, r++, "  H  Legal move hints     U  Undo toggle");
    ln(C, r++, "  O  Opening name         M  Moves: All/Last/None");
    ln(C, r++, "  C  Configure colors");  r++;
    ln(C, r++, "Draw conditions:", "1;36");
    ln(C, r++, "  Stalemate / Insufficient material");
    ln(C, r++, "  3-fold repetition / 50-move rule");
    ln(C, r++, "  Mutual agreement (O to offer draw)");  r++;
    var pm2 = {}; pm2[r] = ' '; ln(C, r++, "Press any key for page 3...", "0;33");
    mouseon();
    readOneKey(pm2);
    // ── Page 3: Quitting, Colour config & blindfold ───────────────────────────
    console.clear();
    r = 2;
    ln(C, r++, "SynChess - Instructions  (3/3)", "1;37");  r++;
    ln(C, r++, "Quitting (Q):", "1;36");
    ln(C, r++, "  S = Save for later  A = Abandon (counts as loss)");  r++;
    ln(C, r++, "Color config (C from Settings):", "1;36");
    ln(C, r++, "  D / L   Dark / Light square colour");
    ln(C, r++, "  S / G   Selected / Hint square colour");
    ln(C, r++, "  W / B   White / Black piece colour (cycles)");
    ln(C, r++, "  V       White pieces: Visible / Hidden");
    ln(C, r++, "  N       Black pieces: Visible / Hidden");
    ln(C, r++, "  R       Reset all colours to defaults");  r++;
    ln(C, r++, "Blindfold chess:", "1;36");
    ln(C, r++, "  Hide one or both sets of pieces via V / N in");
    ln(C, r++, "  Color Config.  The move list and opening name");
    ln(C, r++, "  remain visible to help you track the position.");
    ln(C, r++, "  Use 'Moves: Last only' in Settings to see only");
    ln(C, r++, "  the opponent's last move for extra difficulty.");  r++;
    var pm3 = {}; pm3[r] = ' '; ln(C, r++, "Press any key...", "0;33");
    mouseon();
    readOneKey(pm3);
}

// ── PGN export ────────────────────────────────────────────────────────────────
function exportPGN() {
    var d = new Date();
    var dateStr = d.getFullYear() + "." + format("%02d", d.getMonth()+1) + "." + format("%02d", d.getDate());
    var result = "*";
    if (game.isCheckmate())
        result = (game.turn !== game.playerColor) ? "1-0" : "0-1";
    else if (game.isStalemate() || game.isInsufficientMaterial() ||
             game.isThreefoldRepetition() || game.halfMoveClock >= 100)
        result = "1/2-1/2";

    var pgn = '[Event "SynChess BBS Game"]\n';
    pgn += '[Site "' + system.name + '"]\n';
    pgn += '[Date "' + dateStr + '"]\n';
    pgn += '[White "' + (game.playerColor==='w' ? user.alias : 'Stockfish L'+game.level) + '"]\n';
    pgn += '[Black "' + (game.playerColor==='b' ? user.alias : 'Stockfish L'+game.level) + '"]\n';
    pgn += '[Result "' + result + '"]\n\n';

    var line = "";
    for (var i = 0; i < game.history.length; i++) {
        var tok = (i%2===0 ? Math.floor(i/2+1) + ". " : "") + game.history[i].alg + " ";
        if (line.length + tok.length > 76) { pgn += line + "\n"; line = ""; }
        line += tok;
    }
    if (line) pgn += line + "\n";
    pgn += result + "\n";

    // Display PGN on screen — cap at row 21 so prompt is always visible
    console.clear();
    var lines = pgn.split("\n");
    var maxShow = 22;
    for (var li = 0; li < lines.length && li < maxShow; li++) {
        console.gotoxy(1, li + 1);
        console.print("\x1b[0;37m" + lines[li]);
    }
    if (lines.length > maxShow) {
        console.gotoxy(1, 23);
        console.print("\x1b[0;33m... (" + Math.floor(game.history.length/2) + " moves -- download for full PGN)\x1b[K");
    }
    var ans = readHotspotKey(
        "\x01NDownload PGN?  \x01H\x01Y@`(Y)`Y@\x01Ces  \x01H\x01Y@`(N)`N@\x01Co",
        24
    );
    if (ans !== 'Y') { drawBoard(); return; }

    var tmpPGN = "/tmp/synchess_" + user.number + ".pgn";
    var fp = new File(tmpPGN);
    if (!fp.open("w")) { showStatus("PGN write failed.", "1;31"); return; }
    fp.write(pgn);
    fp.close();

    bbs.send_file(tmpPGN);
    (new File(tmpPGN)).remove();
    drawBoard();
}

// ── Input: keyboard + mouse ───────────────────────────────────────────────────

// Parse @`display`key@ hotspots from a menu string, returning screen column ranges.
// Skips \x01X attribute codes (zero width). Returns [{startCol, endCol, key}].
function parseMenuHotspots(str) {
    var result = [];
    var col = 1;
    var i = 0;
    while (i < str.length) {
        if (str.charCodeAt(i) === 1) { i += 2; continue; }  // skip \x01X attr codes
        if (str[i] === '@' && i+1 < str.length && str[i+1] === '`') {
            i += 2;
            var display = '';
            while (i < str.length && str[i] !== '`') display += str[i++];
            i++;  // skip closing `
            var key = str[i++].toUpperCase();
            if (i < str.length && str[i] === '@') i++;  // skip closing @
            var startCol = col;
            col += display.length;
            // Extend through trailing word chars (stop at space or next hotspot)
            while (i < str.length) {
                if (str.charCodeAt(i) === 1) { i += 2; continue; }
                if (str[i] === ' ' || str[i] === '@') break;
                col++; i++;
            }
            result.push({ startCol: startCol, endCol: col - 1, key: key });
            continue;
        }
        col++;
        i++;
    }
    return result;
}

var menuBarStr = "\x01H\x01Y@`(S)`S@\x01Cettings \x01H\x01Y@`(I)`I@\x01Cnstructions \x01H\x01Y@`(R)`R@\x01Cesign \x01H\x01Y@`(O)`O@\x01Cffer Draw \x01H\x01Y@`(Q)`Q@\x01Cuit \x01WMove: ";
var menuHotspots = parseMenuHotspots(menuBarStr);

function drawMenuBar() {
    console.gotoxy(1, INPUT_ROW);
    console.print(menuBarStr, P_ATCODES);
}

function readPlayerInput() {
    var buf = '';
    var pressSq = null;

    drawMenuBar();
    mouseon();

    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        var ch = console.inkey(0, 100);
        if (ch === '') continue;

        if (ch === '\x1b') {
            var seq = '\x1b';
            for (var t = 0; t < 24; t++) {
                var n = console.inkey(0, 40);
                if (n === '') break;
                seq += n;
                if (n.match(/[A-Za-z~]/)) break;
            }
            var mm = seq.match(/^\x1b\[<(\d+);(\d+);(\d+)([Mm])$/);
            if (mm) {
                var cb = parseInt(mm[1], 10), mx = parseInt(mm[2], 10), my = parseInt(mm[3], 10);
                var released = mm[4] === 'm';
                if (cb & 32) continue;

                var sq = charToSquare(mx, my);
                if (!released) {
                    if (my === INPUT_ROW) {
                        for (var hi = 0; hi < menuHotspots.length; hi++) {
                            if (mx >= menuHotspots[hi].startCol && mx <= menuHotspots[hi].endCol) {
                                console.ctrlkey_passthru = 0;
                                return { type: 'key', input: menuHotspots[hi].key };
                            }
                        }
                        continue;
                    }
                    pressSq = sq;
                    if (sq) {
                        var p = game.board[sq.r][sq.c];
                        var mine = p !== ' ' && ((p===p.toUpperCase()) === (game.playerColor==='w'));
                        if (mine) {
                            var hints = showLegalMoves ? game.getLegalMovesFrom(sq.r, sq.c) : null;
                            drawBoard(sq, hints);
                            mouseon();
                            showStatus("Selected " + p.toUpperCase() + game.toAlgebraic(sq.r,sq.c) +
                                " -- release on target", "1;33");
                        } else { pressSq = null; }
                    }
                } else {
                    if (pressSq && sq && (pressSq.r !== sq.r || pressSq.c !== sq.c)) {
                        var from = pressSq; pressSq = null;
                        console.ctrlkey_passthru = 0;
                        return { type: 'drag', from: from, to: sq };
                    }
                    pressSq = null;
                    drawBoard(null, null);
                    drawMenuBar();
                    mouseon();
                }
                continue;
            }
            continue;
        }

        var code = ch.charCodeAt(0);
        if (ch === '\r' || ch === '\n') { console.ctrlkey_passthru = 0; return { type:'key', input:buf }; }
        if ((ch === '\x08' || ch === '\x7f') && buf.length > 0) {
            buf = buf.substring(0, buf.length-1); console.write('\x08 \x08'); continue;
        }
        if (code >= 32 && buf.length < 10) {
            var up = ch.toUpperCase();
            if (buf.length === 0 && 'Q+-SIURPO'.indexOf(up) !== -1) {
                console.ctrlkey_passthru = 0;
                return { type:'key', input:up };
            }
            buf += up; console.write(up);
            // Auto-submit the moment the buffer is a complete, unambiguous legal move
            var autoTry = game.parseMove(buf);
            if (autoTry && !autoTry.ambiguous) {
                mswait(60);  // tiny pause so the user sees the last char before it fires
                console.ctrlkey_passthru = 0;
                return { type:'key', input:buf };
            }
        }
    }
    console.ctrlkey_passthru = 0;
    return null;
}

// ── Opening book ──────────────────────────────────────────────────────────────
// Each line is a sequence of UCI moves for a complete opening variation.
// White moves at even indices (0,2,4…), black at odd.
var OPENING_BOOK = [
    // ── Ruy Lopez ─────────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1","f8e7","f1e1","b7b5","a4b3","e8g8","c2c3","d7d6"],
    ["e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1","f8e7","f1e1","b7b5","a4b3","d7d6","c2c3","e8g8"],
    ["e2e4","e7e5","g1f3","b8c6","f1b5","g8f6","e1g1","f8e7","f1e1","b7b5","f1b3","d7d6","c2c3","e8g8"],
    ["e2e4","e7e5","g1f3","b8c6","f1b5","g8f6","d2d4","f6e4","e1g1","f6d5","f1e1","f8e7"],
    ["e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5c6","d7c6","e1g1","f7f6","d2d4"],
    // ── Italian / Giuoco ──────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","b8c6","f1c4","f8c5","c2c3","g8f6","d2d4","e5d4","c3d4"],
    ["e2e4","e7e5","g1f3","b8c6","f1c4","f8c5","e1g1","g8f6","d2d4","e5d4","e4e5"],
    ["e2e4","e7e5","g1f3","b8c6","f1c4","g8f6","d2d4","e5d4","e1g1","f6e4","f1e1"],
    ["e2e4","e7e5","g1f3","b8c6","f1c4","f8c5","b2b4","c5b4","c2c3","b4c5","d2d4"],
    // ── Two Knights ───────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","b8c6","f1c4","g8f6","f3g5","d7d5","e4d5","c6a5","f1b5","c7c6","d5c6","b7c6","b5e2","h7h6"],
    // ── Four Knights ──────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","b8c6","b1c3","g8f6","f1b5","f8b4","e1g1","e8g8","d2d3"],
    // ── Scotch ────────────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","b8c6","d2d4","e5d4","f3d4","f8c5","d4b3","c5b6","b1c3","g8f6"],
    ["e2e4","e7e5","g1f3","b8c6","d2d4","e5d4","f3d4","g8f6","d4c6","b7c6","e4e5","d8e7"],
    // ── Petrov ────────────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","g8f6","f3e5","d7d6","e5f3","f6e4","d2d4","d6d5","f1d3","b8c6","e1g1","f8e7"],
    // ── King's Gambit ─────────────────────────────────────────────────────────
    ["e2e4","e7e5","f2f4","e5f4","g1f3","d7d5","e4d5","g8f6","f1b5","c8d7","b5d7","d8d7"],
    ["e2e4","e7e5","f2f4","e5f4","g1f3","g7g5","f1c4","g5g4","e1g1","g4f3","d1f3"],
    // ── Vienna ────────────────────────────────────────────────────────────────
    ["e2e4","e7e5","b1c3","g8f6","f2f4","d7d5","f4e5","f6e4","g1f3","f8b4","d2d4"],
    ["e2e4","e7e5","b1c3","b8c6","g2g3","g8f6","f1g2","f8c5","g1e2","d7d6"],
    // ── Bishop's Opening ──────────────────────────────────────────────────────
    ["e2e4","e7e5","f1c4","g8f6","d2d3","b8c6","g1f3","f8e7","e1g1","e8g8"],
    // ── Philidor ──────────────────────────────────────────────────────────────
    ["e2e4","e7e5","g1f3","d7d6","d2d4","b8d7","f1c4","c7c6","e1g1","f8e7","a2a4"],
    // ── Pirc / Modern ─────────────────────────────────────────────────────────
    ["e2e4","d7d6","d2d4","g8f6","b1c3","g7g6","g1f3","f8g7","f1e2","e8g8","e1g1"],
    ["e2e4","d7d6","d2d4","g8f6","b1c3","g7g6","f2f4","f8g7","g1f3","e8g8","f1e2"],
    ["e2e4","g7g6","d2d4","f8g7","b1c3","d7d6","g1f3","g8f6","f1e2","e8g8"],
    // ── Alekhine's Defense ────────────────────────────────────────────────────
    ["e2e4","g8f6","e4e5","f6d5","d2d4","d7d6","g1f3","c8g4","f1e2","e7e6","e1g1"],
    ["e2e4","g8f6","e4e5","f6d5","c2c4","d5b6","d2d4","d7d6","f2f4","d6e5","f4e5"],
    // ── Scandinavian ─────────────────────────────────────────────────────────
    ["e2e4","d7d5","e4d5","d8d5","b1c3","d5a5","d2d4","g8f6","g1f3","c8f5","f1c4"],
    ["e2e4","d7d5","e4d5","g8f6","d2d4","f6d5","g1f3","g7g6","f1e2","f8g7","e1g1"],
    // ── Sicilian — Najdorf ────────────────────────────────────────────────────
    ["e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","a7a6","c1e3","e7e5","d4b3"],
    ["e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","a7a6","f2f4","e7e5","f4f5"],
    ["e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","a7a6","g2g4","e7e5","d4f3"],
    ["e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","e7e6","f2f4","a7a6","d1f3"],
    // ── Sicilian — Dragon ─────────────────────────────────────────────────────
    ["e2e4","c7c5","g1f3","b8c6","d2d4","c5d4","f3d4","g7g6","b1c3","f8g7","c1e3","g8f6","f1c4","e8g8","f2f3"],
    ["e2e4","c7c5","g1f3","b8c6","d2d4","c5d4","f3d4","g7g6","b1c3","f8g7","c1e3","g8f6","f1c4","e8g8","d1d2","d7d5"],
    // ── Sicilian — Scheveningen ───────────────────────────────────────────────
    ["e2e4","c7c5","g1f3","e7e6","d2d4","c5d4","f3d4","g8f6","b1c3","d7d6","f1e2","a7a6","e1g1","d8c7"],
    ["e2e4","c7c5","g1f3","e7e6","d2d4","c5d4","f3d4","g8f6","b1c3","d7d6","g2g4","b8c6","g4g5","f6d7"],
    // ── Sicilian — Kan / Taimanov ─────────────────────────────────────────────
    ["e2e4","c7c5","g1f3","e7e6","d2d4","c5d4","f3d4","a7a6","b1c3","d8c7","f1e2","g8f6","e1g1","b8c6"],
    ["e2e4","c7c5","g1f3","b8c6","d2d4","c5d4","f3d4","d8c7","b1c3","e7e6","f1e2","a7a6","e1g1"],
    // ── Sicilian — Sveshnikov ─────────────────────────────────────────────────
    ["e2e4","c7c5","g1f3","b8c6","d2d4","c5d4","f3d4","g8f6","b1c3","e7e5","d4b5","d7d6","d1d6","g8d7"],
    // ── Sicilian — Accelerated Dragon ─────────────────────────────────────────
    ["e2e4","c7c5","g1f3","b8c6","d2d4","c5d4","f3d4","g7g6","c2c4","f8g7","f1e2","g8f6","b1c3","e8g8"],
    // ── Sicilian — Closed ─────────────────────────────────────────────────────
    ["e2e4","c7c5","b1c3","b8c6","g2g3","g7g6","f1g2","f8g7","d2d3","d7d6","f2f4","e7e6"],
    // ── Sicilian — Alapin ─────────────────────────────────────────────────────
    ["e2e4","c7c5","c2c3","g8f6","e4e5","f6d5","d2d4","c5d4","c3d4","e7e6","g1f3","d7d6"],
    ["e2e4","c7c5","c2c3","b8c6","d2d4","d7d5","e4d5","d8d5","g1f3","e7e5"],
    // ── Sicilian — Grand Prix Attack ──────────────────────────────────────────
    ["e2e4","c7c5","b1c3","b8c6","f2f4","g7g6","g1f3","f8g7","f1c4","e7e6","f4f5"],
    // ── French — Classical ────────────────────────────────────────────────────
    ["e2e4","e7e6","d2d4","d7d5","b1c3","g8f6","c1g5","f8e7","e4e5","f6d7","g5e7","d8e7","f2f4"],
    ["e2e4","e7e6","d2d4","d7d5","b1c3","g8f6","c1g5","d5e4","c3e4","b8d7","g1f3","f8e7"],
    // ── French — Winawer ─────────────────────────────────────────────────────
    ["e2e4","e7e6","d2d4","d7d5","b1c3","f8b4","e4e5","c7c5","a2a3","b4c3","b2c3","g8e7","g1f3"],
    ["e2e4","e7e6","d2d4","d7d5","b1c3","f8b4","e4e5","c7c5","d1g4","g7g6","g1f3","d8c7"],
    // ── French — Advance ─────────────────────────────────────────────────────
    ["e2e4","e7e6","d2d4","d7d5","e4e5","c7c5","c2c3","b8c6","g1f3","d8b6","a2a3","g8h6"],
    // ── French — Tarrasch ─────────────────────────────────────────────────────
    ["e2e4","e7e6","d2d4","d7d5","b1d2","g8f6","e4e5","f6d7","f1d3","c7c5","c2c3","b8c6"],
    ["e2e4","e7e6","d2d4","d7d5","b1d2","c7c5","g1f3","g8f6","e4d5","e6d5","f1b5","c8d7","b5d7","b8d7"],
    // ── Caro-Kann — Classical ─────────────────────────────────────────────────
    ["e2e4","c7c6","d2d4","d7d5","b1c3","d5e4","c3e4","b8d7","g1f3","g8f6","e4f6","d7f6","f1d3"],
    // ── Caro-Kann — Advance ───────────────────────────────────────────────────
    ["e2e4","c7c6","d2d4","d7d5","e4e5","c8f5","g1f3","e7e6","f1e2","g8e7","e1g1","b8d7"],
    // ── Caro-Kann — Panov ────────────────────────────────────────────────────
    ["e2e4","c7c6","d2d4","d7d5","e4d5","c6d5","c2c4","g8f6","b1c3","e7e6","g1f3","f8e7","c4d5","e6d5"],
    // ── Caro-Kann — Two Knights ───────────────────────────────────────────────
    ["e2e4","c7c6","g1f3","d7d5","b1c3","c8g4","h2h3","g4f3","d1f3","e7e6","d2d4","g8f6"],
    // ── QGD — Orthodox ────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","e7e6","b1c3","g8f6","c1g5","f8e7","e2e3","e8g8","g1f3","b8d7","f1d3"],
    // ── QGD — Exchange ────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","e7e6","b1c3","g8f6","c4d5","e6d5","c1g5","c7c6","e2e3","f8e7","g1f3"],
    // ── QGA ───────────────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","d5c4","g1f3","g8f6","e2e3","e7e6","f1c4","c7c5","e1g1","a7a6"],
    // ── Slav ──────────────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","c7c6","g1f3","g8f6","b1c3","e7e6","c1g5","h7h6","g5f4","d5c4","e2e4"],
    ["d2d4","d7d5","c2c4","c7c6","g1f3","g8f6","b1c3","d5c4","a2a4","c8f5","e2e3","e7e6","f1c4"],
    // ── Semi-Slav ─────────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","c7c6","b1c3","g8f6","g1f3","e7e6","c1g5","b8d7","e2e3","d8a5","g5d2"],
    // ── Catalan ───────────────────────────────────────────────────────────────
    ["d2d4","d7d5","c2c4","e7e6","g1f3","g8f6","g2g3","f8e7","f1g2","e8g8","e1g1","d5c4","d1c2"],
    ["d2d4","g8f6","c2c4","e7e6","g2g3","d7d5","f1g2","d5c4","g1f3","f8b4","c1d2","b4d2","b1d2","e8g8"],
    // ── Nimzo-Indian ─────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","e7e6","b1c3","f8b4","e2e3","b7b6","g1e2","c8b7","a2a3","b4c3","e2c3"],
    ["d2d4","g8f6","c2c4","e7e6","b1c3","f8b4","g1f3","b7b6","d1b3","d8e7","c1g5","h7h6"],
    // ── Queen's Indian ────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","e7e6","g1f3","b7b6","g2g3","c8b7","f1g2","f8e7","e1g1","e8g8","b1c3"],
    ["d2d4","g8f6","c2c4","e7e6","g1f3","b7b6","c1g5","f8b4","b1d2","c8b7","e2e3","e8g8"],
    // ── King's Indian ─────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","g7g6","b1c3","f8g7","e2e4","d7d6","g1f3","e8g8","f1e2","e7e5","e1g1","b8c6"],
    ["d2d4","g8f6","c2c4","g7g6","b1c3","f8g7","e2e4","d7d6","f2f3","e8g8","c1e3","c7c6","d1d2"],
    ["d2d4","g8f6","c2c4","g7g6","b1c3","f8g7","e2e4","d7d6","g1f3","e8g8","f1e2","e7e5","d4d5","b8a6"],
    // ── Grunfeld ──────────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","g7g6","b1c3","d7d5","c4d5","f6d5","e2e4","d5c3","b2c3","c7c5","g1f3","f8g7","f1c4"],
    ["d2d4","g8f6","c2c4","g7g6","b1c3","d7d5","g1f3","f8g7","d1b3","d5c4","b3c4","e8g8","e2e4","c8g4"],
    // ── Bogo-Indian ───────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","e7e6","g1f3","f8b4","c1d2","b4d2","d1d2","d7d5","g2g3","e8g8","f1g2"],
    // ── Benoni ────────────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","c7c5","d4d5","e7e6","b1c3","e6d5","c4d5","d7d6","e2e4","g7g6","f2f4","f8g7","g1f3"],
    // ── Benko Gambit ──────────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","c7c5","d4d5","b7b5","c4b5","a7a6","b5a6","c8a6","b1c3","d7d6","e2e4","f8g7"],
    // ── Budapest Gambit ───────────────────────────────────────────────────────
    ["d2d4","g8f6","c2c4","e7e5","d4e5","f6g4","g1f3","f8c5","e2e3","g4e5","f3e5","c5e3"],
    // ── Dutch — Stonewall ─────────────────────────────────────────────────────
    ["d2d4","f7f5","g1f3","g8f6","g2g3","e7e6","f1g2","d7d5","e1g1","f8d6","c2c4","c7c6","b2b3"],
    // ── Dutch — Leningrad ─────────────────────────────────────────────────────
    ["d2d4","f7f5","g1f3","g8f6","g2g3","g7g6","f1g2","f8g7","e1g1","e8g8","c2c4","d7d6","b1c3"],
    // ── London System ─────────────────────────────────────────────────────────
    ["d2d4","d7d5","g1f3","g8f6","c1f4","e7e6","e2e3","f8d6","f4d6","d8d6","f1d3","e8g8","e1g1","b8d7"],
    ["d2d4","g8f6","g1f3","e7e6","c1f4","f8b4","b1d2","c7c5","e2e3","b4d2","d1d2","b7b6"],
    // ── Trompowsky ────────────────────────────────────────────────────────────
    ["d2d4","g8f6","c1g5","f6e4","g5f4","c7c5","f2f3","d8a5","c2c3","e4f6","d2d5","e7e6"],
    // ── Torre Attack ──────────────────────────────────────────────────────────
    ["d2d4","g8f6","g1f3","e7e6","c1g5","c7c5","e2e3","d7d5","b1d2","f8e7","f1d3","e8g8"],
    // ── Colle System ──────────────────────────────────────────────────────────
    ["d2d4","d7d5","g1f3","g8f6","e2e3","e7e6","f1d3","c7c5","c2c3","b8c6","e1g1","f8d6","b1d2"],
    // ── English ───────────────────────────────────────────────────────────────
    ["c2c4","e7e5","b1c3","g8f6","g1f3","b8c6","g2g3","f8b4","f1g2","e8g8","e1g1","d7d6","d2d3"],
    ["c2c4","c7c5","b1c3","b8c6","g2g3","g7g6","f1g2","f8g7","g1f3","e7e6","e1g1","g8e7","d2d3"],
    ["c2c4","e7e5","g2g3","g8f6","f1g2","d7d5","c4d5","f6d5","b1c3","b8c6","g1f3","b8e7"],
    ["c2c4","g8f6","g2g3","d7d5","f1g2","d5c4","d1a4","b8d7","a4c4","a7a6","g1f3","b7b5"],
    // ── Reti ──────────────────────────────────────────────────────────────────
    ["g1f3","d7d5","c2c4","e7e6","g2g3","g8f6","f1g2","f8e7","e1g1","e8g8","b2b3","b8d7","c1b2"],
    ["g1f3","g8f6","c2c4","e7e6","b1c3","d7d5","d2d4","f8e7","c1g5","e8g8","e2e3","b8d7"],
    ["g1f3","d7d5","g2g3","g8f6","f1g2","g7g6","e1g1","f8g7","d2d3","e8g8","b1d2","b8c6"],
    // ── Bird's Opening ────────────────────────────────────────────────────────
    ["f2f4","d7d5","g1f3","g8f6","e2e3","g7g6","f1e2","f8g7","e1g1","e8g8","d2d3","c7c5"],
];

function getBookMove() {
    var history = [];
    for (var i = 0; i < game.history.length; i++)
        history.push(game.history[i].uci);

    var aiIsWhite = (game.playerColor === 'b');
    var candidates = [];

    for (var li = 0; li < OPENING_BOOK.length; li++) {
        var line = OPENING_BOOK[li];
        var nextIdx = history.length;
        if (nextIdx >= line.length) continue;

        // history must be a prefix of this line
        var match = true;
        for (var mi = 0; mi < nextIdx; mi++) {
            if (line[mi] !== history[mi]) { match = false; break; }
        }
        if (!match) continue;

        // next move must be for the AI's color
        var nextIsWhite = (nextIdx % 2 === 0);
        if (nextIsWhite !== aiIsWhite) continue;

        candidates.push(line[nextIdx]);
    }

    if (candidates.length === 0) return null;
    var uci = candidates[Math.floor(Math.random() * candidates.length)];
    var f = game.toCoords(uci.substring(0, 2));
    var t = game.toCoords(uci.substring(2, 4));
    if (!game.isLegalMove(f.r, f.c, t.r, t.c)) return null;
    return { f: f, t: t, promoteTo: null };
}

// ── Enumerate all legal moves for the AI's current turn ───────────────────────
function getAllAIMoves() {
    var color = game.turn;
    var moves = [];
    for (var r = 0; r < 8; r++) {
        for (var c = 0; c < 8; c++) {
            var p = game.board[r][c];
            if (p === ' ') continue;
            if ((p === p.toUpperCase()) !== (color === 'w')) continue;
            var targets = game.getLegalMovesFrom(r, c);
            for (var ti = 0; ti < targets.length; ti++)
                moves.push({ f: {r:r, c:c}, t: targets[ti], promoteTo: null });
        }
    }
    return moves;
}

// ── AI move — Stockfish or built-in engine ────────────────────────────────────
// Levels 1-10. Level 1 = random. Level 2 = greedy (best capture or random).
// Opening book tried first at all levels > 2.
//
// Built-in engine depth map (levels 3-10):
//   L3=1  L4=2  L5=3  L6=4  L7=5  L8=6  L9=7  L10=8
//   Approx ELO: 500 / 700 / 900 / 1200 / 1400 / 1600 / 1800 / 2000
//
// Stockfish think-time map (seconds, levels 3-10):
//   0.2 / 0.5 / 1 / 1.5 / 2 / 3 / 4 / 6
//   Approx ELO: 1500 / 2000 / 2200 / 2400 / 2500 / 2600 / 2700 / 2800

function getBuiltinMove() {
    var depthMap = [1, 2, 3, 4, 5, 6, 7, 8];  // index = level-3 (L3..L10)
    var depth = depthMap[game.level - 3] || 1;
    // Time limits (ms) to keep higher levels from hanging indefinitely
    var timeMap = [500, 1000, 2000, 4000, 8000, 15000, 25000, 40000];
    var timeMs  = timeMap[game.level - 3] || 500;
    var eng = new ChessEngine(game);
    return eng.getMove(depth, timeMs);
}

function getStockfishMove() {
    var thinkMap = [0, 0, 0, 0.2, 0.5, 1, 1.5, 2, 3, 4];  // index = level-1 (L3+ uses stockfish)
    var thinkSec = thinkMap[game.level - 1] || 1;

    var fen    = game.getFEN();
    var tmpSh  = "/tmp/sc_" + user.number + "_run.sh";
    var tmpOut = "/tmp/sc_" + user.number + "_out.txt";

    var fsh = new File(tmpSh);
    if (!fsh.open("w")) return null;
    fsh.writeln("#!/bin/bash");
    fsh.writeln("(");
    fsh.writeln("  echo uci");
    fsh.writeln("  echo isready");
    fsh.writeln("  echo 'position fen " + fen + "'");
    fsh.writeln("  echo 'go infinite'");
    fsh.writeln("  sleep " + thinkSec);
    fsh.writeln("  echo stop");
    fsh.writeln("  sleep 0.2");
    fsh.writeln(") | " + SF_PATH + " > " + tmpOut + " 2>/dev/null");
    fsh.close();

    system.exec("/bin/bash " + tmpSh);

    var fout = new File(tmpOut);
    if (!fout.open("r")) return null;
    var lines = fout.readAll();
    fout.close();
    (new File(tmpSh)).remove();
    (new File(tmpOut)).remove();

    for (var i = lines.length - 1; i >= 0; i--) {
        var bm = lines[i].match(/^bestmove\s+([a-h][1-8][a-h][1-8])([qrbn]?)/);
        if (bm) return {
            f: game.toCoords(bm[1].substring(0, 2)),
            t: game.toCoords(bm[1].substring(2, 4)),
            promoteTo: bm[2] ? bm[2].toUpperCase() : null
        };
    }
    return null;
}

function getAIMove() {
    // Level 1: pure random
    if (game.level === 1) {
        var moves = getAllAIMoves();
        return moves.length > 0 ? moves[Math.floor(Math.random() * moves.length)] : null;
    }

    // Level 2: greedy — take best capture if available, else random
    if (game.level === 2) {
        var moves = getAllAIMoves();
        if (moves.length === 0) return null;
        var caps = moves.filter(function(m) { return game.board[m.t.r][m.t.c] !== ' '; });
        if (caps.length > 0) {
            caps.sort(function(a, b) {
                return game.pieceValue(game.board[b.t.r][b.t.c]) - game.pieceValue(game.board[a.t.r][a.t.c]);
            });
            return caps[0];
        }
        return moves[Math.floor(Math.random() * moves.length)];
    }

    // All levels > 2: try opening book first
    var bookMove = getBookMove();
    if (bookMove) return bookMove;

    return useStockfish ? getStockfishMove() : getBuiltinMove();
}

// ── Position evaluation (for draw offers) ─────────────────────────────────────
// Returns centipawn score from the perspective of the side to move.
function getStockfishScore() {
    if (!useStockfish) {
        var eng = new ChessEngine(game);
        return eng.evaluate();
    }
    var fen    = game.getFEN();
    var tmpSh  = "/tmp/sc_" + user.number + "_eval.sh";
    var tmpOut = "/tmp/sc_" + user.number + "_eval.txt";

    var fsh = new File(tmpSh);
    if (!fsh.open("w")) return 0;
    fsh.writeln("#!/bin/bash");
    fsh.writeln("(");
    fsh.writeln("  echo uci");
    fsh.writeln("  echo isready");
    fsh.writeln("  echo 'position fen " + fen + "'");
    fsh.writeln("  echo 'go infinite'");
    fsh.writeln("  sleep 0.5");
    fsh.writeln("  echo stop");
    fsh.writeln("  sleep 0.2");
    fsh.writeln(") | " + SF_PATH + " > " + tmpOut + " 2>/dev/null");
    fsh.close();

    system.exec("/bin/bash " + tmpSh);

    var fout = new File(tmpOut);
    if (!fout.open("r")) return 0;
    var lines = fout.readAll();
    fout.close();
    (new File(tmpSh)).remove();
    (new File(tmpOut)).remove();

    // Parse last info line for score cp or score mate
    var score = 0;
    for (var i = 0; i < lines.length; i++) {
        var sm = lines[i].match(/\bscore\s+cp\s+(-?\d+)/);
        if (sm) score = parseInt(sm[1], 10);
        var mm = lines[i].match(/\bscore\s+mate\s+(-?\d+)/);
        if (mm) score = parseInt(mm[1], 10) > 0 ? 30000 : -30000;
    }
    return score;
}

// ── Move animation ───────────────────────────────────────────────────────────
// Slide a piece from its source to destination square.  When flash is true,
// the source square is highlighted twice first to capture attention (AI moves).
// Called before applyMove() so the board still shows the original position.

function animateMove(from, to, flash) {
	var piece = game.board[from.r][from.c];
	if (piece === ' ') return;

	if (flash) {
		// Flash source square twice to capture attention
		for (var fi = 0; fi < 2; fi++) {
			drawSquare(from.r, from.c, 'sel');
			mswait(150);
			drawSquare(from.r, from.c, null);
			mswait(100);
		}
	}

	// JXL images have baked-in backgrounds matching default colors only
	var canJXL = jxlOk && darkBg === '44' && lightBg === '46';

	if (canJXL)
		animateSlideJXL(from, to, piece);
	else
		animateSlideANSI(from, to, piece);

	// Castling: also animate the rook (no flash, just slide)
	if (piece.toUpperCase() === 'K' && from.c === 4 && Math.abs(to.c - from.c) === 2) {
		var rookFrom, rookTo;
		if (to.c === 6) { rookFrom = {r: from.r, c: 7}; rookTo = {r: from.r, c: 5}; }
		else            { rookFrom = {r: from.r, c: 0}; rookTo = {r: from.r, c: 3}; }
		var rook = game.board[rookFrom.r][rookFrom.c];
		if (rook !== ' ') {
			// Draw king at its destination so the rook's background buffer
			// includes it (otherwise the rook erase mask wipes the king)
			game.board[to.r][to.c] = piece;
			drawSquare(to.r, to.c, null);
			if (canJXL)
				animateSlideJXL(rookFrom, rookTo, rook);
			else
				animateSlideANSI(rookFrom, rookTo, rook);
			game.board[to.r][to.c] = ' ';
		}
	}

	// Reset attributes so console.clear() in drawBoard() doesn't fill
	// the screen with the last square's background color (BCE mode)
	console.write("\x1b[0m");
}

// Mask Y-offsets into piece_mask.pbm: each piece has 160 rows (32 draw + 128 erase)
var ANIM_BORDER = 48;  // erase mask border (covers max per-frame movement)
var pieceDrawMY  = { 'P': 0, 'N': 160, 'B': 320, 'R': 480, 'Q': 640, 'K': 800 };
var pieceEraseMY = { 'P': 32, 'N': 192, 'B': 352, 'R': 512, 'Q': 672, 'K': 832 };

// Sprite sheet Y-offsets into piece_sprites.jxl (pre-loaded in buffer 1)
// [darkSY, lightSY] per piece char
// Index: 0=dark, 1=light, 2=cap, 3=promo
var pieceSpriteY = {
	'P': [0, 32, 64, 96],         'p': [128, 160, 192, 224],
	'N': [256, 288, 320, 352],    'n': [384, 416, 448, 480],
	'B': [512, 544, 576, 608],    'b': [640, 672, 704, 736],
	'R': [768, 800, 832, 864],    'r': [896, 928, 960, 992],
	'Q': [1024, 1056, 1088, 1120],'q': [1152, 1184, 1216, 1248],
	'K': [1280, 1312, 1344, 1376],'k': [1408, 1440, 1472, 1504]
};

function animateSlideJXL(from, to, piece) {
	var srcPos = squareToChar(from.r, from.c);
	var dstPos = squareToChar(to.r, to.c);
	var pUp    = piece.toUpperCase();
	var drawMY  = pieceDrawMY[pUp];
	var eraseMY = pieceEraseMY[pUp];
	var B = ANIM_BORDER;

	// Select piece variant from sprite sheet (buffer 1, pre-loaded at startup)
	var dstDark = (to.r + to.c) % 2 !== 0;
	var spriteSY = pieceSpriteY[piece][dstDark ? 0 : 1];  // dark=0, light=1

	// Clear the piece from its source square on screen
	var saved = game.board[from.r][from.c];
	game.board[from.r][from.c] = ' ';
	drawSquare(from.r, from.c, null);
	game.board[from.r][from.c] = saved;

	// Save clean screen (piece removed) to pixel buffer 0
	console.write("\x1b_SyncTERM:P;Copy;B=0\x1b\\");

	// Piece size in pixels
	var pw = cellW * charPixW;
	var ph = cellH * charPixH;
	var ew = pw + 2 * B;  // erase mask width
	var eh = ph + 2 * B;  // erase mask height

	// Source and destination pixel positions (0-based)
	var sx = (srcPos.col - 1) * charPixW;
	var sy = (srcPos.row - 1) * charPixH;
	var dx = (dstPos.col - 1) * charPixW;
	var dy = (dstPos.row - 1) * charPixH;

	var STEPS = 12;
	var prevX = -1, prevY = -1;

	for (var i = 0; i <= STEPS; i++) {
		var t = i / STEPS;
		// Ease-in-out curve
		t = t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
		var cx = Math.round(sx + (dx - sx) * t);
		var cy = Math.round(sy + (dy - sy) * t);

		if (cx === prevX && cy === prevY) continue;

		// Pre-build draw + erase commands, emit in one write (atomic frame)
		// 1. Draw piece at new position from sprite sheet with silhouette mask
		var cmds = "\x1b_SyncTERM:P;Paste;SY=" + spriteSY + ";SH=" + ph +
		           ";DX=" + cx + ";DY=" + cy +
		           ";MBUF;MY=" + drawMY + ";B=1\x1b\\";
		// 2. Erase trail: paste background around new position with inverted
		//    mask (hole preserves the piece just drawn, border erases trail)
		if (prevX >= 0) {
			cmds += "\x1b_SyncTERM:P;Paste;SX=" + (cx - B) + ";SY=" + (cy - B) +
			        ";SW=" + ew + ";SH=" + eh +
			        ";DX=" + (cx - B) + ";DY=" + (cy - B) +
			        ";MBUF;MY=" + eraseMY + ";B=0\x1b\\";
		}
		console.write(cmds);

		prevX = cx;
		prevY = cy;
		mswait(20);
	}

	// Piece remains drawn at its destination — no cleanup needed
}

function animateSlideANSI(from, to, piece) {
	var srcPos = squareToChar(from.r, from.c);
	var dstPos = squareToChar(to.r, to.c);
	var isWp = (piece === piece.toUpperCase());
	var fg = isWp ? whiteFg : blackFg;

	// Clear the piece from its source square on screen
	var saved = game.board[from.r][from.c];
	game.board[from.r][from.c] = ' ';
	drawSquare(from.r, from.c, null);
	game.board[from.r][from.c] = saved;

	// Interpolate from center of source cell to center of dest cell
	var scx = srcPos.col + 2, scy = srcPos.row;
	var dcx = dstPos.col + 2, dcy = dstPos.row;

	var STEPS = 8;
	var prevCol = -1, prevRow = -1;

	for (var i = 0; i <= STEPS; i++) {
		var t = i / STEPS;
		t = t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
		var cx = Math.round(scx + (dcx - scx) * t);
		var cy = Math.round(scy + (dcy - scy) * t);

		if (cx === prevCol && cy === prevRow) continue;

		// Restore previous position by redrawing the board square under it
		if (prevCol >= 0) {
			var sq = charToSquare(prevCol, prevRow);
			if (sq) drawSquare(sq.r, sq.c, null);
			else { console.gotoxy(prevCol, prevRow); console.write("\x1b[0m "); }
		}

		// Draw piece character at new position
		console.gotoxy(cx, cy);
		console.write("\x1b[" + selBg + ";" + fg + "m" + piece.toUpperCase() + "\x1b[0m");

		prevCol = cx;
		prevRow = cy;
		mswait(30);
	}

	// Restore final position (full drawBoard follows)
	if (prevCol >= 0) {
		var sq = charToSquare(prevCol, prevRow);
		if (sq) drawSquare(sq.r, sq.c, null);
		else { console.gotoxy(prevCol, prevRow); console.write("\x1b[0m "); }
	}
}

// ── Apply a player move + trigger AI ─────────────────────────────────────────
function doPlayerMove(from, to) {
    if (!game.isLegalMove(from.r, from.c, to.r, to.c)) {
        showStatus("Illegal move.", "1;31"); mswait(700); return false;
    }
    var promo = null;
    var piece = game.board[from.r][from.c];
    if ((piece==='P' && to.r===0) || (piece==='p' && to.r===7))
        promo = askPromotion(game.playerColor);

    drawBoard();
    animateMove(from, to, false);
    game.applyMove(from, to, promo);
    drawBoard();
    if (game.isCheckmate() || game.isStalemate() ||
        game.isThreefoldRepetition() || game.isInsufficientMaterial() ||
        game.halfMoveClock >= 100) return true;

    if (game.turn !== game.playerColor) {
        showStatus("Thinking...", "0;36");
        var ai = getAIMove();
        if (ai) { animateMove(ai.f, ai.t, true); game.applyMove(ai.f, ai.t, ai.promoteTo); drawBoard(); }
        else { showStatus("Engine returned no move.", "1;31"); mswait(1200); }
    }
    return true;
}

// ── Startup screen ────────────────────────────────────────────────────────────
// Returns false if the user pressed Q to quit, true otherwise.
function runStartup() {
    function rep(s, n) { var o = ''; for (var i = 0; i < n; i++) o += s; return o; }

    var RST = "\x1b[0m";
    var BC  = "\x1b[1;36m";        // bright cyan — borders
    var DSQ = "\x1b[1;33;44m";     // bright yellow on dark blue  (dark square)
    var LSQ = "\x1b[1;32;46m";     // bright green  on dark cyan  (light square)
    var tileColors = [DSQ,LSQ,DSQ,LSQ, DSQ,LSQ,DSQ,LSQ];

    // CP437 box drawing chars sent as low-byte values (Synchronet truncates to low byte)
    var TL = "\xC9", TR = "\xBB", BL = "\xC8", BR = "\xBC"; // ╔ ╗ ╚ ╝
    var VB = "\xBA", HB = "\xCD";                            // ║ ═

    // ── Box helpers ───────────────────────────────────────────────────────
    function hline(l, r, row) {
        console.gotoxy(1, row);
        console.print(BC + l + rep(HB, 78) + r + RST);
    }
    function brow(content, row) {
        console.gotoxy(1, row);
        console.print(BC + VB + RST + content + BC + VB + RST);
    }
    function blank(row) { brow(rep(" ", 78), row); }

    // ── Static screen (rows 1-16) — callable for initial draw and redraw ──
    function drawScreen() {
        console.clear();
        mouseon();

        // Rows 1-2: chess board header strips (20 tiles × 4 chars = 80)
        var pat1 = "", pat2 = "";
        for (var i = 0; i < 20; i++) {
            pat1 += ((i%2===0) ? "\x1b[44m" : "\x1b[46m") + "    ";
            pat2 += ((i%2===0) ? "\x1b[46m" : "\x1b[44m") + "    ";
        }
        console.gotoxy(1,1); console.print(pat1 + RST);
        console.gotoxy(1,2); console.print(pat2 + RST);

        hline(TL, TR, 3);
        blank(4);

        // Rows 5-7: 3-row big title
        var letters = "SYNCHESS";
        function titleRow(showLetters) {
            var s = rep(" ", 3);
            for (var i = 0; i < 8; i++) {
                s += tileColors[i];
                s += showLetters ? "    " + letters[i] + "    " : rep(" ", 9);
                s += RST;
            }
            return s + rep(" ", 3);
        }
        brow(titleRow(false), 5);
        brow(titleRow(true),  6);
        brow(titleRow(false), 7);
        blank(8);

        // Row 9: subtitle
        var uname   = user.alias || user.name || "Player";
        var welcome = "Welcome, " + uname + "!";
        var right   = "Chess for Synchronet BBS  |  v1.0";
        var visLen  = welcome.length + 5 + right.length;
        var lpad    = Math.max(0, Math.floor((78 - visLen) / 2));
        var rpad    = Math.max(0, 78 - visLen - lpad);
        var sub = rep(" ", lpad) +
                  "\x1b[1;33m" + welcome +
                  "  \x1b[0;36m|\x1b[0;37m  " +
                  right + RST +
                  rep(" ", rpad);
        brow(sub, 9);
        blank(10);

        // Rows 11-12: piece images
        var pieceStartCol = 21;
        var pieceRow = ['R','N','B','Q','K','B','N','R'];
        for (var row = 11; row <= 12; row++) {
            console.gotoxy(1, row);
            console.print(BC + VB + RST + rep(" ", 19));
            for (var i = 0; i < 8; i++)
                console.print(((i%2===0) ? "\x1b[44m" : "\x1b[46m") + rep(" ", cellW) + RST);
            console.print(rep(" ", 19) + BC + VB + RST);
        }
        if (jxlOk) {
            for (var i = 0; i < 8; i++)
                drawPieceJXL(pieceRow[i], (i%2===0), pieceStartCol + i*cellW, 11);
        }

        blank(13);
        hline(BL, BR, 14);

        // Rows 15-16: footer strips
        console.gotoxy(1,15); console.print(pat2 + RST);
        console.gotoxy(1,16); console.print(pat1 + RST);
    }

    drawScreen();

    // ── Row 18: color selection ───────────────────────────────────────────
    var colorStr = "         \x01NPlay as:  " +
                   "\x01H\x01Y@`(W)`W@\x01Chite  " +
                   "\x01H\x01Y@`(B)`B@\x01Clack  " +
                   "\x01H\x01Y@`(R)`R@\x01Candom  " +
                   "\x01H\x01Y@`(Q)`Q@\x01Cuit  ";
    var c;
    while (true) {
        c = readHotspotKey(colorStr, 18);
        if (c==='W'||c==='B'||c==='R'||c==='Q'||c===''||c==='\x1b') break;
    }
    if (c === 'Q' || c === '' || c === '\x1b') { return false; }
    if (c === 'R') c = (Math.random() < 0.5) ? 'W' : 'B';
    var chosenName = (c === 'W') ? 'White' : 'Black';
    console.gotoxy(1, 18);
    console.print("\x1b[0;37m         Play as:  \x1b[1;33m" + chosenName + "\x1b[K\x1b[0m");
    game.playerColor = (c === 'W') ? 'w' : 'b';

    // ── Rows 20-22: difficulty selection ─────────────────────────────────
    var diffStr = "   \x01NDifficulty: " +
                  "\x01H\x01Y@`(1)`1@\x01C \x01H\x01Y@`(2)`2@\x01C \x01H\x01Y@`(3)`3@\x01C " +
                  "\x01H\x01Y@`(4)`4@\x01C \x01H\x01Y@`(5)`5@\x01C \x01H\x01Y@`(6)`6@\x01C " +
                  "\x01H\x01Y@`(7)`7@\x01C \x01H\x01Y@`(8)`8@\x01C \x01H\x01Y@`(9)`9@\x01C " +
                  "\x01H\x01Y@`(0)`0@\x01C=10  \x01H\x01Y@`(?)`?@\x01C=Guide";
    console.gotoxy(4, 21);
    console.print("\x01NChoice (1-10, 0=10, Enter=\x01H\x01W" + game.level + "\x01N): \x01H\x01Y");
    var lc, lv;
    while (true) {
        lc = readHotspotKey(diffStr, 20);
        if (lc === '?') {
            // Show ELO info overlay
            console.clear();
            mouseon();
            var C2 = 5;
            var eng2 = useStockfish ? "Stockfish" : "Built-in";
            console.gotoxy(C2, 2);  console.print("\x1b[1;37mLevel Guide (" + eng2 + ")\x1b[0m");
            var eloRows = [
                [" 1", "Random moves",                      "~200"  ],
                [" 2", "Greedy captures",                   "~400"  ],
                [" 3", useStockfish ? "0.2s"  : "depth 1", useStockfish ? "~1500"  : "~500"  ],
                [" 4", useStockfish ? "0.5s"  : "depth 2", useStockfish ? "~2000"  : "~700"  ],
                [" 5", useStockfish ? "1s"    : "depth 3", useStockfish ? "~2200"  : "~900"  ],
                [" 6", useStockfish ? "1.5s"  : "depth 4", useStockfish ? "~2400"  : "~1200" ],
                [" 7", useStockfish ? "2s"    : "depth 5", useStockfish ? "~2500"  : "~1400" ],
                [" 8", useStockfish ? "3s"    : "depth 6", useStockfish ? "~2600"  : "~1600" ],
                [" 9", useStockfish ? "4s"    : "depth 7", useStockfish ? "~2700"  : "~1800" ],
                ["10", useStockfish ? "6s"    : "depth 8", useStockfish ? "~2800"  : "~2000" ],
            ];
            console.gotoxy(C2, 3);
            console.print("\x1b[0;37m\x1b[0m");  // blank line between title and header
            console.gotoxy(C2, 4);
            console.print("\x1b[0;36m" + padR("Lvl", 5) + padR("Engine", 16) + "Approx ELO\x1b[0m");
            for (var si2 = 0; si2 < eloRows.length; si2++) {
                console.gotoxy(C2, 5 + si2);
                console.print("\x1b[0;37m" + padR(eloRows[si2][0], 5) +
                              padR(eloRows[si2][1], 16) + eloRows[si2][2] + "\x1b[0m");
            }
            var pkRow = 5 + eloRows.length + 1;
            console.gotoxy(C2, pkRow);
            console.print("\x1b[0;33mPress any key...\x1b[0m");
            var pkMap2 = {}; pkMap2[pkRow] = ' ';
            readOneKey(pkMap2);
            // Redraw startup screen
            drawScreen();
            console.gotoxy(1, 18);
            console.print("\x1b[0;37m         Play as:  \x1b[1;33m" + chosenName + "\x1b[K\x1b[0m");
            console.gotoxy(4, 21);
            console.print("\x01NChoice (1-10, 0=10, Enter=\x01H\x01W" + game.level + "\x01N): \x01H\x01Y");
            continue;
        }
        if (lc === 'Q' || lc === '\x1b' || lc === '') { return false; }
        if (lc === '\r' || lc === '\n') { lv = game.level; break; }  // Enter = keep current
        lv = (lc === '0') ? 10 : parseInt(lc, 10);
        if (isNaN(lv) || lv < 1 || lv > 10) continue;
        break;
    }
    if (isNaN(lv) || lv < 1 || lv > 10) lv = game.level;
    game.level = lv;
    console.print(RST);
    return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────
mouseon();

var keepPlaying = true;
while (keepPlaying && !console.aborted) {

    // Reset game state for a new game, preserving level from previous game
    var savedLevel = game ? game.level : 3;
    game = new ChessGame(user.number);
    game.init();
    game.level = savedLevel;

    // ── Check for saved game ───────────────────────────────────────────────────
    var skipStartup = false;
    var svf = !isGuest && new File(gameSavePath);
    var hasSave = svf && svf.open("r");
    if (hasSave) svf.close();
    if (hasSave) {
        console.clear();
        console.gotoxy(3, 8);
        console.print("\x1b[1;36mA saved game was found.\x1b[0m\r\n");
        var rk2 = readHotspotKey(
            "   \x01H\x01Y@`(R)`R@\x01Nesume  \x01H\x01Y@`(N)`N@\x01New game  \x01H\x01Y@`(Q)`Q@\x01Nuit",
            10
        );
        if (rk2 === 'Q') { keepPlaying = false; break; }
        if (rk2 === 'R') {
            if (loadSavedGame()) {
                skipStartup = true;
            } else {
                showStatus("Could not load save file. Starting new game.", "1;31");
                mswait(1500); deleteSavedGame();
            }
        } else {
            deleteSavedGame();
        }
    }

    if (!skipStartup) {
        if (!runStartup()) { keepPlaying = false; break; }
    }
    drawBoard();

    if (!skipStartup && game.playerColor === 'b') {
        showStatus("Engine opening as White...", "0;36");
        var aiFirst = getAIMove();
        if (aiFirst) { animateMove(aiFirst.f, aiFirst.t, true); game.applyMove(aiFirst.f, aiFirst.t, aiFirst.promoteTo); drawBoard(); }
    }
    // On resume, if it somehow became AI's turn, let AI move
    if (skipStartup && game.turn !== game.playerColor) {
        showStatus("Stockfish thinking...", "0;36");
        var aiResume = getAIMove();
        if (aiResume) { animateMove(aiResume.f, aiResume.t, true); game.applyMove(aiResume.f, aiResume.t, aiResume.promoteTo); drawBoard(); }
    }

    // ── Inner game loop ───────────────────────────────────────────────────────
    var gameEnded = false;
    var gameResult = "";
    var gameResultColor = "1;37";
    while (!console.aborted && !gameEnded) {

        if (game.isCheckmate()) {
            var won = (game.turn !== game.playerColor);
            if (won) { stats.wins_checkmate++;  saveIni(); gameResult = "Checkmate! You win!";  gameResultColor = "1;32"; }
            else     { stats.losses_checkmate++; saveIni(); gameResult = "Checkmate! You lose."; gameResultColor = "1;31"; }
            deleteSavedGame(); drawBoard(); gameEnded = true; break;
        }
        if (game.isStalemate()) {
            stats.draws_stalemate++; saveIni();
            gameResult = "Stalemate -- draw."; gameResultColor = "1;33";
            deleteSavedGame(); drawBoard(); gameEnded = true; break;
        }
        if (game.isInsufficientMaterial()) {
            stats.draws_insufficient++; saveIni();
            gameResult = "Insufficient material -- draw."; gameResultColor = "1;33";
            deleteSavedGame(); drawBoard(); gameEnded = true; break;
        }
        if (game.isThreefoldRepetition()) {
            stats.draws_threefold++; saveIni();
            gameResult = "3-fold repetition -- draw."; gameResultColor = "1;33";
            deleteSavedGame(); drawBoard(); gameEnded = true; break;
        }
        if (game.halfMoveClock >= 100) {
            stats.draws_fifty++; saveIni();
            gameResult = "50-move rule -- draw."; gameResultColor = "1;33";
            deleteSavedGame(); drawBoard(); gameEnded = true; break;
        }

        if (game.turn !== game.playerColor) { mswait(50); continue; }

        if (game.isInCheck(game.turn)) showStatus("CHECK! You are in check.", "1;31");
        else clearStatus();

        var action = readPlayerInput();
        if (!action) { gameEnded = true; keepPlaying = false; break; }

        if (action.type === 'drag') { doPlayerMove(action.from, action.to); continue; }

        var inp = action.input;
        if (inp === 'Q') {
            if (game.history.length > 0) {
                var qkPrompt = isGuest
                    ? "\x01H\x01YQuit: \x01H\x01Y@`(A)`A@\x01Nbandon (loss)  \x01H\x01Y@`(C)`C@\x01Nancel"
                    : "\x01H\x01YQuit: \x01H\x01Y@`(S)`S@\x01Nave  \x01H\x01Y@`(A)`A@\x01Nbandon (loss)  \x01H\x01Y@`(C)`C@\x01Nancel";
                var qk = readHotspotKey(qkPrompt, STATUS_ROW);
                if (qk === 'S') { saveGame(); gameEnded = true; keepPlaying = false; break; }
                if (qk === 'A') {
                    stats.losses_abandoned++; saveIni(); deleteSavedGame();
                    gameEnded = true; keepPlaying = false; break;
                }
                drawBoard(); continue;  // C or anything else = cancel
            }
            gameEnded = true; keepPlaying = false; break;
        }
        if (inp === '+') { game.level = Math.min(10, game.level+1); saveIni(); drawBoard(); continue; }
        if (inp === '-') { game.level = Math.max(1,  game.level-1); saveIni(); drawBoard(); continue; }
        if (inp === 'S') { showSettings(); drawBoard(); continue; }
        if (inp === 'I') { showInstructions(); drawBoard(); continue; }
        if (inp === 'P') { exportPGN(); drawBoard(); continue; }
        if (inp === 'U') {
            if (!allowUndo) {
                showStatus("Undo is disabled. Enable it in Settings (S).", "1;31");
                mswait(900); clearStatus(); continue;
            }
            var playerMoves = 0;
            for (var uhi = 0; uhi < game.history.length; uhi++)
                if (game.history[uhi].color === game.playerColor) playerMoves++;
            if (playerMoves === 0) {
                showStatus("Nothing to undo.", "1;33"); mswait(700); clearStatus(); continue;
            }
            // Undo 2 half-moves (player's last move + AI response), or 1 if only 1 exists
            var toUndo = Math.min(2, game.stateStack.length);
            for (var ui2 = 0; ui2 < toUndo; ui2++) game.undoMove();
            drawBoard(); continue;
        }
        if (inp === 'R') {
            var rk = readHotspotKey("\x01H\x01YResign?  \x01H\x01Y@`(Y)`Y@\x01Nes  \x01H\x01Y@`(N)`N@\x01No", STATUS_ROW);
            if (rk === 'Y') {
                stats.losses_resign++; saveIni();
                gameResult = "You resigned."; gameResultColor = "1;31";
                deleteSavedGame(); gameEnded = true; break;
            }
            drawBoard(); continue;
        }
        if (inp === 'O') {
            var ok = readHotspotKey("\x01H\x01YOffer draw?  \x01H\x01Y@`(Y)`Y@\x01Nes  \x01H\x01Y@`(N)`N@\x01No", STATUS_ROW);
            if (ok !== 'Y') { drawBoard(); continue; }
            var accepts;
            if (game.level === 1) {
                accepts = (Math.random() < 0.5);
            } else {
                showStatus("Stockfish is considering...", "0;36");
                var score = getStockfishScore();
                // score is from the side to move (player's) perspective.
                // SF accepts if it isn't winning decisively (score >= -100cp).
                accepts = (score >= -100);
            }
            if (accepts) {
                stats.draws_agreed++; saveIni();
                gameResult = "Draw by mutual agreement."; gameResultColor = "1;33";
                deleteSavedGame(); gameEnded = true; break;
            } else {
                showStatus("Stockfish declines the draw! Play on.", "1;31");
                mswait(1500); drawBoard(); continue;
            }
        }

        var result = game.parseMove(inp);
        if (result && result.ambiguous) {
            var cands = result.candidates;
            var msg = "Which " + result.type + "? (";
            for (var ci = 0; ci < cands.length; ci++) {
                if (ci > 0) msg += "/";
                msg += game.toAlgebraic(cands[ci].r, cands[ci].c);
            }
            msg += "):";
            showStatus(msg, "1;33");
            console.gotoxy(1, INPUT_ROW);
            console.print("\x1b[1;37mDisambiguate (e.g. Ra1): \x1b[K");
            var dis = console.getstr(8, K_NOCRLF);
            result = dis.match(/^[a-h][1-8]$/i)
                ? game.parseMove(inp.charAt(0) + dis)
                : game.parseMove(dis);
        }
        if (!result || result.ambiguous) {
            showStatus("Invalid: " + inp, "1;31"); mswait(700); clearStatus(); continue;
        }
        doPlayerMove(result.f, result.t);
    }

    // ── End-of-game options ───────────────────────────────────────────────────
    if (keepPlaying && !console.aborted) {
        var pa;
        while (true) {
            showStatus(gameResult, gameResultColor);
            pa = readHotspotKey(
                "\x01H\x01Y@`(P)`P@\x01CGN   \x01H\x01Y@`(A)`A@\x01Cgain   \x01H\x01Y@`(Q)`Q@\x01Cuit",
                INPUT_ROW
            );
            if (pa === 'P') { exportPGN(); continue; }
            if (pa === 'A' || pa === 'Q') break;
        }
        keepPlaying = (pa === 'A');
    }
}

mouseoff();
console.gotoxy(1, INPUT_ROW);
console.print("\x1b[0mThanks for playing SynChess!  \x1b[K");
mswait(1200);
console.clear();
