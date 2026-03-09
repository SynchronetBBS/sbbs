// flight_sim.js — BBS Flight Simulator for Synchronet, written in JavaScript
// Mode 1: Side-scroll  — classic horizontal-scrolling, side-view plane
// Mode 2: 3D Perspective — forward-flying outside/behind view, banking horizon
// Controls: Arrow keys, Q to quit

// This was completely coded by Claude AI.
// Created by Eric Oulashin (AKA Nightfox).
//
// BBS: Digital Distortion
//  Addresses: digitaldistortionbbs.com
//             digdist.synchro.net

"use strict";

require("sbbsdefs.js", "K_NOCRLF");
require("key_defs.js", "KEY_UP");
require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
require("rip_lib.js", "RIPWindow");

// ═══════════════════════════════════════════════════════════
//  Program name & version
// ═══════════════════════════════════════════════════════════
var gProgramName = "FlightSim";
var gProgramVersion = "1.00";
var gProgramDate = "2026-03-09";


// ═══════════════════════════════════════════════════════════
//  WEATHER TYPE CONSTANTS
// ═══════════════════════════════════════════════════════════
var SUNNY        = 0;
var CLOUDY       = 1;
var RAINY        = 2;
var THUNDERSTORM = 3;

// ═══════════════════════════════════════════════════════════
//  SHARED SCREEN CONSTANTS
// ═══════════════════════════════════════════════════════════

var COLS       = Math.min(console.screen_columns || 80, 80);
var ROWS       = Math.min(console.screen_rows    || 24, 24);
var HUD_ROW    = 1;
var STATUS_ROW = ROWS;
var PLAY_TOP   = 2;
var PLAY_BOT   = ROWS - 1;      // 23
var CENTER_X   = Math.floor(COLS / 2);   // 40
var FRAME_MS   = 90;            // Target ms per frame

// ─── Side-scroll constants ───
var SS_HORIZON   = PLAY_TOP + Math.floor((PLAY_BOT - PLAY_TOP) * 0.64); // ~16
var SS_MAX_TERRH = Math.min(7, SS_HORIZON - PLAY_TOP - 3);
var PL_W  = 11; var PL_H  = 3; var PL_HW = 5;
var PL_MIN_X = PL_HW + 1;  var PL_MAX_X = COLS - PL_HW;
var PL_MIN_Y = PLAY_TOP;   var PL_MAX_Y = SS_HORIZON - PL_H - 1;

// ─── 3D perspective constants ───
var P3_FOCAL     = 12;    // Focal length (perspective strength)
var P3_BASE_H    = 13;    // Nominal horizon row at cruise altitude
var P3_BASE_ALT  = 2.5;   // Camera altitude at cruise (world units)
var P3_ALT_SCALE = 1.8;   // Screen rows of horizon shift per altitude unit
var P3_MAX_TILT  = 5;     // Max horizon tilt rows at screen edge (full roll)
var P3_DRAW_DIST = 85;    // Max draw distance (world units)

// ═══════════════════════════════════════════════════════════
//  CTRL-A COLOR CODES
// ═══════════════════════════════════════════════════════════

var CN  = "\x01n"; var CH  = "\x01h";
var CK  = "\x01k"; var CR  = "\x01r"; var CG  = "\x01g"; var CY  = "\x01y";
var CB  = "\x01b"; var CM  = "\x01m"; var CC  = "\x01c"; var CW  = "\x01w";
var BG0 = "\x010"; var BG1 = "\x011"; var BG2 = "\x012"; var BG3 = "\x013";
var BG4 = "\x014"; var BG5 = "\x015"; var BG6 = "\x016"; var BG7 = "\x017";

// Pre-composed attribute strings
var A_SKY_TOP  = CN+BG0+CB;       var A_SKY_MID  = CN+BG4+CB;
var A_SKY_BRIG = CN+BG4+CH+CB;   var A_SKY_HOR  = CN+BG4+CH+CC;
var A_GND_DARK = CN+BG2+CG;       var A_GND_BRIG = CN+BG2+CH+CG;
var A_ROCK     = CN+BG0+CH+CK;    var A_SNOW     = CN+BG7+CH+CW;
var A_CLOUD_E  = CN+BG4+CH+CW;    var A_CLOUD_M  = CN+BG7+CW;
var A_STAR     = CN+BG0+CH+CW;    var A_HUD      = CN+BG7+CH+CK;
var A_STATUS   = CN+BG4+CH+CW;
var A_PLANE_W  = CN+BG4+CH+CW;   // Wings
var A_PLANE_T  = CN+BG4+CH+CY;   // Tail / engine glow
var A_PLANE_B  = CN+BG4+CH+CC;   // Body

// ═══════════════════════════════════════════════════════════
//  CHARACTER CONSTANTS
// ═══════════════════════════════════════════════════════════

var CARET_CHAR = "^";

// ═══════════════════════════════════════════════════════════
//  SCREEN BUFFER  (diff-based rendering — shared by both modes)
// ═══════════════════════════════════════════════════════════

var cur_buf = [];
var prv_buf = [];

// Allocate (or reset) the double-buffered screen arrays.
// cur_buf holds the frame being built; prv_buf holds the last rendered frame.
// Every cell is a two-element array: [attribute-string, character].
// prv_buf cells are initialised to sentinel values so every cell is dirty on
// the first renderFrame() call, forcing a full repaint.
function initBuffers() {
	var blank = CN+BG0+CK;
	cur_buf = []; prv_buf = [];
	for (var r = 0; r < ROWS; r++) {
		cur_buf.push([]); prv_buf.push([]);
		for (var c = 0; c < COLS; c++) {
			cur_buf[r].push([blank, " "]);
			prv_buf[r].push(["INIT", "X"]);
		}
	}
}

// Write a single character and its colour attribute into the current frame buffer.
// Row and column are 1-based; out-of-bounds writes are silently ignored.
function putCell(row, col, attr, ch) {
	if (row < 1 || row > ROWS || col < 1 || col > COLS) return;
	var cell = cur_buf[row-1][col-1]; cell[0] = attr; cell[1] = ch;
}
// Fill a rectangular region of the current frame buffer with a solid attr+char.
function fillRect(r1, c1, r2, c2, attr, ch) {
	for (var r = r1; r <= r2; r++)
		for (var c = c1; c <= c2; c++) putCell(r, c, attr, ch);
}
// Write a string into the current frame buffer, one character per cell.
// All characters share the same attribute; col is 1-based.
function writeStr(row, col, attr, str) {
	for (var i = 0; i < str.length; i++) putCell(row, col+i, attr, str.charAt(i));
}

// Send only changed cells to terminal
function renderFrame() {
	for (var r = 0; r < ROWS; r++) {
		var rs = -1, rstr = "", rla = "";
		for (var c = 0; c <= COLS; c++) {
			var chg = false;
			if (c < COLS) {
				var cu = cur_buf[r][c], pv = prv_buf[r][c];
				chg = (cu[0] !== pv[0] || cu[1] !== pv[1]);
			}
			if (chg && c < COLS) {
				var cu = cur_buf[r][c];
				if (rs === -1) { rs = c; rstr = ""; rla = ""; }
				if (cu[0] !== rla) { rstr += cu[0]; rla = cu[0]; }
				rstr += cu[1];
				prv_buf[r][c][0] = cu[0]; prv_buf[r][c][1] = cu[1];
			} else if (rs !== -1) {
				console.gotoxy(rs+1, r+1);
				console.print(rstr);
				rs = -1; rstr = ""; rla = "";
			}
		}
	}
	console.line_counter = 0;
}

// ═══════════════════════════════════════════════════════════
//  SHARED DRAWING HELPERS
// ═══════════════════════════════════════════════════════════

// Return the ANSI colour attribute for a sky row, grading from dark at the top
// through mid-blue to bright cyan near the horizon.  frac is the normalised
// distance from PLAY_TOP (0.0) to the horizon row (1.0).
function skyAttrForRow(row, horizRow) {
	var frac = (row - PLAY_TOP) / Math.max(1, (horizRow || SS_HORIZON) - PLAY_TOP);
	if (frac < 0.15) return A_SKY_TOP;
	if (frac < 0.42) return A_SKY_MID;
	if (frac < 0.72) return A_SKY_BRIG;
	return A_SKY_HOR;
}

// Render the top HUD bar (row 1) with altitude, speed, distance, score, and
// the centred program-name title.  Used by all three game modes.
// weatherStr is optional; when provided it is shown between DST and the title.
// Layout: ALT(2) SPD(14) DST(26) [Weather(36)] Title(44) SCR(62)
function drawSharedHUD(altVal, spdVal, distVal, scoreVal, weatherStr) {
	fillRect(HUD_ROW, 1, HUD_ROW, COLS, A_HUD, " ");
	writeStr(HUD_ROW, 2,  A_HUD, "ALT:"+altVal+"ft");
	writeStr(HUD_ROW, 14, A_HUD, "SPD:"+spdVal+"kts");
	writeStr(HUD_ROW, 26, A_HUD, "DST:"+distVal);
	if (weatherStr) writeStr(HUD_ROW, 36, A_HUD, weatherStr);
	var t = CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + " " + gProgramName + " " + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE;
	writeStr(HUD_ROW, 44, CN+BG7+CH+CB, t);
	writeStr(HUD_ROW, 62, A_HUD, "SCR:"+scoreVal);
}
// Render the bottom status bar (last row) with control hints, an optional
// extra label (e.g. mode name or bank/climb prompt), and an ONLINE indicator.
function drawStatusBar(extra) {
	fillRect(STATUS_ROW, 1, STATUS_ROW, COLS, A_STATUS, " ");
	var m = " ["+CP437_DOWNWARDS_ARROW+CP437_DOWNWARDS_ARROW+CP437_LEFTWARDS_ARROW+CP437_RIGHTWARDS_ARROW+"] Steer  [Q] Quit" + (extra ? "  "+extra : "");
	writeStr(STATUS_ROW, 1, A_STATUS, m);
	var ol = " "+CP437_BLACK_SQUARE+" ONLINE "; writeStr(STATUS_ROW, COLS-ol.length+1, CN+BG4+CH+CG, ol);
}

// Return a string containing ch repeated n times (used for box-drawing borders).
function repeatChar(ch, n) { var s=""; for(var i=0;i<n;i++) s+=ch; return s; }

// ═══════════════════════════════════════════════════════════
//  MODE 1 — SIDE-SCROLL
// ═══════════════════════════════════════════════════════════

// Side-view plane facing right (11 wide × 3 tall):
//   Row 0:  "     /\    "   wing sweep (top)
//   Row 1:  "|====O====>   fuselage: tail | body == cockpit O nose >
//   Row 2:  "     \/    "   wing sweep (bottom)
// Left end = tail, right end = nose (plane moves right through world)

function drawSSPlane(px, py) {
	var lx = px - PL_HW;
	// Sky attr for the rows
	var sa0 = skyAttrForRow(py,   SS_HORIZON);
	var sa2 = skyAttrForRow(py+2, SS_HORIZON);

	var row0 = [
		[sa0," "],[sa0," "],[sa0," "],[sa0," "],[sa0," "],
		[A_PLANE_W,"/"],[A_PLANE_W,"\\"],[sa0," "],[sa0," "],[sa0," "],[sa0," "]
	];
	var row1 = [
		[A_PLANE_T,"|"],[A_PLANE_W,"="],[A_PLANE_W,"="],[A_PLANE_W,"="],[A_PLANE_W,"="],
		[A_PLANE_B,"O"],
		[A_PLANE_W,"="],[A_PLANE_W,"="],[A_PLANE_W,"="],[A_PLANE_W,"="],[A_PLANE_T,">"]
	];
	var row2 = [
		[sa2," "],[sa2," "],[sa2," "],[sa2," "],[sa2," "],
		[A_PLANE_W,"\\"],[A_PLANE_W,"/"],[sa2," "],[sa2," "],[sa2," "],[sa2," "]
	];
	var rows = [row0, row1, row2];
	for (var ri = 0; ri < PL_H; ri++) {
		for (var ci = 0; ci < PL_W; ci++) {
			var cell = rows[ri][ci];
			putCell(py+ri, lx+ci, cell[0], cell[1]);
		}
	}
}

// Fill the sky area (PLAY_TOP up to SS_HORIZON) with gradient colour attributes,
// then overlay the star field, toggling each star between a bullet glyph and a
// space on alternate ticks to create a subtle twinkle effect.
function drawSSSky(stars) {
	var overcast = gWeather.type !== SUNNY;
	for (var r = PLAY_TOP; r < SS_HORIZON; r++) {
		var a;
		if (gWeather.lightning) {
			a = CN+BG7+CH+CW;  // lightning flash: white sky
		} else if (overcast) {
			// Cloudy/rainy/storm: dark overcast gradient
			var frac = (r - PLAY_TOP) / Math.max(1, SS_HORIZON - PLAY_TOP);
			a = frac < 0.30 ? CN+BG0+CK : (frac < 0.70 ? CN+BG0+CH+CK : CN+BG4+CB);
		} else {
			a = skyAttrForRow(r, SS_HORIZON);
		}
		for (var c = 1; c <= COLS; c++) putCell(r, c, a, " ");
	}
	// Stars only visible in clear weather
	if (stars && !overcast && !gWeather.lightning) {
		for (var i = 0; i < stars.length; i++) {
			var s = stars[i];
			var tw = ((gScrollTick + i*7) % 5 < 2);
			putCell(s.y, s.x, tw ? A_STAR : A_SKY_TOP, tw ? CP437_BULLET_OPERATOR : " ");
		}
	}
}

// Render the ground area (SS_HORIZON to PLAY_BOT), terrain columns, and ground
// features (trees = feat 1, buildings = feat 2) for the side-scroll mode.
// terrain[c] is the height in rows of raised terrain at column c (0 = flat).
// gndFeat[c] is a feature code placed at the horizon edge when terrain is flat.
function drawSSGround(terrain, gndFeat) {
	// Flat ground fill
	for (var r = SS_HORIZON; r <= PLAY_BOT; r++) {
		var a = (r - SS_HORIZON < 2) ? A_GND_BRIG : A_GND_DARK;
		for (var c = 1; c <= COLS; c++) putCell(r, c, a, " ");
	}
	// Horizon edge
	for (var c = 1; c <= COLS; c++) {
		if (terrain[c-1] === 0)
			putCell(SS_HORIZON, c, CN+BG4+BG2+CH+CG, CP437_UPPER_HALF_BLOCK);
	}
	// Terrain and features
	for (var c = 1; c <= COLS; c++) {
		var h = terrain[c-1], feat = gndFeat[c-1];
		if (h > 0) {
			var topRow = SS_HORIZON - h;
			for (var r = topRow; r < SS_HORIZON; r++) {
				// fT = rows from the top of this terrain column.
				// Row 0 (peak): snow/rock cap or half-block; rows below: rock shade or ground fill.
				var fT = r - topRow;
				var at, ch;
				if (fT === 0) {
					at = h >= 6 ? A_SNOW : (h >= 4 ? A_ROCK : A_GND_BRIG);
					ch = h >= 6 ? CARET_CHAR : CP437_UPPER_HALF_BLOCK;
				} else if (h >= 5 && fT <= 1) {
					at = CN+BG7+CK; ch = CP437_DARK_SHADE;
				} else if (h >= 3 && fT < h-1) {
					at = A_ROCK; ch = CP437_DARK_SHADE;
				} else {
					at = A_GND_BRIG; ch = " ";
				}
				putCell(r, c, at, ch);
			}
			putCell(SS_HORIZON, c, A_GND_BRIG, " ");
		}
		if (feat === 1 && h === 0) {
			putCell(SS_HORIZON-1, c, CN+BG4+CH+CG, CP437_BLACK_CLUB_SUIT);
			if (SS_HORIZON-2 >= PLAY_TOP) putCell(SS_HORIZON-2, c, CN+BG4+CG, CP437_BLACK_SPADE_SUIT);
		} else if (feat === 2 && h === 0) {
			putCell(SS_HORIZON-1, c, CN+BG2+CH+CK, CP437_FULL_BLOCK);
			if (SS_HORIZON-2 >= PLAY_TOP) putCell(SS_HORIZON-2, c, CN+BG1+CH+CR, CP437_UPPER_HALF_BLOCK);
		}
	}
}

// Render the cloud list for the side-scroll mode.  Each cloud is a small
// rectangle; cells at the horizontal edges use a lighter shade to give a
// rounded appearance, while the interior uses a solid fill character.
function drawSSClouds(clouds) {
	for (var i = 0; i < clouds.length; i++) {
		var cl = clouds[i];
		for (var cy = 0; cy < cl.h; cy++) {
			var row = cl.y + cy;
			if (row < PLAY_TOP || row >= SS_HORIZON) continue;
			for (var cx = 0; cx < cl.w; cx++) {
				var col = cl.x + cx;
				if (col < 1 || col > COLS) continue;
				var ed = Math.min(cx, cl.w-1-cx);
				var at = ed === 0 ? A_CLOUD_E : A_CLOUD_M;
				var ch = ed === 0 ? CP437_LIGHT_SHADE : (ed === 1 ? CP437_MEDIUM_SHADE : " ");
				putCell(row, col, at, ch);
			}
		}
	}
}

// Main game loop for Mode 1 (side-scroll).  Initialises terrain, clouds, and
// stars; processes keyboard input each frame; scrolls terrain left one column
// per tick; rebuilds the full screen via the draw helpers; and returns a result
// object with final score, distance, and any game-over message.
function runSideScroll() {
	// ── State ──
	var planeX = Math.floor(COLS/2), planeY = PLAY_TOP + Math.floor((SS_HORIZON-PLAY_TOP)/2)-1;
	var score = 0, distance = 0, gameOver = false, gameOverMsg = "";
	var terrain = [], gndFeat = [];
	var terrCur = 0.0, terrTgt = 0;
	var clouds = [], stars = [];
	gScrollTick = 0;

	for (var c = 0; c < COLS; c++) { terrain.push(0); gndFeat.push(0); }

	// Stars
	for (var i = 0; i < 25; i++)
		stars.push({ x: Math.floor(Math.random()*COLS)+1,
		             y: Math.floor(Math.random()*Math.min(6,SS_HORIZON-PLAY_TOP))+PLAY_TOP });
	// Clouds
	for (var i = 0; i < 6; i++)
		clouds.push(ssMakeCloud(Math.floor(Math.random()*(COLS-10))+1));

	// ── Terrain gen ──
	function nextTerrH() {
		if (Math.random() < 0.015) terrTgt = Math.floor(Math.random()*SS_MAX_TERRH);
		terrCur = terrCur*0.91 + terrTgt*0.09 + (Math.random()-0.5)*0.3;
		terrCur = Math.max(0, Math.min(SS_MAX_TERRH-1, terrCur));
		return Math.round(terrCur);
	}
	function scrollTerrain() {
		for (var c = 0; c < COLS-1; c++) { terrain[c]=terrain[c+1]; gndFeat[c]=gndFeat[c+1]; }
		var nh = nextTerrH();
		terrain[COLS-1] = nh;
		gndFeat[COLS-1] = (nh===0 && Math.random()<0.055) ? (Math.random()<0.65?1:2) : 0;
	}

	// ── Collision ──
	function checkCrash() {
		// Fly-through mode: plane passes through terrain without crashing
	}

	// ── Input ──
	function handleKey(key) {
		if (!key || key==="") return;
		if (key.toUpperCase()==="Q") { gameOver=true; gameOverMsg="Flight aborted."; return; }
		switch (key) {
			case KEY_UP:    planeY = Math.max(PL_MIN_Y, planeY-1); score+=3; break;
			case KEY_DOWN:  planeY = Math.min(PL_MAX_Y, planeY+1); break;
			case KEY_LEFT:  planeX = Math.max(PL_MIN_X, planeX-2); score+=1; break;
			case KEY_RIGHT: planeX = Math.min(PL_MAX_X, planeX+2); score+=1; break;
		}
	}

	// ── Main loop ──
	initBuffers();
	initWeather();
	while (!gameOver && bbs.online && !js.terminated) {
		var key = console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, FRAME_MS);
		handleKey(key);
		if (gameOver) break;

		gScrollTick++;
		distance++; score++;
		updateWeather();
		scrollTerrain();
		if (gScrollTick % 3 === 0) {
			for (var i = 0; i < clouds.length; i++) { clouds[i].x--; }
			clouds = clouds.filter(function(cl){ return cl.x+cl.w >= 1; });
			if (clouds.length < 8 && Math.random()<0.18) clouds.push(ssMakeCloud(COLS+2));
		}
		checkCrash();

		// Build frame
		drawSSSky(stars);
		drawSSGround(terrain, gndFeat);
		drawSSClouds(clouds);
		drawSSPlane(planeX, planeY);
		// Rain streaks overlay for rainy/thunderstorm weather
		if (gWeather.type >= RAINY && !gWeather.lightning) {
			var drops = gWeather.type === THUNDERSTORM ? 20 : 11;
			var ra = CN+BG4+CB;
			for (var ri = 0; ri < drops; ri++) {
				var rc = Math.floor(Math.random() * COLS) + 1;
				var rr = PLAY_TOP + Math.floor(Math.random() * (SS_HORIZON - PLAY_TOP));
				putCell(rr, rc, ra, "|");
			}
		}
		var ssWeatherNames = ["Sunny","Cloudy","Rain","Storm"];
		drawSharedHUD((SS_HORIZON-planeY-2)*500, 340, distance, score, ssWeatherNames[gWeather.type]);
		drawStatusBar("[Side-scroll]");
		renderFrame();
	}
	return { score: score, distance: distance, msg: gameOverMsg };
}

// Create a new cloud object for side-scroll mode, starting at screen column x.
// Width and height are randomised; y is placed in the upper portion of the sky.
function ssMakeCloud(x) {
	var sh = SS_HORIZON - PLAY_TOP - 2;
	return { x:x, y:Math.floor(Math.random()*Math.floor(sh*0.65))+PLAY_TOP+2,
	         w:Math.floor(Math.random()*14)+7, h:1+Math.floor(Math.random()*2) };
}

// Fix typo above (drawSSSky → drawSSSky already defined)
// The call in runSideScroll uses drawSSSky — already defined above, named drawSSSky.
// That's correct. (drawSSSky = drawSSSky with stars param)

// ═══════════════════════════════════════════════════════════
//  MODE 2 — 3D PERSPECTIVE
// ═══════════════════════════════════════════════════════════

var cam3d = { x:0, y:P3_BASE_ALT, z:0, roll:0.0, speed:0.35 };

// Horizon row at given screen column, accounting for altitude and banking
function p3HorizonRow(col) {
	var base = P3_BASE_H + Math.round((cam3d.y - P3_BASE_ALT) * P3_ALT_SCALE);
	var nx = (col - CENTER_X) / CENTER_X;   // −1 … +1
	return base - Math.round(cam3d.roll * nx * P3_MAX_TILT);
}

// Sky colour for 3D — changes with weather.
// Sunny: FS4-style deep blue → bright cyan.
// Overcast/rain/storm: dark charcoal → steel blue.
function p3SkyAttr(row) {
	var frac = (row - PLAY_TOP) / Math.max(1, P3_BASE_H - PLAY_TOP);
	if (gWeather.lightning) return CN+BG7+CH+CW;  // lightning: white flash
	if (gWeather.type === SUNNY) {
		// Sunny: FS4 blue → cyan
		return frac < 0.30 ? CN+BG4+CB : CN+BG4+CH+CC;
	} else {
		// Cloudy/rainy/storm: dark overcast
		if (frac < 0.30) return CN+BG0+CK;
		if (frac < 0.70) return CN+BG0+CH+CK;
		return CN+BG4+CB;
	}
}

// Draw weather overlays (rain streaks, lightning) on top of the 3D scene.
function drawWeather3D() {
	if (gWeather.type === SUNNY) return;
	if (gWeather.type >= RAINY && !gWeather.lightning) {
		// Rain: vertical streaks scattered across sky and upper play area
		var drops = gWeather.type === THUNDERSTORM ? 20 : 11;
		var ra = CN+BG4+CB;
		for (var i = 0; i < drops; i++) {
			var dr = Math.floor(Math.random() * (P3_BASE_H - PLAY_TOP)) + PLAY_TOP;
			var dc = Math.floor(Math.random() * COLS) + 1;
			putCell(dr, dc, ra, "|");
		}
	}
}

// Ground perspective depth at screen (row, col)
function p3GroundDepth(row, col) {
	var dRow = row - p3HorizonRow(col);
	if (dRow <= 0) return null;
	return P3_FOCAL * cam3d.y / dRow;
}

// Biome type at world-z (cycles every 80 units): 0=plains 1=hills 2=mountains 3=valley 4=forest
function p3BiomeAt(wz) { return Math.floor(wz / 80) % 5; }

// Ground colour — FS4 Microsoft Flight Simulator 4.0 inspired:
//   Flat solid-colour polygons, patchwork farm fields, dark-blue water bodies.
function p3GroundAttr(wx, wz) {
	var biome = p3BiomeAt(wz);

	// ── Water features (FS4's signature dark-navy rivers/lakes) ──
	if (biome === 3 && Math.abs(wx) < 3.0) return CN+BG1+CH+CB;  // valley river
	// Deterministic ponds in plains (hash on grid cell)
	var ph = ((Math.floor(wz/20)*7 + Math.floor(wx/15)*13) & 0x7f);
	if (biome === 0 && ph < 14 && Math.abs(wx % 20 - 10) < 4) return CN+BG1+CB;

	// ── Road / runway centre strip ──
	if (Math.abs(wx) < 1.2 && biome !== 3) return CN+BG0+CK;

	// ── FS4 patchwork farm-field pattern (large geometric rectangles) ──
	var fz = Math.floor(wz / 9);
	var fx = Math.floor(wx / 7);
	var ft = ((fz * 3) ^ (fx * 5) ^ fz ^ fx) % 5;

	switch (biome) {
		case 0: // Plains — FS4 farmland: bright green, dark green, harvest brown, earth
			switch (ft) {
				case 0: return CN+BG2+CH+CG;   // bright green meadow
				case 1: return CN+BG2+CG;       // dark green crop
				case 2: return CN+BG3+CH+CY;   // yellow-brown harvest
				case 3: return CN+BG3+CK;       // dark plowed earth
				default: return CN+BG2+CH+CK;  // olive undergrowth
			}
		case 1: // Hills
			if (Math.abs(wx) > 22) return ft<2 ? CN+BG0+CH+CK : CN+BG0+CK;
			return ft<3 ? CN+BG2+CH+CG : CN+BG2+CG;
		case 2: // Mountains
			if (Math.abs(wx) > 10) return ft<2 ? CN+BG0+CH+CK : CN+BG0+CK;
			return ft<2 ? CN+BG3+CK : CN+BG0+CK;
		case 3: // Valley
			return Math.abs(wx)<12 ? (ft<3?CN+BG2+CH+CG:CN+BG2+CG)
			                       : (ft<2?CN+BG3+CH+CY:CN+BG2+CG);
		case 4: // Forest
			return ft<2 ? CN+BG2+CG : (ft<4 ? CN+BG0+CG : CN+BG2+CH+CK);
		default: return CN+BG2+CH+CG;
	}
}

// 3D world: mountains, trees, clouds ahead of camera
var world3d = { mountains:[], trees:[], clouds:[], nextZ:0 };

// Procedurally generate world objects (mountains, trees, clouds) up to world-z
// coordinate toZ.  Each call advances world3d.nextZ in steps of 6 units and
// appends objects based on the current biome and random rolls.  Objects that
// have fallen behind the camera are pruned at the end of the call.
function p3GenWorld(toZ) {
	while (world3d.nextZ < toZ) {
		var z = world3d.nextZ;
		world3d.nextZ += 6;
		var r = Math.random();
		var biome = p3BiomeAt(z);

		// Mountain clusters — much more frequent in mountains biome
		var mtnChance = biome===2 ? 0.32 : (biome===1 ? 0.18 : (biome===3 ? 0.04 : 0.06));
		if (r < mtnChance) {
			var n = biome===2 ? (2+Math.floor(Math.random()*4)) : (1+Math.floor(Math.random()*2));
			for (var i = 0; i < n; i++) {
				var ctr = biome===3 ? 0.05 : (biome===2 ? 0.08 : 0.05);
				var side = Math.random()<ctr ? 0 : (Math.random()<0.5?-1:1);
				var minH = biome===2 ? 4.0 : (biome===1 ? 2.0 : 1.5);
				var maxH = biome===2 ? 12.0 : (biome===1 ? 7.0 : 4.5);
				world3d.mountains.push({
					x: side * (6 + Math.random()*(biome===3?35:28)),
					z: z + 2 + Math.random()*20,
					h: minH + Math.random()*(maxH-minH),
					w: 10 + Math.random()*30
				});
			}
		}

		// Rolling hills in plains/hills (low, wide)
		if ((biome===0||biome===1) && Math.random() < 0.18) {
			world3d.mountains.push({
				x: (Math.random()-0.5)*65,
				z: z + Math.random()*18,
				h: 0.4 + Math.random()*(biome===1 ? 2.2 : 1.0),
				w: 18 + Math.random()*32
			});
		}

		// Trees — very dense in forest, sparse in mountains
		var treeChance = biome===4 ? 0.70 : (biome===3 ? 0.48 : (biome===1 ? 0.28 : (biome===0 ? 0.14 : 0.07)));
		if (r < treeChance) {
			var side2 = Math.random()<0.5?-1:1;
			var tx = biome===4 ? side2*(2+Math.random()*32) : side2*(7+Math.random()*28);
			world3d.trees.push({ x:tx, z:z+Math.random()*6 });
			if (biome===4 && Math.random()<0.5)
				world3d.trees.push({ x:(Math.random()-0.5)*40, z:z+Math.random()*6 });
		}

		// Clouds
		if (Math.random() < (biome===3 ? 0.05 : 0.12)) {
			world3d.clouds.push({
				x: (Math.random()-0.5)*90,
				y: cam3d.y + 2 + Math.random()*5,
				z: z + Math.random()*40,
				w: 16 + Math.random()*35
			});
		}
	}
	// Prune behind camera
	world3d.mountains = world3d.mountains.filter(function(m){ return m.z > cam3d.z-3; });
	world3d.trees      = world3d.trees.filter(function(t){ return t.z > cam3d.z-1; });
	world3d.clouds     = world3d.clouds.filter(function(c){ return c.z > cam3d.z-3; });
}

// Draw sky + ground with banked horizon
function p3DrawScene() {
	for (var r = PLAY_TOP; r <= PLAY_BOT; r++) {
		for (var c = 1; c <= COLS; c++) {
			var hr = p3HorizonRow(c);
			if (r < hr) {
				putCell(r, c, p3SkyAttr(r), " ");
			} else {
				var dRow = r - hr;
				if (dRow <= 0) { putCell(r, c, p3SkyAttr(r), " "); continue; }
				var depth = P3_FOCAL * cam3d.y / dRow;
				var wx = cam3d.x + (c - CENTER_X) * depth / P3_FOCAL;
				var wz = cam3d.z + depth;
				// FS4 style: solid flat colour polygons; only light stipple very close
				var gCh = depth < 4 ? CP437_MEDIUM_SHADE : " ";
				putCell(r, c, p3GroundAttr(wx, wz), gCh);
			}
		}
	}
}

// Draw stars in upper sky
function p3DrawStars(stars) {
	for (var i = 0; i < stars.length; i++) {
		var s = stars[i];
		if (s.y < PLAY_TOP || s.y >= p3HorizonRow(s.x)) continue;
		var tw = ((gScrollTick + i*7) % 5 < 2);
		putCell(s.y, s.x, tw ? A_STAR : A_SKY_TOP, tw ? CP437_BULLET_OPERATOR : " ");
	}
}

// Project a world point; returns screen {x,y,scale} or null
function p3Project(wx, wy, wz) {
	var rz = wz - cam3d.z;
	if (rz < 0.3) return null;
	var sc = P3_FOCAL / rz;
	var scx = CENTER_X + Math.round((wx - cam3d.x) * sc);
	return {
		x: scx,
		y: p3HorizonRow(Math.max(1, Math.min(COLS, scx))) - Math.round((wy - cam3d.y) * sc),
		scale: sc, rz: rz
	};
}

// Draw mountains (back to front)
function p3DrawMountains() {
	var sorted = world3d.mountains.slice().sort(function(a,b){ return b.z-a.z; });
	for (var i = 0; i < sorted.length; i++) {
		var m = sorted[i];
		var rz = m.z - cam3d.z;
		if (rz < 0.5 || rz > P3_DRAW_DIST) continue;
		var sc = P3_FOCAL / rz;
		var scx  = CENTER_X + Math.round((m.x - cam3d.x) * sc);
		var hAtM = p3HorizonRow(Math.max(1, Math.min(COLS, scx)));
		var sTop = hAtM - Math.round((m.h - cam3d.y) * sc);
		var sBot = hAtM + Math.round(cam3d.y * sc);
		var sHW  = Math.max(1, Math.round(m.w/2 * sc));
		sTop = Math.max(PLAY_TOP, sTop); sBot = Math.min(PLAY_BOT-3, sBot);
		if (sBot < PLAY_TOP) continue;

		// Depth-based fill: far mountains are faint silhouettes, near are solid
		var dFade = rz > 50 ? CP437_LIGHT_SHADE : (rz > 25 ? CP437_MEDIUM_SHADE : CP437_DARK_SHADE);
		var dFill = rz > 50 ? CP437_LIGHT_SHADE : (rz > 20 ? CP437_MEDIUM_SHADE : (rz > 8 ? CP437_LIGHT_SHADE : " "));
		var nearAt = rz < 8 ? CN+BG3+CH+CK : CN+BG3+CK;
		for (var r = sTop; r <= sBot; r++) {
			var relH = (sBot > sTop) ? (sBot - r) / (sBot - sTop) : 0.5;
			var hw = Math.max(1, Math.round(sHW * (1 - relH*0.55)));
			var at, ch;
			if      (relH > 0.88) { at = A_SNOW;    ch = rz > 40 ? CP437_LIGHT_SHADE : CP437_MEDIUM_SHADE; }
			else if (relH > 0.65) { at = A_ROCK;    ch = dFade; }
			else if (relH > 0.35) { at = nearAt;    ch = dFill; }
			else                  { at = A_GND_BRIG; ch = rz < 8 ? CP437_MEDIUM_SHADE : " "; }

			for (var dc = -hw; dc <= hw; dc++) {
				var col = scx + dc;
				if (col >= 1 && col <= COLS) putCell(r, col, at, ch);
			}
		}
		// Peak char
		if (sTop >= PLAY_TOP && scx >= 1 && scx <= COLS && m.h >= 4)
			putCell(sTop, scx, A_SNOW, CARET_CHAR);
	}
}

// Draw trees (back to front)
function p3DrawTrees() {
	var sorted = world3d.trees.slice().sort(function(a,b){ return b.z-a.z; });
	for (var i = 0; i < sorted.length; i++) {
		var t = sorted[i];
		var rz = t.z - cam3d.z;
		if (rz < 0.5 || rz > 45) continue;
		var sc = P3_FOCAL / rz;
		var sx   = CENTER_X + Math.round((t.x - cam3d.x) * sc);
		var hAtT = p3HorizonRow(Math.max(1, Math.min(COLS, sx)));
		var sBot = hAtT + Math.round(cam3d.y * sc);    // base on ground
		var sTop = hAtT - Math.round((1.2 - cam3d.y) * sc); // tree height 1.2wu
		sBot = Math.min(PLAY_BOT-3, sBot);
		sTop = Math.max(PLAY_TOP, sTop);
		if (sx < 1 || sx > COLS || sTop >= sBot) continue;
		var th = sBot - sTop;
		putCell(sTop, sx, CN+BG2+CH+CG, th >= 2 ? CP437_BLACK_CLUB_SUIT : CP437_BULLET_OPERATOR);
		if (th >= 2 && sx > 1)  putCell(sTop, sx-1, CN+BG2+CG, CP437_BLACK_SPADE_SUIT);
		if (th >= 2 && sx < COLS) putCell(sTop, sx+1, CN+BG2+CG, CP437_BLACK_SPADE_SUIT);
	}
}

// Draw clouds (back to front)
function p3DrawClouds() {
	var sorted = world3d.clouds.slice().sort(function(a,b){ return b.z-a.z; });
	for (var i = 0; i < sorted.length; i++) {
		var cl = sorted[i];
		var rz = cl.z - cam3d.z;
		if (rz < 1 || rz > 100) continue;
		var sc  = P3_FOCAL / rz;
		var scx   = CENTER_X + Math.round((cl.x - cam3d.x) * sc);
		var hAtCl = p3HorizonRow(Math.max(1, Math.min(COLS, scx)));
		var scy   = hAtCl - Math.round((cl.y - cam3d.y) * sc);
		var shw   = Math.max(1, Math.round(cl.w/2 * sc));
		if (scy < PLAY_TOP || scy > hAtCl + 2) continue;
		for (var dc = -shw; dc <= shw; dc++) {
			var col = scx + dc;
			if (col < 1 || col > COLS) continue;
			var ed = Math.abs(dc);
			var at = ed > shw-1 ? A_CLOUD_E : A_CLOUD_M;
			var ch = ed > shw-1 ? CP437_LIGHT_SHADE : (ed > shw-2 ? CP437_MEDIUM_SHADE : " ");
			putCell(scy, col, at, ch);
		}
	}
}

// Draw Learjet 35 from behind — T-tail, swept wings, aft-mounted twin engines
// Row 0: T-tail horizontal stabiliser (Learjet's signature feature)
// Row 1: vertical fin
// Row 2: swept main wings + fuselage
// Row 3: aft engine pods (no under-wing engines on a Learjet)
function p3DrawPlane(roll) {
	var py = PLAY_BOT - 3;   // top row of 4-row sprite
	var px = CENTER_X;

	// Wing lengths: Learjet has moderately swept wings; roll tilts apparent length
	var lwLen = Math.max(2, Math.min(8, Math.round(6 + roll * 3)));
	var rwLen = Math.max(2, Math.min(8, Math.round(6 - roll * 3)));

	// Row 0 (py): T-tail horizontal stabiliser (tilts slightly with roll)
	var tsL = Math.max(1, Math.round(2 - roll));
	var tsR = Math.max(1, Math.round(2 + roll));
	for (var i = 1; i <= tsL; i++) putCell(py, px-i, A_PLANE_T, "_");
	putCell(py, px, A_PLANE_T, "^");
	for (var i = 1; i <= tsR; i++) putCell(py, px+i, A_PLANE_T, "_");

	// Row 1 (py+1): vertical fin
	putCell(py+1, px-1, A_PLANE_T, "|");
	putCell(py+1, px,   A_PLANE_T, "|");

	// Row 2 (py+2): swept main wings + narrow fuselage
	var wr = py + 2;
	for (var i = 1; i <= lwLen; i++)
		putCell(wr, px-i, A_PLANE_W, i === lwLen ? "/" : "=");
	for (var i = 1; i <= rwLen; i++)
		putCell(wr, px+i, A_PLANE_W, i === rwLen ? "\\" : "=");
	putCell(wr, px, A_PLANE_B, "O");

	// Row 3 (py+3): aft-mounted engine pods — Learjet's defining rear silhouette
	var er = py + 3;
	putCell(er, px-3, A_PLANE_B, "(");
	putCell(er, px-2, A_PLANE_T, "*");
	putCell(er, px+2, A_PLANE_T, "*");
	putCell(er, px+3, A_PLANE_B, ")");

	// Roll indicator on status bar
	var rollPct = Math.round(roll * 5);
	var ri = CENTER_X + rollPct;
	if (ri >= 1 && ri <= COLS)
		putCell(STATUS_ROW, ri, CN+BG4+CH+CY, CP437_BLACK_DOWN_POINTING_TRIANGLE);
}

// Explosion in 3D mode
function p3DrawExplosion(frame) {
	var px = CENTER_X, py = PLAY_BOT - 1;
	var rs = [" ",".","+","*","x","o","+"," "];
	var as = [CN+BG3+CH+CR, CN+BG1+CH+CY, CN+BG1+CH+CR, CN+BG0+CH+CY,
	          CN+BG0+CH+CR, CN+BG0+CK, A_SKY_BRIG, A_SKY_BRIG];
	var ch = rs[Math.min(frame,rs.length-1)];
	var at = as[Math.min(frame,as.length-1)];
	var rad = frame;
	for (var dr = -rad; dr <= rad; dr++) {
		for (var dc = -rad*2; dc <= rad*2; dc++) {
			var d = Math.sqrt(dr*dr + (dc/2)*(dc/2));
			if (d >= rad-0.8 && d <= rad+0.8)
				putCell(py+dr, px+dc, at, ch);
		}
	}
}

// Main game loop for Mode 2 (ASCII 3D perspective).  Resets the camera and
// world state, pre-generates the initial draw-distance of terrain objects,
// then runs the per-frame cycle: read input, update physics, build the frame
// with the p3Draw* helpers, overlay HUD/status, and flip the buffer.
// Returns a result object with score, distance, flight statistics, and quit flag.
function run3D() {
	// ── Camera / game state ──
	cam3d.x=0; cam3d.y=P3_BASE_ALT; cam3d.z=0; cam3d.roll=0; cam3d.pitch=0; cam3d.speed=0.35;
	world3d.mountains=[]; world3d.trees=[]; world3d.clouds=[]; world3d.nextZ=0;
	gScrollTick = 0;
	initWeather();

	var score=0, gameOver=false, quitGame=false, gameOverMsg="";
	var explFrame=0, explActive=false;
	var stars=[];
	var flightFrames=0, altAccum=0, altMax=cam3d.y, speedAccum=0;
	for (var i=0; i<20; i++)
		stars.push({ x:Math.floor(Math.random()*COLS)+1,
		             y:Math.floor(Math.random()*(P3_BASE_H-PLAY_TOP-1))+PLAY_TOP });

	// Pre-generate initial world
	p3GenWorld(P3_DRAW_DIST + 10);

	function handle3DKey(key) {
		if (!key || key==="") return;
		if (key.toUpperCase()==="Q") { quitGame=true; gameOver=true; gameOverMsg="Flight aborted."; return; }
		switch (key) {
			case KEY_UP:
				cam3d.y = Math.max(0.4, cam3d.y-0.18);
				break;
			case KEY_DOWN:
				cam3d.y = Math.min(P3_BASE_ALT+4, cam3d.y+0.18);
				score += 4;
				break;
			case KEY_LEFT:
				cam3d.roll = Math.max(-1.0, cam3d.roll-0.18);
				break;
			case KEY_RIGHT:
				cam3d.roll = Math.min(1.0, cam3d.roll+0.18);
				break;
		}
	}

	function update3D() {
		gScrollTick++;
		cam3d.z += cam3d.speed;
		// Banking turns the plane laterally; pulling up while banked tightens the turn
		var turnBoost = 1.0 + Math.max(0, cam3d.pitch) * 1.8;
		cam3d.x += cam3d.roll * cam3d.speed * 0.75 * turnBoost;
		// Roll and pitch damp toward zero when keys released
		cam3d.roll *= 0.92;
		if (Math.abs(cam3d.roll) < 0.03) cam3d.roll = 0;
		cam3d.pitch *= 0.82;
		if (Math.abs(cam3d.pitch) < 0.02) cam3d.pitch = 0;
		// Flight statistics
		flightFrames++;
		altAccum += cam3d.y;
		if (cam3d.y > altMax) altMax = cam3d.y;
		speedAccum += cam3d.speed;

		score++;
		updateWeather();

		// Generate world ahead
		p3GenWorld(cam3d.z + P3_DRAW_DIST + 10);

		// Fly-through mode: clamp altitude floor, no crash
		if (cam3d.y < 0.35) cam3d.y = 0.35;
	}

	// ── Main loop ──
	initBuffers();
	while (!gameOver && bbs.online && !js.terminated) {
		var prevY3 = cam3d.y;
		var key = console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, FRAME_MS);
		handle3DKey(key);
		if (!gameOver) {
			cam3d.pitch = Math.max(-1, Math.min(1, cam3d.pitch * 0.82 + (cam3d.y - prevY3) * 5));
			update3D();
		}

		if (explActive) {
			explFrame++;
			if (explFrame > 8) gameOver = true;
		}

		// Build frame
		p3DrawScene();
		if (gWeather.type === SUNNY) p3DrawStars(stars);  // stars only when sunny
		p3DrawMountains();
		p3DrawTrees();
		p3DrawClouds();
		drawWeather3D();  // rain streaks / lightning flash on top of scene

		if (explActive) {
			p3DrawExplosion(explFrame);
		} else {
			p3DrawPlane(cam3d.roll);
		}

		var alt = Math.round(Math.max(0, cam3d.y-0.3) * 1000);
		var weatherNames = ["Sunny","Cloudy","Rain","Storm"];
		drawSharedHUD(alt, 340, Math.floor(cam3d.z*10), score, weatherNames[gWeather.type]);
		// Altitude danger overlay (overwrites HUD centre)
		if (alt < 800 && !explActive) {
			var wStr = alt < 350 ? "*** PULL UP ***" : " LOW ALTITUDE  ";
			var wAttr = alt < 350 ? CN+BG0+CH+CR : CN+BG3+CH+CY;
			writeStr(HUD_ROW, Math.floor((COLS-wStr.length)/2)+1, wAttr, wStr);
		}
		// Scan for nearby mountain threats
		var nearMtn = false;
		for (var wi = 0; wi < world3d.mountains.length; wi++) {
			var wm = world3d.mountains[wi];
			var wrz = wm.z - cam3d.z;
			if (wrz > 0 && wrz < 14 && Math.abs(wm.x - cam3d.x) < wm.w/2 + 3) { nearMtn = true; break; }
		}
		var biomeNames = ["Plains","Hills","Mountains","Valley","Forest"];
		drawStatusBar(biomeNames[p3BiomeAt(cam3d.z)]
		              +"  Bank:"+CP437_LEFTWARDS_ARROW+CP437_RIGHTWARDS_ARROW+"  Climb:"+CP437_DOWNWARDS_ARROW+CP437_DOWNWARDS_ARROW);
		if (nearMtn && (gScrollTick % 4 < 2))
			writeStr(STATUS_ROW, 2, CN+BG0+CH+CR, "!MTN!");
		p3DrawPlane(cam3d.roll);  // redraw on top of status roll indicator
		renderFrame();
	}
	return { score:score, distance:Math.floor(cam3d.z*10), msg:gameOverMsg, quit:quitGame,
	         frames:flightFrames, altAvg:altAccum/Math.max(1,flightFrames), altMax:altMax,
	         speedAvg:speedAccum/Math.max(1,flightFrames) };
}

// ═══════════════════════════════════════════════════════════
//  MODE 3 — RIP VECTOR GRAPHICS
// ═══════════════════════════════════════════════════════════

// Main game loop for Mode 3 (RIP vector graphics).  Uses the same cam3d /
// world3d state as run3D() but renders each frame as a stream of RIP protocol
// commands (filled polygons, ovals) sent to the terminal via ripSend().
// The HUD is drawn as plain text over the RIP graphics layer.
// Returns the same result object shape as run3D().
function runRIP() {
	// RIP screen geometry
	var RW = 640, RH = 350, RCX = 320;
	var R_BASE_HOR = 180;   // nominal horizon y (pixels, 0=top)
	var R_FOCAL    = 160;   // focal length in RIP pixels
	var R_ALT_PX   = 42;    // horizon shift per altitude unit
	var R_TILT_PX  = 72;    // max horizon tilt at screen edge for full roll

	// RIP color indices (EGA 16-colour palette)
	var RC_BLUE=1, RC_RED=4;
	var RC_LTGRAY=7, RC_DKGRAY=8;
	var RC_YELLOW=14, RC_WHITE=15;

	function ripSend(cmds) {
		// \r moves cursor to col 1 so !| appears at line start (required by RIP 1.54 trigger)
		console.write("\r!" + cmds + "\r\n");
		console.line_counter = 0;
	}

	// Horizon y at given rip-x accounting for altitude and roll.
	// Banking right (+roll): left horizon goes DOWN (larger y), right goes UP (smaller y).
	// So the tilt term is SUBTRACTED (matches the ASCII p3HorizonRow convention).
	function rHorY(rx) {
		var hBase = R_BASE_HOR + Math.round((cam3d.y - P3_BASE_ALT) * R_ALT_PX);
		var nx = (rx - RCX) / RCX;   // -1..+1
		return Math.max(0, Math.min(RH-1, hBase - Math.round(cam3d.roll * nx * R_TILT_PX)));
	}

	// Reset camera / world state (shared with 3D mode)
	cam3d.x=0; cam3d.y=P3_BASE_ALT; cam3d.z=0; cam3d.roll=0; cam3d.pitch=0; cam3d.speed=0.35;
	world3d.mountains=[]; world3d.trees=[]; world3d.clouds=[]; world3d.nextZ=0;
	gScrollTick = 0;
	initWeather();

	var score=0, gameOver=false, quitGame=false, gameOverMsg="";
	var explActive=false, explFrame=0;
	var flightFrames=0, altAccum=0, altMax=cam3d.y, speedAccum=0;

	p3GenWorld(P3_DRAW_DIST + 10);

	// Initial clear
	console.clear();
	ripSend(RIPEraseGraphicsWindow());

	while (!gameOver && bbs.online && !js.terminated) {
		var prevY = cam3d.y;
		var key = console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, FRAME_MS);
		if (key) {
			if (key.toUpperCase() === "Q") {
				quitGame=true; gameOver=true; gameOverMsg="Flight aborted."; break;
			}
			switch (key) {
				case KEY_UP:    cam3d.y = Math.max(0.4, cam3d.y-0.18); break;
				case KEY_DOWN:  cam3d.y = Math.min(P3_BASE_ALT+4, cam3d.y+0.18); score+=4; break;
				case KEY_LEFT:  cam3d.roll = Math.max(-1.0, cam3d.roll-0.18); break;
				case KEY_RIGHT: cam3d.roll = Math.min(1.0, cam3d.roll+0.18); break;
			}
		}
		if (gameOver) break;

		// Physics update
		gScrollTick++;
		cam3d.z += cam3d.speed;
		// Banking turns the plane laterally; pulling up while banked tightens the turn
		var turnBoost = 1.0 + Math.max(0, cam3d.pitch) * 1.8;
		cam3d.x += cam3d.roll * cam3d.speed * 0.75 * turnBoost;
		cam3d.roll *= 0.92;
		if (Math.abs(cam3d.roll) < 0.03) cam3d.roll = 0;
		cam3d.pitch = Math.max(-1, Math.min(1, cam3d.pitch * 0.82 + (cam3d.y - prevY) * 5));
		score++;
		flightFrames++;
		altAccum += cam3d.y;
		if (cam3d.y > altMax) altMax = cam3d.y;
		speedAccum += cam3d.speed;
		updateWeather();
		p3GenWorld(cam3d.z + P3_DRAW_DIST + 10);

		// Fly-through mode: clamp altitude floor, no crash
		if (cam3d.y < 0.35) cam3d.y = 0.35;

		// ── Build RIP frame ──
		var cmds = RIPEraseGraphicsWindow();

		// Horizon y at left/right screen edges (used for sky/ground fill)
		var hLeft  = rHorY(0);
		var hRight = rHorY(RW-1);

		// Sky — colour depends on weather
		var hMin = Math.min(hLeft, hRight), hMax = Math.max(hLeft, hRight);
		if (gWeather.type === SUNNY) {
			// Sunny: FS4 deep blue → bright cyan (EGA colour 11)
			cmds += RIPFillStyleNumeric(1, RC_BLUE);
			cmds += RIPBarNumeric(0, 0, RW-1, Math.round(hMin*0.45));
			cmds += RIPFillStyleNumeric(1, 11);
			cmds += RIPBarNumeric(0, Math.round(hMin*0.45), RW-1, hMax);
		} else {
			// Cloudy / rainy / storm: dark charcoal → steel blue
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPBarNumeric(0, 0, RW-1, Math.round(hMin*0.5));
			cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
			cmds += RIPBarNumeric(0, Math.round(hMin*0.5), RW-1, hMax);
		}

		// Ground base: FS4-style bright green
		cmds += RIPFillStyleNumeric(1, 10);   // EGA bright green
		cmds += RIPFillPolygonNumeric([
			{x:0, y:hLeft}, {x:RW-1, y:hRight},
			{x:RW-1, y:RH-1}, {x:0, y:RH-1}
		]);

		// FS4 patchwork farm-field strips — banked polygon quads so they follow the
		// tilted horizon when the player banks.  Left and right edges are computed
		// separately using rHorY() so the quad stays glued to the ground plane.
		var fsColors = [10, 2, 6, 14, 2, 10, 8];  // ltgreen,green,brown,yellow,green,ltgreen,dkgray
		var fsDists  = [80, 35, 18, 9, 4.5, 2.2];  // world-unit depth breakpoints
		for (var fsi = 0; fsi < fsDists.length - 1; fsi++) {
			var fsHL = rHorY(0),    fsHR = rHorY(RW-1);
			var fsGy1L = Math.min(RH-1, fsHL + Math.round(cam3d.y * R_FOCAL / fsDists[fsi]));
			var fsGy1R = Math.min(RH-1, fsHR + Math.round(cam3d.y * R_FOCAL / fsDists[fsi]));
			var fsGy2L = Math.min(RH-1, fsHL + Math.round(cam3d.y * R_FOCAL / fsDists[fsi+1]));
			var fsGy2R = Math.min(RH-1, fsHR + Math.round(cam3d.y * R_FOCAL / fsDists[fsi+1]));
			if (Math.min(fsGy1L,fsGy1R) >= Math.max(fsGy2L,fsGy2R)) continue;
			var fsIdx = (Math.floor(cam3d.z / 4) + fsi) % fsColors.length;
			cmds += RIPFillStyleNumeric(1, fsColors[fsIdx]);
			cmds += RIPFillPolygonNumeric([
				{x:0,    y:fsGy1L}, {x:RW-1, y:fsGy1R},
				{x:RW-1, y:fsGy2R}, {x:0,    y:fsGy2L}
			]);
		}

		// River / water strip in valley biome (FS4's dark navy water)
		if (p3BiomeAt(cam3d.z) === 3) {
			var hAtRiv = rHorY(RCX);
			var rvGy   = Math.min(RH-2, hAtRiv + Math.round(cam3d.y * R_FOCAL / 2));
			cmds += RIPFillStyleNumeric(1, RC_BLUE);
			cmds += RIPFillPolygonNumeric([
				{x:RCX-10, y:rvGy}, {x:RCX+10, y:rvGy},
				{x:RCX+3,  y:hAtRiv+2}, {x:RCX-3, y:hAtRiv+2}
			]);
		}

		// Mountains (back to front) — only render mountains whose peak clears the horizon.
		// Mountains shorter than camera altitude appear entirely below the horizon as flat
		// ground patches, so they are skipped.  Peak height is boosted 1.8× so the
		// projection reads as a proper mountain rather than a flat hump at the horizon.
		var mSorted = world3d.mountains.slice().sort(function(a,b){ return b.z-a.z; });
		for (var i=0; i<mSorted.length; i++) {
			var m = mSorted[i];
			var rz = m.z - cam3d.z;
			if (rz<0.5 || rz>P3_DRAW_DIST) continue;
			var sc = R_FOCAL / rz;
			var scx  = RCX + Math.round((m.x - cam3d.x) * sc);
			// Use banked horizon at the mountain's screen-x so it sits correctly
			// on the ground plane even when rolling.
			var hAtM  = rHorY(Math.max(0, Math.min(RW-1, scx)));
			// 1.8× vertical boost: prevents peaks from looking like flat horizon humps
			var peakRise = Math.round((m.h - cam3d.y) * sc * 1.8);
			var sTopY = hAtM - peakRise;
			var sBotY = Math.min(RH-1, hAtM + Math.round(cam3d.y * sc));
			var sHW   = Math.max(3, Math.round(m.w/2 * sc));
			// Skip mountains whose peak does not clear the horizon — they appear flat
			if (sTopY >= hAtM || sBotY < 0 || sTopY >= RH) continue;
			sTopY = Math.max(0, sTopY);
			// Body — triangle with single apex (more dramatic than a flat-top trapezoid)
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPFillPolygonNumeric([
				{x:scx-sHW, y:sBotY}, {x:scx+sHW, y:sBotY},
				{x:scx, y:sTopY}
			]);
			// Rock face detail: lighter lower slope triangle
			if (m.h >= 3) {
				var midY = Math.round((sTopY*0.40 + sBotY*0.60));
				var midHW = Math.max(2, Math.round(sHW*0.60));
				cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
				cmds += RIPFillPolygonNumeric([
					{x:scx-midHW, y:sBotY}, {x:scx+midHW, y:sBotY},
					{x:scx, y:midY}
				]);
			}
			// Snow cap
			if (m.h >= 3.5) {
				var snowY = sTopY + Math.round((sBotY-sTopY)*0.18);
				var snowHW = Math.max(2, Math.round(sHW*0.22));
				cmds += RIPFillStyleNumeric(1, RC_WHITE);
				cmds += RIPFillPolygonNumeric([
					{x:scx, y:sTopY}, {x:scx-snowHW, y:snowY}, {x:scx+snowHW, y:snowY}
				]);
			}
		}

		// Trees — rendered as foliage blobs on the ground plane.  Because the camera
		// flies above tree height (cam3d.y 2.5 > tree h 1.2), trees appear below the
		// horizon as textured ground features rather than sky objects.
		var tSorted = world3d.trees.slice().sort(function(a,b){ return b.z-a.z; });
		for (var ti=0; ti<tSorted.length; ti++) {
			var t = tSorted[ti];
			var rz = t.z - cam3d.z;
			if (rz<0.5 || rz>50) continue;
			var sc = R_FOCAL / rz;
			var scx = RCX + Math.round((t.x - cam3d.x) * sc);
			if (scx < -30 || scx >= RW+30) continue;
			var hAtT = rHorY(Math.max(0, Math.min(RW-1, scx)));
			// Tree top (world h=1.2) and base (world h=0) projected into ground area
			var tTopY = Math.round(hAtT + (cam3d.y - 1.2) * sc);
			var tBotY = Math.min(RH-1, Math.round(hAtT + cam3d.y * sc));
			if (tTopY >= tBotY || tBotY < 0 || tTopY >= RH) continue;
			var crownHW = Math.max(3, Math.round(0.55 * sc));
			var crownHH = Math.max(2, Math.round((tBotY - tTopY) * 0.45));
			var crownY  = tTopY + Math.round((tBotY - tTopY) * 0.30);
			// Foliage crown (EGA dark green = 2)
			cmds += RIPFillStyleNumeric(1, 2);
			cmds += RIPFilledOvalNumeric(scx, crownY, crownHW, crownHH);
		}

		// Clouds (back to front)
		var cSorted = world3d.clouds.slice().sort(function(a,b){ return b.z-a.z; });
		for (var i=0; i<cSorted.length; i++) {
			var cl = cSorted[i];
			var rz = cl.z - cam3d.z;
			if (rz<1 || rz>100) continue;
			var sc = R_FOCAL / rz;
			var scx = RCX + Math.round((cl.x - cam3d.x) * sc);
			// Use banked horizon at cloud's screen-x for correct sky placement.
			var hAtCl = rHorY(Math.max(0, Math.min(RW-1, scx)));
			var scy = hAtCl - Math.round((cl.y - cam3d.y) * sc);
			var shw = Math.max(5, Math.round(cl.w/2 * sc));
			var shh = Math.max(3, Math.round(cl.w/10 * sc));
			if (scy < 5 || scy >= hAtCl-3) continue;
			cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
			cmds += RIPFilledOvalNumeric(scx, scy+Math.round(shh*0.4), shw-2, Math.max(2,shh-2));
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFilledOvalNumeric(scx, scy, shw, shh);
		}

		// ── Weather overlay (drawn after scenery, before plane) ──
		if (gWeather.lightning) {
			// Lightning flash: flood the whole sky with white
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPBarNumeric(0, 0, RW-1, Math.max(hLeft, hRight));
		} else if (gWeather.type >= RAINY) {
			// Rain streaks — random thin bars scattered in the sky
			var nDrops = gWeather.type === THUNDERSTORM ? 32 : 18;
			cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
			for (var rdi = 0; rdi < nDrops; rdi++) {
				var rdx = Math.floor(Math.random() * RW);
				var rdy = Math.floor(Math.random() * Math.min(hLeft, hRight));
				cmds += RIPBarNumeric(rdx, rdy, rdx+1, rdy+9);
			}
		}

		// Aircraft or explosion — centred at bottom of display
		var pCx = RCX, pBy = RH - 58;
		if (explActive) {
			var er = Math.min(90, explFrame * 14);
			cmds += RIPFillStyleNumeric(1, RC_YELLOW);
			cmds += RIPFilledOvalNumeric(pCx, pBy, er, Math.round(er*0.6));
			cmds += RIPFillStyleNumeric(1, RC_RED);
			cmds += RIPFilledOvalNumeric(pCx, pBy, Math.round(er*0.6), Math.round(er*0.38));
		} else {
			// ── Learjet 35 — rear/chase-cam view, unified roll rotation ──
			// All geometry is defined in unrotated local coords; every point is then
			// rotated together around the aircraft centre so the whole plane tilts
			// as a single rigid body.

			var pivX  = pCx;
			// Pivot shifts up when climbing, down when descending
			var pivY  = pBy - 50 - Math.round(cam3d.pitch * 18);
			var cos_a = Math.cos(cam3d.roll);
			var sin_a = Math.sin(cam3d.roll);
			// Pitch: scale the vertical component (cos_p) to model the 3D rear-view
			// projection of nose-up/down attitude (sprite appears compressed when pitched)
			var cos_p = Math.cos(cam3d.pitch * 0.45);

			// Rotate one screen point around pivot with combined roll + pitch, return rounded {x, y}
			var rp = function(px, py) {
				var dx = px - pivX, dy = py - pivY;
				var dy2 = dy * cos_p;   // pitch: vertical compression/expansion
				return {
					x: Math.round(pivX + dx*cos_a - dy2*sin_a),
					y: Math.round(pivY + dx*sin_a + dy2*cos_a)
				};
			};

			// Geometry constants (y-down, all relative to pBy anchor)
			var wY     = pBy - 24;   // wing root row
			var engY   = pBy - 28;   // engine nacelle centre
			var finBY  = pBy - 14;   // fin base / fuselage top
			var finTY  = pBy - 110;  // fin top
			var stY    = finTY + 6;  // stabiliser attachment row
			var wDih   = 7;          // dihedral: tips rise above root (px)
			var wSpan  = 140;        // wing half-span (fuselage edge to tip)
			var engX   = 52;         // engine lateral offset
			var stSpan = 54;         // stabiliser half-span

			// ── 1. Main wings — wide, thin, slight upward dihedral ──
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPFillPolygonNumeric([          // left wing lower surface
				rp(pCx-13,    wY+7),
				rp(pCx-wSpan, wY-wDih+7),
				rp(pCx-wSpan, wY-wDih+1),
				rp(pCx-13,    wY+1)
			]);
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFillPolygonNumeric([          // left wing upper surface
				rp(pCx-13,    wY+1),
				rp(pCx-wSpan, wY-wDih+1),
				rp(pCx-wSpan, wY-wDih-4),
				rp(pCx-13,    wY-4)
			]);
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPFillPolygonNumeric([          // right wing lower surface
				rp(pCx+13,    wY+7),
				rp(pCx+wSpan, wY-wDih+7),
				rp(pCx+wSpan, wY-wDih+1),
				rp(pCx+13,    wY+1)
			]);
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFillPolygonNumeric([          // right wing upper surface
				rp(pCx+13,    wY+1),
				rp(pCx+wSpan, wY-wDih+1),
				rp(pCx+wSpan, wY-wDih-4),
				rp(pCx+13,    wY-4)
			]);
			var ltp = rp(pCx-wSpan, wY-wDih+1);
			var rtp = rp(pCx+wSpan, wY-wDih+1);
			cmds += RIPFillStyleNumeric(1, RC_RED);
			cmds += RIPFilledOvalNumeric(ltp.x, ltp.y, 9, 6);  // left wingtip
			cmds += RIPFilledOvalNumeric(rtp.x, rtp.y, 9, 6);  // right wingtip

			// ── 2. Narrow fuselage centre section ──
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFillPolygonNumeric([
				rp(pCx-11, finBY), rp(pCx+11, finBY),
				rp(pCx+11, wY+5),  rp(pCx-11, wY+5)
			]);

			// ── 3. Aft engine nacelles — face-on ovals, centres rotated ──
			var lep = rp(pCx-engX, engY);
			var rep = rp(pCx+engX, engY);
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFilledOvalNumeric(lep.x, lep.y, 27, 21);
			cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
			cmds += RIPFilledOvalNumeric(lep.x, lep.y, 21, 16);
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPFilledOvalNumeric(lep.x, lep.y, 13, 10);
			cmds += RIPFillStyleNumeric(1, RC_YELLOW);
			cmds += RIPFilledOvalNumeric(lep.x, lep.y,  6,  5);
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFilledOvalNumeric(rep.x, rep.y, 27, 21);
			cmds += RIPFillStyleNumeric(1, RC_LTGRAY);
			cmds += RIPFilledOvalNumeric(rep.x, rep.y, 21, 16);
			cmds += RIPFillStyleNumeric(1, RC_DKGRAY);
			cmds += RIPFilledOvalNumeric(rep.x, rep.y, 13, 10);
			cmds += RIPFillStyleNumeric(1, RC_YELLOW);
			cmds += RIPFilledOvalNumeric(rep.x, rep.y,  6,  5);

			// ── 4. Red T-tail vertical fin ──
			cmds += RIPFillStyleNumeric(1, RC_RED);
			cmds += RIPFillPolygonNumeric([
				rp(pCx-8, finBY), rp(pCx+8, finBY),
				rp(pCx+4, finTY), rp(pCx-4, finTY)
			]);
			cmds += RIPFillStyleNumeric(1, RC_WHITE);  // accent stripe
			cmds += RIPFillPolygonNumeric([
				rp(pCx-2, finBY-8), rp(pCx+2, finBY-8),
				rp(pCx+2, finTY+18), rp(pCx-2, finTY+18)
			]);

			// ── 5. White horizontal stabilisers with red tips ──
			cmds += RIPFillStyleNumeric(1, RC_WHITE);
			cmds += RIPFillPolygonNumeric([            // left stab
				rp(pCx-4,      stY+4),  rp(pCx-4,      stY+10),
				rp(pCx-stSpan, stY+13), rp(pCx-stSpan, stY+7)
			]);
			cmds += RIPFillPolygonNumeric([            // right stab
				rp(pCx+4,      stY+4),  rp(pCx+4,      stY+10),
				rp(pCx+stSpan, stY+13), rp(pCx+stSpan, stY+7)
			]);
			var lstp = rp(pCx-stSpan, stY+10);
			var rstp = rp(pCx+stSpan, stY+10);
			cmds += RIPFillStyleNumeric(1, RC_RED);
			cmds += RIPFilledOvalNumeric(lstp.x, lstp.y, 8, 5);
			cmds += RIPFilledOvalNumeric(rstp.x, rstp.y, 8, 5);
		}

		ripSend(cmds);

		// Text-layer HUD (prints over graphics in RIP terminal)
		var alt = Math.round(Math.max(0,cam3d.y-0.3)*1000);
		var wNames = ["Sunny","Cloudy","Rain","Storm"];
		console.gotoxy(1, 1);
		console.print(A_HUD+" ALT:"+alt+"ft  SPD:340kts  DST:"+Math.floor(cam3d.z*10)+
		              "  SCR:"+score+"  "+wNames[gWeather.type]+"  [Q]=Quit "+CN);
		console.line_counter = 0;

		if (explActive) {
			explFrame++;
			if (explFrame > 8) gameOver = true;
		}
	}

	// Restore text display
	ripSend(RIPEraseGraphicsWindow());
	console.clear();
	return { score:score, distance:Math.floor(cam3d.z*10), msg:gameOverMsg, quit:quitGame,
	         frames:flightFrames, altAvg:altAccum/Math.max(1,flightFrames), altMax:altMax,
	         speedAvg:speedAccum/Math.max(1,flightFrames) };
}

// ═══════════════════════════════════════════════════════════
//  MODE SELECTION SCREEN
// ═══════════════════════════════════════════════════════════

// Display the animated mode-selection screen and wait for the player to press
// 1 (side-scroll), 2 (3D perspective), 3 (RIP — only if terminal supports it),
// or Q (quit).  Returns the pressed key as a string.
function showModeSelect() {
	var hasRIP = console.term_supports(USER_RIP);
	initBuffers();
	fillRect(1, 1, ROWS, COLS, CN+BG0+CK, " ");

	// Starfield background
	for (var i=0; i<60; i++)
		putCell(Math.floor(Math.random()*(ROWS-2))+1,
		        Math.floor(Math.random()*COLS)+1, A_STAR, CP437_BULLET_OPERATOR);

	// ── Title ──
	var title = CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE
	          + "  " + gProgramName + " v" + gProgramVersion + " (" + gProgramDate + ")  " + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE
	          + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE + CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE;
	writeStr(2, Math.floor((COLS-title.length)/2)+1, CN+BG0+CH+CY, title);
	writeStr(4, Math.floor((COLS-22)/2)+1, CN+BG0+CH+CW, "Choose your flight mode:");

	// ── Option 1: Side-Scroll ──
	var o1box = "  [1]  Side-Scroll Mode  ";
	var o1des = "Classic horizontal view - side profile of the plane";
	writeStr(6, Math.floor((COLS-o1box.length)/2)+1, CN+BG4+CH+CY, o1box);
	writeStr(7, Math.floor((COLS-o1des.length)/2)+1, CN+BG0+CH+CW, o1des);
	var p1x = Math.floor((COLS-11)/2)+1;
	writeStr(8,  p1x, CN+BG0+CW,    "     /\\    ");
	writeStr(9,  p1x, CN+BG0+CH+CW, "|====O====>");
	writeStr(10, p1x, CN+BG0+CW,    "     \\/    ");

	// ── Option 2: 3D Perspective ──
	var o2box = "  [2]  3D Perspective Mode  ";
	var o2des = "Forward view, banking horizon, biome scenery";
	writeStr(12, Math.floor((COLS-o2box.length)/2)+1, CN+BG4+CH+CC, o2box);
	writeStr(13, Math.floor((COLS-o2des.length)/2)+1, CN+BG0+CH+CW, o2des);
	var p2x = Math.floor((COLS-11)/2)+1;
	writeStr(14, p2x, CN+BG0+CH+CY, "    _|_    ");
	writeStr(15, p2x, CN+BG0+CH+CW, "===/   \\===");
	writeStr(16, p2x, CN+BG0+CH+CC, "   |(*)|   ");

	// ── Option 3: RIP (conditional) ──
	if (hasRIP) {
		var o3box = "  [3]  RIP Vector Graphics Mode  ";
		var o3des = "640x350 vector graphics - mountains, clouds & plane";
		writeStr(18, Math.floor((COLS-o3box.length)/2)+1, CN+BG4+CH+CG, o3box);
		writeStr(19, Math.floor((COLS-o3des.length)/2)+1, CN+BG0+CH+CW, o3des);
	}

	var pk = hasRIP ? "[ Press 1, 2 or 3" : "[ Press 1 or 2";
	pk += " (Q=Quit) ]"
	writeStr(ROWS-1, Math.floor((COLS-pk.length)/2)+1, CN+BG0+CH+CG, pk);

	renderFrame();
	while (bbs.online && !js.terminated) {
		var k = console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, 60000);
		if (k === "1" || k === "2") return k;
		if (k === "3" && hasRIP) return k;
		if (k.toUpperCase() === "Q") return "Q";
	}
	return "Q";
}

// ═══════════════════════════════════════════════════════════
//  GAME OVER / DEBRIEF SCREEN
// ═══════════════════════════════════════════════════════════

// Show either the flight debrief (if the player quit voluntarily) or a
// crash / game-over dialog box with score and distance, then wait for a keypress.
function showGameOver(result) {
	if (result.quit) {
		showFlightDebrief(result);
		return;
	}
	// Crash / game-over box
	var bw=46, bh=9;
	var bx=Math.floor((COLS-bw)/2)+1, by=Math.floor((ROWS-bh)/2);
	fillRect(by, bx, by+bh-1, bx+bw-1, CN+BG0+CH+CR, " ");
	writeStr(by,      bx, CN+BG0+CH+CR, CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE+repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,bw-2)+CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE);
	writeStr(by+bh-1, bx, CN+BG0+CH+CR, CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE+repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,bw-2)+CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE);
	for (var r=by+1; r<by+bh-1; r++) {
		putCell(r, bx,      CN+BG0+CH+CR, CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
		putCell(r, bx+bw-1, CN+BG0+CH+CR, CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
	}
	writeStr(by+1, bx+Math.floor((bw-16)/2), CN+BG0+CH+CY, "*** GAME OVER ***");
	writeStr(by+2, bx+1, CN+BG0+CH+CR, repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, bw-2));
	var msg = (result.msg||"").substring(0,bw-4);
	writeStr(by+3, bx+Math.floor((bw-msg.length)/2), CN+BG0+CH+CW, msg);
	var sl = "Score: "+result.score;
	writeStr(by+4, bx+Math.floor((bw-sl.length)/2), CN+BG0+CH+CG, sl);
	var dl = "Distance: "+result.distance+" units";
	writeStr(by+5, bx+Math.floor((bw-dl.length)/2), CN+BG0+CH+CG, dl);
	var cont = "[ Press any key ]";
	writeStr(by+6, bx+Math.floor((bw-cont.length)/2), CN+BG0+CH+CC, cont);
	renderFrame();
	console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, 30000);
}

// Display a full-screen post-flight statistics panel: flight time, distance,
// average and maximum altitude, average speed, and final score.
// Waits up to 60 seconds for the player to acknowledge before returning.
function showFlightDebrief(result) {
	// Format MM:SS flight time from frame count
	var totalSec = Math.floor((result.frames || 0) * FRAME_MS / 1000);
	var tMins = Math.floor(totalSec / 60);
	var tSecs = totalSec % 60;
	var timeStr = tMins + "m " + (tSecs < 10 ? "0" : "") + tSecs + "s";

	// Altitude in feet (cam3d.y - 0.3 gives AGL, * 1000 for feet)
	var avgAltFt = Math.round(Math.max(0, (result.altAvg || 0) - 0.3) * 1000);
	var maxAltFt = Math.round(Math.max(0, (result.altMax || 0) - 0.3) * 1000);

	// Speed in knots (cam3d.speed 0.35 ≈ 340 kts as shown on HUD)
	var avgSpd = Math.round(((result.speedAvg || 0.35) / 0.35) * 340);

	var rows = [
		"Flight time:       " + timeStr,
		"Distance flown:    " + result.distance + " nm",
		"Average altitude:  " + avgAltFt + " ft",
		"Max altitude:      " + maxAltFt + " ft",
		"Avg ground speed:  " + avgSpd + " kts",
		"Avg air speed:     " + avgSpd + " kts",
		"Score:             " + result.score
	];

	// Size box to fit content
	var bw = 48, bh = rows.length + 6;
	var bx = Math.floor((COLS - bw) / 2) + 1;
	var by = Math.floor((ROWS - bh) / 2);

	initBuffers();
	fillRect(1, 1, ROWS, COLS, CN+BG0+CK, " ");

	// Box border
	fillRect(by, bx, by+bh-1, bx+bw-1, CN+BG4+CW, " ");
	writeStr(by,      bx, CN+BG4+CH+CY, CP437_BOX_DRAWINGS_UPPER_LEFT_DOUBLE+repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,bw-2)+CP437_BOX_DRAWINGS_UPPER_RIGHT_DOUBLE);
	writeStr(by+bh-1, bx, CN+BG4+CH+CY, CP437_BOX_DRAWINGS_LOWER_LEFT_DOUBLE+repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE,bw-2)+CP437_BOX_DRAWINGS_LOWER_RIGHT_DOUBLE);
	for (var r = by+1; r < by+bh-1; r++) {
		putCell(r, bx,      CN+BG4+CH+CY, CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
		putCell(r, bx+bw-1, CN+BG4+CH+CY, CP437_BOX_DRAWINGS_DOUBLE_VERTICAL);
	}

	// Title
	var title = " FLIGHT DEBRIEF ";
	writeStr(by+1, bx+Math.floor((bw-title.length)/2), CN+BG4+CH+CY, title);
	writeStr(by+2, bx+1, CN+BG4+CH+CY, repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, bw-2));

	// Stat rows
	for (var i = 0; i < rows.length; i++) {
		writeStr(by+3+i, bx+2, CN+BG4+CH+CW, rows[i]);
	}

	// Separator and prompt
	writeStr(by+3+rows.length, bx+1, CN+BG4+CH+CY, repeatChar(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, bw-2));
	var cont = "[ Press any key to return to menu ]";
	writeStr(by+4+rows.length, bx+Math.floor((bw-cont.length)/2), CN+BG4+CH+CC, cont);

	renderFrame();
	console.inkey(K_NOSPIN|K_NOCRLF|K_NOECHO, 60000);
}

// ═══════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════

if (!console.term_supports(USER_ANSI)) {
	console.print(CN + CR + gProgramName + " requires an ANSI terminal!\r\n");
	exit(1);
}

// Read settings
var gSettings = readCfgFile();

console.write("\x1b[?25l");  // Hide cursor

var gScrollTick = 0;  // Global tick counter (used by both modes)

// ── Weather system (shared by ASCII-3D and RIP modes) ──
// Types: SUNNY, CLOUDY, RAINY, THUNDERSTORM (see weather constants above)
var gWeather = { type: SUNNY, nextChange: 0, lightning: false, lightningFlash: 0 };

// Randomly select a weather type using the configured weighted probabilities.
// Each probability is a 0.0–1.0 value from gSettings.weather; they are treated
// as cumulative thresholds against a single random roll.
function pickWeather() {
	var sunnyProb       = gSettings.weather.sunny_probability;
	var cloudyProb      = gSettings.weather.cloudy_probability;
	var rainyProb       = gSettings.weather.rainy_probability;
	var r = Math.random();
	if (r < sunnyProb)
		return SUNNY;
	else if (r < sunnyProb + cloudyProb)
		return CLOUDY;
	else if (r < sunnyProb + cloudyProb + rainyProb)
		return RAINY;
	else
		return THUNDERSTORM;
}

// Initialise the weather state at the start of a 3D or RIP flight session.
// Picks an initial weather type and schedules the first weather change between
// 200 and 500 frames from now.
function initWeather() {
	gWeather.type          = pickWeather();
	gWeather.nextChange    = 200 + Math.floor(Math.random() * 300);
	gWeather.lightning     = false;
	gWeather.lightningFlash = 0;
}

// Called once per frame in 3D/RIP modes.  Counts down to the next weather
// change and handles lightning: in a thunderstorm a flash lasts ~2 frames,
// triggered at a 20% per-frame probability.
function updateWeather() {
	if (--gWeather.nextChange <= 0) {
		gWeather.type       = pickWeather();
		gWeather.nextChange = 200 + Math.floor(Math.random() * 300);
	}
	// Lightning flashes in thunderstorms (20% chance per frame to start a flash)
	if (gWeather.lightningFlash > 0) {
		gWeather.lightningFlash--;
		gWeather.lightning = (gWeather.lightningFlash > 0);
	} else if (gWeather.type === THUNDERSTORM && Math.random() < 0.20) {
		gWeather.lightningFlash = 2;  // lasts ~2 frames
		gWeather.lightning = true;
	} else {
		gWeather.lightning = false;
	}
}

// Reads the configuration file & returns an object with the settings.
function readCfgFile()
{
	var settings = {
		weather: {
			sunny_probability: 0.60,
			cloudy_probability: 0.13,
			rainy_probability: 0.13,
			thunderstorm_probability: 0.14
		}
	};

	var cfgFileName = genFullPathCfgFilename("flight_sim.ini", js.exec_dir);
	// If the configuration file hasn't been found, look to see if there's a gttrivia.example.ini file
	// available in the same directory as the script
	if (!file_exists(cfgFileName))
	{
		var exampleFileName = file_cfgname(js.exec_dir, "flight_sim.example.ini");
		if (file_exists(exampleFileName))
			cfgFileName = exampleFileName;
	}
	var iniFile = new File(cfgFileName);
	if (iniFile.open("r"))
	{
		var weatherSettings = iniFile.iniGetObject("WEATHER");
		iniFile.close();
		if (weatherSettings != null && typeof(weatherSettings) === "object")
		{
			if (typeof(weatherSettings.sunny_probability) === "number" && weatherSettings.sunny_probability >= 0.0 && weatherSettings.sunny_probability <= 1.0)
				settings.weather.sunny_probability = weatherSettings.sunny_probability;
			if (typeof(weatherSettings.cloudy_probability) === "number" && weatherSettings.cloudy_probability >= 0.0 && weatherSettings.cloudy_probability <= 1.0)
				settings.weather.cloudy_probability = weatherSettings.cloudy_probability;
			if (typeof(weatherSettings.rainy_probability) === "number" && weatherSettings.rainy_probability >= 0.0 && weatherSettings.rainy_probability <= 1.0)
				settings.weather.rainy_probability = weatherSettings.rainy_probability;
			if (typeof(weatherSettings.thunderstorm_probability) === "number" && weatherSettings.thunderstorm_probability >= 0.0 && weatherSettings.thunderstorm_probability <= 1.0)
				settings.weather.thunderstorm_probability = weatherSettings.thunderstorm_probability;
		}
	}

	return settings;
}
// For configuration files, this function returns a fully-pathed filename.
// This function first checks to see if the file exists in the sbbs/mods
// directory, then the sbbs/ctrl directory, and if the file is not found there,
// this function defaults to the given default path.
//
// Parameters:
//  pFilename: The name of the file to look for
//  pDefaultPath: The default directory (must have a trailing separator character)
function genFullPathCfgFilename(pFilename, pDefaultPath)
{
	var fullyPathedFilename = system.mods_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
		fullyPathedFilename = system.ctrl_dir + pFilename;
	if (!file_exists(fullyPathedFilename))
	{
		if (typeof(pDefaultPath) === "string")
		{
			// Make sure the default path has a trailing path separator
			var defaultPath = pDefaultPath;
			if (defaultPath.length > 0 && defaultPath.charAt(defaultPath.length-1) != "/" && defaultPath.charAt(defaultPath.length-1) != "\\")
				defaultPath += "/";
			fullyPathedFilename = defaultPath + pFilename;
		}
		else
			fullyPathedFilename = pFilename;
	}
	return fullyPathedFilename;
}

var keepPlaying = true;
while (keepPlaying && bbs.online && !js.terminated) {
	var mode = showModeSelect();
	if (mode === "Q" || !bbs.online || js.terminated) break;

	var result;
	if (mode === "1") {
		result = runSideScroll();
	} else if (mode === "2") {
		result = run3D();
	} else {
		result = runRIP();
	}

	if (!bbs.online || js.terminated) break;

	showGameOver(result);

	// After any flight (quit or crash), always return to mode select
}

console.write("\x1b[?25h");  // Restore cursor
console.print(CN);
console.gotoxy(1, ROWS);
console.newline();
console.print(CN+CG+"Thanks for flying " + gProgramName + "!\r\n");
console.line_counter = 0;
