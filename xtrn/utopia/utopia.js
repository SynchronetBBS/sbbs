"use strict";
load("sbbsdefs.js");
// --- CONFIG & FILES ---
var INTRO_FILE = js.exec_dir + "utopia_intro.txt";
var HELP_FILE = js.exec_dir + "utopia_help.txt";
var SAVE_FILE = js.exec_dir + "utopia_save_" + user.alias.replace(/\W/g, '') + ".json";
var SCORE_FILE = js.exec_dir + "utopia_scores.json";
var BASE_DESTRUCTION_COST = 5;
var BUILDINGS = {
	"H": { char: "\x7F", cost: 10, name: "Housing", shadow: "+15 PopCap", land: true, sea: false },
	"F": { char: "\xB1", cost: 20, name: "Farm", shadow: "+2 Food/t", land: true, sea: false },
	"I": { char: "\xDC", cost: 50, name: "Industry", shadow: "+5 Gold/t", land: true, sea: false },
	"B": { char: "\x1E", cost: 25, name: "PT Boat", shadow: "Patrols", land: false, sea: true },
	"S": { char: "\x15", cost: 100, name: "School", shadow: "-Rebels", land: true, sea: false },
	"R": { char: "\xCD", cost: 15, name: "Bridge", shadow: "Transit", land: true, sea: true },
	"T": { char: "\xCF", cost: 40, name: "Trawler", shadow: "Fish Catch", land: false, sea: true },
	"C": { char: "\xFE", cost: 250, name: "Cmd Ctr", shadow: "Radar/Def", land: true, sea: false }
};
var MAX_FISH = 8;
var MAX_TURNS = 500;
var Utopia = {
	state: {
		running: true,
		turn: 1,
		lastTick: system.timer,
		gold: 500,
		food: 500,
		pop: 20,
		popCap: 25,
		rebels: 0,
		martialLaw: 0,
		cursor: { x: 30, y: 11 },
		msg: "\x01h\x01wGovernor Online.",
		log: [],
		fishPool: [],
		merchant: { x: -1, y: 0, active: false },
		pirate: { x: -1, y: 0, active: false },
		storm: { x: -1, y: 0, active: false },
		stats: { piratesSunk: 0, tradesWon: 0, boatsLost: 0 }
	},
	map: [],
	bldgMap: [],
	pendingUpdate: [],
};
// --- CORE UTILITIES ---
Utopia.log = function(msg) {
	log("Utopia: " + msg);
	this.state.log.push({turn: this.state.turn, msg: msg });
	this.state.msg = msg;
}
Utopia.spawnFish = function() {
	while (this.state.fishPool.length < MAX_FISH) {
		var rx = Math.floor(Math.random() * 60);
		var ry = Math.floor(Math.random() * 22);
		// Only spawn in water (0) and not on a building
		if (this.map[ry][rx] === 0 && !this.bldgMap[ry][rx]) {
			this.state.fishPool.push({ x: rx, y: ry });
		}
	}
};
Utopia.drawCell = function(x, y, more_attr) {
	x = Math.floor(x);
	y = Math.floor(y);
	if (x < 0 || x >= 60 || y < 0 || y >= 22) return;
	var isLand = (this.map[y][x] === 1);
	var attr = isLand ? (BG_GREEN | BLACK) : (BG_BLUE | LIGHTBLUE);
	var char = isLand ? " " : "\xF7";
	// Check Fish Pool
	for (var i = 0; i < this.state.fishPool.length; i++) {
		if (this.state.fishPool[i].x === x && this.state.fishPool[i].y === y) {
			char = "\xE0";
			attr = (BG_BLUE | CYAN | HIGH);
		}
	}
	if (this.state.merchant.active && Math.floor(this.state.merchant.x) === x && Math.floor(this.state.merchant.y) === y) {
		char = "$";
		attr = BG_BLUE | YELLOW | HIGH;
	}
	if (this.state.pirate.active && Math.floor(this.state.pirate.x) === x && Math.floor(this.state.pirate.y) === y) {
		char = "\x9E";
		attr = BG_BLUE | RED | HIGH;
	}
	if (this.state.storm.active) {
		var sx = Math.floor(this.state.storm.x),
			sy = Math.floor(this.state.storm.y);
		if ((x === sx && y === sy) || (x === sx && Math.abs(y - sy) === 1) || (y === sy && Math.abs(x - sx) === 1)) {
			char = "\xDB";
			attr = BG_BLACK | WHITE | HIGH;
		}
	}
	if (this.bldgMap[y][x]) {
		char = BUILDINGS[this.bldgMap[y][x]].char;
		attr = isLand ? (BG_GREEN | WHITE | HIGH) : (BG_BLUE | WHITE | HIGH);
	}
	if (x == this.state.cursor.x && y == this.state.cursor.y) {
		attr = BG_RED | WHITE | BLINK;
		if (this.bldgMap[y][x]) char = BUILDINGS[this.bldgMap[y][x]].char;
	}
	else
		attr |= more_attr;
	console.gotoxy(x + 1, y + 1);
	console.attributes = attr;
	console.write(char);
};
Utopia.forceScrub = function(x, y, size) {
	var radius = size || 1;
	for (var dx = -radius; dx <= radius; dx++) {
		for (var dy = -radius; dy <= radius; dy++) this.drawCell(x + dx, y + dy);
	}
};
Utopia.handlePendingUpdates = function() {
	while(this.pendingUpdate.length) { // Erase the caught fish, sunk boats, etc.
		var cell = this.pendingUpdate.pop();
		this.drawCell(cell.x, cell.y);
	}
}
// --- SIMULATION ---
Utopia.handlePopulation = function() {
	this.state.food = Math.max(0, this.state.food - Math.floor(this.state.pop / 5));
	if (this.state.turn % 5 === 0) {
		if (this.state.food > this.state.pop && this.state.pop < this.state.popCap) {
			var growth = Math.floor(Math.random() * 3) + 1;
			this.state.pop = Math.min(this.state.popCap, this.state.pop + growth);
			this.log("\x01h\x01gPopulation Growth!");
		}
		else if (this.state.food <= 0 && this.state.pop > 5) {
			this.state.pop -= Math.floor(Math.random() * 2) + 1;
			this.log("\x01h\x01rFAMINE!");
		}
	}
	if (this.state.food > 0) this.state.gold += Math.floor(this.state.pop / 10);
};
Utopia.handlePatrols = function() {
	if (!this.state.pirate.active) return;
	var px = Math.floor(this.state.pirate.x),
		py = Math.floor(this.state.pirate.y);
	for (var y = 0; y < 22; y++) {
		for (var x = 0; x < 60; x++) {
			if (this.bldgMap[y][x] === "B") {
				var dist = Math.sqrt(Math.pow(x - px, 2) + Math.pow(y - py, 2));
				if (dist < 10) {
					var nx = x + (px > x ? 1 : (px < x ? -1 : 0)),
						ny = y + (py > y ? 1 : (py < y ? -1 : 0));
					if (ny >= 0 && ny < 22 && nx >= 0 && nx < 60 && this.map[ny][nx] === 0 && !this.bldgMap[ny][nx]) {
						this.bldgMap[ny][nx] = "B";
						this.bldgMap[y][x] = null;
						this.drawCell(x, y);
						this.drawCell(nx, ny);
					}
					if (Math.abs(nx - px) <= 1 && Math.abs(ny - py) <= 1) {
						if (Math.random() < 0.7) {
							this.drawCell(px, py, BLINK);
							this.state.pirate.active = false;
							this.state.stats.piratesSunk++;
							this.log("\x01h\x01bPirate SUNK by PT Boat!");
							this.pendingUpdate.push({ x: px, y: py});
						}
						else {
							this.drawCell(nx, ny, BLINK);
							this.bldgMap[ny][nx] = null;
							this.state.stats.boatsLost++;
							this.log("\x01h\x01rPT Boat SUNK by Pirate!");
							this.pendingUpdate.push({ x: nx, y: ny});
						}
						return;
					}
					return;
				}
			}
		}
	}
};
Utopia.moveEntities = function() {
	this.handlePatrols();
	var oldMerc = {
		x: Math.floor(this.state.merchant.x),
		y: Math.floor(this.state.merchant.y),
		active: this.state.merchant.active
	};
	var oldPirate = {
		x: Math.floor(this.state.pirate.x),
		y: Math.floor(this.state.pirate.y),
		active: this.state.pirate.active
	};
	var oldStorm = {
		x: Math.floor(this.state.storm.x),
		y: Math.floor(this.state.storm.y),
		active: this.state.storm.active
	};
	// Fish
	// Iterate backwards through the fish pool so we can safely remove caught fish
	for (var i = this.state.fishPool.length - 1; i >= 0; i--) {
		var f = this.state.fishPool[i];
		var oldX = f.x,
			oldY = f.y;
		if (Math.random() < 0.4) {
			var nx = f.x + (Math.random() > 0.5 ? 1 : -1);
			var ny = f.y + (Math.random() > 0.5 ? 1 : -1);
			// If fish leaves map, remove it
			if (nx < 0 || nx > 59 || ny < 0 || ny > 21) {
				this.state.fishPool.splice(i, 1);
				this.drawCell(oldX, oldY);
				continue;
			}
			// Move if target is water
			if (this.map[ny][nx] === 0) {
				f.x = nx;
				f.y = ny;
				this.drawCell(oldX, oldY);
				this.drawCell(f.x, f.y);
				// Check for Trawlers nearby
				for (var dx = -1; dx <= 1; dx++) {
					for (var dy = -1; dy <= 1; dy++) {
						var tx = f.x + dx,
							ty = f.y + dy;
						if (ty >= 0 && ty < 22 && tx >= 0 && tx < 60) {
							if (this.bldgMap[ty][tx] === "T") {
								this.state.food += 25;
								this.log("\x01h\x01gFish Trawled!");
								this.drawCell(f.x, f.y, BLINK);
								this.state.fishPool.splice(i, 1); // Fish is caught
								this.pendingUpdate.push(f);
								break;
							}
						}
					}
				}
			}
		}
	}
	// Replenish the pool
	this.spawnFish();
	// Merchant (West to East)
	if (this.state.merchant.active) {
		this.state.merchant.x += 0.5;
		this.state.gold += 1;
		var mx = Math.floor(this.state.merchant.x),
			my = Math.floor(this.state.merchant.y);
		if (mx >= 59) {
			this.state.merchant.active = false;
			this.state.gold += 150;
			this.state.stats.tradesWon++;
			this.log("\x01h\x01yTrade Bonus!");
			this.forceScrub(mx, my, 1);
		}
		else if (this.map[my][mx] === 1) {
			this.state.merchant.active = false;
			this.forceScrub(mx, my, 1);
		}
	}
	else if (Math.random() < 0.03) {
		var ry = Math.floor(Math.random() * 20);
		if (this.map[ry][0] === 0) {
			this.state.merchant.active = true;
			this.state.merchant.x = 0;
			this.state.merchant.y = ry;
		}
	}
	// Pirate (East to West)
	if (this.state.pirate.active) {
		this.state.pirate.x -= 0.6;
		var px = Math.floor(this.state.pirate.x),
			py = Math.floor(this.state.pirate.y);
		if (this.state.merchant.active && Math.abs(px - Math.floor(this.state.merchant.x)) <= 1 && Math.abs(py - Math.floor(this.state.merchant.y)) <= 1) {
			this.state.merchant.active = false;
			this.log("\x01h\x01rMerchant Sunk!");
			this.forceScrub(Math.floor(this.state.merchant.x), Math.floor(this.state.merchant.y), 1);
		}
		if (px <= 0) {
			this.state.gold = Math.max(0, this.state.gold - 100);
			this.state.pirate.active = false;
			this.forceScrub(0, py, 1);
		}
		else if (this.map[py][px] === 1) {
			this.state.pirate.active = false;
			this.forceScrub(px, py, 1);
		}
	}
	else if (Math.random() < 0.02) {
		var ry = Math.floor(Math.random() * 20);
		if (this.map[ry][59] === 0) {
			this.state.pirate.active = true;
			this.state.pirate.x = 59;
			this.state.pirate.y = ry;
		}
	}
	// Storm
	if (this.state.storm.active) {
		this.state.storm.x -= 0.5;
		var sx = Math.floor(this.state.storm.x),
			sy = Math.floor(this.state.storm.y);
		if (sx < 1) {
			this.state.storm.active = false;
			this.forceScrub(0, sy, 2);
		}
		else if (this.bldgMap[sy][sx]) {
			this.bldgMap[sy][sx] = null;
			this.log("\x01h\x01rSTORM DAMAGE!");
		}
	}
	else if (Math.random() < 0.01) {
		this.state.storm.active = true;
		this.state.storm.x = 58;
		this.state.storm.y = 1 + Math.floor(Math.random() * 18);
	}
	// Redraw
	if (oldMerc.active) this.drawCell(oldMerc.x, oldMerc.y);
	if (oldPirate.active) this.drawCell(oldPirate.x, oldPirate.y);
	if (oldStorm.active)
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++) this.drawCell(oldStorm.x + dx, oldStorm.y + dy);
	if (this.state.merchant.active) this.drawCell(this.state.merchant.x, this.state.merchant.y);
	if (this.state.pirate.active) this.drawCell(this.state.pirate.x, this.state.pirate.y);
	if (this.state.storm.active) {
		var nsx = Math.floor(this.state.storm.x),
			nsy = Math.floor(this.state.storm.y);
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++) this.drawCell(nsx + dx, nsy + dy);
	}
	console.flush();
};
// --- SCORING & UI ---
Utopia.calculateScore = function() {
	return (this.state.pop * 10) + Math.floor(this.state.gold / 10) + this.state.food - (this.state.rebels * 5);
};
Utopia.getRank = function() {
	var score = this.calculateScore();
	if (this.state.stats.piratesSunk >= 10) return "Grand Admiral";
	if (this.state.stats.tradesWon >= 10) return "Merchant Prince";
	if (score > 5000) return "Utopian Visionary";
	if (this.state.pop > 200) return "Metropolitan Governor";
	return "Local Magistrate";
};
Utopia.drawUI = function(legend) {
	var lx = 62;
	if (legend) {
		console.gotoxy(lx, 1);
		console.putmsg("--- Legend ---");
		var keys = ["H", "F", "I", "B", "S", "R", "T", "C"];
		for (var i = 0; i < keys.length; i++) {
			console.gotoxy(lx, 2 + i);
			console.attributes = BG_BLACK | CYAN;
			console.putmsg("\x01h\x01w" + BUILDINGS[keys[i]].char + " \x01n" + BUILDINGS[keys[i]].name);
		}
	}
	console.gotoxy(lx, 11);
	console.putmsg("\x01h\x01wRebels: " + this.state.rebels + "%  ");
	console.gotoxy(lx, 12);
	console.putmsg("\x01h\x01wD-Cost: \x01y$" + (BASE_DESTRUCTION_COST + Math.floor(this.state.rebels / 2)));
	console.gotoxy(lx, 13);
	console.putmsg("\x01h\x01wM-Law:  " + (this.state.martialLaw > 0 ? "\x01h\x01rON " : "\x01nOFF"));
	if (this.state.lastbuilt) {
		console.gotoxy(lx, 14);
		console.putmsg("\x01h\x01wSPACE:  " + BUILDINGS[this.state.lastbuilt].name);
	}
	console.gotoxy(1, 23);
	console.attributes = BG_BLACK | LIGHTGRAY;
	console.putmsg(format("\x01h\x01yGold:\x01n %-5d \x01h\x01gFood:\x01n %-5d \x01h\x01wPop:\x01n %d/%-5d \x01h\x01cTurn:\x01n %d/%d"
		, this.state.gold, this.state.food, this.state.pop, this.state.popCap, this.state.turn, MAX_TURNS));
	console.cleartoeol();
	console.gotoxy(1, 24);
	var hasB = !!(this.bldgMap[this.state.cursor.y][this.state.cursor.x]);
	var dStr = (hasB ? "\x01h" : "\x01n") + "[D]estroy";
	console.putmsg((hasB ? "\x01n" : "\x01h") + "[B]uild " + dStr + " \x01h\x01w[M]Law [Q]uit [?]Help | " + this.state.msg);
	console.cleartoeol();
};
Utopia.build = function(cmd) {
	var b = BUILDINGS[cmd],
		ter = this.map[this.state.cursor.y][this.state.cursor.x];
	if (this.bldgMap[this.state.cursor.y][this.state.cursor.x]) this.state.msg = "Occupied!";
	else if (!((ter === 1 && b.land) || (ter === 0 && b.sea))) this.state.msg = "Wrong Terrain!";
	else if (this.state.gold < b.cost) this.state.msg = "Low Gold!";
	else {
		this.state.gold -= b.cost;
		this.bldgMap[this.state.cursor.y][this.state.cursor.x] = cmd;
		if (cmd === "H") this.state.popCap += 15;
		this.drawCell(this.state.cursor.x, this.state.cursor.y);
		this.state.lastbuilt = cmd;
		this.log("Built " + b.name + " for $" + b.cost);
	}
}
Utopia.showBuildMenu = function() {
	var menuX = 12,
		menuY = 5;
	for (var i = 0; i < 11; i++) {
		console.gotoxy(menuX, menuY + i);
		console.attributes = BG_BLACK | LIGHTGRAY;
		console.write("                                              ");
	}
	var keys = ["H", "F", "I", "B", "S", "R", "T", "C"];
	console.gotoxy(menuX + 2, menuY + 1);
	console.putmsg("\x01h\x01bCONSTRUCTION MENU");
	for (var i = 0; i < keys.length; i++) {
		var b = BUILDINGS[keys[i]];
		console.gotoxy(menuX + 2, menuY + 2 + i);
		console.putmsg(format("\x01h\x01b(%s)\x01n %-10s \x01h\x01g$%3d \x01b\xAF \x01h%s", keys[i], b.name, b.cost, b.shadow));
	}
	var cmd = console.getkey(K_UPPER | K_NOSPIN);
	for (var y = 0; y < 22; y++)
		for (var x = 0; x < 60; x++) this.drawCell(x, y);
	if (BUILDINGS[cmd])
		this.build(cmd);
	this.drawUI();
};
Utopia.saveHighScore = function() {
	var score = this.calculateScore(),
		scores = [];
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			scores = JSON.parse(f.read());
			f.close();
		}
	}
	scores.push({ name: user.alias, score: score, pop: this.state.pop, date: new Date().toLocaleDateString() });
	scores.sort(function(a, b) { return b.score - a.score; });
	var f = new File(SCORE_FILE);
	if (f.open("w")) {
		f.write(JSON.stringify(scores.slice(0, 10)));
		f.close();
	}
};
Utopia.showHighScores = function() {
	console.clear();
	console.putmsg("\x01h\x01y--- UTOPIA HALL OF FAME ---\r\n\r\nRank  Governor             Score    Pop    Date\r\n\x01n\x01b--------------------------------------------------\r\n");
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			var s = JSON.parse(f.read());
			f.close();
			for (var i = 0; i < s.length; i++) console.putmsg(format("\x01h\x01w#%-2d   \x01n%-20s \x01h\x01g%-8d \x01n%-6d %s\r\n", i + 1, s[i].name, s[i].score, s[i].pop, s[i].date));
		}
	}
	console.putmsg("\r\nAny key to return...");
	console.getkey();
};
Utopia.finishGame = function() {
	this.saveHighScore();
	console.clear();
	console.putmsg("\x01h\x01y--- FINAL GOVERNOR'S REPORT ---\r\n\r\n");
	console.putmsg("\x01h\x01wCareer Title:   \x01c" + this.getRank() + "\r\n");
	console.putmsg("\x01h\x01wTotal Score:   \x01g" + this.calculateScore() + "\r\n\r\n");
	console.putmsg("\x01h\x01wSTATISTICS:\r\n");
	console.putmsg("\x01n\x01g- Trade Missions Success: " + this.state.stats.tradesWon + "\r\n");
	console.putmsg("\x01n\x01r- Pirate Ships Sunk:      " + this.state.stats.piratesSunk + "\r\n");
	console.putmsg("\x01n\x01m- PT Boats Lost:          " + this.state.stats.boatsLost + "\r\n");
	console.putmsg("\r\n\x01h\x01wPress any key for High Scores...");
	console.getkey();
	this.showHighScores();
	if (file_exists(SAVE_FILE)) file_remove(SAVE_FILE);
	this.state.running = false;
};
Utopia.redraw = function(cls) {
	if (cls !== false)
		console.clear(LIGHTGRAY);
	for (var y = 0; y < 22; y++)
		for (var x = 0; x < 60; x++)
			this.drawCell(x, y);
	this.drawUI(/* legend: */true);
}
// --- RUNTIME ---
try {
	if (file_exists(INTRO_FILE)) {
		console.clear();
		console.printfile(INTRO_FILE, P_PCBOARD);
		console.pause();
	}
	var saveF = new File(SAVE_FILE);
	if (saveF.exists && saveF.open("r")) {
		var s = JSON.parse(saveF.read());
		Utopia.state = s.state;
		Utopia.map = s.map;
		Utopia.bldgMap = s.bldgMap;
		saveF.close();
	}
	else {
		for (var y = 0; y < 22; y++) {
			Utopia.map[y] = [];
			Utopia.bldgMap[y] = [];
			for (var x = 0; x < 60; x++) {
				Utopia.map[y][x] = 0;
				Utopia.bldgMap[y][x] = null;
			}
		}
		for (var i = 0; i < 3; i++) {
			var cx = 10 + (i * 20),
				cy = 11;
			for (var n = 0; n < 300; n++) {
				if (cy >= 0 && cy < 22 && cx >= 0 && cx < 60) Utopia.map[cy][
					cx
				] = 1;
				cx += Math.floor(Math.random() * 3) - 1;
				cy += Math.floor(Math.random() * 3) - 1;
				if (cx < 2) cx = 2;
				if (cx > 57) cx = 57;
				if (cy < 2) cy = 2;
				if (cy > 19) cy = 19;
			}
		}
		Utopia.spawnFish();
	}
	Utopia.redraw();
	while (Utopia.state.running && !js.terminated) {
		if (system.timer - Utopia.state.lastTick >= 2) {
			if (Utopia.state.turn < MAX_TURNS) {
				Utopia.state.turn++;
				for (var y = 0; y < 22; y++)
					for (var x = 0; x < 60; x++) {
						var b = Utopia.bldgMap[y][x];
						if (b === "F") Utopia.state.food += 2;
						if (b === "I") Utopia.state.gold += 5;
						if (b === "S" && Math.random() < 0.1) Utopia.state.rebels = Math.max(0, Utopia.state.rebels - 1);
					}
				Utopia.handlePopulation();
				Utopia.handlePendingUpdates();
				Utopia.moveEntities();
				if (Utopia.state.martialLaw > 0) {
					Utopia.state.martialLaw--;
					Utopia.state.rebels = Math.max(0, Utopia.state.rebels - 2);
				}
				else if (Math.random() < 0.08) Utopia.state.rebels = Math.min(100, Utopia.state.rebels + 1);
			}
			else { Utopia.state.msg = "\x01h\x01rTIME IS UP! [Q]uit to score."; }
			Utopia.state.lastTick = system.timer;
			Utopia.drawUI();
		}
		var k = console.inkey(K_NOECHO | K_NOSPIN, 100);
		if (k) {
			var key = k.toUpperCase(),
				oX = Utopia.state.cursor.x,
				oY = Utopia.state.cursor.y;
			if (k === KEY_UP && Utopia.state.cursor.y > 0) Utopia.state.cursor.y--;
			if (k === KEY_DOWN && Utopia.state.cursor.y < 21) Utopia.state.cursor.y++;
			if (k === KEY_LEFT && Utopia.state.cursor.x > 0) Utopia.state.cursor.x--;
			if (k === KEY_RIGHT && Utopia.state.cursor.x < 59) Utopia.state.cursor.x++;
			if (k === CTRL_R) Utopia.redraw();
			if (key === 'B' && Utopia.state.turn < MAX_TURNS) Utopia.showBuildMenu();
			if (key === ' ' && Utopia.state.lastbuilt) Utopia.build(Utopia.state.lastbuilt);
			if (key === 'M' && Utopia.state.martialLaw === 0) {
				Utopia.state.martialLaw = 30;
				Utopia.log("MARTIAL LAW!");
			}
			if (key === 'L') {
				console.clear(LIGHTGRAY);
				for (var i = 0; i < Utopia.state.log.length && !console.aborted; ++i) {
					var entry = Utopia.state.log[i];
					console.attributes = LIGHTGRAY;
					console.print(format("%3d: %s\r\n", entry.turn, entry.msg));
				}
				Utopia.redraw();
			}
			if (key === '?' || key === 'H') {
				if (file_exists(HELP_FILE)) {
					console.clear();
					console.printfile(HELP_FILE, P_PCBOARD);
					console.pause();
				}
				Utopia.redraw();
			}
			if (key === 'Q') {
				console.gotoxy(1, 24);
				if (Utopia.state.turn >= MAX_TURNS) {
					console.write(" [F]inish & Score | [V]iew Hall of Fame ");
					var choice = console.getkey(K_UPPER);
					if (choice === 'F') Utopia.finishGame();
					if (choice === 'V') {
						Utopia.showHighScores();
						Utopia.redraw();
					}
				}
				else {
					console.write(" [S]ave Game | [F]inish & Score | [V]iew Hall of Fame ");
					var choice = console.getkey(K_UPPER);
					if (choice === 'S') {
						var sf = new File(SAVE_FILE);
						sf.open("w");
						sf.write(JSON.stringify(Utopia));
						sf.close();
						Utopia.state.running = false;
					}
					if (choice === 'F')
						Utopia.finishGame();
					if (choice === 'V') 
						Utopia.showHighScores();
					Utopia.redraw(false);
				}
			}
			if (key === 'D' && Utopia.bldgMap[Utopia.state.cursor.y][Utopia.state.cursor.x]) {
				var dc = BASE_DESTRUCTION_COST + Math.floor(Utopia.state.rebels / 2);
				if (Utopia.state.gold >= dc) {
					Utopia.state.gold -= dc;
					Utopia.bldgMap[Utopia.state.cursor.y][Utopia.state.cursor.x] = null;
					Utopia.drawCell(Utopia.state.cursor.x, Utopia.state.cursor.y);
				}
			}
			if (oX !== Utopia.state.cursor.x || oY !== Utopia.state.cursor.y) {
				Utopia.drawCell(oX, oY, false);
				Utopia.drawCell(Utopia.state.cursor.x, Utopia.state.cursor.y);
				Utopia.drawUI();
			}
		}
	}
}
catch (e) {
	console.newline();
	log(LOG_WARNING, alert("Utopia Error: " + e.message + " line " + e.lineNumber));
}