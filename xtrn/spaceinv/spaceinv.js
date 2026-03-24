"use strict";
load("cterm_lib.js");

// ── Layout constants ──────────────────────────────────────────────────────────
var COLS      = 80,  ROWS     = 24;
var SPRITE_W  = 3,   SPRITE_H = 1;   // 24×16 px sprites (half-size)
var INV_GAP   = 1;
var INV_STEP  = SPRITE_W + INV_GAP;  // 4
var INV_COLS  = 11,  INV_ROWS = 5;
var INV_GRID_W = INV_COLS * INV_STEP - INV_GAP;  // 43
var INV_X0    = COLS - INV_GRID_W + 1;            // 38 — right-aligned

var SCORE_ROW  = 1;
var UFO_ROW    = 2;
var INV_Y0     = 4;                 // row 4 = just under UFO row
var SHIELD_ROW = 17;
var PLAYER_ROW = 21;
var GROUND_ROW = 23;
var STATUS_ROW = 24;
var DANGER_ROW = 16;                // game over when any invader top reaches row 16

var SHIELD_COLS = [14, 30, 46, 62];
var SHIELD_W    = 5,  SHIELD_H = 3;
// Classic arch/bunker shape: .XXX. / XXXXX / XX.XX
var SHIELD_MASK = [
    [0,1,1,1,0],
    [1,1,1,1,1],
    [1,1,0,1,1],
];

// ── Sprite sheet Y-offsets (SH=16 px per sprite) ─────────────────────────────
var SP = {
    SQUID_A: 0,   SQUID_B: 16,
    CRAB_A:  32,  CRAB_B:  48,
    OCTO_A:  64,  OCTO_B:  80,
    PLAYER:  96,
    UFO:     112,
    EXPLODE: 128,
};
var INV_SPRITE = [
    [SP.SQUID_A, SP.SQUID_B],
    [SP.CRAB_A,  SP.CRAB_B ],
    [SP.OCTO_A,  SP.OCTO_B ],
];
var INV_ANSI = [
    "\x1b[1;37m<^>",
    "\x1b[1;32m[H]",
    "\x1b[1;33m(O)",
];
var INV_PTS = [30, 20, 10];
var ROW_TYPE = [0, 0, 1, 2, 2];

// ── Pixel sizes ───────────────────────────────────────────────────────────────
var charPixW = 8, charPixH = 16;

// ── JXL ──────────────────────────────────────────────────────────────────────
var jxlOk  = supports_jpegxl();
var imgDir = js.exec_dir + "images/";

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

function loadSprites() {
    if (!jxlOk) return;
    if (uploadToCache(imgDir + "spaceinv_sprites.jxl", "spaceinv_sprites.jxl"))
        console.write("\x1b_SyncTERM:C;LoadJXL;B=1;spaceinv_sprites.jxl\x1b\\");
    else
        jxlOk = false;
    if (jxlOk)
        uploadToCache(imgDir + "spaceinv_title.jxl", "spaceinv_title.jxl");
}

// ── Timing ───────────────────────────────────────────────────────────────────
function ms() { return (new Date()).getTime(); }

// ── Frame buffer ──────────────────────────────────────────────────────────────
// Batch all per-tick rendering into one console.write() call → one TCP packet.
// This eliminates the jitter caused by many small writes each triggering a send.
var _fb = "";
function fw(s)        { _fb += s; }
function ff()         { if (_fb.length) { console.write(_fb); _fb = ""; } }
function fgoto(c, r)  { fw("\x1b[" + r + ";" + c + "H"); }

// ── ANSI march tones (SyncTERM ESC[| music, background, no screen corruption) ─
// Cycle of 4 notes approximating the classic Space Invaders march rhythm.
// \x1b[| enters SyncTERM music mode; MB = background (non-blocking);
// L32 = 32nd-note (very short); \x0f terminates the music string.
// Terminator must be \x0e (decimal 14) — cterm.c line 7009: if(ch[0]==14) play_music()
var MARCH_PITCHES = ["O1B", "O1A", "O1G", "O1F"];
var marchStep     = 0;
var soundOn       = true;
function playMarch() {
    if (!soundOn) return;
    if (invAlive === 1) {
        // single alien: rapid high pip (L64 = very short 64th-note, O5A alternating O5E)
        var pip = (marchStep % 2) ? "O5E" : "O5A";
        console.write("\x1b[|MBL64" + pip + "\x0e");
        marchStep = (marchStep + 1) % 4;
        return;
    }
    if (invMoveMs < 50) return;          // too fast — skip to avoid lag & overlap
    var len = invMoveMs >= 120 ? "L16" : "L32";   // L16=~120ms, L32=~60ms
    console.write("\x1b[|MB" + len + MARCH_PITCHES[marchStep] + "\x0e");
    marchStep = (marchStep + 1) % 4;
}

// ── Global hi-score leaderboard ───────────────────────────────────────────────
var SCORES_PATH = js.exec_dir + "spaceinv_scores.dat";
var MAX_SCORES  = 10;
var hiscores    = [];   // [{name, score}] sorted desc

function loadHiScores() {
    hiscores = [];
    var f = new File(SCORES_PATH);
    if (!f.open("r")) return;
    var lines = f.readAll();
    f.close();
    for (var i = 0; i < lines.length; i++) {
        var parts = lines[i].split("\t");
        if (parts.length >= 2) {
            var sc = parseInt(parts[0], 10);
            var nm = parts[1].trim();
            if (!isNaN(sc) && sc > 0 && nm) hiscores.push({ name: nm, score: sc });
        }
    }
    hiscores.sort(function(a, b) { return b.score - a.score; });
    if (hiscores.length > MAX_SCORES) hiscores.length = MAX_SCORES;
}

function saveHiScores() {
    var f = new File(SCORES_PATH);
    if (!f.open("w")) return;
    for (var i = 0; i < hiscores.length; i++)
        f.writeln(hiscores[i].score + "\t" + hiscores[i].name);
    f.close();
}

function submitScore(sc) {
    if (isGuest || sc <= 0) return;
    loadHiScores();
    var nm = user.alias || user.name || "Unknown";
    hiscores.push({ name: nm, score: sc });
    hiscores.sort(function(a, b) { return b.score - a.score; });
    if (hiscores.length > MAX_SCORES) hiscores.length = MAX_SCORES;
    saveHiScores();
}

function personalBest() {
    if (isGuest) return 0;
    var nm  = user.alias || user.name || "";
    var best = 0;
    for (var i = 0; i < hiscores.length; i++)
        if (hiscores[i].name === nm && hiscores[i].score > best)
            best = hiscores[i].score;
    return best;
}

function showHiScoreChart() {
    loadHiScores();
    console.clear();
    var W = "\x1b[1;37m", Y = "\x1b[1;33m", C = "\x1b[1;36m",
        G = "\x1b[1;32m", D = "\x1b[0m";

    var title = " SPACE INVADERS  -  HIGH SCORES ";
    var bar   = "+";
    for (var i = 0; i < 38; i++) bar += "-";
    bar += "+";

    console.gotoxy(21,  3); console.print(Y + bar + D);
    console.gotoxy(21,  4); console.print(Y + "|" + W + centre2(title, 38) + Y + "|" + D);
    console.gotoxy(21,  5); console.print(Y + bar + D);
    console.gotoxy(21,  6); console.print(Y + "|" + C + "  #   SCORE      NAME              " + Y + "|" + D);
    console.gotoxy(21,  7); console.print(Y + "|" + W + "  -   ------     ----              " + Y + "|" + D);

    var myName = user.alias || user.name || "";
    for (var r = 0; r < MAX_SCORES; r++) {
        var row = 8 + r;
        console.gotoxy(21, row);
        if (r < hiscores.length) {
            var hs   = hiscores[r];
            var rank = padL(r + 1, 3);
            var sc   = padL(hs.score, 8);
            var nm   = hs.name.substring(0, 18);
            var col  = (hs.name === myName) ? Y : W;
            console.print(Y + "|" + col + rank + "  " + sc + "   " + nm);
            // pad to column 59
            var used = 3 + 2 + 8 + 3 + nm.length;
            for (var p2 = used; p2 < 37; p2++) console.print(" ");
            console.print(Y + "|" + D);
        } else {
            console.print(Y + "|" + W + "                                      " + Y + "|" + D);
        }
    }

    console.gotoxy(21, 18); console.print(Y + bar + D);
    console.gotoxy(21, 20); console.print(W + centre("Press any key to continue") + D);

    console.inkey(0, 0); // drain
    console.ctrlkey_passthru = -1;
    while (!console.aborted && bbs.online) {
        if (console.inkey(0, 100) !== '') break;
    }
}

// Centre s in a field of width w
function centre2(s, w) {
    var pad = Math.max(0, Math.floor((w - s.length) / 2));
    var r = '';
    for (var i = 0; i < pad; i++) r += ' ';
    r += s;
    while (r.length < w) r += ' ';
    return r;
}

// ── Per-user / session state ──────────────────────────────────────────────────
var isGuest = user.compare_ars("GUEST");
var score, lives, level;

// ── Game state ────────────────────────────────────────────────────────────────
var invGrid, invFrame, invOffX, invOffY, invDirX, invAlive, invMoveMs;
var playerCol, bullet, invBullets, shields, ufo, explosions, timers;

function invSpeed() {
    // Authentic: each invader removed speeds movement linearly.
    // At 55 alive = 960ms, at 1 alive = 20ms; cap at 960.
    // Level multiplier: each level adds 15% speed (divides interval).
    var ms = Math.max(20, Math.min(960, invAlive * 20));
    return Math.max(20, Math.floor(ms / (1 + (level - 1) * 0.15)));
}

function initLevel() {
    invGrid = [];
    for (var gi = 0; gi < INV_ROWS; gi++) {
        invGrid[gi] = [];
        for (var gj = 0; gj < INV_COLS; gj++)
            invGrid[gi][gj] = { alive: true, type: ROW_TYPE[gi] };
    }
    invFrame  = 0;  invOffX = 0;  invOffY = 0;
    invDirX   = -1;   // right-aligned start → move left first
    invAlive  = INV_ROWS * INV_COLS;
    invMoveMs = invSpeed();

    playerCol  = 1;   // start far left
    bullet     = null;  invBullets = [];  ufo = null;  explosions = [];

    shields = [];
    for (var si = 0; si < SHIELD_COLS.length; si++) {
        var cells = [];
        for (var sr = 0; sr < SHIELD_H; sr++)
            for (var sc = 0; sc < SHIELD_W; sc++)
                cells.push(SHIELD_MASK[sr][sc] ? true : false);
        shields.push({ col: SHIELD_COLS[si], cells: cells });
    }

    var t = ms();
    timers = {
        bullet: 0, invBullet: 0, invMove: 0,
        ufoSpawn: t + 12000 + Math.floor(Math.random() * 12000),
        ufoMove:  0,
    };
}

function initGame() { score = 0; lives = 3; level = 1; initLevel(); }

// ── Low-level rendering ───────────────────────────────────────────────────────
function blackOut(col, row, w, h) {
    var blank = "\x1b[40m";
    for (var c = 0; c < w; c++) blank += " ";
    for (var r = 0; r < h; r++) { fgoto(col, row + r); fw(blank); }
}

function pasteSpr(sprY, dpx, dpy) {
    var sw = SPRITE_W * charPixW;
    var sh = SPRITE_H * charPixH;
    fw("\x1b_SyncTERM:P;Paste;SY=" + sprY + ";SW=" + sw + ";SH=" + sh +
       ";DX=" + dpx + ";DY=" + dpy + ";B=1\x1b\\");
}

function cpx(col) { return (col - 1) * charPixW; }
function cpy(row) { return (row - 1) * charPixH; }

// ── Invader position ──────────────────────────────────────────────────────────
function invPos(gi, gj) {
    return { col: INV_X0 + invOffX + gj * INV_STEP,
             row: INV_Y0 + invOffY + gi * SPRITE_H };
}

// ── Draw objects ──────────────────────────────────────────────────────────────
function drawInvader(gi, gj) {
    var inv = invGrid[gi][gj];
    if (!inv.alive) return;
    var p = invPos(gi, gj);
    if (jxlOk) {
        blackOut(p.col, p.row, SPRITE_W, SPRITE_H);
        pasteSpr(INV_SPRITE[inv.type][invFrame], cpx(p.col), cpy(p.row));
    } else {
        fgoto(p.col, p.row);
        fw(INV_ANSI[inv.type] + "\x1b[0m");
    }
}

function eraseInvader(gi, gj) {
    var p = invPos(gi, gj);
    blackOut(p.col, p.row, SPRITE_W, SPRITE_H);
}

function drawPlayer() {
    if (jxlOk) {
        blackOut(playerCol, PLAYER_ROW, SPRITE_W, SPRITE_H);
        pasteSpr(SP.PLAYER, cpx(playerCol), cpy(PLAYER_ROW));
    } else {
        fgoto(playerCol, PLAYER_ROW);
        fw("\x1b[1;36m/^\\\x1b[0m");
    }
}

function erasePlayer() { blackOut(playerCol, PLAYER_ROW, SPRITE_W, SPRITE_H); }

function drawUFO() {
    if (!ufo) return;
    if (jxlOk) {
        blackOut(ufo.col, UFO_ROW, SPRITE_W, SPRITE_H);
        pasteSpr(SP.UFO, cpx(ufo.col), cpy(UFO_ROW));
    } else {
        fgoto(ufo.col, UFO_ROW);
        fw("\x1b[1;31m<^>\x1b[0m");
    }
}

function eraseUFO() {
    if (!ufo) return;
    blackOut(ufo.col, UFO_ROW, SPRITE_W, SPRITE_H);
}

function drawShields() {
    for (var si = 0; si < shields.length; si++) {
        var s = shields[si];
        for (var sr = 0; sr < SHIELD_H; sr++) {
            for (var sc = 0; sc < SHIELD_W; sc++) {
                if (!SHIELD_MASK[sr][sc]) continue;
                var idx = sr * SHIELD_W + sc;
                fgoto(s.col + sc, SHIELD_ROW + sr);
                fw(s.cells[idx] ? "\x1b[1;32m\xdb\x1b[0m" : "\x1b[40m ");
            }
        }
    }
}

function drawBullet(b) {
    var ch, color;
    if (b.type === undefined) {
        ch = "\xBA";  color = "1;37";            // player — white double bar
    } else if (b.type === 0) {
        ch = b.frame ? "/" : "\\";  color = "1;31"; // squiggly — red zigzag
    } else {
        ch = b.frame ? "\xCA" : "\xCB";  color = "1;33"; // plunger — yellow cross
    }
    fgoto(b.col, b.row);
    fw("\x1b[" + color + "m" + ch + "\x1b[0m");
}

function eraseAt(col, row) { fgoto(col, row); fw("\x1b[40m "); }

function drawExplodeAt(col, row) {
    if (jxlOk) {
        blackOut(col, row, SPRITE_W, SPRITE_H);
        pasteSpr(SP.EXPLODE, cpx(col), cpy(row));
    } else {
        fgoto(col, row); fw("\x1b[1;33m*\x1b[0m");
    }
}

function drawScore() {
    var pb = personalBest();
    var hi = Math.max(pb, p1Score, p2Score, score);
    var s1 = (currentPlayer === 1) ? score : p1Score;
    var s2 = (currentPlayer === 2) ? score : p2Score;
    var W = "\x1b[1;37m", C = "\x1b[1;36m", Y = "\x1b[1;33m";
    // Visible text: "SCORE<1> 000000  HI-SCORE 000000  SCORE<2> 000000" = 50 chars
    var plain = "SCORE<1> " + padL(s1,6) + "  HI-SCORE " + padL(hi,6) + "  SCORE<2> " + padL(s2,6);
    var pad   = Math.floor((COLS - plain.length) / 2);
    var sp    = ""; for (var i = 0; i < pad; i++) sp += " ";
    var colored = C+"SCORE"+W+"<1> "+W+padL(s1,6)+
                  "  "+C+"HI-SCORE "+Y+padL(hi,6)+
                  "  "+C+"SCORE"+W+"<2> "+W+padL(s2,6);
    fgoto(1, SCORE_ROW);
    fw(sp + colored + "\x1b[K\x1b[0m");
}

function drawLives() {
    fgoto(1, STATUS_ROW);
    var s = "\x1b[1;36mLIVES:";
    for (var i = 0; i < lives; i++) s += "\x1b[1;37m \xdb";
    fw(s + "\x1b[K\x1b[0m");
}

function drawGround() {
    fgoto(1, GROUND_ROW);
    var g = "\x1b[1;32m";
    for (var i = 0; i < COLS; i++) g += "\xdf";
    fw(g + "\x1b[0m");
}

function drawAll() {
    console.clear();
    drawScore(); drawGround(); drawLives(); drawShields();
    for (var gi = 0; gi < INV_ROWS; gi++)
        for (var gj = 0; gj < INV_COLS; gj++)
            if (invGrid[gi][gj].alive) drawInvader(gi, gj);
    drawPlayer();
    ff();
}

function padL(n, w) { var s = String(n); while (s.length < w) s = " " + s; return s; }

// ── Invader movement ──────────────────────────────────────────────────────────
function moveInvaders(t) {
    if (t - timers.invMove < invMoveMs) return;
    timers.invMove = t;

    for (var gi = 0; gi < INV_ROWS; gi++)
        for (var gj = 0; gj < INV_COLS; gj++)
            if (invGrid[gi][gj].alive) eraseInvader(gi, gj);

    invFrame = 1 - invFrame;

    var leftGj = INV_COLS, rightGj = -1;
    for (var gi2 = 0; gi2 < INV_ROWS; gi2++) {
        for (var gj2 = 0; gj2 < INV_COLS; gj2++) {
            if (!invGrid[gi2][gj2].alive) continue;
            if (gj2 < leftGj)  leftGj  = gj2;
            if (gj2 > rightGj) rightGj = gj2;
        }
    }

    var newOffX = invOffX + invDirX;
    var lPx = INV_X0 + newOffX + leftGj  * INV_STEP;
    var rPx = INV_X0 + newOffX + rightGj * INV_STEP + SPRITE_W - 1;
    if (lPx < 1 || rPx > COLS) {
        newOffX = invOffX;
        invOffY++;
        invDirX = -invDirX;
    }
    invOffX   = newOffX;
    invMoveMs = invSpeed();

    for (var gi3 = 0; gi3 < INV_ROWS; gi3++) {
        for (var gj3 = 0; gj3 < INV_COLS; gj3++) {
            if (!invGrid[gi3][gj3].alive) continue;
            var p = invPos(gi3, gj3);
            for (var r = 0; r < SPRITE_H; r++) {
                var row = p.row + r;
                var shRow = row - SHIELD_ROW;
                if (shRow < 0 || shRow >= SHIELD_H) continue;
                for (var c = 0; c < SPRITE_W; c++) {
                    var col = p.col + c;
                    for (var si2 = 0; si2 < shields.length; si2++) {
                        var sh = shields[si2];
                        var shCol = col - sh.col;
                        if (shCol >= 0 && shCol < SHIELD_W && SHIELD_MASK[shRow][shCol])
                            sh.cells[shRow * SHIELD_W + shCol] = false;
                    }
                }
            }
            drawInvader(gi3, gj3);
        }
    }

    playMarch();
}

// ── Bullets ───────────────────────────────────────────────────────────────────
function movePlayerBullet(t) {
    if (!bullet) return;
    // 240 px/sec (4× invader speed), charPixH=16 → 1 row per 67 ms
    if (t - timers.bullet < 67) return;
    timers.bullet = t;
    var prevRow = bullet.row;
    bullet.row--;
    if (bullet.row < 1) { eraseAt(bullet.col, prevRow); bullet = null; return; }
    drawBullet(bullet);          // draw new position first — no flicker gap
    eraseAt(bullet.col, prevRow); // then wipe old position
}

function moveInvaderBullets(t) {
    // 60 px/sec, charPixH=16 px/row → 1 row per 267 ms
    if (t - timers.invBullet < 267) return;
    timers.invBullet = t;
    for (var i = invBullets.length - 1; i >= 0; i--) {
        var b = invBullets[i];
        var prevRow = b.row;
        b.row++;
        b.frame = (b.frame + 1) % 2;
        if (b.row >= GROUND_ROW) { eraseAt(b.col, prevRow); invBullets.splice(i, 1); continue; }
        drawBullet(b);            // draw new position first — no flicker gap
        eraseAt(b.col, prevRow);  // then wipe old position
    }
    var fireChance = 0.03 + (1 - invAlive / (INV_ROWS * INV_COLS)) * 0.12;
    if (invBullets.length < 3 && invAlive > 0 && Math.random() < fireChance)
        fireInvaderBullet();
}

function fireInvaderBullet() {
    var cols = [];
    for (var gj = 0; gj < INV_COLS; gj++)
        for (var gi = INV_ROWS - 1; gi >= 0; gi--)
            if (invGrid[gi][gj].alive) { cols.push({ gi: gi, gj: gj }); break; }
    if (!cols.length) return;
    var pick = cols[Math.floor(Math.random() * cols.length)];
    var p    = invPos(pick.gi, pick.gj);
    // squid(0) and octopus(2) fire squiggly (type 0); crab(1) fires plunger (type 1)
    var btype = (invGrid[pick.gi][pick.gj].type === 1) ? 1 : 0;
    invBullets.push({ col: p.col + 2, row: p.row + SPRITE_H, type: btype, frame: 0 });
}

// ── UFO ───────────────────────────────────────────────────────────────────────
function updateUFO(t) {
    if (!ufo) {
        if (t < timers.ufoSpawn) return;
        ufo = { col: 1, points: (Math.floor(Math.random() * 6) + 1) * 50 };
        timers.ufoMove  = t;
        timers.ufoSpawn = t + 15000 + Math.floor(Math.random() * 15000);
        drawUFO();
        return;
    }
    if (t - timers.ufoMove < 65) return;
    timers.ufoMove = t;
    eraseUFO();
    ufo.col++;
    if (ufo.col + SPRITE_W > COLS) { ufo = null; return; }
    drawUFO();
}

// ── Collisions ────────────────────────────────────────────────────────────────
function checkCollisions() {
    // Invasion check first — if aliens have reached the danger line, immediate game over
    for (var gi4 = 0; gi4 < INV_ROWS; gi4++)
        for (var gj4 = 0; gj4 < INV_COLS; gj4++)
            if (invGrid[gi4][gj4].alive && invPos(gi4, gj4).row >= DANGER_ROW)
                return 'gameover';

    if (bullet) {
        for (var gi = 0; gi < INV_ROWS; gi++) {
            for (var gj = 0; gj < INV_COLS; gj++) {
                if (!invGrid[gi][gj].alive) continue;
                var p = invPos(gi, gj);
                if (bullet.col >= p.col && bullet.col < p.col + SPRITE_W &&
                    bullet.row >= p.row && bullet.row < p.row + SPRITE_H) {
                    eraseAt(bullet.col, bullet.row);
                    bullet = null;
                    eraseInvader(gi, gj);
                    invGrid[gi][gj].alive = false;
                    invAlive--;
                    score += INV_PTS[invGrid[gi][gj].type] * level;
                    drawScore();
                    drawExplodeAt(p.col, p.row);

                    explosions.push({ col: p.col, row: p.row, expireAt: ms() + 180 });
                    invMoveMs = invSpeed();
                    return null;
                }
            }
        }
        if (ufo && bullet.col >= ufo.col && bullet.col < ufo.col + SPRITE_W &&
                   bullet.row >= UFO_ROW  && bullet.row < UFO_ROW + SPRITE_H) {
            score += ufo.points * level;
            drawScore();
            eraseAt(bullet.col, bullet.row);
            bullet = null;
            drawExplodeAt(ufo.col, UFO_ROW);

            explosions.push({ col: ufo.col, row: UFO_ROW, expireAt: ms() + 220 });
            ufo = null;
            return null;
        }
        var bsr = bullet.row - SHIELD_ROW;
        if (bsr >= 0 && bsr < SHIELD_H) {
            for (var si = 0; si < shields.length; si++) {
                var sh = shields[si];
                var bsc = bullet.col - sh.col;
                if (bsc >= 0 && bsc < SHIELD_W && SHIELD_MASK[bsr][bsc]) {
                    var ci = bsr * SHIELD_W + bsc;
                    if (sh.cells[ci]) {
                        sh.cells[ci] = false;
                        console.gotoxy(sh.col + bsc, bullet.row);
                        console.print("\x1b[40m ");
                        eraseAt(bullet.col, bullet.row);
                        bullet = null;
                        return null;
                    }
                }
            }
        }
    }

    for (var bi = invBullets.length - 1; bi >= 0; bi--) {
        var b = invBullets[bi];
        if (b.col >= playerCol && b.col < playerCol + SPRITE_W &&
            b.row >= PLAYER_ROW && b.row < PLAYER_ROW + SPRITE_H) {
            eraseAt(b.col, b.row);
            invBullets.splice(bi, 1);
            playerHit();
            return null;
        }
        var ibsr = b.row - SHIELD_ROW;
        if (ibsr >= 0 && ibsr < SHIELD_H) {
            for (var sj = 0; sj < shields.length; sj++) {
                var sh2 = shields[sj];
                var ibsc = b.col - sh2.col;
                if (ibsc >= 0 && ibsc < SHIELD_W && SHIELD_MASK[ibsr][ibsc]) {
                    var cj = ibsr * SHIELD_W + ibsc;
                    if (sh2.cells[cj]) {
                        sh2.cells[cj] = false;
                        console.gotoxy(sh2.col + ibsc, b.row);
                        console.print("\x1b[40m ");
                        eraseAt(b.col, b.row);
                        invBullets.splice(bi, 1);
                        break;
                    }
                }
            }
        }
    }

    return null;
}

// ── Player death ──────────────────────────────────────────────────────────────
function playerHit() {
    heldKey = null;   // stop any queued movement before and after death
    erasePlayer();
    drawExplodeAt(playerCol, PLAYER_ROW);
    mswait(700);
    blackOut(playerCol, PLAYER_ROW, SPRITE_W, SPRITE_H);
    lives--;
    drawLives();
    if (lives <= 0) return;
    mswait(500);
    bullet    = null;
    heldKey   = null;   // clear again after pause — drain any keys pressed during wait
    console.clearkeybuffer();
    drawPlayer();
}

function handleExplosions(t) {
    for (var i = explosions.length - 1; i >= 0; i--) {
        if (t > explosions[i].expireAt) {
            blackOut(explosions[i].col, explosions[i].row, SPRITE_W, SPRITE_H);
            explosions.splice(i, 1);
        }
    }
}

// ── Input — two-phase key-hold movement ──────────────────────────────────────
// Phase 1 (before OS repeat fires, ~0-300ms): our auto-repeat bridges the gap.
//   Wide release window so we don't stop during the OS initial-repeat silence.
// Phase 2 (OS repeat detected): stop auto-repeating, let OS events drive movement.
//   Tight release window so ship stops the moment you let go.
var heldKey    = null;
var lastKeyAt  = 0;
var lastMoveAt = 0;
var osRepeat   = false;   // true once OS key-repeat rate is detected

var REPEAT_MS    = 120;   // phase-1 auto-repeat interval
var RELEASE_INIT = 450;   // phase-1 release timeout (> OS initial gap ~300ms)
var RELEASE_OS   = 80;    // phase-2 release timeout (tight)

function readKey() {
    console.ctrlkey_passthru = -1;
    var ch = console.inkey(0, 0);
    if (!ch) return null;
    if (ch === '\x1b') {
        var n1 = console.inkey(0, 30);
        if (n1 === '[') {
            var n2 = console.inkey(0, 30);
            if (n2 === 'D') return 'LEFT';
            if (n2 === 'C') return 'RIGHT';
        }
        return null;
    }
    return ch.toUpperCase();
}

function movePlayer(dir) {
    if (dir === 'LEFT'  && playerCol > 1)
        { erasePlayer(); playerCol--; drawPlayer(); }
    else if (dir === 'RIGHT' && playerCol + SPRITE_W <= COLS)
        { erasePlayer(); playerCol++; drawPlayer(); }
}

// Returns 'quit', 'pause', or null
function processInput(t) {
    var key = readKey();
    var dk  = null;

    if      (key === 'LEFT'  || key === 'A' || key === 'Z') dk = 'LEFT';
    else if (key === 'RIGHT' || key === 'D' || key === 'X') dk = 'RIGHT';

    if (dk) {
        // Detect OS repeat: same direction, event arrived quickly (OS repeat ~30ms)
        if (dk === heldKey && (t - lastKeyAt) < 200) {
            osRepeat = true;
        } else {
            osRepeat = false;  // fresh press or direction change
        }
        heldKey    = dk;
        lastKeyAt  = t;
        lastMoveAt = t;
        movePlayer(dk);
    } else if (key) {
        heldKey = null; osRepeat = false;
        if (key === 'Q') return 'quit';
        if (key === 'P') return 'pause';
        if ((key === ' ' || key === '\r') && !bullet) {
            bullet = { col: playerCol + Math.floor(SPRITE_W / 2), row: PLAYER_ROW - 1 };
            drawBullet(bullet);
        }
    } else if (heldKey) {
        var releaseMs = osRepeat ? RELEASE_OS : RELEASE_INIT;
        if (t - lastKeyAt > releaseMs) {
            heldKey = null; osRepeat = false;
        } else if (!osRepeat && t - lastMoveAt >= REPEAT_MS) {
            // phase 1 only: bridge the OS initial-gap with our own repeat
            movePlayer(heldKey);
            lastMoveAt = t;
        }
    }
    return null;
}

// ── String helpers ────────────────────────────────────────────────────────────
function centre(s) {
    var pad = Math.max(0, Math.floor((COLS - s.length) / 2));
    var r = ''; for (var i = 0; i < pad; i++) r += ' '; return r + s;
}

// ── Title screen ──────────────────────────────────────────────────────────────
function showTitle() {
    loadHiScores();
    console.clear();
    var Wh = "\x1b[1;37m", C = "\x1b[1;36m", G = "\x1b[1;32m",
        Y  = "\x1b[1;33m", D = "\x1b[0m";
    var bar = G + centre("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *");
    var sndRow, optRow, pbRow;

    if (jxlOk) {
        // JXL title banner (640×128 px = rows 1-8 at 16 px/row)
        console.write("\x1b_SyncTERM:C;DrawJXL;DX=0;DY=0;spaceinv_title.jxl\x1b\\");
        console.gotoxy(1, 10); console.print(bar + D);
        console.gotoxy(1, 12); console.print(Y  + centre("= UFO mystery   <^^> 30 pts   [HH] 20 pts   (OO) 10 pts") + D);
        console.gotoxy(1, 14); console.print(C  + centre("Controls:") + D);
        console.gotoxy(1, 15); console.print(D  + centre("Z/A/Left  Move left    X/D/Right  Move right    SPACE/ENTER  Fire"));
        console.gotoxy(1, 16); console.print(D  + centre("P  Pause    Q  Quit"));
        pbRow  = 18;
        sndRow = 20;
        optRow = 22;
    } else {
        console.gotoxy(1, 2);  console.print(bar);
        console.gotoxy(1, 4);  console.print(C + centre(" ___  ____  __   ___  __     ___  _  _  _  _  __   ___  ___  ___  ____ "));
        console.gotoxy(1, 5);  console.print(C + centre("/  _||  _ ||  \\ /  _||  \\   |_ _|| \\| || || ||  \\ /   ||  _||  __||  __|"));
        console.gotoxy(1, 6);  console.print(C + centre("\\_ \\ |  _/|  _/|  __|  _/    | | |  . || \\/ ||  _/\\_ _|| |__| |__ |  _/ "));
        console.gotoxy(1, 7);  console.print(C + centre("|___/|_|  |_/   \\___||_|     |_| |_|\\_| \\__/ |_|   |_|  \\___||____||_|  "));
        console.gotoxy(1, 9);  console.print(Wh + centre("for Synchronet BBS"));
        console.gotoxy(1, 11); console.print(bar);
        console.gotoxy(1, 13); console.print(Y  + centre("= UFO mystery   <^^> 30 pts   [HH] 20 pts   (OO) 10 pts"));
        console.gotoxy(1, 15); console.print(C  + centre("Controls:"));
        console.gotoxy(1, 16); console.print(D  + centre("Z / A / Left Arrow    Move left"));
        console.gotoxy(1, 17); console.print(D  + centre("X / D / Right Arrow   Move right"));
        console.gotoxy(1, 18); console.print(D  + centre("SPACE / ENTER         Fire"));
        console.gotoxy(1, 19); console.print(D  + centre("P  Pause    Q  Quit"));
        pbRow  = 21;
        sndRow = 22;
        optRow = 24;
    }

    var pb = personalBest();
    if (pb > 0) { console.gotoxy(1, pbRow); console.print(Y + centre("Your best score: " + pb) + D); }

    function drawSnd() {
        var s = soundOn ? G + "ON " + D : "\x1b[1;31mOFF\x1b[0m";
        console.gotoxy(1, sndRow);
        console.print(Wh + centre("Sound: " + s) + D);
    }
    drawSnd();

    console.gotoxy(1, optRow);
    console.print(Wh + centre("1 = One Player   2 = Two Players   H = Hi-Scores   S = Sound   Q = Quit") + D);

    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        var k = console.inkey(0, 100);
        if (!k) continue;
        if (k === '1' || k === ' ') return 1;
        if (k === '2') return 2;
        if (k.toUpperCase() === 'H') { showHiScoreChart(); return showTitle(); }
        if (k.toUpperCase() === 'S') { soundOn = !soundOn; drawSnd(); continue; }
        if (k.toUpperCase() === 'Q') return 0;
    }
    return 0;
}

// ── End-of-round screen ───────────────────────────────────────────────────────
function showEndScreen(won, canReplay) {
    if (canReplay === undefined) canReplay = true;
    submitScore(score);
    loadHiScores();

    var msg   = won ? " *** LEVEL CLEARED! *** " : " *** GAME  OVER *** ";
    var color = won ? "\x1b[1;32m" : "\x1b[1;31m";

    console.gotoxy(1,  9); console.print(color + centre(msg) + "\x1b[0m");
    console.gotoxy(1, 11); console.print("\x1b[1;37m" + centre("Score:     " + score) + "\x1b[0m");
    var pb = personalBest();
    if (pb > 0)
        { console.gotoxy(1, 12); console.print("\x1b[1;33m" + centre("Your best: " + pb) + "\x1b[0m"); }

    var opts = canReplay ? "H = Hi-Scores   SPACE = Play again   Q = Quit"
                         : "H = Hi-Scores   SPACE = Continue   Q = Quit";
    console.gotoxy(1, 14); console.print("\x1b[0;37m" + centre(opts) + "\x1b[0m");

    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        var k2 = console.inkey(0, 100);
        if (!k2) continue;
        k2 = k2.toUpperCase();
        if (k2 === 'H') { showHiScoreChart(); return showEndScreen(won); }
        if (k2 === 'Q') return 'quit';
        if (k2 === ' ') return won ? 'next' : 'restart';
    }
    return 'quit';
}

// ── Main game loop ────────────────────────────────────────────────────────────
function runLevel() {
    console.write("\x1b[?25l");   // hide cursor
    drawAll();
    heldKey = null;

    while (!console.aborted && bbs.online) {
        var t   = ms();
        var act = processInput(t);
        if (act === 'quit') { console.write("\x1b[?25h"); return 'quit'; }
        if (act === 'pause') {
            console.gotoxy(1, 12);
            console.print("\x1b[1;33m" + centre("--- PAUSED  (P to continue) ---") + "\x1b[0m");
            while (!console.aborted && bbs.online) {
                console.ctrlkey_passthru = -1;
                var pk = console.inkey(0, 100);
                if (!pk) continue;
                if (pk.toUpperCase() === 'P') break;
                if (pk.toUpperCase() === 'Q') { console.write("\x1b[?25h"); return 'quit'; }
            }
            console.clearkeybuffer();
            drawAll(); heldKey = null;
            continue;
        }

        movePlayerBullet(t);
        moveInvaderBullets(t);
        updateUFO(t);
        handleExplosions(t);

        var cr = checkCollisions();
        if (cr === 'gameover') { console.write("\x1b[?25h"); return 'gameover'; }
        if (lives <= 0)        { console.write("\x1b[?25h"); return 'gameover'; }
        if (invAlive === 0)    { console.write("\x1b[?25h"); return 'levelclear'; }

        moveInvaders(t);
        ff();       // flush frame buffer → single TCP packet per tick
        mswait(5);
    }
    console.write("\x1b[?25h");   // restore cursor
    return 'quit';
}

// ── Two-player helpers ────────────────────────────────────────────────────────
function showPlayerBanner(n) {
    console.clear();
    var Y = "\x1b[1;33m", W = "\x1b[1;37m", D = "\x1b[0m";
    console.gotoxy(1, 10); console.print(Y + centre("PLAYER " + n) + D);
    console.gotoxy(1, 12); console.print(W + centre("Press SPACE to begin") + D);
    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        var k = console.inkey(0, 100);
        if (!k) continue;
        if (k === ' ' || k === '\r') return true;
        if (k.toUpperCase() === 'Q') return false;
    }
    return false;
}

function showFinalScores(s1, s2) {
    console.clear();
    var W = "\x1b[1;37m", Y = "\x1b[1;33m", G = "\x1b[1;32m", D = "\x1b[0m";
    console.gotoxy(1,  8); console.print(Y + centre("═══ FINAL SCORES ═══") + D);
    console.gotoxy(1, 11); console.print(W + centre("Player 1:  " + s1) + D);
    console.gotoxy(1, 12); console.print(W + centre("Player 2:  " + s2) + D);
    console.gotoxy(1, 14);
    if (s1 > s2)      console.print(G + centre("Player 1 wins!") + D);
    else if (s2 > s1) console.print(G + centre("Player 2 wins!") + D);
    else              console.print(G + centre("It's a tie!") + D);
    console.gotoxy(1, 18); console.print(W + centre("Press any key") + D);
    while (!console.aborted && bbs.online) {
        console.ctrlkey_passthru = -1;
        if (console.inkey(0, 100)) break;
    }
}

// ── Bootstrap ─────────────────────────────────────────────────────────────────
// Query char pixel dimensions
(function() {
    var buf = "", ch;
    console.ctrlkey_passthru = -1;
    console.clearkeybuffer();
    console.write("\x1b[=3n");
    var deadline = 800;
    while ((ch = console.inkey(0, deadline)) !== '') {
        deadline = 60;
        if (ch === '\x1b') continue;
        buf += ch;
        if (ch === 'n') break;
    }
    var m = buf.match(/\[=(\d+);(\d+)n/);
    if (m) { charPixH = parseInt(m[1], 10); charPixW = parseInt(m[2], 10); }
    console.ctrlkey_passthru = 0;
})();

loadSprites();
loadHiScores();

var p1Score = 0, p2Score = 0, currentPlayer = 1;
var numPlayers  = showTitle();

if (numPlayers > 0) {
    // ── Player 1 ──────────────────────────────────────────────────────────────
    if (numPlayers === 2 && !showPlayerBanner(1)) numPlayers = 0;
}

if (numPlayers > 0) {
    currentPlayer = 1;
    var canReplay = (numPlayers === 1);
    initGame();
    var keepGoing = true;
    while (keepGoing && !console.aborted && bbs.online) {
        var result = runLevel();
        if (!bbs.online || console.aborted || result === 'quit') { keepGoing = false; numPlayers = 0; break; }

        if (result === 'levelclear') {
            level++;
            initLevel();
            drawAll();
        } else {
            p1Score = score;
            var nr2 = showEndScreen(false, canReplay);
            if (nr2 === 'quit') { keepGoing = false; numPlayers = 0; }
            else if (canReplay && nr2 === 'restart') { initGame(); }
            else { keepGoing = false; }
        }
    }
}

if (numPlayers === 2 && bbs.online && !console.aborted) {
    // ── Player 2 ──────────────────────────────────────────────────────────────
    if (showPlayerBanner(2)) {
        currentPlayer = 2;
        initGame();
        var keepGoing2 = true;
        while (keepGoing2 && !console.aborted && bbs.online) {
            var result2 = runLevel();
            if (!bbs.online || console.aborted || result2 === 'quit') { keepGoing2 = false; break; }

            if (result2 === 'levelclear') {
                level++;
                initLevel();
                drawAll();
            } else {
                p2Score = score;
                showEndScreen(false, false);
                keepGoing2 = false;
            }
        }
        if (bbs.online && !console.aborted) showFinalScores(p1Score, p2Score);
    }
}

console.ctrlkey_passthru = 0;
console.clear();
console.gotoxy(1, 12);
console.print("\x1b[1;36m" + centre("Thanks for playing Space Invaders on Synchronet!") + "\x1b[0m\r\n");
mswait(1500);
console.clear();
