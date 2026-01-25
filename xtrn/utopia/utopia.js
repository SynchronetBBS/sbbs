// Synchronet Utopia
// Inspired by the Intellivision game of the same name
// Original code and basic design by Google Gemini

"use strict";
load("sbbsdefs.js");
// --- CONFIG & FILES ---
const INTRO_FILE = js.exec_dir + "utopia_intro.txt";
const HELP_FILE = js.exec_dir + "utopia_help.txt";
const SAVE_FILE = system.data_dir + format("user/%04u.utopia", user.number);
const DEBUG_FILE = js.exec_dir + "utopia_debug.json";
const SCORE_FILE = js.exec_dir + "utopia_scores.json";

const ITEM_HOUSE    = "H";
const ITEM_CROPS    = "C";
const ITEM_DOCK     = "D";
const ITEM_INDUSTRY = "I";
const ITEM_PTBOAT   = "P";
const ITEM_SCHOOL   = "S";
const ITEM_BRIDGE   = "B";
const ITEM_TRAWLER  = "T";

var rules = {
	max_turns: 500,
	turn_interval: 4.0,
	round_interval: 10,
	pirate_speed: 0.6,
	merchant_speed: 0.5,
	martial_law_length: 30,

	// Economics
	initial_gold: 150,
	initial_food: 100,
	house_capacity: 50,
	population_interval: 5,
	merchant_gold_value: 1,
	merchant_dock_bonus: 25,
	merchant_trade_bonus: 250,
	pirate_gold_value: 250,
	max_fish: 8,
	fish_per_school: 25,
	fish_catch_max : 3,
	fish_food_value: 2,
	fish_gold_value: 1,
	fish_catch_potential: 0.1,
	rain_gold_value: 1,
	demo_base_cost: 5,
	demo_rebels_multiplier: 0.5,
	demo_item_cost_multiplier: 0.5,
	// (per-round)
	pop_tax_multiple: 0.10,
	land_tax_multiple: 0.50,
	fish_tax_multiple: 2.00,
	industry_tax_multiple: 4,
	school_cost_multiple: 2,
	fish_food_multiple: 1,
	crop_food_multiple: 1.5,
	rebels_decrease_multiplier: 2,
	// (per-turn)
	crop_food_per_turn: 0.25,
	trawler_food_per_turn: 0.5,
	rebels_decrement_potential: 0.1,
	rebels_increase_potential: 0.08,
	rebels_mutiny_theshold: 50,

	// Weather
	rain_speed: 1.3,
	rain_healing: 20,
	rain_potential: 0.05,
	storm_speed: 1.5,
	storm_potential: 0.005,
	storm_damage: 10,
	wave_potential: 0.10,
	wave_min_width: 7,

	// Nature
	fish_school_potential: 0.3,

	item: {
		"C": { cost:  3, max_damage:  75, land: true, sea: false },
		"T": { cost: 25, max_damage: 150, land: false, sea: true },
		"H": { cost: 50, max_damage: 300, land: true, sea: false },
		"D": { cost: 75, max_damage: 350, land: true, sea: false },
		"I": { cost: 75, max_damage: 350, land: true, sea: false },
		"P": { cost: 50, max_damage: 300, land: false, sea: true },
		"S": { cost: 75, max_damage: 350, land: true, sea: false },
		"B": { cost: 50, max_damage: 400, land: true, sea: true  },
	}
}

var item_list = {};
item_list[ITEM_CROPS]   ={ name: "Crops"    ,char: "\xB1", shadow: "Grow Food", attr: BROWN };
item_list[ITEM_TRAWLER] ={ name: "Trawler"  ,char: "\x1E", shadow: "Catch Fish", attr: YELLOW | HIGH };
item_list[ITEM_HOUSE]   ={ name: "Housing"  ,char: "\x7F", shadow: rules.house_capacity + " Population Cap" };
item_list[ITEM_DOCK]    ={ name: "Dock"     ,char: "\xDB", shadow: "Build Boats and Trade", attr: CYAN | HIGH };
item_list[ITEM_INDUSTRY]={ name: "Industry" ,char: "\xDB", shadow: "Income", attr: LIGHTGRAY };
item_list[ITEM_PTBOAT]  ={ name: "PT Boat"  ,char: "\x1E", shadow: "Patrol and Protect", attr: MAGENTA | HIGH };
item_list[ITEM_SCHOOL]  ={ name: "School"   ,char: "\x15", shadow: "Reduce Rebels" };
item_list[ITEM_BRIDGE]  ={ name: "Bridge"   ,char: "\xCE", shadow: "Expand Land and Docks" };

// Cosmetics
var TICK_INTERVAL = 0.5;
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
var MAP_WIDTH = 60;
var MAP_HEIGHT = 22;
var MAP_LEFT_COL = 0;
var MAP_TOP_ROW = 0;
var MAX_ALERTS  = 5;
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

var tick = 0;
var state = {
	started: false,
	in_progress: false,
	turn: 0,
	gold: rules.initial_gold,
	food: rules.initial_food,
	pop: 5,
	popCap: 0,
	rebels: 0,
	martialLaw: 0,
	cursor: { x: MAP_MID_COL, y: MAP_MID_ROW },
	log: [],
	msg: "\x01h\x01wWelcome Governor.  Build to claim your homeland!",
	fishPool: [],
	merchant: { x: -1, y: 0, active: false },
	pirate: { x: -1, y: 0, active: false },
	storm: { x: -1, y: 0, active: false },
	rain: { x: -1, y: 0, active: false },
	waves: [],
	stats: { piratesSunk: 0, piratesEscaped: 0, intlTrades: 0, dockTrades: 0, boatsLost: 0, trawlersSunk: 0 }
};
var map = [];
var itemMap = [];
var damageMap = [];
var pendingUpdate = [];

function xy_str(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return x + " x " + y;
}

// --- CORE UTILITIES ---

var debug_enabled = argv.indexOf("-debug") >= 0;

var debugLog = function(msg) {
	if(debug_enabled)
		js.global.log("Utopia: " + msg);
}

var alert = function(msg) {
	debugLog(msg);
	state.msg = msg;
}

var announce = function(msg) {
	debugLog(msg);
	state.msg = msg;
	state.log.push({ turn: state.turn, msg: msg });
}

var cellIsLand = function(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return map[y][x] !== WATER;
};

function itemIsBoat(item) {
	return item  == ITEM_TRAWLER || item == ITEM_PTBOAT;
}

function itemIsAdjacent(item, coord) {
	for (var dx = -1; dx <= 1; dx++)
		for (var dy = -1; dy <= 1; dy++)
			if (itemMap[coord.y + dy][coord.x + dx] == item)
				return true;
	return false;
}

function itemDemoCost(item) {
	return Math.floor(rules.demo_base_cost
		+ Math.floor(state.rebels * rules.demo_rebels_multiplier)
		+ (rules.item[item].cost * rules.demo_item_cost_multiplier));
}

var spawnFish = function() {
	while (state.fishPool.length < rules.max_fish) {
		var x, y;
		if (state.fishPool.length & Math.random() < rules.fish_school_potential) {
			var f = state.fishPool[random(state.fishPool.length)];
			x = f.x;
			y = f.y;
		} else {
			x = Math.floor(Math.random() * MAP_WIDTH);
			y = Math.floor(Math.random() * MAP_HEIGHT);
		}
		if (map[y][x] === WATER
			&& !itemMap[y][x]
			&& pathBetween({x:x, y:y}, {x:0, y:0}, WATER))
			state.fishPool.push({ x: x, y: y, count: rules.fish_per_school });
	}
};

var assessItems = function() {
	for (var i in item_list) {
		item_list[i].count = 0;
		item_list[i].total_damage = 0;
	}
	var count = 0;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			var item = itemMap[y][x];
			if (!item)
				continue;
			damageMap[y][x]++; // age/drought
			if (item == ITEM_TRAWLER && onWave(x, y))
				damageMap[y][x]++;
			if (damageMap[y][x] > rules.item[item].max_damage) {
				var cause = "Age";
				if (item == ITEM_CROPS)
					cause = "Drought"
				announce(item_list[item].name + " Lost to Damage/" + cause);
				drawCell(x, y, BLINK);
				pendingUpdate.push({ x: x, y: y});
				destroyItem(x, y);
				continue;
			} else {
				if (!cellIsLand(x, y) || pathToHomeland(x, y)) {
					item_list[item].total_damage += damageMap[y][x];
					item_list[item].count++;
				}
			}
		}
	}
}

var assessLand = function() {
	var count = 0;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (cellIsLand(x, y) && pathToHomeland(x, y))
				++count;
		}
	}
	return count;
}

function hideCursor()
{
	console.attributes = 0;
	console.gotoxy(console.screen_columns, console.screen_rows);
}

var refreshMap = function()
{
	for (var y = 0; y < MAP_HEIGHT; y++)
		for (var x = 0; x < MAP_WIDTH; x++)
			drawCell(x, y);
}

var refreshScreen = function(cls) {
	if (cls === true)
		console.clear(LIGHTGRAY);
	refreshMap();
	drawUI(/* legend: */true);
}

var landChar = function(char)
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

var drawCell = function(x, y, more_attr) {
	x = Math.floor(x);
	y = Math.floor(y);
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return;
	if (map[y] === undefined)
		return;
	var isLand = cellIsLand(x, y);
	var homeland = isLand ? state.homeland && pathToHomeland(x, y) : true;
	var bg_attr = isLand ? (homeland ? BG_GREEN : BG_CYAN) : BG_BLUE;
	var attr = isLand ? (BLUE | bg_attr) : (LIGHTBLUE | bg_attr);
	var char = isLand ? landChar(map[y][x]) : CHAR_WATER;
	if (!isLand && onWave(x, y))
		char = CHAR_WAVE;
	// Check Fish Pool
	for (var i = 0; i < state.fishPool.length; i++) {
		if (state.fishPool[i].x === x && state.fishPool[i].y === y) {
			char = CHAR_FISH;
			attr = (BG_BLUE | GREEN | HIGH);
		}
	}
	if (state.merchant.active && Math.floor(state.merchant.x) === x && Math.floor(state.merchant.y) === y) {
		char = CHAR_MERCHANT;
		attr = BG_BLUE | YELLOW | HIGH;
	}
	if (state.pirate.active && Math.floor(state.pirate.x) === x && Math.floor(state.pirate.y) === y) {
		char = CHAR_PIRATE;
		attr = BG_BLUE | RED | HIGH;
	}
	if (itemMap[y][x]) {
		var item = item_list[itemMap[y][x]];
		if ((itemMap[y][x] == ITEM_INDUSTRY) && map[y][x] !== LAND) {
			attr = (item.attr << 4) | BLUE;
		} else {
			char = item.char;
			if (item.attr)
				attr = item.attr;
			else
				attr = WHITE | HIGH;
			attr |= bg_attr;
		}
		if (itemMap[y][x] == ITEM_BRIDGE) {
			var BIT_NORTH = (1<<0);
			var BIT_SOUTH = (1<<1);
			var BIT_EAST  = (1<<2);
			var BIT_WEST  = (1<<3);
			var b = 0;
			if (itemMap[y - 1][x] == ITEM_BRIDGE) b |= BIT_NORTH;
			if (itemMap[y + 1][x] == ITEM_BRIDGE) b |= BIT_SOUTH;
			if (itemMap[y][x + 1] == ITEM_BRIDGE) b |= BIT_EAST;
			if (itemMap[y][x - 1] == ITEM_BRIDGE) b |= BIT_WEST;
			if (map[y][x] == WATER) {
				if (map[y - 1][x] != WATER) b |= BIT_NORTH;
				if (map[y + 1][x] != WATER) b |= BIT_SOUTH;
				if (map[y][x + 1] != WATER) b |= BIT_EAST;
				if (map[y][x - 1] != WATER) b |= BIT_WEST;
			}
//			debugLog(format("bridge at %d/%d = 0x%02X", x, y, b));
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
	if (state.rain.active) {
		var rx = Math.floor(state.rain.x),
			ry = Math.floor(state.rain.y);
		if (y == ry && Math.abs(x - rx) <= 2) {
			attr = HIGH | BG_LIGHTGRAY;
			char = "\xB1";
		} else if (y == ry + 1 && Math.abs(x - rx) <= 2) {
			char = (tick & 1) ? "\xB0" : "\xB2";
			attr &= ~(HIGH | BLINK);
			attr |= LIGHTGRAY;
			if (tick & 1)
				attr = (attr >> 4) | ((attr & 0x0f) << 4);
		}
	}
	if (state.storm.active) {
		var sx = Math.floor(state.storm.x),
			sy = Math.floor(state.storm.y);
		if (x === sx && y === sy) {
			char = "\xDB";
			attr |= WHITE | HIGH;
		} else if (Math.abs(y - sy) <= 1 && Math.abs(x - sx) <= 1) {
//			char = "\xB0";
			switch (tick % 4) {
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
	if (x == state.cursor.x && y == state.cursor.y) {
		attr = BG_RED | WHITE | BLINK;
//		if (itemMap[y][x])
//			char = item_list[itemMap[y][x]].char;
	}
	else
		attr |= more_attr;
	console.gotoxy(x + 1, y + 1);
	console.attributes = attr;
	console.write(char);
};

var drawRow = function(start, y, length) {
	var end = (length === undefined) ? MAP_RIGHT_COL : start + length;
	for (var x = Math.max(0, start); x <= end; ++x)
		drawCell(x, y);
};

var forceScrub = function(x, y, size) {
	var radius = size || 1;
	for (var dx = -radius; dx <= radius; dx++) {
		for (var dy = -radius; dy <= radius; dy++)
			drawCell(x + dx, y + dy);
	}
};

var handlePendingUpdates = function() {
	while (pendingUpdate.length) { // Erase the caught fish, sunk boats, etc.
		var cell = pendingUpdate.pop();
		drawCell(cell.x, cell.y);
	}
}

// --- SIMULATION ---
var handleRainfall = function() {
	if (!state.storm.active && !state.rain.active)
		return;
	var sx = Math.floor(state.storm.x),
		sy = Math.floor(state.storm.y);
	var rx = Math.floor(state.rain.x),
		ry = Math.floor(state.rain.y);
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			var item = itemMap[y][x];
			if (!item)
				continue;
			if (Math.abs(sx - x) <= 1 && Math.abs(sy - y) <= 1) {
				var msg = "Storm Damage";
				var dd = rules.storm_damage;
				if (item == ITEM_CROPS) {
					msg = "Rainfall";
					dd = -rules.rain_healing;
					if (!state.martialLaw)
						state.gold += rules.rain_gold_value;
				}
				damageMap[y][x] = Math.max(0, damageMap[y][x] + dd);
				alert(item_list[item].name + " received " + msg);
			} else if (Math.abs(rx - x) <= 3 && Math.abs(ry - y) <= 1) {
				if (item == ITEM_CROPS) {
					damageMap[y][x] = Math.max(0, damageMap[y][x] - rules.rain_healing);
					if (!state.martialLaw)
						state.gold += rules.rain_gold_value;
					alert(item_list[item].name + " received Rainfall");
				}
			}
		}
	}
}

var itemHealth = function(item) {
	var count = 1;
	var total_damage = 0;
	if (typeof item == "object") {
		var coord = item;
		item = itemMap[coord.y][coord.x];
		total_damage = damageMap[coord.y][coord.x];
	} else {
		count = item_list[item].count;
		if (count == undefined || count < 1)
			return 0;
		total_damage = item_list[item].total_damage;
	}
	var health = (count * rules.item[item].max_damage) - total_damage;
	return health / (count * rules.item[item].max_damage);
}

var handleEconomics = function() {
	assessItems();
	state.popCap = item_list[ITEM_HOUSE].count * rules.house_capacity;
	if (Math.random() < item_list[ITEM_SCHOOL].count * rules.rebels_decrement_potential)
		state.rebels = Math.max(0, state.rebels - 1);

	// Food adjustments PER TURN
	state.food += Math.floor(item_list[ITEM_TRAWLER].count * itemHealth(ITEM_TRAWLER) * rules.trawler_food_per_turn);
	state.food += Math.floor(item_list[ITEM_CROPS].count * itemHealth(ITEM_CROPS) * rules.crop_food_per_turn);
	state.food = Math.max(0, state.food - Math.floor(state.pop / 5));
	if (state.turn % rules.population_interval === 0) {
		if (state.food > state.pop && state.pop < state.popCap) {
			var before = state.pop;
			var growth = Math.floor(Math.random() * 3) + 1;
			state.pop = Math.min(state.popCap, state.pop + growth);
			announce("\x01h\x01gPopulation Increase from " + before + " to " + state.pop);
		}
		else if (state.food <= 0 && state.pop > 5) {
			state.pop -= Math.floor(Math.random() * 2) + 1;
			announce("\x01h\x01rFAMINE!");
		}
	}

	if (state.turn % rules.round_interval === 0) {
		// Gold adjustments
		var fishtax = Math.floor(item_list[ITEM_TRAWLER].count * rules.fish_tax_multiple)
		var indutax = Math.floor(item_list[ITEM_INDUSTRY].count * rules.industry_tax_multiple); // TODO: More as well-being of people increases.
		var education = Math.floor(item_list[ITEM_SCHOOL].count * rules.school_cost_multiple);

		var poptax = 0;
		// Food adjustments PER ROUND
		var fish = Math.floor(item_list[ITEM_TRAWLER].count * rules.fish_food_multiple);
		var crops = Math.floor(item_list[ITEM_CROPS].count * rules.crop_food_multiple);
		var food = fish + crops;
		state.food += food;
		if (state.food > 0)
			poptax = Math.floor(state.pop * rules.pop_tax_multiple);
        state.rebels = Math.max(0, state.rebels - (item_list[ITEM_SCHOOL].count * rules.rebels_decrease_multiplier));
        if (state.food < state.pop) {
			state.rebels += 5;
			state.pop--;
		}
		if (!state.martialLaw) {
			var landtax = Math.floor(assessLand() * rules.land_tax_multiple);
			debugLog(format("Revenue $(fish: %u, ind: %u, pop: %u, land: %u), " +
				"Costs $(edu: %u), Food (fish: %u, crops: %u)"
				, fishtax, indutax, poptax, landtax, education
				, fish, crops));
			var revenue = (fishtax + indutax + poptax + landtax) - education;
			announce(format("\x01y\x01hRound surplus: $\x01w%u\x01y Revenue and \x01w%u\x01y Food "
				, revenue, food));
			state.gold += revenue;
		}
        var netFood = state.food - (state.pop / 2);
        if (netFood < 0) {
            state.pop = Math.max(5, state.pop - 5);
            state.rebels = Math.min(100, state.rebels + 10);
            alert("\x01h\x01rSTARVATION! Rebels rising ...");
        } else if (state.pop > state.popCap) {
            state.rebels = Math.min(100, state.rebels + 10);
            alert("\x01h\x01rHOUSING CRISIS! Rebels rising ...");
		}
	}
	if (state.martialLaw > 0) {
		state.martialLaw--;
		state.rebels = Math.max(0, state.rebels - 2);
	}
	else if (Math.random() < rules.rebels_increase_potential)
		state.rebels = Math.min(100, state.rebels + 1);
};

var onWave = function(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	x = Math.floor(x);
	y = Math.floor(y);
	for (var i = 0; i < state.waves.length; ++i) {
		var wave = state.waves[i];
		if (y == wave.y && x <= wave.x && wave.x - x < wave.length)
			return true;
	}
	return false;
}

var moveItem = function(ox, oy, nx, ny) {
	debugLog("Moving '" + itemMap[oy][ox] + "' at " + xy_str(ox, oy) + " with " + damageMap[oy][ox] + " damage");
	itemMap[ny][nx] = itemMap[oy][ox];
	damageMap[ny][nx] = damageMap[oy][ox];
	destroyItem(ox, oy);
	debugLog("After destroy '" + itemMap[oy][ox] + "' at " + xy_str(ox, oy) + " with " + damageMap[oy][ox] + " damage");
	debugLog("New location '" + itemMap[ny][nx] + "' at " + xy_str(nx, ny) + " with " + damageMap[ny][nx] + " damage");
	drawCell(ox, oy);
	drawCell(nx, ny);
	return true;
}

var handlePiracy = function() {
	if (!state.pirate.active) return;
	var px = Math.floor(state.pirate.x),
		py = Math.floor(state.pirate.y);
	var ptboat_moved = false;
	var pirate_moved = false;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (itemMap[y][x] === ITEM_PTBOAT) { // PT Boat pursues Pirate
				var dist = Math.sqrt(Math.pow(x - px, 2) + Math.pow(y - py, 2));
				if (dist < 10) {
					var nx = x + (px > x ? 1 : (px < x ? -1 : 0)),
						ny = y + (py > y ? 1 : (py < y ? -1 : 0));
					if (!ptboat_moved
						&& ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
						&& map[ny][nx] === 0 && !itemMap[ny][nx]) {
						ptboat_moved = moveItem(x, y, nx, ny);
					}
					if (Math.abs(nx - px) <= 1 && Math.abs(ny - py) <= 1) {
						if (Math.random() < 0.7) {
							drawCell(px, py, BLINK);
							state.pirate.active = false;
							state.stats.piratesSunk++;
							announce("\x01h\x01bPirate ship sunk by PT Boat!");
							pendingUpdate.push({ x: px, y: py});
						}
						else {
							drawCell(nx, ny, BLINK);
							destroyItem(nx, ny);
							state.stats.boatsLost++;
							announce("\x01h\x01rPT Boat sunk by Pirates!");
							pendingUpdate.push({ x: nx, y: ny});
						}
					} 
				}
			} // Pirate pursues Trawler
			else if (x <= state.pirate.x && itemMap[y][x] === ITEM_TRAWLER) {
				var dist = Math.sqrt(Math.pow(x - px, 2) + Math.pow(y - py, 2));
				if (dist < 10) {
					var nx = px + (x > px ? 1 : (x < px ? -1 : 0)),
						ny = py + (y > py ? 1 : (y < py ? -1 : 0));
					if (!pirate_moved
						&& ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
						&& map[ny][nx] === 0 && !itemMap[ny][nx]) {
						state.pirate.x = nx;
						state.pirate.y = ny;
						drawCell(x, y);
						drawCell(px, py);
						pirate_moved = true;
					}
					if (Math.abs(nx - x) <= 1 && Math.abs(ny - y) <= 1 && Math.random() < 0.7) {
						drawCell(x, y, BLINK);
						destroyItem(x, y);
						state.stats.trawlersSunk++;
						announce("\x01h\x01rTrawler sunk by Pirates!");
						pendingUpdate.push({ x: nx, y: ny});
					}
					return;
				}
			}
		}
	}
	// Pirate pursues Merchant ship
	if (state.merchant.active
		&& state.merchant.x <= state.pirate.x
		&& !pirate_moved) {
		var mx = Math.floor(state.merchant.x);
		var my = Math.floor(state.merchant.y);
		var px = Math.floor(state.pirate.x);
		var py = Math.floor(state.pirate.y);
		var dist = Math.sqrt(Math.pow(mx - px, 2) + Math.pow(my - py, 2));
		if (dist < 10 && !onWave(state.pirate)) {
			var nx = px + (mx > px ? 1 : (mx < px ? -1 : 0)),
				ny = py + (my > py ? 1 : (my < py ? -1 : 0));
			if (ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
				&& map[ny][nx] === 0 && !itemMap[ny][nx]) {
				state.pirate.x = nx;
				state.pirate.y = ny;
				drawCell(mx, my);
				drawCell(px, py);
			}
		}
	}
};

// Trawlers pursue fish
var moveTrawlers = function() {
	if (item_list[ITEM_TRAWLER].count < 1 || state.fishPool.length < 1)
		return;
	for (var y = 0; y < MAP_HEIGHT; y++) {
		for (var x = 0; x < MAP_WIDTH; x++) {
			if (itemMap[y][x] !== ITEM_TRAWLER)
				continue;
			var fish = [];
			for (var i = 0; i < state.fishPool.length; ++i) {
				var f = state.fishPool[i];
				var dist = Math.sqrt(Math.pow(x - f.x, 2) + Math.pow(y - f.y, 2));
				if (dist >= 10)
					continue;
				f = { dist: dist, x: f.x, y: f.y };
//				debugLog("fish nearby: " + JSON.stringify(f));
				fish.push(f);
			}
			if (!fish.length)
				continue;
			fish.sort(function (a, b) { return b.dist - a.dist; });
			var f = fish.pop();
			debugLog("closest fish: " + JSON.stringify(f));
			if (f.dist < 2)
				continue;
			var nx = x + (f.x > x ? 1 : (f.x < x ? -1 : 0)),
				ny = y + (f.y > y ? 1 : (f.y < y ? -1 : 0));
			if (ny >= 0 && ny < MAP_HEIGHT && nx >= 0 && nx < MAP_WIDTH
				&& map[ny][nx] === 0 && !itemMap[ny][nx]) {
				moveItem(x, y, nx, ny);
				return; // NOTE: only moves ONE trawler per turn
			}
		}
	}
};

var checkMutiny = function() {
	// Only happens if rebels are high and a pirate isn't already active
	if (state.rebels > rules.rebels_mutiny_theshold && !state.pirate.active) {
		// Chance increases as rebels approach 100%
		var mutinyChance = (state.rebels - 50) / 500;
		if (Math.random() < mutinyChance) {

			// Find all PT Boats
			var boats = [];
			for (var y = 0; y < MAP_HEIGHT; y++) {
				for (var x = 0; x < MAP_WIDTH; x++) {
                    if (itemMap[y][x] === ITEM_PTBOAT)
						boats.push({x: x, y: y});
                }
            }
            if (boats.length > 0) {
                var target = boats[Math.floor(Math.random() * boats.length)];

                // The Mutiny occurs
				destroyItem(target.x, target.y);
                state.pirate.active = true;
                state.pirate.x = target.x;
                state.pirate.y = target.y;
                announce("\x01h\x01rMUTINY! Rebels stole a PT Boat!");
                state.stats.boatsLost++;
                // Visual update
                drawCell(target.x, target.y);
                console.beep(); // Alert the governor!
            }
        }
    }
};

var spawnMerchant = function() {
	var ry = Math.floor(Math.random() * 20);
	if (map[ry][0] === WATER) {
		state.merchant = {
			active: true,
			x: 0, y: ry,
			moves: [],
		};
		debugLog("Merchant spawned at row " + ry);
		drawCell(0, ry);
		alert("\x01g\x01hMerchant voyage has begun");
		return true;
	}
	debugLog("Could NOT spawn merchant at row " + ry);
	return true;
}

var spawnPirate = function() {
	var ry = Math.floor(Math.random() * 20);
	if (map[ry][MAP_RIGHT_COL] === WATER) {
		state.pirate = {
			active: true,
			x: MAP_RIGHT_COL,	y: ry,
			moves: []

		};
		debugLog("Pirate spawned at row " + ry);
		drawCell(state.pirate.x, ry);
		alert("\x01r\x01hArgh, be wary Matey!");
		return true;
	}
	debugLog("Could NOT spawn pirate at row " + ry);
	return false;
}

var spawnStorm = function() {
	state.storm.active = true;
	state.storm.x = MAP_RIGHT_COL;
	state.storm.y = (MAP_MID_ROW / 2) + Math.floor(Math.random() * (MAP_HEIGHT * 0.60));
	state.storm.bias = (Math.random() - Math.random()) * 1.5;
	alert("\x01n\x01hSevere storm warning!");
	debugLog("storm spawned at row " + state.storm.y
		+ " with bias " + state.storm.bias);
}

var spawnRain = function() {
	state.rain.active = true;
	state.rain.y = -1;
	state.rain.x = 3 + Math.floor(Math.random() * (MAP_WIDTH - 3));
	state.rain.bias = (Math.random() - Math.random()) * 3.0;
	debugLog("rain spawned at column " + state.rain.x
		+ " with bias " + state.rain.bias);
}

var spawnWave = function() {
	var newWave = {
		x: Math.floor(Math.random() * MAP_WIDTH * 0.75),
		y: Math.floor(Math.random() * MAP_HEIGHT),
		length: 1
	};
	if (map[newWave.y][newWave.x] != WATER)
		newWave.x = -1;
	state.waves.push(newWave);
//	debugLog("wave spawned at " + xy_str(newWave) + " with length " + newWave.length);
	return true;
}

function count(arr, val) {
	var total = 0;
	for(var i = 0; i < arr.length; ++i)
		if(arr[i] === val) ++total;
	return total;
}

var avoidStuff = function(ship, x, y, eastward, objects) {
//	debugLog((eastward ? "merchant" : "pirate") + " avoiding land from " + x + " x " + y);

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
//	debugLog((eastward ? "merchant" : "pirate") + " considering moves " + moves
//		+ " from " + x + " x " + y);
	var move;
	while ((move = moves.shift()) != undefined) {
//		debugLog((eastward ? "merchant" : "pirate") + " considering move " + move
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
		if (map[ny][nx] == WATER && (!objects || !objects[ny][nx])) {
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

var pathBetween = function(a, b, ter) {
	var coord_search = true;
	if (typeof b == "object") {
		if (compareXY(a, b) == 0)
			return true;
	} else {
		coord_search = false;
	}
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
		if (coord_search) {
			if (cx === b.x && cy === b.y)
				return true;
		} else { // Search by item type
			if (itemMap[cy][cx] == b)
				return true;
		}
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
				if (coord_search) {
					if (ter == WATER) {
						if (map[ny][nx] == WATER)
							stack.push([nx, ny]);
					} else {
						// Path can travel through Land OR Bridges
						if (map[ny][nx] !== WATER || itemMap[ny][nx] === ITEM_BRIDGE)
							stack.push([nx, ny]);
					}
				} else {
//					debugLog("item at " + xy_str(nx, ny) + " = " + itemMap[ny][nx]);
					if (itemMap[ny][nx] === b)
						return true;
					if (b === ITEM_DOCK && itemMap[ny][nx] === ITEM_BRIDGE) {
						stack.push([nx, ny]);
					}
				}
			}
		}
	}
	return false;
}

var pathToHomeland = function(x, y) {
	if (!state.homeland)
		return cellIsLand(x, y);
	if (typeof x != "object")
		x = { x: x, y: y };
	return pathBetween(x, state.homeland, LAND);
}

var pathToDock = function(x, y) {
	if (typeof x != "object")
		x = { x: x, y: y };
	return pathBetween(x, state.homeland, LAND) && pathBetween(x, ITEM_DOCK);
}

var handleWeather = function() {
	var oldStorm = {
		x: Math.floor(state.storm.x),
		y: Math.floor(state.storm.y),
		active: state.storm.active
	};
	var oldRain = {
		x: Math.floor(state.rain.x),
		y: Math.floor(state.rain.y),
		active: state.rain.active
	};

	// Storms (E->W)
	if (state.storm.active) {
		var ox = state.storm.x;
		state.storm.x -= Math.random() * rules.storm_speed;
		if (Math.floor(ox) != Math.floor(state.storm.x)) {
			if (Math.random() < 0.5)
				state.storm.y += state.storm.bias;
		}
		var sx = Math.floor(state.storm.x),
			sy = Math.floor(state.storm.y);
		if (sx < 0 || sy < 0 || sy > MAP_BOTTOM_ROW) {
			state.storm.active = false;
			forceScrub(0, sy, 2);
		}
		else if (itemMap[sy][sx]) {
			announce("\x01h\x01rStorm Destroyed " + item_list[itemMap[sy][sx]].name + "!");
			destroyItem(sx, sy);
		}
		else if (state.merchant.active
			&& compareXY(state.merchant, state.storm) == 0) {
			announce("\x01h\x01rStorm destroyed Merchant ship!");
			state.merchant.active = false;
		}
		else if (state.pirate.active
			&& compareXY(state.pirate, state.storm) == 0) {
			announce("\x01h\x01gStorm destroyed Pirate ship!");
			state.pirate.active = false;
		}
	}
	else if (Math.random() < rules.storm_potential)
		spawnStorm();

	// Redraw Storm
	if (oldStorm.active)
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++)
				drawCell(oldStorm.x + dx, oldStorm.y + dy);
	if (state.storm.active) {
		var nsx = Math.floor(state.storm.x),
			nsy = Math.floor(state.storm.y);
		for (var dx = -1; dx <= 1; dx++)
			for (var dy = -1; dy <= 1; dy++)
				drawCell(nsx + dx, nsy + dy);
	}

	// Rain clouds (N->S)
	if (state.rain.active) {
		var oy = state.rain.y;
		state.rain.y += Math.random() * rules.rain_speed;
		if (Math.floor(oy) != Math.floor(state.rain.y)) {
			if (Math.random() < 0.5)
				state.rain.x += state.rain.bias;
		}
		var rx = Math.floor(state.rain.x),
			ry = Math.floor(state.rain.y);
		if (ry >= MAP_BOTTOM_ROW || rx < 0 || rx > MAP_RIGHT_COL) {
			state.rain.active = false;
			forceScrub(0, Math.floor(ry), 2);
		}
	}
	else if (Math.random() < rules.rain_potential)
		spawnRain();

	// Redraw Rain
	if (oldRain.active)
		for (var dx = -2; dx <= 2; dx++)
			for (var dy = 0; dy <= 1; dy++)
				drawCell(oldRain.x + dx, oldRain.y + dy);
	if (state.rain.active) {
		var nrx = Math.floor(state.rain.x),
			nry = Math.floor(state.rain.y);
		for (var dx = -2; dx <= 2; dx++)
			for (var dy = 0; dy <= 1; dy++)
				drawCell(nrx + dx, nry + dy);
	}

	// Waves (W->E)
	for (var i = state.waves.length - 1; i >= 0; i--) {
		var wave = state.waves[i];
		if (map[wave.y][wave.x + 1] !== WATER)
			wave.length--;
		else {
			wave.x++;
			if (wave.length < rules.wave_min_width || Math.random() < 0.2)
				wave.length++;
			else if (Math.random() < 0.15)
				wave.length--;
		}
		if (wave.length < 1 || wave.x - wave.length >= MAP_RIGHT_COL)
			state.waves.splice(i, 1);
		drawRow(wave.x - (wave.length + 1), wave.y, wave.length + 1);
	}
	if (state.waves.length < 1 || Math.random() < rules.wave_potential)
		spawnWave();
}

var moveEntities = function() {
	handlePiracy();
	moveTrawlers();
	var oldMerc = {
		x: Math.floor(state.merchant.x),
		y: Math.floor(state.merchant.y),
		active: state.merchant.active
	};
	var oldPirate = {
		x: Math.floor(state.pirate.x),
		y: Math.floor(state.pirate.y),
		active: state.pirate.active
	};
	// Fish
	// Iterate backwards through the fish pool so we can safely remove caught fish
	for (var i = state.fishPool.length - 1; i >= 0; i--) {
		var f = state.fishPool[i];
		if (f.count < 1) {
			state.fishPool.splice(i, 1); // Fish are all gone
			continue;
		}
		if (itemMap[f.y][f.x] || Math.random() < 0.4) {
			var oldX = f.x,
				oldY = f.y;
			var nx = f.x + (random(3) - 1);
			var ny = f.y + (random(3) - 1);
			// If fish leaves map, remove it
			if (nx < 0 || nx > MAP_RIGHT_COL || ny < 0 || ny > MAP_BOTTOM_ROW) {
				state.fishPool.splice(i, 1);
				drawCell(oldX, oldY);
				continue;
			}
			// Move if target is water
			if (map[ny][nx] === WATER) {
				f.x = nx;
				f.y = ny;
				drawCell(oldX, oldY);
				drawCell(f.x, f.y);
			}
		}
		// Check for Trawlers nearby
		for (var dx = -1; dx <= 1; dx++) {
			for (var dy = -1; dy <= 1; dy++) {
				var tx = f.x + dx,
					ty = f.y + dy;
				if (ty >= 0 && ty < MAP_HEIGHT && tx >= 0 && tx < MAP_WIDTH) {
					if (itemMap[ty][tx] === ITEM_TRAWLER && Math.random() < rules.fish_catch_potential) {
						var count = 1 + random(rules.fish_catch_max);
						state.food += rules.fish_food_value * count;
						if (!state.martialLaw)
							state.gold += rules.fish_gold_value * count;
						drawCell(f.x, f.y, BLINK);
						pendingUpdate.push(f);
//						alert("\x01h\x01gCaught " + count + " fish!");
						f.count -= count;
						if (f.count < 1)
							state.fishPool.splice(i, 1); // Fish is caught
						else
							debugLog(JSON.stringify(f));
						break;
					}
				}
			}
		}
	}
	// Replenish the pool
	spawnFish();

	// Merchant (West to East)
	if (state.merchant.active) {
		if (onWave(state.merchant))
			state.merchant.x += rules.merchant_speed * 2;
		else
			state.merchant.x += rules.merchant_speed;
		if (!state.martialLaw)
			state.gold += rules.merchant_gold_value;
		var mx = Math.floor(state.merchant.x),
			my = Math.floor(state.merchant.y);
		if (mx >= MAP_RIGHT_COL || pathToDock(mx, my)) {
			state.merchant.active = false;
			if (mx >= MAP_RIGHT_COL) {
				if (!state.martialLaw)
					state.gold += rules.merchant_trade_bonus;
				announce("\x01h\x01yInternational Trade Successful! +$" + rules.merchant_trade_bonus);
				state.stats.intlTrades++;
			} else {
				if (!state.martialLaw)
					state.gold += rules.merchant_dock_bonus;
				announce("\x01h\x01yDock Trade Successful! +$" + rules.merchant_dock_bonus);
				state.stats.dockTrades++;
			}
			forceScrub(mx, my, 1);
		}
		else if (map[my][mx] !== WATER || itemMap[my][mx]) {
			// Try to move up or down around a coast line
			if (!avoidStuff(state.merchant, mx - 1, my, /* eastward: */true, itemMap)) {
				debugLog("Merchant beached at " + mx + " x " + my);
				state.merchant.active = false;
				forceScrub(mx, my, 1);
			} else
				debugLog("Merchant successfully avoided stuff by moving to " + xy_str(state.merchant));
//			debugLog("Merchant moves: " + state.merchant.moves);
		}
	}
	else if (Math.random() < 0.03)
		spawnMerchant();

	// Pirate (East to West)
	if (state.pirate.active) {
		if (onWave(state.pirate))
			state.pirate.x -= rules.pirate_speed / 2;
		else
			state.pirate.x -= rules.pirate_speed;
		var px = Math.floor(state.pirate.x),
			py = Math.floor(state.pirate.y);
		if (state.merchant.active
			&& Math.abs(px - Math.floor(state.merchant.x)) <= 1
			&& Math.abs(py - Math.floor(state.merchant.y)) <= 1) {
			state.merchant.active = false;
			announce("\x01h\x01rMerchant ship sunk by Pirates!");
			forceScrub(Math.floor(state.merchant.x), Math.floor(state.merchant.y), 1);
		}
		if (px <= 0) {
			state.gold = Math.max(0, state.gold - rules.pirate_gold_value);
			state.pirate.active = false;
			announce("\x01h\x01rPirates Escaped with Loot! -$" + rules.pirate_gold_value);
			forceScrub(0, py, 1);
		}
		else if (map[py][px] !== WATER) {
			// Try to move up or down around a coast line
			if (!avoidStuff(state.pirate, px + 1, py, /* eastward: */false)) {
				debugLog("Pirate beached at " + px + " x " + py);
				state.pirate.active = false;
				forceScrub(px, py, 1);
			}
//			debugLog("Pirate moves: " + state.pirate.moves);
		}
	}
	else if (Math.random() < 0.02)
		spawnPirate();

	// Check for Rebel uprisings
	checkMutiny();

	// Redraw
	if (oldMerc.active) drawCell(oldMerc.x, oldMerc.y);
	if (oldPirate.active) drawCell(oldPirate.x, oldPirate.y);
	if (state.merchant.active) drawCell(state.merchant.x, state.merchant.y);
	if (state.pirate.active) drawCell(state.pirate.x, state.pirate.y);
	console.flush();
};

// --- SCORING & UI ---
var calculateScore = function() {
	return (state.pop * 10)
			+ Math.floor(state.gold / 10)
			+ Math.floor(state.food) - (state.rebels * 5);
};

var getRank = function() {
	if (state.stats.piratesSunk >= 10) return "Grand Admiral";
	if (state.stats.intoTrades >= 10) return "Merchant Prince";
	if (state.stats.dockTrades >= 10) return "Port Authority";
	if (state.pop > 200) return "Metropolitan Visionary";
	if (state.food > 5000) return "Harvester of Bounty";
	if (state.gold > 5000) return "Hoarder of Treasures";
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

var getRetirementTitle = function(score) {
    for (var i = 0; i < TITLES.length; i++) {
        if (score >= TITLES[i].min)
			return TITLES[i].name;
    }
    return "Absentee Administrator";
};

var drawUI = function(legend) {
	var lx = 62;
	var ly = 1;

	var item_key = itemMap[state.cursor.y][state.cursor.x];

	if (legend) {
		console.gotoxy(lx, ly++);
		console.attributes = LIGHTGRAY;
		console.putmsg("  Items     Health");
		for (var i in item_list) {
			console.gotoxy(lx, ly++);
			var item = item_list[i];
			console.attributes = item.attr || (WHITE | HIGH);
			console.print(item.char + " ");
			if (i == item_key)
				console.attributes = HIGH | WHITE;
			else if (item_key)
				console.attributes = HIGH | BLACK;
			else
				console.attributes = LIGHTGRAY;
			console.print(item.name);
		}
	}
	// Item health
	var ly = 2;
	for (var i in item_list) {
		console.attributes = WHITE | HIGH;
		console.gotoxy(lx + 11, ly);
		console.cleartoeol();
		if (!item_key && item_list[i].count)
			console.print(format("%3ux", item_list[i].count));
		if (!item_key || i == item_key) {
			console.gotoxy(lx + 15, ly);
			if (!rules.item[i].max_damage)
				console.print(" N/A");
			else {
				var health = itemHealth(item_key ? state.cursor : i);
				if (item_list[i].count) {
					if (health < 0.33)
						console.attributes = RED | HIGH;
					if (health < 0.15)
						console.attributes |= BLINK;
				}
				console.print(format("%3u%%", 100.0 * health));
			}
		}
		ly++;
	}

	var build_attr = "";
	var demo_attr = "";
	var demo_cost = "";
	if (item_key) {
		demo_attr = "\x01h";
		demo_cost = "\x01y\x01h$" + itemDemoCost(item_key);
	} else
		build_attr = "\x01h";
	ly = 11;
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[" + demo_attr + "DEL\x01n] Demolish " + demo_cost + "\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[" + build_attr + "INS\x01n] Build Item");
	console.gotoxy(lx, ly++);
	if (state.lastbuilt)
		console.putmsg("\x01n[" + build_attr + "SPC\x01n] " + item_list[state.lastbuilt].name
			+ " \x01y\x01h$" + rules.item[state.lastbuilt].cost + "\x01>");
	console.cleartoeol();
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01h+\x01n/\x01h-\x01n] Adjust Speed\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hL\x01n]og of News\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hM\x01n]artial Law: " + (state.martialLaw > 0 ? ("\x01h\x01r" + state.martialLaw) : "\x01nOFF")  + "\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hH\x01n]elp\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hQ\x01n]uit\x01>");
	console.gotoxy(1, 23);
	console.attributes = BG_BLACK | LIGHTGRAY;
	var gold = state.gold > 999 ? format("%1.1fK", state.gold / 1000) : state.gold;
	var food = state.food > 999 ? format("%1.1fK", state.food / 1000) : state.food;
	console.putmsg(format(
		"\x01n\x01h\x01yGold:%s %s " +
		"\x01n\x01h\x01gFood:%s %s " +
		"\x01n\x01h\x01wPop:\x01n%4d/%s%-5d " +
		"\x01n\x01h\x01wReb:%s %d%%  " +
		"\x01n\x01h\x01cTurn:%s %1.1f/%d\x01>"
		, state.gold < state.popCap ? "\x01r\x01i" : "\x01n"
		, gold
		, state.started && state.food < state.pop ? "\x01r\x01i" : "\x01n"
		, food
		, state.pop
		, state.started && state.pop >= state.popCap ? "\x01r\x01i" : "\x01n"
		, state.popCap
		, state.rebels >= 50 ? "\x01r\x01i" : "\x01n"
		, state.rebels
		, state.turn / 10 >= (rules.max_turns - 1) / 10 ? "\x01r" : "\x01n"
		, state.turn / 10, rules.max_turns / 10));
	console.cleartoeol();
	console.gotoxy(1, 24);
	if (state.msg) {
		console.putmsg("\x01n\x01hNews: " + state.msg);
		console.cleartoeol();
	}
	hideCursor();
};

var buildItem = function(type) {
	var item = item_list[type];
	if (!item) {
		debugLog("buildItem() called with " + typeof item + ": " + item);
		return false;
	}
	var ter = map[state.cursor.y][state.cursor.x];
	if (itemMap[state.cursor.y][state.cursor.x]) {
		alert("Space already occupied!");
		return false;
	}
	if (!((ter !== WATER && rules.item[type].land) || (ter === WATER && rules.item[type].sea))) {
		alert("Wrong terrain to build " + item.name);
		return false;
	}
	if (itemIsBoat(type)) {
		if (!pathToDock(state.cursor)) {
			alert("Need a nearby Dock or Pier to build " + item.name);
			return false;
		}
	} else if (!pathToHomeland(state.cursor)) {
		alert("No connection to Homeland (build a bridge?)");
		return false;
	}
	if (type == ITEM_BRIDGE) {
		if (state.cursor.y < 1
			|| state.cursor.x < 1
			|| state.cursor.y >= MAP_BOTTOM_ROW
			|| state.cursor.x >= MAP_RIGHT_COL) {
			alert("Cannot build " + item.name + " there");
			return false;
		}
	}
	if (state.gold < rules.item[type].cost) {
		alert("Need $" + rules.item[type].cost + " to build " + item.name);
		return false;
	}
	state.gold -= rules.item[type].cost;
	itemMap[state.cursor.y][state.cursor.x] = type;
	damageMap[state.cursor.y][state.cursor.x] = 0;
	forceScrub(state.cursor.x, state.cursor.y, 1);
	state.lastbuilt = type;
	var msg = "Built " + item.name + " -$" + rules.item[type].cost;
	if (ter != WATER && !state.homeland) {
		state.homeland = { x: state.cursor.x, y: state.cursor.y };
		msg += " and Homeland claimed!";
		refreshMap();
	}
	else if (type == ITEM_BRIDGE)
		refreshMap();
	announce(msg);
	return true;
}

var destroyItem = function(x, y) {
	var item = itemMap[y][x];
	if (!item) {
		debugLog("No item to destroy at " + xy_str(x, y));
		return false;
	}
	itemMap[y][x] = "";
	damageMap[y][x] = 0;
	if (item == ITEM_BRIDGE)
		refreshScreen(false);
	return true;
}

var showBuildMenu = function() {
	var menuX = 8,
		menuY = 5;
	for (var i = 0; i < 14; i++) {
		console.gotoxy(menuX, menuY + i);
		console.attributes = BG_BLACK | LIGHTGRAY;
		console.write(format("%48s", ""));
	}
	menuY++
	console.gotoxy(menuX + 2, menuY++);
	console.putmsg("\x01h\x01bBuy and Build an Item");
	menuY++
	for (var i in item_list) {
		var item = item_list[i];
		console.gotoxy(menuX + 2, menuY++);
		console.putmsg(format("\x01h\x01b(%s)\x01n %-10s \x01h\x01g$%3d \x01b\xAF \x01h%s"
			, i, item.name, rules.item[i].cost, item.shadow));
	}
	if (state.lastbuilt) {
		console.gotoxy(menuX + 2, ++menuY);
		console.putmsg("Last built: " + item_list[state.lastbuilt].name);
		console.putmsg(" (SPACE to build again)");
	}
	hideCursor();
	var cmd = console.getkey(K_UPPER | K_NOSPIN);
	for (var y = 0; y < MAP_HEIGHT; y++)
		for (var x = 0; x < MAP_WIDTH; x++)
			drawCell(x, y);
	var success = false;
	if (cmd == ' ')
		success = buildItem(state.lastbuilt);
	else if (item_list[cmd])
		success = buildItem(cmd);
	drawUI();
	if (success) {
		state.started = true;
		state.in_progress = true;
	}
};

var saveHighScore = function(title) {
	var score = calculateScore(),
		scores = [];
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			scores = JSON.parse(f.read());
			f.close();
		}
	}
	scores.push({ name: user.alias, score: score, title: title, pop: state.pop, date: Date.now() / 1000 });
	scores.sort(function(a, b) { return b.score - a.score; });
	var f = new File(SCORE_FILE);
	if (f.open("w")) {
		f.write(JSON.stringify(scores.slice(0, 10)));
		f.close();
	}
};

var showHighScores = function() {
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

var finishGame = function() {
	state.in_progress = false;
	var score = calculateScore();
	var title = getRank() + " and " + getRetirementTitle(score);
	console.clear();
	console.putmsg("\x01h\x01y--- FINAL GOVERNOR'S REPORT ---\r\n\r\n");
	console.putmsg("\x01h\x01wTotal Score:    \x01y" + score + "\r\n");
	console.putmsg("\x01h\x01wRank and Title: \x01b" + title + "\r\n");
	console.newline();

	console.putmsg("\x01h\x01wSTATISTICS:\r\n");
	console.putmsg("\x01h\x01w- Final Population:    " + state.pop + "\r\n");
	console.putmsg("\x01h\x01g- Final Food Store:    " + state.food + "\r\n");
	console.putmsg("\x01h\x01c- Treasury Gold:       " + state.gold + "\r\n");
	console.putmsg("\x01h\x01r- Rebel Unrest:        " + state.rebels + "%\r\n");
	console.putmsg("\x01n\x01g- Dock Trades:         " + state.stats.dockTrades + "\r\n");
	console.putmsg("\x01n\x01g- Internationl Trades: " + state.stats.intlTrades + "\r\n");
	console.putmsg("\x01h\x01m- Pirates Defeated:    " + state.stats.piratesSunk + "\r\n");
	console.putmsg("\x01h\x01m- Pirates Absconded:   " + state.stats.piratesEscaped + "\r\n");
	console.putmsg("\x01n\x01m- PT Boats Lost:       " + state.stats.boatsLost + "\r\n");
	console.putmsg("\x01n\x01m- Trawlers Lost:       " + state.stats.trawlersSunk + "\r\n");
	console.newline();

	console.putmsg("\x01h\x01wPress any key for High Scores...");
	saveHighScore(title);
	console.getkey();
	showHighScores();
};

var handleOperatorCommand = function(key) {
	switch (key) {
		case '*':
			rules.max_turns += 100;
			break;
		case '&':
			state.food += 100;
			break;
		case '#':
			state.gold += 100;
			break;
		case '$':
			spawnMerchant();
			break;
		case '!':
			spawnPirate();
			break;
		case '%':
			spawnStorm();
			break;
		case '"':
			spawnRain();
			break;
		case '>':
			spawnWave();
			break;
		case CTRL_S:
			saveGame();
			break;
		case CTRL_T:
			saveGame('\t');
			break;
		case CTRL_D:
			debugGame(["damage"]);
			break;
	}
	drawUI();
}

var saveGame = function(space) {
	var sf = new File(SAVE_FILE);
	if (sf.open("w")) {
		sf.write(JSON.stringify({
			state: state,
			map: map,
			itemMap: itemMap,
			damageMap: damageMap
		}, null, space));
		sf.close();
		debugLog("Saved game data to " + SAVE_FILE);
		return true;
	}
	js.global.log(LOG_WARNING, "Failed to save game data to " + SAVE_FILE);
	return false;
}

var debugGame = function(replacer, space) {
	var sf = new File(DEBUG_FILE);
	if (sf.open("w")) {
		sf.write(JSON.stringify(this, replacer, space));
		sf.close();
		debugLog("Saved game debug to " + DEBUG_FILE);
		return true;
	}
	js.global.log(LOG_WARNING, "Failed to save game data to " + DEBUG_FILE);
	return false;
}

var removeSavedGame = function() {
	if (file_exists(SAVE_FILE) && file_remove(SAVE_FILE))
		debugLog("Saved game data removed: " + SAVE_FILE);
}

var handleQuit = function() {
    console.gotoxy(1, 24);
    console.cleartoeol();

    var prompt = "";
    var canSave = (state.turn < rules.max_turns);

    if (canSave)
        prompt += "\x01h\x01w[S]ave | "
	prompt += "\x01h\x01w[F]inish & Score | [A]bort Game | [V]iew Hall of Fame | [C]ontinue: ";

    console.putmsg(prompt);
    var choice = console.getkey(K_UPPER);

    if (choice === 'S' && canSave) {
		if (saveGame())
			console.putmsg("\r\n\x01h\x01gGame Saved. ");
		console.putmsg("See you next time, Governor!");
		return true;
	} else if (choice === 'A') {
		state.in_progress = false;
		return true;
    } else if (choice === 'F') {
        finishGame();
		return true;
    } else if (choice === 'V') {
        showHighScores();
        refreshScreen(); // Redraw map after returning from high scores
    } else if (choice === 'C') {
        alert("Welcome back.");
        drawUI();
    }
	return false;
};

// --- RUNTIME ---
function main () {
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
			state = s.state;
			map = s.map;
			itemMap = s.itemMap;
			damageMap = s.damageMap;
			game_restored = true;
		} catch(e) {
			js.global.log(LOG_WARNING, e + ": " + SAVE_FILE);
		}
		saveF.close();
	}
	if (!game_restored) {
		for (var y = 0; y < MAP_HEIGHT; y++) {
			map[y] = [];
			itemMap[y] = [];
			damageMap[y] = [];
			for (var x = 0; x < MAP_WIDTH; x++) {
				map[y][x] = WATER;
				itemMap[y][x] = "";
				damageMap[y][x] = 0;
			}
		}
		for (var i = 0; i < 3; i++) {
			var cx = 10 + (i * 20)
			var cy = 11;
			for (var n = 0; n < 300; n++) {
				if (cy >= 0 && cy < MAP_HEIGHT && cx >= 0 && cx < MAP_WIDTH)
					map[cy][cx] = LAND;
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
				if (map[y][x] === WATER)
					continue;
				switch (random(5)) {
					case 0:
						if (map[y - 1][x] == WATER) {
							map[y][x] = LAND_N;
							break;
						}
					case 1:
						if (map[y + 1][x] == WATER) {
							map[y][x] = LAND_S;
							break;
						}
					case 2:
						if (map[y][x + 1] == WATER) {
							map[y][x] = LAND_E;
							break;
						}
					case 3:
						if (map[y][x - 1] == WATER) {
							map[y][x] = LAND_W;
							break;
						}
				}
			}
		}
		spawnFish();
	}
	refreshScreen(true);
	var lastTick = system.timer;
	while (bbs.online && !js.terminated) {
		if (system.timer - lastTick >= TICK_INTERVAL) {
			if (state.in_progress) {
				if (state.turn < rules.max_turns) {
					handleWeather();
					handleRainfall();
					if (tick % rules.turn_interval == 0) {
						state.turn++;
						handleEconomics();
						handlePendingUpdates();
						moveEntities();
						drawUI();
					}
					tick++;
				}
				else {
					alert("\x01h\x01rTIME IS UP! [Q]uit to score.");
					state.in_progress = false;
					drawUI();
				}
			}
			lastTick = system.timer;
		}
		var k = console.inkey(K_EXTKEYS, 500 * TICK_INTERVAL);
		if (!k)
			continue;
		var key = k.toUpperCase();
		var cursor = { x: state.cursor.x, y: state.cursor.y };

		switch (key) {
			case KEY_UP:
				if (state.cursor.y > 0)
					state.cursor.y--;
				break;
			case KEY_DOWN:
				if (state.cursor.y < MAP_BOTTOM_ROW)
					state.cursor.y++;
				break;
			case KEY_LEFT:
				if (state.cursor.x > 0)
					state.cursor.x--;
				break;
			case KEY_RIGHT:
				if (state.cursor.x < MAP_RIGHT_COL)
					state.cursor.x++;
				break;
			case CTRL_R:
				refreshScreen(true);
				break;
			case KEY_INSERT:
			case '\r':
				if (!state.started || state.in_progress)
					showBuildMenu();
				break;
			case ' ':
				if (state.in_progress) {
					if (state.lastbuilt)
						buildItem(state.lastbuilt);
					else
						showBuildMenu();
				}
				break;
			case 'M':
				if (state.martialLaw === 0) {
					state.martialLaw = rules.martial_law_length;
					announce("MARTIAL LAW!");
				}
				break;
			case 'L':
				console.clear(LIGHTGRAY);
				for (var i = 0; i < state.log.length && !console.aborted; ++i) {
					var entry = state.log[i];
					console.attributes = LIGHTGRAY;
					console.print(format("T-%4.1f: %s\r\n", entry.turn / 10, entry.msg));
				}
				refreshScreen();
				break;
			case '?':
			case 'H':
				if (file_exists(HELP_FILE)) {
					console.clear();
					console.printfile(HELP_FILE, P_PCBOARD);
					console.pause();
				}
				refreshScreen();
				break;
			case 'Q':
				if (handleQuit())
					return;
				continue;
			case KEY_DEL:
				if (itemMap[state.cursor.y][state.cursor.x]) {
					var dc = itemDemoCost(itemMap[state.cursor.y][state.cursor.x]);
					if (state.gold >= dc) {
						state.gold -= dc;
						announce("\x01m\x01hDemolished "
							+ item_list[itemMap[state.cursor.y][state.cursor.x]].name
							+ " for $" + dc);
						destroyItem(state.cursor.x, state.cursor.y);
						drawCell(state.cursor.x, state.cursor.y);
					} else {
						alert("Need $" + dc + " to demolish "
							+ item_list[itemMap[state.cursor.y][state.cursor.x]].name);
					}
					drawUI();
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
					handleOperatorCommand(key);
				break;
		}
		if (compareXY(state.cursor, cursor) != 0) {
			drawCell(cursor.x, cursor.y);
			drawCell(state.cursor.x, state.cursor.y);
			drawUI(itemMap[state.cursor.y][state.cursor.x]
				|| itemMap[cursor.y][cursor.x]);
		}
	}
}

try {
	main();
} catch (e) {
	console.newline();
	js.global.log(LOG_WARNING, js.global.alert("Utopia Error: " + e.message + " line " + e.lineNumber));
}
if (state.in_progress)
	saveGame();
else
	removeSavedGame();
