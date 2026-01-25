// Synchronet Utopia
// Inspired by the Intellivision game of the same name
// Original code and basic design by Google Gemini

"use strict";
load("sbbsdefs.js");
// --- CONFIG & FILES ---
var INTRO_FILE = js.exec_dir + "utopia_intro.txt";
var HELP_FILE = js.exec_dir + "utopia_help.txt";
var SAVE_FILE = system.data_dir + format("user/%04u.utopia", user.number);
var DEBUG_FILE = js.exec_dir + "utopia_debug.json";
var SCORE_FILE = js.exec_dir + "utopia_scores.json";

var MAX_TURNS = 500;
var TICK_INTERVAL = 0.5;
var TURN_INTERVAL = 4.0;
var ROUND_INTERVAL = 10;
var PIRATE_SPEED = 0.6;
var MERCHANT_SPEED = 0.5;

// Economics
var HOUSE_CAPACITY = 50;
var POPULATION_INTERVAL = 5;
var MERCHANT_GOLD_VALUE = 1;
var MERCHANT_PORT_BONUS = 25;
var MERCHANT_TRADE_BONUS = 250;
var PIRATE_GOLD_VALUE = 250;
var FISH_PER_SCHOOL = 25
var FISH_CATCH_MAX  = 3;
var FISH_FOOD_VALUE = 2;
var FISH_GOLD_VALUE = 1;
var FISH_CATCH_POTENTIAL = 0.1;
var RAIN_GOLD_VALUE = 1;
var BASE_DESTRUCTION_COST = 5;
// (per-round)
var POP_TAX_MULTIPLE = 0.10;
var LAND_TAX_MULTIPLE = 0.50;
var FISH_TAX_MULTIPLE = 2.00;
var INDUSTRY_TAX_MULTIPLE = 4;
var SCHOOL_COST_MULTIPLE = 2;
var FISH_FOOD_MULTIPLE = 1;
var CROP_FOOD_MULTIPLE = 1.5;
// (per-turn)
var CROP_FOOD_PER_TURN = 0.25;
var TRAWLER_FOOD_PER_TURN = 0.5;

// Weather
var RAIN_HEALING = 20;
var RAIN_POTENTIAL = 0.05;
var STORM_POTENTIAL = 0.005;
var STORM_DAMAGE = 10;
var WAVE_POTENTIAL = 0.10;
var WAVE_MIN_WIDTH = 7;

// Cosmetics
var CHAR_WATER    = "\xF7";
var CHAR_WAVE     = "~";
var CHAR_LAND     = " ";
var CHAR_LAND_N   = "\xDF";
var CHAR_LAND_S   = "\xDC";
var CHAR_LAND_E   = "\xDE";
var CHAR_LAND_W   = "\xDD";
var CHAR_PIRATE   = "\x9E";
var CHAR_FISH     = "\"";
var CHAR_MERCHANT = "$";
var ITEM_HOUSE    = "H";
var ITEM_CROPS    = "C";
var ITEM_INDUSTRY = "I";
var ITEM_PTBOAT   = "P";
var ITEM_SCHOOL   = "S";
var ITEM_BRIDGE   = "B";
var ITEM_TRAWLER  = "T";
var ITEM = {};
ITEM[ITEM_CROPS]   ={ name: "Crops",   char: "\xB1", cost:  3, max_damage:  75, shadow: "Food per Turn/Round", land: true, sea: false, attr: BROWN };
ITEM[ITEM_TRAWLER] ={ name: "Trawler", char: "\x1E", cost: 25, max_damage: 150, shadow: "Catch Fish", land: false, sea: true, attr: YELLOW | HIGH };
ITEM[ITEM_HOUSE]   ={ name: "Housing", char: "\x7F", cost: 50, max_damage: 300, shadow: HOUSE_CAPACITY + " Population Cap", land: true, sea: false };
ITEM[ITEM_INDUSTRY]={ name: "Industry",char: "\xDB", cost: 75, max_damage: 350, shadow: "Taxes and Ports", land: true, sea: false, attr: LIGHTGRAY };
ITEM[ITEM_PTBOAT]  ={ name: "PT Boat", char: "\x1E", cost: 50, max_damage: 300, shadow: "Patrols", land: false, sea: true, attr: MAGENTA|HIGH };
ITEM[ITEM_SCHOOL]  ={ name: "School",  char: "\x15", cost: 75, max_damage: 350, shadow: "Reduce Rebels", land: true, sea: false };
ITEM[ITEM_BRIDGE]  ={ name: "Bridge",  char: "\xCE", cost: 50, max_damage: 400, shadow: "Expansion", land: true, sea: true };
var MAX_FISH = 8;
var MAP_WIDTH = 60;
var MAP_HEIGHT = 22;
var MAP_LEFT_COL = 0;
var MAP_TOP_ROW = 0;
var MAP_MID_COL = (MAP_WIDTH / 2);
var MAP_MID_ROW = (MAP_HEIGHT / 2);
var MAP_RIGHT_COL = (MAP_WIDTH - 1);
var MAP_BOTTOM_ROW = (MAP_HEIGHT - 1);
var WATER = 0;
var LAND = 1;
var LAND_N = 2;
var LAND_S = 3;
var LAND_E = 4;
var LAND_W = 5;
var Utopia = {
	tick: 0,
	state: {
		started: false,
		in_progress: false,
		turn: 0,
		gold: 100,
		food: 100,
		pop: 5,
		popCap: 0,
		rebels: 0,
		martialLaw: 0,
		cursor: { x: MAP_MID_COL, y: MAP_MID_ROW },
		msg: "\x01h\x01wWelcome Governor. Build to claim your homeland!",
		log: [],
		fishPool: [],
		merchant: { x: -1, y: 0, active: false },
		pirate: { x: -1, y: 0, active: false },
		storm: { x: -1, y: 0, active: false },
		rain: { x: -1, y: 0, active: false },
		waves: [],
		stats: { piratesSunk: 0, piratesEscaped: 0, intlTrades: 0, portTrades: 0, boatsLost: 0, trawlersSunk: 0 }
	},
	map: [],
	items: [],
	damage: [],
	pendingUpdate: [],
};

function xy_str(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return x + " x " + y;
}

// --- CORE UTILITIES ---

var debug_enabled = argv.indexOf("-debug") >= 0;

Utopia.debugLog = function(msg) {
	if(debug_enabled)
		log("Utopia: " + msg);
}

Utopia.log = function(msg) {
	this.debugLog(msg);
	this.state.log.push({turn: this.state.turn, msg: msg });
	this.state.msg = msg;
}

Utopia.spawnFish = function() {
	while (this.state.fishPool.length < MAX_FISH) {
		var x, y;
		if (this.state.fishPool.length & Math.random() < 0.3) {
			var f = this.state.fishPool[random(this.state.fishPool.length)];
			x = f.x;
			y = f.y;
		} else {
			x = Math.floor(Math.random() * MAP_WIDTH);
			y = Math.floor(Math.random() * MAP_HEIGHT);
		}
		if (this.map[y][x] === WATER
			&& !this.items[y][x]
			&& this.pathBetween(x, y, 0, 0, WATER))
			this.state.fishPool.push({ x: x, y: y, count: FISH_PER_SCHOOL });
	}
};

Utopia.assessItems = function() {
	for (var i in ITEM) {
		ITEM[i].count = 0;
		ITEM[i].total_damage = 0;
	}
	var count = 0;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			var item = this.items[y][x];
			if (!item)
				continue;
			this.damage[y][x]++; // age/drought
			if (item == ITEM_TRAWLER && this.onWave(x, y))
				this.damage[y][x]++;
			if (this.damage[y][x] > ITEM[item].max_damage) {
				var cause = "Age";
				if (item == ITEM_CROPS)
					cause = "Drought"
				this.log(ITEM[item].name + " Lost to Damage/" + cause);
				this.drawCell(x, y, BLINK);
				this.pendingUpdate.push({ x: x, y: y});				
				this.destroyItem(x, y);
				continue;
			} else {
				if (!this.isLand(x, y) || this.pathToHomeland(x, y)) {
					ITEM[item].total_damage += this.damage[y][x];
					ITEM[item].count++;
				}
			}
		}
	}
}

Utopia.assessLand = function() {
	var count = 0;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (this.isLand(x, y) && this.pathToHomeland(x, y))
				++count;
		}
	}
	return count;
}

Utopia.refreshMap = function()
{
	for (var y = 0; y < MAP_HEIGHT; y++)
		for (var x = 0; x < MAP_WIDTH; x++)
			this.drawCell(x, y);
}

Utopia.refreshScreen = function(cls) {
	if (cls !== false)
		console.clear(LIGHTGRAY);
	this.refreshMap();
	this.drawUI(/* legend: */true);
}

Utopia.landChar = function(char)
{
	switch (char) {
		default:
		case LAND:   return CHAR_LAND;
		case LAND_N: return CHAR_LAND_N;
		case LAND_S: return CHAR_LAND_S;
		case LAND_E: return CHAR_LAND_E;
		case LAND_W: return CHAR_LAND_W;
	}
}

Utopia.isLand = function(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return this.map[y][x] !== WATER;
};

Utopia.drawCell = function(x, y, more_attr) {
	x = Math.floor(x);
	y = Math.floor(y);
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return;
	if (this.map[y] === undefined)
		return;
	var isLand = this.isLand(x, y);
	var homeland = isLand ? this.state.homeland && this.pathToHomeland(x, y) : true;
	var bg_attr = isLand ? (homeland ? BG_GREEN : BG_CYAN) : BG_BLUE;
	var attr = isLand ? (BLUE | bg_attr) : (LIGHTBLUE | bg_attr);
	var char = isLand ? this.landChar(this.map[y][x]) : CHAR_WATER;
	if (!isLand && this.onWave(x, y))
		char = CHAR_WAVE;
	// Check Fish Pool
	for (var i = 0; i < this.state.fishPool.length; i++) {
		if (this.state.fishPool[i].x === x && this.state.fishPool[i].y === y) {
			char = CHAR_FISH;
			attr = (BG_BLUE | GREEN | HIGH);
		}
	}
	if (this.state.merchant.active && Math.floor(this.state.merchant.x) === x && Math.floor(this.state.merchant.y) === y) {
		char = CHAR_MERCHANT;
		attr = BG_BLUE | YELLOW | HIGH;
	}
	if (this.state.pirate.active && Math.floor(this.state.pirate.x) === x && Math.floor(this.state.pirate.y) === y) {
		char = CHAR_PIRATE;
		attr = BG_BLUE | RED | HIGH;
	}
	if (this.items[y][x]) {
		var item = ITEM[this.items[y][x]];
		if (this.items[y][x] == ITEM_INDUSTRY && this.map[y][x] !== LAND) {
			attr = BLUE | BG_LIGHTGRAY;
		} else {
			char = item.char;
			if (item.attr)
				attr = item.attr;
			else
				attr = WHITE | HIGH;
			attr |= bg_attr;
		}
		if (this.items[y][x] == ITEM_BRIDGE) {
			var BIT_NORTH = (1<<0);
			var BIT_SOUTH = (1<<1);
			var BIT_EAST  = (1<<2);
			var BIT_WEST  = (1<<3);
			var b = 0;
			if (this.items[y - 1][x] == ITEM_BRIDGE) b |= BIT_NORTH;
			if (this.items[y + 1][x] == ITEM_BRIDGE) b |= BIT_SOUTH;
			if (this.items[y][x + 1] == ITEM_BRIDGE) b |= BIT_EAST;
			if (this.items[y][x - 1] == ITEM_BRIDGE) b |= BIT_WEST;
			if (this.map[y][x] == WATER) {
				if (this.map[y - 1][x] != WATER) b |= BIT_NORTH;
				if (this.map[y + 1][x] != WATER) b |= BIT_SOUTH;
				if (this.map[y][x + 1] != WATER) b |= BIT_EAST;
				if (this.map[y][x - 1] != WATER) b |= BIT_WEST;
			}
//			this.debugLog(format("bridge at %d/%d = 0x%02X", x, y, b));
			switch (b) {
				case BIT_NORTH | BIT_SOUTH | BIT_WEST:
					char = "\xB9";
					break;
				case BIT_NORTH:
				case BIT_SOUTH:
				case BIT_NORTH | BIT_SOUTH:
					char = "\xBA";
					break;
				case BIT_SOUTH | BIT_WEST:
					char = "\xBB";
					break;
				case BIT_NORTH | BIT_WEST:
					char = "\xBC";
					break;
				case BIT_NORTH | BIT_EAST:
					char = "\xC8";
					break;
				case BIT_SOUTH | BIT_EAST:
					char = "\xC9";
					break;
				case BIT_NORTH | BIT_EAST | BIT_WEST:
					char = "\xCA";
					break;
				case BIT_SOUTH | BIT_EAST | BIT_WEST:
					char = "\xCB";
					break;
				case BIT_NORTH | BIT_SOUTH | BIT_EAST:
					char = "\xCC";
					break;
				case BIT_EAST:
				case BIT_WEST:
				case BIT_EAST | BIT_WEST:
					char = "\xCD";
					break;
			}
		}
	}
	if (this.state.rain.active) {
		var rx = Math.floor(this.state.rain.x),
			ry = Math.floor(this.state.rain.y);
		if (y == ry && Math.abs(x - rx) <= 2) {
			attr = HIGH | BG_LIGHTGRAY;
			char = "\xB1";
		} else if (y == ry + 1 && Math.abs(x - rx) <= 2) {
			char = (this.tick & 1) ? "\xB0" : "\xB2";
			attr &= ~(HIGH | BLINK);
			attr |= LIGHTGRAY;
			if (this.tick & 1)
				attr = (attr >> 4) | ((attr & 0x0f) << 4);
		}
	}
	if (this.state.storm.active) {
		var sx = Math.floor(this.state.storm.x),
			sy = Math.floor(this.state.storm.y);
		if (x === sx && y === sy) {
			char = "\xDB";
			attr |= WHITE | HIGH;
		} else if (Math.abs(y - sy) <= 1 && Math.abs(x - sx) <= 1) {
//			char = "\xB0";
			switch (this.tick % 4) {
				case 0:
					if (x === sx)
						char = "\xB2";
					else if ((x === sx + 1 && y < sy)
						|| (x === sx - 1 && y > sy))
						char = "\xB1";
					break;
				case 1:
					if ((x === sx - 1 && y < sy)
						|| (x === sx + 1 && y > sy))
						char = "\xB2";
					else if (x === sx)
						char = "\xB1";
					break;
				case 2:
					if (y === sy)
						char = "\xB2";
					else if ((x === sx - 1 && y < sy)
						|| (x === sx + 1 && y > sy))
						char = "\xB1";
					break;
				case 3:
					if ((x === sx + 1 && y < sy)
						|| (x === sx - 1 && y > sy))
						char = "\xB2";
					else if (y === sy)
						char = "\xB1";
					break;
			}
			attr |= WHITE | HIGH;
		}
	}
	if (x == this.state.cursor.x && y == this.state.cursor.y) {
		attr = BG_RED | WHITE | BLINK;
//		if (this.items[y][x])
//			char = ITEM[this.items[y][x]].char;
	}
	else
		attr |= more_attr;
	console.gotoxy(x + 1, y + 1);
	console.attributes = attr;
	console.write(char);
};

Utopia.drawRow = function(start, y) {
	for (var x = Math.max(0, start); x <= MAP_RIGHT_COL; ++x)
		this.drawCell(x, y);
};

Utopia.forceScrub = function(x, y, size) {
	var radius = size || 1;
	for (var dx = -radius; dx <= radius; dx++) {
		for (var dy = -radius; dy <= radius; dy++)
			this.drawCell(x + dx, y + dy);
	}
};

Utopia.handlePendingUpdates = function() {
	while (this.pendingUpdate.length) { // Erase the caught fish, sunk boats, etc.
		var cell = this.pendingUpdate.pop();
		this.drawCell(cell.x, cell.y);
	}
}

// --- SIMULATION ---
Utopia.handleRainfall = function() {
	if (!this.state.storm.active && !this.state.rain.active)
		return;
	var sx = Math.floor(this.state.storm.x),
		sy = Math.floor(this.state.storm.y);
	var rx = Math.floor(this.state.rain.x),
		ry = Math.floor(this.state.rain.y);
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			var item = this.items[y][x];
			if (!item)
				continue;
			if (Math.abs(sx - x) <= 1 && Math.abs(sy - y) <= 1) {
				var msg = "Storm Damage";
				var damage = STORM_DAMAGE;
				if (item == ITEM_CROPS) {
					msg = "Rainfall";
					damage = -RAIN_HEALING;
					this.state.gold += RAIN_GOLD_VALUE;
				}
				this.damage[y][x] = Math.max(0, this.damage[y][x] + damage);
				this.state.msg = ITEM[item].name + " received " + msg;
			} else if (Math.abs(rx - x) <= 3 && Math.abs(ry - y) <= 1) {
				if (item == ITEM_CROPS) {
					this.damage[y][x] = Math.max(0, this.damage[y][x] - RAIN_HEALING);
					this.state.gold += RAIN_GOLD_VALUE;
					this.state.msg = ITEM[item].name + " received Rainfall";
				}
			}
		}
	}
}

Utopia.itemHealth = function(item) {
	if (ITEM[item].count == undefined || ITEM[item].count < 1)
		return 0;
	var health = (ITEM[item].count * ITEM[item].max_damage) - ITEM[item].total_damage;
	return health / (ITEM[item].count * ITEM[item].max_damage);
}

Utopia.handleEconomics = function() {
	this.assessItems();
	this.state.popCap = ITEM[ITEM_HOUSE].count * HOUSE_CAPACITY;
	if (Math.random() < ITEM[ITEM_SCHOOL].count * 0.1)
		this.state.rebels = Math.max(0, this.state.rebels - 1);	

	// Food adjustments PER TURN
	this.state.food += Math.floor(ITEM[ITEM_TRAWLER].count * this.itemHealth(ITEM_TRAWLER) * TRAWLER_FOOD_PER_TURN);
	this.state.food += Math.floor(ITEM[ITEM_CROPS].count * this.itemHealth(ITEM_CROPS) * CROP_FOOD_PER_TURN);
	this.state.food = Math.max(0, this.state.food - Math.floor(this.state.pop / 5));
	if (this.state.turn % POPULATION_INTERVAL === 0) {
		if (this.state.food > this.state.pop && this.state.pop < this.state.popCap) {
			var before = this.state.pop;
			var growth = Math.floor(Math.random() * 3) + 1;
			this.state.pop = Math.min(this.state.popCap, this.state.pop + growth);
			this.log("\x01h\x01gPopulation Increase from " + before + " to " + this.state.pop);
		}
		else if (this.state.food <= 0 && this.state.pop > 5) {
			this.state.pop -= Math.floor(Math.random() * 2) + 1;
			this.log("\x01h\x01rFAMINE!");
		}
	}

	if (this.state.turn % ROUND_INTERVAL === 0) {
		// Gold adjustments
		var fishtax = Math.floor(ITEM[ITEM_TRAWLER].count * FISH_TAX_MULTIPLE)
		var indutax = Math.floor(ITEM[ITEM_INDUSTRY].count * INDUSTRY_TAX_MULTIPLE); // TODO: More as well-being of people increases.
		var education = Math.floor(ITEM[ITEM_SCHOOL].count * SCHOOL_COST_MULTIPLE);

		var poptax = 0;
		// Food adjustments PER ROUND
		var fish = Math.floor(ITEM[ITEM_TRAWLER].count * FISH_FOOD_MULTIPLE);
		var crops = Math.floor(ITEM[ITEM_CROPS].count * CROP_FOOD_MULTIPLE);
		var food = fish + crops;
		this.state.food += food;
		if (this.state.food > 0)
			poptax = Math.floor(this.state.pop * POP_TAX_MULTIPLE);
        this.state.rebels = Math.max(0, this.state.rebels - (ITEM[ITEM_SCHOOL].count * 2));
        if (this.state.food < this.state.pop) {
			this.state.rebels += 5;
			this.state.pop--;
		}
		var landtax = Math.floor(this.assessLand() * LAND_TAX_MULTIPLE);
		this.debugLog(format("Revenue $(fish: %u, ind: %u, pop: %u, land: %u), " +
			"Costs $(edu: %u), Food (fish: %u, crops: %u)"
			, fishtax, indutax, poptax, landtax, education
			, fish, crops));
		var revenue = (fishtax + indutax + poptax + landtax) - education;
		this.log(format("\x01y\x01hRound surplus: $\x01w%u\x01y Revenue and \x01w%u\x01y Food "
			, revenue, food));
		this.state.gold += revenue;
        var netFood = this.state.food - (this.state.pop / 2);
        if (netFood < 0) {
            this.state.pop = Math.max(5, this.state.pop - 5);
            this.state.rebels = Math.min(100, this.state.rebels + 10);
            this.log("\x01h\x01rSTARVATION! Rebels rising!");
        } else if (this.state.pop > this.state.popCap) {
            this.state.rebels = Math.min(100, this.state.rebels + 10);
            this.log("\x01h\x01rHOUSING CRISIS! Rebels rising!");
		}
	}
	if (this.state.martialLaw > 0) {
		this.state.martialLaw--;
		this.state.rebels = Math.max(0, this.state.rebels - 2);
	}
	else if (Math.random() < 0.08)
		this.state.rebels = Math.min(100, this.state.rebels + 1);

};

Utopia.onWave = function(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	x = Math.floor(x);
	y = Math.floor(y);
	for (var i = 0; i < this.state.waves.length; ++i) {
		var wave = this.state.waves[i];
		if (y == wave.y && x <= wave.x && wave.x - x < wave.length)
			return true;
	}
	return false;
}

Utopia.moveItem = function(ox, oy, nx, ny) {
	this.debugLog("Moving '" + this.items[oy][ox] + "' at " + xy_str(ox, oy) + " with " + this.damage[oy][ox] + " damage");
	this.items[ny][nx] = this.items[oy][ox];
	this.damage[ny][nx] = this.damage[oy][ox];
	this.destroyItem(ox, oy);
	this.debugLog("After destroy '" + this.items[oy][ox] + "' at " + xy_str(ox, oy) + " with " + this.damage[oy][ox] + " damage");
	this.debugLog("New location '" + this.items[ny][nx] + "' at " + xy_str(nx, ny) + " with " + this.damage[ny][nx] + " damage");
	this.drawCell(ox, oy);
	this.drawCell(nx, ny);
	return true;
}

Utopia.handlePiracy = function() {
	if (!this.state.pirate.active) return;
	var px = Math.floor(this.state.pirate.x),
		py = Math.floor(this.state.pirate.y);
	var ptboat_moved = false;
	var pirate_moved = false;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (this.items[y][x] === ITEM_PTBOAT) { // PT Boat pursues Pirate
				var dist = Math.sqrt(Math.pow(x - px, 2) + Math.pow(y - py, 2));
				if (dist < 10) {
					var nx = x + (px > x ? 1 : (px < x ? -1 : 0)),
						ny = y + (py > y ? 1 : (py < y ? -1 : 0));
					if (!ptboat_moved
						&& ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
						&& this.map[ny][nx] === 0 && !this.items[ny][nx]) {
						ptboat_moved = this.moveItem(x, y, nx, ny);
					}
					if (Math.abs(nx - px) <= 1 && Math.abs(ny - py) <= 1) {
						if (Math.random() < 0.7) {
							this.drawCell(px, py, BLINK);
							this.state.pirate.active = false;
							this.state.stats.piratesSunk++;
							this.log("\x01h\x01bPirate ship sunk by PT Boat!");
							this.pendingUpdate.push({ x: px, y: py});
						}
						else {
							this.drawCell(nx, ny, BLINK);
							this.destroyItem(nx, ny);
							this.state.stats.boatsLost++;
							this.log("\x01h\x01rPT Boat sunk by Pirates!");
							this.pendingUpdate.push({ x: nx, y: ny});
						}
					} 
				}
			} // Pirate pursues Trawler
			else if (x <= this.state.pirate.x && this.items[y][x] === ITEM_TRAWLER) {
				var dist = Math.sqrt(Math.pow(x - px, 2) + Math.pow(y - py, 2));
				if (dist < 10) {
					var nx = px + (x > px ? 1 : (x < px ? -1 : 0)),
						ny = py + (y > py ? 1 : (y < py ? -1 : 0));
					if (!pirate_moved
						&& ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
						&& this.map[ny][nx] === 0 && !this.items[ny][nx]) {
						this.state.pirate.x = nx;
						this.state.pirate.y = ny;
						this.drawCell(x, y);
						this.drawCell(px, py);
						pirate_moved = true;
					}
					if (Math.abs(nx - x) <= 1 && Math.abs(ny - y) <= 1 && Math.random() < 0.7) {
						this.drawCell(x, y, BLINK);
						this.destroyItem(x, y);
						this.state.stats.trawlersSunk++;
						this.log("\x01h\x01rTrawler sunk by Pirates!");
						this.pendingUpdate.push({ x: nx, y: ny});
					}
					return;
				}
			}
		}
	}
	// Pirate pursues Merchant ship
	if (this.state.merchant.active
		&& this.state.merchant.x <= this.state.pirate.x
		&& !pirate_moved) {
		var mx = Math.floor(this.state.merchant.x);
		var my = Math.floor(this.state.merchant.y);
		var px = Math.floor(this.state.pirate.x);
		var py = Math.floor(this.state.pirate.y);
		var dist = Math.sqrt(Math.pow(mx - px, 2) + Math.pow(my - py, 2));
		if (dist < 10 && !this.onWave(this.state.pirate)) {
			var nx = px + (mx > px ? 1 : (mx < px ? -1 : 0)),
				ny = py + (my > py ? 1 : (my < py ? -1 : 0));
			if (ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
				&& this.map[ny][nx] === 0 && !this.items[ny][nx]) {
				this.state.pirate.x = nx;
				this.state.pirate.y = ny;
				this.drawCell(mx, my);
				this.drawCell(px, py);
			}
		}
	}
};

// Trawlers pursue fish
Utopia.moveTrawlers = function() {
	if (ITEM[ITEM_TRAWLER].count < 1 || this.state.fishPool.length < 1)
		return;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (this.items[y][x] !== ITEM_TRAWLER)
				continue;
			var fish = [];
			for (var i = 0; i < this.state.fishPool.length; ++i) {
				var f = this.state.fishPool[i];
				var dist = Math.sqrt(Math.pow(x - f.x, 2) + Math.pow(y - f.y, 2));
				if (dist >= 10)
					continue;
				f = { dist: dist, x: f.x, y: f.y };
//				this.debugLog("fish nearby: " + JSON.stringify(f));
				fish.push(f);
			}
			if (!fish.length)
				continue;
			fish.sort(function (a, b) { return b.dist - a.dist; });
			var f = fish.pop();
			this.debugLog("closest fish: " + JSON.stringify(f));
			if (f.dist < 2)
				continue;
			var nx = x + (f.x > x ? 1 : (f.x < x ? -1 : 0)),
				ny = y + (f.y > y ? 1 : (f.y < y ? -1 : 0));
			if (ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
				&& this.map[ny][nx] === 0 && !this.items[ny][nx]) {
				this.moveItem(x, y, nx, ny);
				return; // NOTE: only moves ONE trawler per turn
			}
		}
	}
};

Utopia.checkMutiny = function() {
	// Only happens if rebels are high and a pirate isn't already active
	if (this.state.rebels > 50 && !this.state.pirate.active) {
		// Chance increases as rebels approach 100%
		var mutinyChance = (this.state.rebels - 50) / 500;
		if (Math.random() < mutinyChance) {

			// Find all PT Boats
			var boats = [];
			for (var y = 0; y < MAP_HEIGHT; y++) {
				for (var x = 0; x < MAP_WIDTH; x++) {
                    if (this.items[y][x] === ITEM_PTBOAT)
						boats.push({x: x, y: y});
                }
            }
            if (boats.length > 0) {
                var target = boats[Math.floor(Math.random() * boats.length)];

                // The Mutiny occurs
				this.destroyItem(target.x, target.y);
                this.state.pirate.active = true;
                this.state.pirate.x = target.x;
                this.state.pirate.y = target.y;
                this.log("\x01h\x01rMUTINY! Rebels stole a PT Boat!");
                this.state.stats.boatsLost++;
                // Visual update
                this.drawCell(target.x, target.y);
                console.beep(); // Alert the governor!
            }
        }
    }
};

Utopia.spawnMerchant = function() {
	var ry = Math.floor(Math.random() * 20);
	if (this.map[ry][0] === WATER) {
		this.state.merchant = {
			active: true,
			x: 0, y: ry,
			moves: [],
		};
		this.debugLog("Merchant spawned at row " + ry);
		this.drawCell(0, ry);
		this.state.msg = "\x01g\x01hA Merchant Voyage Has Begun"
		return true;
	}
	this.debugLog("Could NOT spawn merchant at row " + ry);
	return true;
}

Utopia.spawnPirate = function() {
	var ry = Math.floor(Math.random() * 20);
	if (this.map[ry][MAP_RIGHT_COL] === WATER) {
		this.state.pirate = {
			active: true,
			x: MAP_RIGHT_COL,	y: ry,
			moves: []

		};
		this.debugLog("Pirate spawned at row " + ry);
		this.drawCell(this.state.pirate.x, ry);
		this.state.msg = "\x01r\x01hArgh! Be Wary Matey!";
		return true;
	}
	this.debugLog("Could NOT spawn pirate at row " + ry);
	return false;
}

Utopia.spawnStorm = function() {
	this.state.storm.active = true;
	this.state.storm.x = MAP_RIGHT_COL;
	this.state.storm.y = (MAP_MID_ROW / 2) + Math.floor(Math.random() * (MAP_HEIGHT * 0.60));
	this.state.storm.bias = (Math.random() - Math.random()) * 1.5;
	this.state.msg = "\x01n\x01hSevere Storm Warning!";
	this.debugLog("storm spawned at row " + this.state.storm.y
		+ " with bias " + this.state.storm.bias);
}

Utopia.spawnRain = function() {
	this.state.rain.active = true;
	this.state.rain.y = -1;
	this.state.rain.x = 3 + Math.floor(Math.random() * (MAP_WIDTH - 3));
	this.state.rain.bias = (Math.random() - Math.random()) * 3.0;
	this.debugLog("rain spawned at column " + this.state.rain.x
		+ " with bias " + this.state.rain.bias);
}

Utopia.spawnWave = function() {
	var newWave = {
		x: Math.floor(Math.random() * MAP_WIDTH * 0.75),
		y: Math.floor(Math.random() * MAP_HEIGHT),
		length: 1
	};
	if (this.map[newWave.y][newWave.x] != WATER)
		newWave.x = -1;
	this.state.waves.push(newWave);
//	this.debugLog("wave spawned at " + xy_str(newWave) + " with length " + newWave.length);
	return true;
}

function count(arr, val) {
	var total = 0;
	for(var i = 0; i < arr.length; ++i)
		if(arr[i] === val) ++total;
	return total;
}

Utopia.avoidStuff = function(ship, x, y, eastward, objects) {
//	this.debugLog((eastward ? "merchant" : "pirate") + " avoiding land from " + x + " x " + y);

	if (ship.moves.length >= 20 || y <= 0 || y >= MAP_BOTTOM_ROW)
		return false;

	var moves = [];
	if (ship.moves.length) // Prioritize the last successful direction
		moves.push(ship.moves[ship.moves.length - 1]);
	if (eastward) {
		if (y < MAP_MID_ROW)
			moves.push("NE", "SE", "N");
		else
			moves.push("SE", "NE", "S");
		if (Math.random() < 0.5)
			moves.push("SW", "NW");
		else
			moves.push("NW", "SW");
		if (y < MAP_MID_ROW)
			moves.push("S");
		else
			moves.push("N");
		moves.push("W");
	} else { // westward
		if (y < MAP_MID_ROW)
			moves.push("NW", "SW", "N");
		else
			moves.push("SW", "NW", "S");
		if (Math.random() < 0.5)
			moves.push("SE", "NE");
		else
			moves.push("NE", "SE");
		if (y < MAP_MID_ROW)
			moves.push("S");
		else
			moves.push("N");
		moves.push("E");
	}
//	this.debugLog((eastward ? "merchant" : "pirate") + " considering moves " + moves
//		+ " from " + x + " x " + y);
	var move;
	while ((move = moves.shift()) != undefined) {
//		this.debugLog((eastward ? "merchant" : "pirate") + " considering move " + move
//			+ " from " + x + " x " + y);
		var nx = x, ny = y;
		switch(move) {
			case "N":
				--ny;
				break;
			case "S":
				++ny;
				break;
			case "NE":
				--ny;
				++nx;
				break;
			case "SE":
				++ny;
				++nx;
				break;
			case "NW":
				--ny;
				--nx;
				break;
			case "SW":
				++ny;
				--nx;
				break;
		}
		if (this.map[ny][nx] == WATER && (!objects || !objects[ny][nx])) {
			ship.y = ny;
			ship.x = nx;
			return ship.moves.push(move);
		}
	}
	return false;
}

function compareXY(a, b) {
	return Math.max(Math.abs(Math.floor(a.x) - Math.floor(b.x))
		, Math.abs(Math.floor(a.y) - Math.floor(b.y)));
}

Utopia.pathBetween = function(a, b, type) {
	if (compareXY(a, b) == 0)
		return true;
	var visited = {};
	var stack = [[a.x, a.y]];
	while (stack.length > 0) {
		var curr = stack.pop();
		var cx = curr[0]
		var cy = curr[1]
		var key = cx + "," + cy;
		if (visited[key])
			continue;
		visited[key] = true;
		if (cx === b.x && cy === b.y)
			return true;
		var dirs = [
			[0, 1],
			[0, -1],
			[1, 0],
			[-1, 0]
		];
		for (var i = 0; i < dirs.length; i++) {
			var nx = cx + dirs[i][0],
				ny = cy + dirs[i][1];
			if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT) {
				if (type == WATER) {
					if (this.map[ny][nx] == WATER)
						stack.push([nx, ny]);
				} else {
					// Path can travel through Land OR Bridges
					if (this.map[ny][nx] !== WATER || this.items[ny][nx] === ITEM_BRIDGE)
						stack.push([nx, ny]);
				}
			}
		}
	}
	return false;
}

Utopia.pathToHomeland = function(x, y) {
	if (!this.state.homeland)
		return this.isLand(x, y);
	if (typeof x != "object")
		x = { x: x, y: y};
	return this.pathBetween(x, this.state.homeland, LAND);
}

Utopia.handleWeather = function() {
	var oldStorm = {
		x: Math.floor(this.state.storm.x),
		y: Math.floor(this.state.storm.y),
		active: this.state.storm.active
	};
	var oldRain = {
		x: Math.floor(this.state.rain.x),
		y: Math.floor(this.state.rain.y),
		active: this.state.rain.active
	};

	// Storms (E->W)
	if (this.state.storm.active) {
		var ox = this.state.storm.x;
		this.state.storm.x -= Math.random() * 1.5;
		if (Math.floor(ox) != Math.floor(this.state.storm.x)) {
			if (Math.random() < 0.5)
				this.state.storm.y += this.state.storm.bias;
		}
		var sx = Math.floor(this.state.storm.x),
			sy = Math.floor(this.state.storm.y);
		if (sx < 0 || sy < 0 || sy > MAP_BOTTOM_ROW) {
			this.state.storm.active = false;
			this.forceScrub(0, sy, 2);
		}
		else if (this.items[sy][sx]) {
			this.log("\x01h\x01rStorm Destroyed " + ITEM[this.items[sy][sx]].name + "!");
			this.destroyItem(sx, sy);
		}
		else if (this.state.merchant.active
			&& compareXY(this.state.merchant, this.state.storm) == 0) {
			this.log("\x01h\x01rStorm destroyed Merchant ship!");
			this.state.merchant.active = false;
		}
		else if (this.state.pirate.active
			&& compareXY(this.state.pirate, this.state.storm) == 0) {
			this.log("\x01h\x01gStorm destroyed Pirate ship!");
			this.state.pirate.active = false;
		}
	}
	else if (Math.random() < STORM_POTENTIAL)
		this.spawnStorm();

	// Redraw Storm
	if (oldStorm.active)
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++)
				this.drawCell(oldStorm.x + dx, oldStorm.y + dy);
	if (this.state.storm.active) {
		var nsx = Math.floor(this.state.storm.x),
			nsy = Math.floor(this.state.storm.y);
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++)
				this.drawCell(nsx + dx, nsy + dy);
	}

	// Rain clouds (N->S)
	if (this.state.rain.active) {
		var oy = this.state.rain.y;
		this.state.rain.y += Math.random() * 1.5;
		if (Math.floor(oy) != Math.floor(this.state.rain.y)) {
			if (Math.random() < 0.5)
				this.state.rain.x += this.state.rain.bias;
		}
		var rx = Math.floor(this.state.rain.x),
			ry = Math.floor(this.state.rain.y);
		if (ry >= MAP_BOTTOM_ROW || rx < 0 || rx > MAP_RIGHT_COL) {
			this.state.rain.active = false;
			this.forceScrub(0, Math.floor(ry), 2);
		}
	}
	else if (Math.random() < RAIN_POTENTIAL)
		this.spawnRain();

	// Redraw Rain
	if (oldRain.active)
		for (var dx = -2; dx <= 2; dx++)
			for (var dy = 0; dy <= 1; dy++)
				this.drawCell(oldRain.x + dx, oldRain.y + dy);
	if (this.state.rain.active) {
		var nrx = Math.floor(this.state.rain.x),
			nry = Math.floor(this.state.rain.y);
		for (var dx = -2; dx <= 2; dx++)
			for (var dy = 0; dy <= 1; dy++)
				this.drawCell(nrx + dx, nry + dy);
	}

	// Waves (W->E)
	for (var i = this.state.waves.length - 1; i >= 0; i--) {
		var wave = this.state.waves[i];
		if (this.map[wave.y][wave.x + 1] !== WATER)
			wave.length--;
		else {
			wave.x++;
			if (wave.length < WAVE_MIN_WIDTH || Math.random() < 0.1)
				wave.length++;
			else if (Math.random() < 0.1)
				wave.length--;
		}
		if (wave.length < 1 || wave.x - wave.length >= MAP_RIGHT_COL)
			this.state.waves.splice(i, 1);
		this.drawRow(wave.x - (wave.length + 1), wave.y);
	}
	if (this.state.waves.length < 1 || Math.random() < WAVE_POTENTIAL)
		this.spawnWave();
}

Utopia.moveEntities = function() {
	this.handlePiracy();
	this.moveTrawlers();
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
	// Fish
	// Iterate backwards through the fish pool so we can safely remove caught fish
	for (var i = this.state.fishPool.length - 1; i >= 0; i--) {
		var f = this.state.fishPool[i];
		if (f.count < 1) {
			this.state.fishPool.splice(i, 1); // Fish are all gone
			continue;
		}
		if (this.items[f.y][f.x] || Math.random() < 0.4) {
			var oldX = f.x,
				oldY = f.y;
			var nx = f.x + (random(3) - 1);
			var ny = f.y + (random(3) - 1);
			// If fish leaves map, remove it
			if (nx < 0 || nx > MAP_RIGHT_COL || ny < 0 || ny > MAP_BOTTOM_ROW) {
				this.state.fishPool.splice(i, 1);
				this.drawCell(oldX, oldY);
				continue;
			}
			// Move if target is water
			if (this.map[ny][nx] === WATER) {
				f.x = nx;
				f.y = ny;
				this.drawCell(oldX, oldY);
				this.drawCell(f.x, f.y);
			}
		}
		// Check for Trawlers nearby
		for (var dx = -1; dx <= 1; dx++) {
			for (var dy = -1; dy <= 1; dy++) {
				var tx = f.x + dx,
					ty = f.y + dy;
				if (ty >= 0 && ty < MAP_HEIGHT && tx >= 0 && tx < MAP_WIDTH) {
					if (this.items[ty][tx] === ITEM_TRAWLER && Math.random() < FISH_CATCH_POTENTIAL) {
						var count = 1 + random(FISH_CATCH_MAX);
						this.state.food += FISH_FOOD_VALUE * count;
						this.state.gold += FISH_GOLD_VALUE * count;
						this.drawCell(f.x, f.y, BLINK);
						this.pendingUpdate.push(f);
						this.log("\x01h\x01gCaught " + count + " fish!");
						f.count -= count;
						if (f.count < 1)
							this.state.fishPool.splice(i, 1); // Fish is caught
						else
							this.debugLog(JSON.stringify(f));
						break;
					}
				}
			}
		}
	}
	// Replenish the pool
	this.spawnFish();

	// Merchant (West to East)
	if (this.state.merchant.active) {
		if (this.onWave(this.state.merchant))
			this.state.merchant.x += MERCHANT_SPEED * 2;
		else
			this.state.merchant.x += MERCHANT_SPEED;
		this.state.gold += MERCHANT_GOLD_VALUE;
		var mx = Math.floor(this.state.merchant.x),
			my = Math.floor(this.state.merchant.y);
		if (mx >= MAP_RIGHT_COL || this.items[my][mx] == ITEM_INDUSTRY) {
			this.state.merchant.active = false;
			if (mx >= MAP_RIGHT_COL) {
				this.state.gold += MERCHANT_TRADE_BONUS;
				this.log("\x01h\x01yInternational Trade Successful! +$" + MERCHANT_TRADE_BONUS);
				this.state.stats.intlTrades++;
			} else {
				this.state.gold += MERCHANT_PORT_BONUS;
				this.log("\x01h\x01yPort Trade Successful! +$" + MERCHANT_PORT_BONUS);
				this.state.stats.portTrades++;
			}
			this.forceScrub(mx, my, 1);
		}
		else if (this.map[my][mx] !== WATER || this.items[my][mx]) {
			// Try to move up or down around a coast line
			if (!this.avoidStuff(this.state.merchant, mx - 1, my, /* eastward: */true, this.items)) {
				this.debugLog("Merchant beached at " + mx + " x " + my);
				this.state.merchant.active = false;
				this.forceScrub(mx, my, 1);
			} else
				this.debugLog("Merchant successfully avoided stuff by moving to " + xy_str(this.state.merchant));
//			this.debugLog("Merchant moves: " + this.state.merchant.moves);
		}
	}
	else if (Math.random() < 0.03)
		this.spawnMerchant();

	// Pirate (East to West)
	if (this.state.pirate.active) {
		if (this.onWave(this.state.pirate))
			this.state.pirate.x -= PIRATE_SPEED / 2;
		else
			this.state.pirate.x -= PIRATE_SPEED;
		var px = Math.floor(this.state.pirate.x),
			py = Math.floor(this.state.pirate.y);
		if (this.state.merchant.active
			&& Math.abs(px - Math.floor(this.state.merchant.x)) <= 1
			&& Math.abs(py - Math.floor(this.state.merchant.y)) <= 1) {
			this.state.merchant.active = false;
			this.log("\x01h\x01rMerchant ship sunk by Pirates!");
			this.forceScrub(Math.floor(this.state.merchant.x), Math.floor(this.state.merchant.y), 1);
		}
		if (px <= 0) {
			this.state.gold = Math.max(0, this.state.gold - PIRATE_GOLD_VALUE);
			this.state.pirate.active = false;
			this.log("\x01h\x01rPirates Escaped with Loot! -$" + PIRATE_GOLD_VALUE);
			this.forceScrub(0, py, 1);
		}
		else if (this.map[py][px] !== WATER) {
			// Try to move up or down around a coast line
			if (!this.avoidStuff(this.state.pirate, px + 1, py, /* eastward: */false)) {
				this.debugLog("Pirate beached at " + px + " x " + py);
				this.state.pirate.active = false;
				this.forceScrub(px, py, 1);
			}
//			this.debugLog("Pirate moves: " + this.state.pirate.moves);
		}
	}
	else if (Math.random() < 0.02)
		this.spawnPirate();

	// Check for Rebel uprisings
	this.checkMutiny();

	// Redraw
	if (oldMerc.active) this.drawCell(oldMerc.x, oldMerc.y);
	if (oldPirate.active) this.drawCell(oldPirate.x, oldPirate.y);
	if (this.state.merchant.active) this.drawCell(this.state.merchant.x, this.state.merchant.y);
	if (this.state.pirate.active) this.drawCell(this.state.pirate.x, this.state.pirate.y);
	console.flush();
};

// --- SCORING & UI ---
Utopia.calculateScore = function() {
	return (this.state.pop * 10)
			+ Math.floor(this.state.gold / 10)
			+ Math.floor(this.state.food) - (this.state.rebels * 5);
};

Utopia.getRank = function() {
	if (this.state.stats.piratesSunk >= 10) return "Grand Admiral";
	if (this.state.stats.intoTrades >= 10) return "Merchant Prince";
	if (this.state.stats.portTrades >= 10) return "Port Authority";
	if (this.state.pop > 200) return "Metropolitan Visionary";
	if (this.state.food > 5000) return "Harvester of Bounty";
	if (this.state.gold > 5000) return "Hoarder of Treasures";
	return "Local Magistrate";
};

var TITLES = [
    { min: 10000, name: "Deity of the Archipelago" },
    { min: 7500,  name: "Father of Freedom" },
    { min: 5000,  name: "Founder of Nations" },
    { min: 3000,  name: "Benevolent Governor" },
    { min: 1500,  name: "Island Overseer" },
    { min: 500,   name: "Tattered Refugee" },
    { min: 0,     name: "Despotic Exile" },
];

Utopia.getRetirementTitle = function(score) {
    for (var i = 0; i < TITLES.length; i++) {
        if (score >= TITLES[i].min)
			return TITLES[i].name;
    }
    return "Absentee Administrator";
};

Utopia.drawUI = function(legend) {
	var lx = 62;
	var ly = 1;
	if (legend) {
		console.gotoxy(lx, ly++);
		console.putmsg("  Items     Health");
		for (var i in ITEM) {
			console.gotoxy(lx, ly++);
			var item = ITEM[i];
			console.attributes = item.attr || (WHITE | HIGH);
			console.print(item.char + " \x01n" + item.name);
		}
	}
	// Item health
	var ly = 2;
	console.attributes = WHITE | HIGH;
	for (var i in ITEM) {
		if (ITEM[i].count) {
			console.gotoxy(lx + 11, ly);
			console.print(format("%3u", ITEM[i].count));
		}
		console.gotoxy(lx + 15, ly++);
		if (!ITEM[i].max_damage)
			console.print(" N/A");
		else
			console.print(format("%3u%%", 100.0 * this.itemHealth(i)));
	}

	var build_attr = "";
	var demo_attr = "";
	if (this.items[this.state.cursor.y][this.state.cursor.x])
		demo_attr = "\x01h";
	else
		build_attr = "\x01h";
	ly = 11;
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[" + demo_attr + "D\x01n]emolish\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[" + build_attr + "B\x01n]uild an Item");
	if (this.state.lastbuilt) {
		console.gotoxy(lx, ly++);
		console.putmsg("\x01n[" + build_attr + "SPACE\x01n] " + ITEM[this.state.lastbuilt].name + "\x01>");
	}
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01h+\x01n/\x01h-\x01n] Adjust Speed\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hL\x01n]og of Msgs\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hM\x01n]artial Law: " + (this.state.martialLaw > 0 ? "\x01h\x01rON " : "\x01nOFF")  + "\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hQ\x01n]uit\x01>");
	console.gotoxy(1, 23);
	console.attributes = BG_BLACK | LIGHTGRAY;
	console.putmsg(format(
		"\x01n\x01h\x01yGold:%s %-5d " +
		"\x01n\x01h\x01gFood:%s %-5d " +
		"\x01n\x01h\x01wPop:\x01n%4d/%s%-5d " +
		"\x01n\x01h\x01wReb:%s %d%%  " +
		"\x01n\x01h\x01cTurn:%s %1.1f/%d\x01>"
		, this.state.gold < this.state.popCap ? "\x01r\x01i" : "\x01n"
		, this.state.gold
		, this.state.started && this.state.food < this.state.pop ? "\x01r\x01i" : "\x01n"
		, this.state.food
		, this.state.pop
		, this.state.started && this.state.pop >= this.state.popCap ? "\x01r\x01i" : "\x01n"
		, this.state.popCap
		, this.state.rebels >= 50 ? "\x01r\x01i" : "\x01n"
		, this.state.rebels
		, this.state.turn / 10 >= (MAX_TURNS - 1) / 10 ? "\x01r" : "\x01n"
		, this.state.turn / 10, MAX_TURNS / 10));
	console.cleartoeol();
	console.gotoxy(1, 24);
	console.putmsg("\x01n\x01hMsg: " + this.state.msg);
	console.cleartoeol();
};

Utopia.buildItem = function(cmd) {
	var item = ITEM[cmd];
	if (!item) {
		this.debugLog("buildItem() called with " + typeof item + ": " + item);
		return false;
	}
	var ter = this.map[this.state.cursor.y][this.state.cursor.x];
	if (this.items[this.state.cursor.y][this.state.cursor.x]) {
		this.state.msg = "Occupied!";
		return false;
	}
	if (!((ter !== WATER && item.land) || (ter === WATER && item.sea))) {
		this.state.msg = "Wrong Terrain!";
		return false;
	}
	if (!this.pathToHomeland(this.state.cursor)) {
		this.state.msg = "No connection to Homeland!";
		return false;
	}
	if (cmd == ITEM_BRIDGE) {
		if (this.state.cursor.y < 1 
			|| this.state.cursor.x < 1
			|| this.state.cursor.y >= MAP_BOTTOM_ROW
			|| this.state.cursor.x >= MAP_RIGHT_COL) {
			this.state.msg = "Cannot build " + item.name + " there";
			return false;
		}
	}
	if (this.state.gold < item.cost) {
		this.state.msg = "Need $" + item.cost + " to buy " + item.name;
		return false;
	}
	this.state.gold -= item.cost;
	this.items[this.state.cursor.y][this.state.cursor.x] = cmd;
	this.damage[this.state.cursor.y][this.state.cursor.x] = 0;
	this.forceScrub(this.state.cursor.x, this.state.cursor.y, 1);
	this.state.lastbuilt = cmd;
	var msg = "Built " + item.name + " -$" + item.cost;
	if (ter != WATER && !this.state.homeland) {
		this.state.homeland = { x: this.state.cursor.x, y: this.state.cursor.y };
		msg += " and Homeland Claimed!";
		this.refreshMap();
	}
	else if (cmd == ITEM_BRIDGE)
		this.refreshMap();
	this.log(msg);
	return true;
}

Utopia.destroyItem = function(x, y) {
	var item = this.items[y][x];
	if (!item) {
		this.debugLog("No item to destroy at " + xy_str(x, y));
		return false;
	}
	this.items[y][x] = "";
	this.damage[y][x] = 0;
	if (item == ITEM_BRIDGE)
		this.refreshScreen(false);
	return true;
}

Utopia.showBuildMenu = function() {
	var menuX = 12,
		menuY = 5;
	for (var i = 0; i < 13; i++) {
		console.gotoxy(menuX, menuY + i);
		console.attributes = BG_BLACK | LIGHTGRAY;
		console.write("                                              ");
	}
	menuY++
	console.gotoxy(menuX + 2, menuY++);
	console.putmsg("\x01h\x01bBuy and Build an Item");
	menuY++
	for (var i in ITEM) {
		var item = ITEM[i];
		console.gotoxy(menuX + 2, menuY++);
		console.putmsg(format("\x01h\x01b(%s)\x01n %-10s \x01h\x01g$%3d \x01b\xAF \x01h%s"
			, i, item.name, item.cost, item.shadow));
	}
	if (this.state.lastbuilt) {
		console.gotoxy(menuX + 2, ++menuY);
		console.putmsg("Last built: " + ITEM[this.state.lastbuilt].name);
		console.putmsg(" (SPACE to build again)");
	}
	var cmd = console.getkey(K_UPPER | K_NOSPIN);
	for (var y = 0; y < MAP_HEIGHT; y++)
		for (var x = 0; x < MAP_WIDTH; x++)
			this.drawCell(x, y);
	var success = false;
	if (cmd == ' ')
		success = this.buildItem(this.state.lastbuilt);
	else if (ITEM[cmd])
		success = this.buildItem(cmd);
	this.drawUI();
	if (success) {
		this.state.started = true;
		this.state.in_progress = true;
	}
};

Utopia.saveHighScore = function(title) {
	var score = this.calculateScore(),
		scores = [];
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			scores = JSON.parse(f.read());
			f.close();
		}
	}
	scores.push({ name: user.alias, score: score, title: title, pop: this.state.pop, date: Date.now() / 1000 });
	scores.sort(function(a, b) { return b.score - a.score; });
	var f = new File(SCORE_FILE);
	if (f.open("w")) {
		f.write(JSON.stringify(scores.slice(0, 10)));
		f.close();
	}
};

Utopia.showHighScores = function() {
	console.clear();
	console.putmsg("\x01h\x01y--- UTOPIA HALL OF FAME ---\r\n\r\nRank  Governor                  Score    Pop    Date\r\n" +
		"\x01n\x01b--------------------------------------------------\r\n");
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			var s = JSON.parse(f.read());
			f.close();
			for (var i = 0; i < s.length; i++)
				console.putmsg(format("\x01h\x01w#%-2d   \x01n%-25s \x01h\x01g%-8d \x01n%-6d %s\r\n"
					, i + 1, s[i].name, s[i].score, s[i].pop, system.datestr(s[i].date)));
		}
	}
	console.putmsg("\r\nAny key to return...");
	console.getkey();
};

Utopia.finishGame = function() {
	this.state.in_progress = false;
	var score = this.calculateScore();
	var title = this.getRank() + " and " + this.getRetirementTitle(score);
	console.clear();
	console.putmsg("\x01h\x01y--- FINAL GOVERNOR'S REPORT ---\r\n\r\n");
	console.putmsg("\x01h\x01wTotal Score:    \x01y" + score + "\r\n");
	console.putmsg("\x01h\x01wRank and Title: \x01b" + title + "\r\n");
	console.newline();

	console.putmsg("\x01h\x01wSTATISTICS:\r\n");
	console.putmsg("\x01h\x01w- Final Population:    " + this.state.pop + "\r\n");
	console.putmsg("\x01h\x01g- Final Food Store:    " + this.state.food + "\r\n");
	console.putmsg("\x01h\x01c- Treasury Gold:       " + this.state.gold + "\r\n");
	console.putmsg("\x01h\x01r- Rebel Unrest:        " + this.state.rebels + "%\r\n");
	console.putmsg("\x01n\x01g- Port Trades:         " + this.state.stats.portTrades + "\r\n");
	console.putmsg("\x01n\x01g- Internationl Trades: " + this.state.stats.intlTrades + "\r\n");
	console.putmsg("\x01h\x01m- Pirates Defeated:    " + this.state.stats.piratesSunk + "\r\n");
	console.putmsg("\x01h\x01m- Pirates Absconded:   " + this.state.stats.piratesEscaped + "\r\n");
	console.putmsg("\x01n\x01m- PT Boats Lost:       " + this.state.stats.boatsLost + "\r\n");
	console.putmsg("\x01n\x01m- Trawlers Lost:       " + this.state.stats.trawlersSunk + "\r\n");
	console.newline();

	console.putmsg("\x01h\x01wPress any key for High Scores...");
	this.saveHighScore(title);
	console.getkey();
	this.showHighScores();
};

Utopia.handleOperatorCommand = function(key) {
	switch (key) {
		case '*':
			MAX_TURNS += 100;
			break;
		case '&':
			this.state.food += 100;
			break;
		case '#':
			this.state.gold += 100;
			break;
		case '$':
			this.spawnMerchant();
			break;
		case '!':
			this.spawnPirate();
			break;
		case '%':
			this.spawnStorm();
			break;
		case '"':
			this.spawnRain();
			break;
		case '>':
			this.spawnWave();
			break;
		case CTRL_S:
			this.saveGame();
			break;
		case CTRL_T:
			this.saveGame('\t');
			break;
		case CTRL_D:
			this.debugGame(["damage"]);
			break;
	}
	this.drawUI();
}

Utopia.saveGame = function(space) {
	var sf = new File(SAVE_FILE);
	if (sf.open("w")) {
		sf.write(JSON.stringify({
			state: this.state,
			map: this.map,
			items: this.items,
			damage: this.damage
		}, null, space));
		sf.close();
		this.debugLog("Saved game data to " + SAVE_FILE);
		return true;
	}
	log(LOG_WARNING, "Failed to save game data to " + SAVE_FILE);
	return false;
}

Utopia.debugGame = function(replacer, space) {
	var sf = new File(DEBUG_FILE);
	if (sf.open("w")) {
		sf.write(JSON.stringify(this, replacer, space));
		sf.close();
		this.debugLog("Saved game debug to " + DEBUG_FILE);
		return true;
	}
	log(LOG_WARNING, "Failed to save game data to " + DEBUG_FILE);
	return false;
}

Utopia.removeSavedGame = function() {
	if (file_exists(SAVE_FILE) && file_remove(SAVE_FILE))
		this.debugLog("Saved game data removed: " + SAVE_FILE);
}

Utopia.handleQuit = function() {
    console.gotoxy(1, 24);
    console.cleartoeol();

    var prompt = "";
    var canSave = (this.state.turn < MAX_TURNS);

    if (canSave)
        prompt += "\x01h\x01w[S]ave | "
    prompt += "\x01h\x01w[F]inish & Score | [V]iew Hall of Fame | [C]ontinue: ";

    console.putmsg(prompt);
    var choice = console.getkey(K_UPPER);

    if (choice === 'S' && canSave) {
		if (this.saveGame()) 
			console.putmsg("\r\n\x01h\x01gGame Saved. ");
		console.putmsg("See you next time, Governor!");
		return true;
    } else if (choice === 'F') {
        this.finishGame();
		return true;
    } else if (choice === 'V') {
        this.showHighScores();
        this.refreshScreen(); // Redraw map after returning from high scores
    } else if (choice === 'C') {
        this.state.msg = "Welcome back.";
        this.drawUI();
    }
	return false;
};

// --- RUNTIME ---
Utopia.run = function() {
	if (file_exists(INTRO_FILE)) {
		console.clear();
		console.printfile(INTRO_FILE, P_PCBOARD);
		console.pause();
	}
	var game_restored = false;
	var saveF = new File(SAVE_FILE);
	if (saveF.exists && saveF.open("r")) {
		try {
			var s = JSON.parse(saveF.read());
			this.state = s.state;
			this.map = s.map;
			this.items = s.items;
			this.damage = s.damage;
			game_restored = true;
		} catch(e) {
			log(LOG_WARNING, e + ": " + SAVEFILE);
		}
		saveF.close();
	}
	if (!game_restored) {
		for (var y = 0; y < MAP_HEIGHT; y++) {
			this.map[y] = [];
			this.items[y] = [];
			this.damage[y] = [];
			for (var x = 0; x < MAP_WIDTH; x++) {
				this.map[y][x] = WATER;
				this.items[y][x] = "";
				this.damage[y][x] = 0;
			}
		}
		for (var i = 0; i < 3; i++) {
			var cx = 10 + (i * 20)
			var cy = 11;
			for (var n = 0; n < 300; n++) {
				if (cy >= 0 && cy < MAP_HEIGHT && cx >= 0 && cx < MAP_WIDTH)
					this.map[cy][cx] = LAND;
				cx += Math.floor(Math.random() * 3) - 1;
				cy += Math.floor(Math.random() * 3) - 1;
				if (cx < 2) cx = 2;
				if (cx > 57) cx = 57;
				if (cy < 2) cy = 2;
				if (cy > 19) cy = 19;
			}
		}

		// Rough up the edges of land masses
		for (var y = 2; y < MAP_BOTTOM_ROW - 1; ++y) {
			for (x = 2; x < MAP_RIGHT_COL - 1; ++x) {
				if (this.map[y][x] === WATER)
					continue;
				switch (random(5)) {
					case 0:
						if (this.map[y - 1][x] == WATER) {
							this.map[y][x] = LAND_N;
							break;
						}
					case 1:
						if (this.map[y + 1][x] == WATER) {
							this.map[y][x] = LAND_S;
							break;
						}
					case 2:
						if (this.map[y][x + 1] == WATER) {
							this.map[y][x] = LAND_E;
							break;
						}
					case 3:
						if (this.map[y][x - 1] == WATER) {
							this.map[y][x] = LAND_W;
							break;
						}
				}
			}
		}
		this.spawnFish();
	}
	this.refreshScreen();
	var lastTick = system.timer;
	while (bbs.online && !js.terminated) {
		if (system.timer - lastTick >= TICK_INTERVAL) {
			if (this.state.in_progress) {
				if (this.state.turn < MAX_TURNS) {
					this.handleWeather();
					this.handleRainfall();
					if (this.tick % TURN_INTERVAL == 0) {
						this.state.turn++;
						this.handleEconomics();
						this.handlePendingUpdates();
						this.moveEntities();
						this.drawUI();
					}
					this.tick++;
				}
				else {
					this.state.msg = "\x01h\x01rTIME IS UP! [Q]uit to score.";
					this.state.in_progress = false;
					this.drawUI();
				}
			}
			lastTick = system.timer;
		}
		var k = console.inkey(K_CTRLKEYS | K_EXTKEYS, 100);
		if (!k)
			continue;
		var key = k.toUpperCase();
		var cursor = { x: this.state.cursor.x, y: this.state.cursor.y };

		switch (key) {
			case KEY_UP:
				if (this.state.cursor.y > 0)
					this.state.cursor.y--;
				break;
			case KEY_DOWN:
				if (this.state.cursor.y < MAP_BOTTOM_ROW)
					this.state.cursor.y++;
				break;
			case KEY_LEFT:
				if (this.state.cursor.x > 0)
					this.state.cursor.x--;
				break;
			case KEY_RIGHT:
				if (this.state.cursor.x < MAP_RIGHT_COL)
					this.state.cursor.x++;
				break;
			case CTRL_R:
				this.refreshScreen();
				break;
			case 'B':
				if (!this.state.started || this.state.in_progress)
					this.showBuildMenu();
				break;
			case ' ':
				if (this.state.in_progress) {
					if (this.state.lastbuilt)
						this.buildItem(this.state.lastbuilt);
					else
						this.showBuildMenu();
				}
				break;
			case 'M':
				if (this.state.martialLaw === 0) {
					this.state.martialLaw = 30;
					this.log("MARTIAL LAW!");
				}
				break;
			case 'L':
				console.clear(LIGHTGRAY);
				for (var i = 0; i < this.state.log.length && !console.aborted; ++i) {
					var entry = this.state.log[i];
					console.attributes = LIGHTGRAY;
					console.print(format("T-%4.1f: %s\r\n", entry.turn / 10, entry.msg));
				}
				this.refreshScreen();
				break;
			case '?':
			case 'H':
				if (file_exists(HELP_FILE)) {
					console.clear();
					console.printfile(HELP_FILE, P_PCBOARD);
					console.pause();
				}
				this.refreshScreen();
				break;
			case 'Q':
				if (this.handleQuit())
					return;
				continue;
			case 'D':
				if (this.items[this.state.cursor.y][this.state.cursor.x]) {
					var dc = BASE_DESTRUCTION_COST + Math.floor(this.state.rebels / 2);
					if (this.state.gold >= dc) {
						this.state.gold -= dc;
						this.log("\x01m\x01hDemolished "
							+ ITEM[this.items[this.state.cursor.y][this.state.cursor.x]].name
							+ " for $" + dc);
						this.destroyItem(this.state.cursor.x, this.state.cursor.y);
						this.drawCell(this.state.cursor.x, this.state.cursor.y);
						this.drawUI();
					}
				}
				break;
			case '+':
				TICK_INTERVAL /= 2;
				break;
			case '-':
				TICK_INTERVAL *= 2;
				break;
			default:
				if (user.is_sysop)
					this.handleOperatorCommand(key);
				break;
		}
		if (compareXY(this.state.cursor, cursor) != 0) {
			this.drawCell(cursor.x, cursor.y);
			this.drawCell(this.state.cursor.x, this.state.cursor.y);
			this.drawUI();
		}
	}
}

try {
	Utopia.run();
} catch (e) {
	console.newline();
	log(LOG_WARNING, alert("Utopia Error: " + e.message + " line " + e.lineNumber));
}
if (Utopia.state.in_progress)
	Utopia.saveGame();
else
	Utopia.removeSavedGame();
