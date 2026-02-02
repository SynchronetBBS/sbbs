// Synchronet Utopia
// Inspired by the Intellivision game of the same name
// Original code and basic design by Google Gemini

"use strict";
load("sbbsdefs.js");

// --- CONFIG & FILES ---
var settings = {
	intro_msg: js.exec_dir + "intro.msg",
	tick_interval: 0.5,
	rules_list: [],
}

const HELP_FILE = js.exec_dir + "help.msg";
const SAVE_FILE = system.data_dir + format("user/%04u.utopia", user.number);
const MAP_FILE = js.exec_dir + "map.json";
const DEBUG_FILE = js.exec_dir + "debug.json";
const SCORE_FILE = js.exec_dir + "scores.json";

const ITEM_VILLAS   = "V";
const ITEM_HOSPITAL = "H";
const ITEM_FORT     = "F";
const ITEM_CROPS    = "C";
const ITEM_ANIMALS  = "A";
const ITEM_DOCK     = "D";
const ITEM_INDUSTRY = "I";
const ITEM_PTBOAT   = "P";
const ITEM_SCHOOL   = "S";
const ITEM_BRIDGE   = "B";
const ITEM_TRAWLER  = "T";

var rules = {
	name: "Challenging",
	max_turns: 500,
	turn_interval: 4.0,
	round_interval: 10,
	pirate_speed: 0.6,
	merchant_speed: 0.5,
	martial_law_length: 30,

	// Economics
	initial_gold: 150,
	initial_food: 100,
	population_interval: 5,
	industry_pollution_multiple: 0.25,
	animals_pollution_multiple: 0.25,
	boat_pollution_multiple: 0.01,
	crops_pollution_reduction: 0.5,
	hospital_pop_increase_multiplier: 5.0,
	merchant_spawn_potential: 0.03,
	merchant_gold: 250,
	merchant_turn_gold_value: 1,
	merchant_dock_interval: 10,
	merchant_dock_bonus: 25,
	pirate_spawn_potential: 0.02,
	pirate_land_attack_interval: 10,
	pirate_land_attack_damage: 50,
	pirate_sea_attack_damage: 50,
	pirate_gold: 125,
	pirate_escape_penalty_multiplier: 0.5,
	fish_pool_min: 5,
	fish_pool_max: 10,
	fish_per_school: 25,
	fish_food_value: 5,
	fish_gold_value: 1,
	fish_move_potential: 0.4,
	fish_catch_potential: 0.2,
	fish_catch_max: 5,
	fish_spawn_potential: 0.5,
	fish_school_potential: 0.3,
	rain_gold_value: 1,
	demo_base_cost: 5,
	demo_rebels_multiplier: 0.5,
	demo_item_cost_multiplier: 0.5,
	ptboat_sink_pirate_potential: 0.9,
	pirate_merchant_plunder_multiplier: 0.5,
	sunk_pirate_reward_multiplier: 0.5,
	// (per-round)
	industry_income_multiple: 4,
	pop_tax_multiple: 0.10,
	land_tax_multiple: 0.15,
	rebels_education_multiplier: 2,
	rebels_starvation_increase: 10,
	rebels_underhoused_increase: 10,
	// (per-turn)
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
	wave_increase_potential: 0.3,
	wave_deccrease_potential: 0.3,

	item: {
		"C": { cost:  5, max_damage: 100, tax:  0.10, food_turn: 0.90, food_round: 1,   land: true,  sea: false },
		"A": { cost: 20, max_damage: 500, tax:  0.10, food_turn: 0.10, food_round: 4,   land: true,  sea: false },
		"T": { cost: 25, max_damage: 200, tax:  0.25, food_turn: 0.50, food_round: 5,   land: false, sea: true  },
		"P": { cost:100, max_damage: 300, tax:  0.25,                                   land: false, sea: true  },
		"V": { cost: 50, max_damage: 300, tax:  0.25, cap: 50,                          land: true,  sea: false },
		"D": { cost: 75, max_damage: 350, tax:  1.00, cap: 2,                           land: true,  sea: false },
		"I": { cost: 75, max_damage: 350, tax:  1.00, cap: 1,                           land: true,  sea: false },
		"B": { cost: 50, max_damage: 400, tax: -1.00,                                   land: true,  sea: true  },
		"S": { cost: 75, max_damage: 350, tax: -2.50,                                   land: true,  sea: false },
		"H": { cost:100, max_damage: 400, tax: -3.00, cap: 3,                           land: true,  sea: false },
		"F": { cost:150, max_damage: 500, tax: -5.00, cap: 10,                          land: true,  sea: false },
	}
}

var item_list = {};
item_list[ITEM_CROPS]   ={ name: "Crops"    ,char: "\xB1", shadow: "Food & Materials", attr: LIGHTGREEN };
item_list[ITEM_ANIMALS] ={ name: "Animals"  ,char: "\xB1", shadow: "Meat & Dairy", attr: BROWN };
item_list[ITEM_TRAWLER] ={ name: "Trawler"  ,char: "\x1E", shadow: "Catch Fish", attr: YELLOW | HIGH };
item_list[ITEM_PTBOAT]  ={ name: "PT Boat"  ,char: "\x1E", shadow: "Patrol & Protect", attr: MAGENTA | HIGH };
item_list[ITEM_VILLAS]  ={ name: "Villas"   ,char: "\xF6", shadow: "Increase Population Cap" };
item_list[ITEM_DOCK]    ={ name: "Dock"     ,char: "\xDB", shadow: "Build Boats & Trade", attr: CYAN | HIGH };
item_list[ITEM_BRIDGE]  ={ name: "Bridge"   ,char: "\xCE", shadow: "Expand Land & Docks" };
item_list[ITEM_INDUSTRY]={ name: "Industry" ,char: "\xDB", shadow: "Generate Income", attr: LIGHTGRAY };
item_list[ITEM_SCHOOL]  ={ name: "School"   ,char: "\x15", shadow: "Reduce Rebels" };
item_list[ITEM_HOSPITAL]={ name: "Hospital" ,char: "\xC5", shadow: "Promote Health & Births", attr: RED | BG_LIGHTGRAY };
item_list[ITEM_FORT]    ={ name: "Fortress" ,char: "\xE3", shadow: "Defend Properties", attr: BLACK | HIGH };

// Cosmetics
var CHAR_WATER    = " "; //"\xF7";
var CHAR_WAVE     = "~";
var CHAR_LAND     = " ";
var CHAR_LAND_N   = "\xDF";
var CHAR_LAND_S   = "\xDC";
var CHAR_LAND_E   = "\xDE";
var CHAR_LAND_W   = "\xDD";
var CHAR_PIRATE   = "\xF3";
var CHAR_PIRATAIL = "\xF0";
var CHAR_FISH1    = "\xFA";
var CHAR_FISH2    = "\xF9";
var CHAR_MERCHANT = "\xF2";
var CHAR_MERCTAIL = "\xF1";
var MAP_WIDTH = 60;
var MAP_HEIGHT = 20;
var MAP_LEFT_COL = 0;
var MAP_TOP_ROW = 0;
var MAX_ALERTS  = 5;
var MAP_MID_COL = (MAP_WIDTH / 2);
var MAP_MID_ROW = (MAP_HEIGHT / 2);
var MAP_RIGHT_COL = (MAP_WIDTH - 1);
var MAP_BOTTOM_ROW = (MAP_HEIGHT - 1);
var MAX_MSGS = 3;
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
	msg: [],
	fishPool: [],
	merchant: { x: -1, y: 0, active: false, gold: 0 },
	pirate: { x: -1, y: 0, active: false, gold: 0 },
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

function xy_pair(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return Math.floor(x) + "," + Math.floor(y);
}

// --- CORE UTILITIES ---

var debug_enabled = argv.indexOf("-debug") >= 0;

function debugLog(msg) {
	if(debug_enabled)
		js.global.log("Utopia: " + msg);
}

function pushMsg(msg) {
	if (state.msg.length > 0 && state.msg[state.msg.length - 1].txt == msg) {
		if (state.msg[state.msg.length - 1].repeat === undefined)
			state.msg[state.msg.length - 1].repeat = 1;
		else
			state.msg[state.msg.length - 1].repeat++;
	} else {
		state.msg.push({ txt: msg });
		while(state.msg.length > MAX_MSGS)
			state.msg.shift();
	}
}


function alert(msg) {
	debugLog(msg);
	pushMsg("\x01n\x01h\x01cAlert: \x01h" + msg);
}

function announce(msg) {
	debugLog(msg);
	pushMsg("\x01n\x01h\x01yNews: \x01w" + msg);
	state.log.push({ turn: state.turn, msg: msg });
}

function cellIsWater(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return map[y][x] === WATER;
};

function cellIsLand(x, y) {
	if (typeof x == "object") {
		y = x.y;
		x = x.x;
	}
	return map[y][x] !== WATER;
};

function cellOnMap(x, y) {
	return x >= MAP_LEFT_COL && x <= MAP_RIGHT_COL && y >= MAP_TOP_ROW && y <= MAP_BOTTOM_ROW;
}

function itemIsBoat(item) {
	return item  == ITEM_TRAWLER || item == ITEM_PTBOAT;
}

function cellIsSame(a, b) {
	return Math.floor(a.x) == Math.floor(b.x) && Math.floor(a.y) == Math.floor(b.y);
}

function cellIsAdjacent(a, b) {
	for (var dx = -1; dx <= 1; dx++)
		for (var dy = -1; dy <= 1; dy++)
			if (Math.floor(a.x) + dx == Math.floor(b.x)
				&& Math.floor(a.y) + dy == Math.floor(b.y))
				return true;
	return false;
}

function itemIsAdjacent(obj, item) {
	for (var dx = -1; dx <= 1; dx++) {
		for (var dy = -1; dy <= 1; dy++) {
			var x = Math.floor(obj.x) + dx;
			var y = Math.floor(obj.y) + dy;
			if (cellOnMap(x,y ) && itemAt(x, y) == item)
				return true;
		}
	}
	return false;
}

function itemDemoCost(item) {
	return Math.floor(rules.demo_base_cost
		+ Math.floor(state.rebels * rules.demo_rebels_multiplier)
		+ Math.floor(rules.item[item].cost * rules.demo_item_cost_multiplier));
}

function spawnFish() {
	while (state.fishPool.length < rules.fish_pool_max
		&& (state.fishPool.length < rules.fish_pool_min || Math.random() < rules.fish_spawn_potential)) {
		var x, y;
		if (state.fishPool.length && Math.random() < rules.fish_school_potential) {
			var f = state.fishPool[random(state.fishPool.length)];
			x = f.x;
			y = f.y;
		} else {
			x = Math.floor(Math.random() * MAP_WIDTH);
			y = Math.floor(Math.random() * MAP_HEIGHT);
		}
//		debugLog("trying to spawn fish at " + xy_str(x, y));
		if (cellIsWater(x, y)
			&& !cellIsOccupied(x, y)
			&& pathBetween({x:x, y:y}, {x:0, y:0}, WATER))
			state.fishPool.push({ x: x, y: y, count: rules.fish_per_school });
	}
};

function assessItems() {
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
				alert(item_list[item].name + " Lost to Damage/" + cause);
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

function assessLand() {
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

function refreshMap()
{
	for (var y = 0; y < MAP_HEIGHT; y++)
		drawRow(/* starting at x */0, y);
}

function refreshScreen(cls) {
	if (cls === true)
		console.clear(LIGHTGRAY);
	refreshMap();
	drawUI(/* verbosity: */2);
}

function cellContainsFish(obj) {
	if (!cellIsWater(obj))
		return false;
	var x = obj.x;
	var y = obj.y;
	for (var i = 0; i < state.fishPool.length; i++) {
		if (state.fishPool[i].x === x && state.fishPool[i].y === y)
			return true;
	}
	return false;
}

function landChar(char)
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

function drawCell(x, y, more_attr, move_cursor) {
	x = Math.floor(x);
	y = Math.floor(y);
	if (!cellOnMap(x, y))
		return false;
	if (map[y] === undefined)
		return false;
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
			char = Math.random() < 0.5 ? CHAR_FISH1 : CHAR_FISH2;
			if ((x ^ state.turn) & 1)
				attr = HIGH | BLACK | bg_attr;
		}
	}
	if (state.merchant.active) {
		var front = Math.floor(state.merchant.x) === x && Math.floor(state.merchant.y) === y;
		var tail = Math.floor(state.merchant.x) - 1 === x && Math.floor(state.merchant.y) === y && !isLand;
		if (front || tail) {
			char = front ? CHAR_MERCHANT : CHAR_MERCTAIL;
			attr = BG_BLUE | YELLOW | HIGH;
		}
	}
	if (state.pirate.active) {
		var front = Math.floor(state.pirate.x) === x && Math.floor(state.pirate.y) === y;
		var tail = Math.floor(state.pirate.x) + 1 === x && Math.floor(state.pirate.y) === y && !isLand;
		if (front || tail) {
			char = front ? CHAR_PIRATE : CHAR_PIRATAIL;
			attr = BG_BLUE | RED | HIGH;
		}
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
			if (cellIsWater(x, y)) {
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
	else if (more_attr !== undefined)
		attr |= more_attr;
	if (move_cursor !== false)
		console.gotoxy(x + 1, y + 1);
	console.attributes = attr;
	console.write(char);
	return true;
};

function drawRow(start, y, length) {
	var end = (length === undefined) ? MAP_RIGHT_COL : start + length;
	var x = Math.max(0, start);
	console.gotoxy(x + 1, y + 1);
	for (; x <= end; ++x)
		drawCell(x, y, undefined, /* move cursor */false);
};

function forceScrub(x, y, size) {
	var radius = size || 1;
	for (var dx = -radius; dx <= radius; dx++) {
		for (var dy = -radius; dy <= radius; dy++)
			drawCell(x + dx, y + dy);
	}
};

function handlePendingUpdates() {
	while (pendingUpdate.length) { // Erase the caught fish, sunk boats, etc.
		var cell = pendingUpdate.pop();
		drawCell(cell.x, cell.y);
	}
}

// --- SIMULATION ---
function handleRainfall() {
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
				if (item == ITEM_CROPS || item == ITEM_ANIMALS) {
					msg = "Rainfall";
					dd = -rules.rain_healing;
					if (!state.martialLaw)
						state.gold += rules.rain_gold_value;
				}
				damageMap[y][x] = Math.max(0, damageMap[y][x] + dd);
				alert(item_list[item].name + " received " + msg);
			} else if (Math.abs(rx - x) <= 3 && Math.abs(ry - y) <= 1) {
				if (item == ITEM_CROPS || item == ITEM_ANIMALS) {
					damageMap[y][x] = Math.max(0, damageMap[y][x] - rules.rain_healing);
					if (!state.martialLaw)
						state.gold += rules.rain_gold_value;
					alert(item_list[item].name + " received Rainfall");
				}
			}
		}
	}
}

// Returns the health of item as fraction (1.0 = 100%)
function itemHealth(item) {
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

function handleEconomics() {
	assessItems();
	if (Math.random() < item_list[ITEM_SCHOOL].count * rules.rebels_decrement_potential)
		state.rebels = Math.max(0, state.rebels - 1);

	state.popCap = 0;
	// Food adjustments and Population Cap PER TURN
	var add_food = 0;
	for (i in item_list) {
		if (rules.item[i].food_turn)
			add_food += item_list[i].count * rules.item[i].food_turn * (0.5 + (0.5 * itemHealth(i)));
		if (rules.item[i].cap)
			state.popCap += item_list[i].count * rules.item[i].cap;
	}
	state.food += Math.floor(add_food);
	state.food = Math.max(0, state.food - Math.floor(state.pop / 5));
	if (state.turn % rules.population_interval === 0) {
		if (state.food > state.pop && state.pop < state.popCap) {
			var before = state.pop;
			var growth = Math.floor(Math.random() * 3) + 1; // Betweeen 1 and 3
			growth += Math.floor(item_list[ITEM_HOSPITAL].count * rules.hospital_pop_increase_multiplier);
			state.pop = Math.min(state.popCap, state.pop + growth);
			announce("\x01h\x01gPopulation Increase from \x01w" + before + "\x01g to \x01w" + state.pop);
		}
		else if (state.food <= 0 && state.pop > 5) {
			state.pop -= Math.floor(Math.random() * 2) + 1;
			alert("\x01h\x01rFAMINE!");
		}
		var pollution = (item_list[ITEM_INDUSTRY].count * rules.industry_pollution_multiple)
			          + (item_list[ITEM_ANIMALS].count * rules.animals_pollution_multiple)
					  + (item_list[ITEM_TRAWLER].count * rules.boat_pollution_multiple)
					  + (item_list[ITEM_PTBOAT].count * rules.boat_pollution_multiple);
		pollution -= item_list[ITEM_CROPS].count * rules.crops_pollution_reduction;
		if (pollution > 0) {
			state.pop -= Math.floor(Math.random() * 2) + 1;
			alert("\x01h\x01rPOLLUTION!");
		}
	}

	if (state.turn % rules.round_interval === 0) {
		// Gold adjustments
		var taxes = 0;
		var costs = 0;
		for (var i in item_list) {
			if (rules.item[i].tax >= 0)
				taxes += item_list[i].count * rules.item[i].tax;
			else
				costs += item_list[i].count * -rules.item[i].tax;
		}
		taxes = Math.floor(taxes);
		costs = Math.floor(costs);
		var income = Math.floor(item_list[ITEM_INDUSTRY].count * (0.5 + (0.5 * itemHealth(ITEM_INDUSTRY)))
			* item_list[ITEM_CROPS].count * (0.5 + (0.5 * itemHealth(ITEM_CROPS)))
			* rules.industry_income_multiple);

		var poptax = 0;
		// Food adjustments PER ROUND
		var add_food = 0;
		for (var i in item_list) {
			if (rules.item[i].food_round)
				add_food += item_list[i].count * rules.item[i].food_round;
		}
		add_food = Math.floor(add_food);
		state.food += add_food;
		if (state.food > 0)
			poptax = Math.floor(state.pop * rules.pop_tax_multiple);
        state.rebels = Math.max(0, state.rebels - (item_list[ITEM_SCHOOL].count * rules.rebels_education_multiplier));
        if (state.food < state.pop) {
			state.rebels += 5;
			state.pop--;
		}
		if (state.martialLaw) {
			announce(format("\x01h\x01hMATIAL LAW \x01yRound costs: -\x01w$%u\x01y and +\x01w%u\x01y Food"
				, costs, add_food));
			state.gold -= costs;
		} else {
			var landtax = Math.floor(assessLand() * rules.land_tax_multiple);
			debugLog("Income $" + income + " Taxes: $" + taxes + " PopTax: $" +
				poptax + " LandTax: $" + landtax + " and Costs: $" + costs);
			var revenue = (income + taxes + poptax + landtax) - costs;
			announce(format("\x01y\x01hRound surplus: +\x01w$%u\x01y and +\x01w%u\x01y Food"
				, revenue, add_food));
			state.gold += revenue;
		}
        var netFood = state.food - (state.pop / 2);
        if (netFood < 0) {
            state.pop = Math.max(5, state.pop - 5);
            state.rebels = Math.min(100, state.rebels + rules.rebels_starvation_increase);
            alert("\x01h\x01rSTARVATION! Rebels rising ...");
        } else if (state.pop > state.popCap) {
            state.rebels = Math.min(100, state.rebels + rules.rebels_underhoused_increase);
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

function onWave(x, y) {
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

// Does not include fish
function cellIsOccupied(x, y)
{
	if (itemMap[y][x])
		return true;
	if (state.pirate.active && cellIsSame({x:x, y:y}, state.pirate))
		return true;
	if (state.merchant.active && cellIsSame({x:x, y:y}, state.merchant))
		return true;
	return false;
}

function cellContents(x, y)
{
	var things = [];
	if (itemMap[y][x])
		things.push(format("Item:%s", itemMap[y][x]));
	if (state.pirate.active && cellIsSame({x:x, y:y}, state.pirate))
		things.push("Pirate");
	if (state.merchant.active && cellIsSame({x:x, y:y}, state.merchant))
		things.push("Merchant");
	return things.toString();
}

function moveItem(ox, oy, nx, ny) {
	if (cellIsOccupied(nx, ny))
		debugLog("ITEM COLLIDING with " + cellContents(nx, ny) + " at " + xy_str(nx, ny));
	debugLog("Moving '" + itemMap[oy][ox] + "' at " + xy_str(ox, oy) + " with " + damageMap[oy][ox] + " damage to " + xy_str(nx, ny));
	itemMap[ny][nx] = itemMap[oy][ox];
	damageMap[ny][nx] = damageMap[oy][ox];
	destroyItem(ox, oy);
	drawCell(ox, oy);
	drawCell(nx, ny);
	return true;
}

/* Randomize array in-place using Durstenfeld shuffle algorithm */
function shuffleArray(array) {
    for (var i = array.length - 1; i > 0; i--) {
        var j = Math.floor(Math.random() * (i + 1));
        var temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

function itemAt(x, y) {
	return itemMap[y][x];
}

function moveItem1rand(x, y, ter, diagonal) {
	var options = [
			{x:x, y:y - 1},
			{x:x, y:y + 1},
			{x:x - 1, y:y},
			{x:x + 1, y:y}];
	if (diagonal)
		options = options.concat([
			{x:x - 1, y:y - 1},
			{x:x - 1, y:y + 1},
			{x:x + 1, y:y + 1},
			{x:x + 1, y:y - 1}]);
	shuffleArray(options);

	for (var i = 0; i < options.length; ++i) {
		var nx = options[i].x;
		var ny = options[i].y;
		if (!cellOnMap(nx, ny))
			continue;
		if (cellIsLand(nx, ny) != ter)
			continue;
		if (cellIsOccupied(nx, ny))
			continue;
		var moved = moveItem(x, y, nx, ny);
		if (moved) {
			debugLog("Moved item " + itemAt(nx, ny) + " randomly from " +
				xy_str(x, y) + " to " + xy_str(nx, ny));
			return true;
		}
	}
	return false;
}

function getArrayOfItems(type) {
	var arr = [];
	for (var y = 0; y < MAP_HEIGHT && arr.length < item_list[type].count; ++y) {
		for (var x = 0; x < MAP_WIDTH && arr.length < item_list[type].count; ++x) {
			if (itemMap[y][x] == type)
				arr.push({ x:x, y:y});
		}
	}
	return arr;
}

function objDist(a, b) {
	return Math.sqrt(Math.pow(a.x - b.x, 2) + Math.pow(a.y - b.y, 2));
}

function findItemAtCoord(list, coord) {
	for (var i = 0; i < list.length; ++i)
		if (list[i].x == coord.x && list[i].y == coord.y)
			return i;
	return -1;
}

// Move PT Boats closer to closest target (to attack or protect)
function handlePatrols() {
	var boats = getArrayOfItems(ITEM_PTBOAT);
	if (boats.length < 1)
		return;
	var trawlers = getArrayOfItems(ITEM_TRAWLER);
	if (trawlers.length < 1 && !state.pirate.active && !state.merchant.active)
		return;

	for (var i = 0; i < boats.length; ++i) {
		var boat = boats[i];
		var target = [];
		for (var t = 0; t < trawlers.length; ++t) {
			var trawler = trawlers[t];
			var dist = objDist(boat, trawler);
			if (dist > 15) // too far away
				continue;
			if (dist < 3) // close enough
				continue;
			target.push({ x:trawler.x, y:trawler.y, dist: dist });
		}
		target.sort(function (a, b) { return b.dist - a.dist; });
		// Pirate > Merchant > Trawlers
		var pdist = state.pirate.active ? objDist(boat, state.pirate) : Infinity;
		if (state.merchant.active) {
			var mdist = objDist(boat, state.merchant);
			if (mdist < 3 && pdist > 10) // close enough, don't block the merchant!
				continue;
			target.push({ x:Math.floor(state.merchant.x), y:Math.floor(state.merchant.y), dist:mdist});
		}
		if (state.pirate.active)
			target.push({ x:Math.floor(state.pirate.x), y:Math.floor(state.pirate.y), dist:pdist});
		var moved = false;
		var t;
		while ((t = target.pop()) && !moved) {
			debugLog("PT Boat at " + xy_str(boat) + " next closest target at " + xy_str(t) + " at distance " + Math.floor(t.dist));
			var nx = boat.x + (t.x > boat.x ? 1 : (t.x < boat.x ? -1 : 0)),
				ny = boat.y + (t.y > boat.y ? 1 : (t.y < boat.y ? -1 : 0));
			if (cellOnMap(nx, ny)) {
				if (itemMap[ny][nx] == ITEM_PTBOAT) // Another PT boat in the way? Look elsewhere
					continue;
				var ti = findItemAtCoord(trawlers, t);
				if (ti >= 0) // Let's not have multiple PT boats chase the same trawler
					trawlers.splice(ti, 1);
				if (cellIsWater(nx, ny) && !cellIsOccupied(nx, ny))
					moved = moveItem(boat.x, boat.y, nx, ny);
			}
		}
		if (!moved)
			moveItem1rand(boat.x, boat.y, WATER, /* diagonal */true);
	}
}

function pirateAttack() {
	if (!state.pirate.active)
		return false;
	var px = Math.floor(state.pirate.x),
		py = Math.floor(state.pirate.y);

	if (onWave(px, py))
		return false;

	// Search surrounding (9 including own) cell
	for (var y = Math.max(py - 1, MAP_TOP_ROW); y <= Math.min(py + 1, MAP_BOTTOM_ROW); y++) {
		for (var x = Math.max(px - 1, MAP_LEFT_COL); x <= Math.min(px + 1, MAP_RIGHT_COL); x++) {
			var fort_protected = itemIsAdjacent({x:x, y:y}, ITEM_FORT);
			if (state.merchant.active && state.merchant.x == x && state.merchant.y == y) {
				if (fort_protected)
					alert("Pirate attack on Merchat ship thwarted by Fort defense!");
				else {
					state.merchant.active = false;
					var plunder  = Math.floor(state.merchant.gold * rules.pirate_merchant_plunder_multiplier);
					state.pirate.gold += plunder ;
					announce("\x01h\x01rMerchant ship sunk by Pirates!  Pirates plundered \x01w$" + plunder);
					forceScrub(Math.floor(state.merchant.x), Math.floor(state.merchant.y), 1);
				}
				return true;
			}
			var item = itemAt(x, y);
			if (!item)
				continue;
			if (item == ITEM_PTBOAT) { // PT Boat / Pirate conflict
				var health = itemHealth({ x:x, y:y });
				if (Math.random() < rules.ptboat_sink_pirate_potential * (0.5 + (0.5 * health))) {
					drawCell(px, py, BLINK);
					drawCell(px + 1, py, BLINK);
					state.pirate.active = false;
					state.stats.piratesSunk++;
					var reward = 0;
					if (state.pirate.gold > 0)
						reward = Math.floor(state.pirate.gold * rules.sunk_pirate_reward_multiplier);
					announce("\x01h\x01bPirate ship sunk by PT Boat! Reward: \x01w$" + reward);
					state.gold += reward;
					pendingUpdate.push({ x: px, y: py});
					pendingUpdate.push({ x: px + 1, y: py});
					return true;
				}
				if (!fort_protected) {
					if (damageItem(x, y)) {
						var plunder = Math.floor(rules.item[ITEM_PTBOAT].cost * (0.5 + (0.5 * health)));
						state.pirate.gold += plunder;
						state.stats.boatsLost++;
						announce("\x01h\x01rPirates plundered \x01w$" + plunder + "\x01r from " + item_list[item].name);
					}
					return true;
				}
				continue;
			}
			var isLand = cellIsLand(x, y);
			if (!isLand || (state.turn - state.pirate.last_attack) >= rules.pirate_land_attack_interval) {
				if (fort_protected)
					alert("Pirate attack on " + item_list[item].name + " thwarted by Fort defense!");
				else {
					var damage = isLand ? rules.pirate_land_attack_damage : rules.pirate_sea_attack_damage;
					var plunder = Math.floor(rules.item[item].cost * (0.5 + (0.5 * itemHealth({ x:x, y:y }))));
					if (damageItem(x, y, "Pirate attack", damage)) {
						state.pirate.gold += plunder
						if (item == ITEM_TRAWLER)
							state.stats.trawlersSunk++;
						announce("\x01h\x01rPirates plundered \x01w$" + plunder + "\x01r from " + item_list[item].name);
					}
					state.pirate.last_attack = state.turn;
				}
				return true;
			}
		}
	}
}

// Trawlers pursue fish
function moveTrawlers() {
	var trawlers = getArrayOfItems(ITEM_TRAWLER);
	for (var ti = 0; ti < trawlers.length; ++ti) {
		var trawler = trawlers[ti];
		var fish = [];
		for (var fi = 0; fi < state.fishPool.length; ++fi) {
			var f = state.fishPool[fi];
			var dist = objDist(trawler, f);
			if (dist > 10) // too far away
				continue;
			f = { dist: dist, x: f.x, y: f.y };
//				debugLog("fish nearby: " + JSON.stringify(f));
			fish.push(f);
		}
		fish.sort(function (a, b) { return b.dist - a.dist; });
		var moved = false;
		var f = fish.pop();
		if (f) {
//			debugLog("closest fish: " + JSON.stringify(f));q
			if (f.dist < 2) // We're catching fish here, don't move!
				continue;
			var nx = trawler.x + (f.x > trawler.x ? 1 : (f.x < trawler.x ? -1 : 0)),
				ny = trawler.y + (f.y > trawler.y ? 1 : (f.y < trawler.y ? -1 : 0));
			if (cellOnMap(nx, ny) && !cellIsLand(nx, ny) && !cellIsOccupied(nx, ny))
				moved = moveItem(trawler.x, trawler.y, nx, ny);
		}
		if (!moved)
			moveItem1rand(trawler.x, trawler.y, WATER, /* diagonal */true);
	}
};

function checkMutiny() {
	// Only happens if rebels are high and a pirate isn't already active
	if (!state.martialLaw && state.rebels > rules.rebels_mutiny_theshold && !state.pirate.active) {
		// Chance increases as rebels approach 100%
		var mutinyChance = (state.rebels - rules.rebels_mutiny_theshold) / (rules.rebels_mutiny_theshold * 10);
		if (Math.random() < mutinyChance) {

			// Find all PT Boats
			var boats = getArrayOfItems(ITEM_PTBOAT);
            if (boats.length > 0) {
                var target = boats[Math.floor(Math.random() * boats.length)];
				if (itemIsAdjacent(target, ITEM_FORT))
					announce("\x01h\x01rATTEMPTY MUTINTY of PT Boat thwarted by Fort Defenses!");
				else {
					// The Mutiny occurs
					state.pirate.active = true;
					state.pirate.x = target.x;
					state.pirate.y = target.y;
					state.pirate.gold = Math.floor(rules.pirate_gold * (0.5 + (0.5 * itemHealth({ x:target.x, y:target.y }))));
					destroyItem(target.x, target.y);
					announce("\x01h\x01rMUTINY! Rebels stole a PT Boat!");
					state.stats.boatsLost++;
					// Visual update
					drawCell(target.x, target.y);
					console.beep(); // Alert the governor!
				}
            }
        }
    }
};

function spawnMerchant() {
	var ry = Math.floor(Math.random() * 20);
//	debugLog("trying totrying to spawn merchant at " + xy_str(0, ry));
	if (cellIsWater(0, ry)) { // What if cell is occupied by boat?
		state.merchant = {
			active: true,
			x: 0, y: ry,
			gold: rules.merchant_gold,
			last_dock: 0,
			blocked: {},
			moves: {},
		};
		debugLog("Merchant spawned at row " + ry);
		drawCell(0, ry);
		alert("\x01g\x01hMerchant voyage sighted in the West!");
		return true;
	}
	debugLog("Could NOT spawn merchant at row " + ry);
	return true;
}

function spawnPirate() {
	var ry = Math.floor(Math.random() * 20);
//	debugLog("trying to spawn pirate at " + xy_str(0, ry));
	if (cellIsWater(MAP_RIGHT_COL, ry)) { // What if cell is occupied by boat?
		state.pirate = {
			active: true,
			x: MAP_RIGHT_COL, y: ry,
			gold: rules.pirate_gold,
			last_attack: 0,
			blocked: {},
			moves: {},

		};
		debugLog("Pirate spawned at row " + ry);
		drawCell(state.pirate.x, ry);
		alert("\x01r\x01hArgh, be wary Matey!  Pirates spied in the East!");
		return true;
	}
	debugLog("Could NOT spawn pirate at row " + ry);
	return false;
}

function spawnStorm() {
	state.storm.active = true;
	state.storm.x = MAP_RIGHT_COL + 1;
	state.storm.y = Math.floor((MAP_MID_ROW / 2) + (Math.random() * (MAP_HEIGHT * 0.60)));
	state.storm.bias = (Math.random() - Math.random()) * 1.5;
	alert("\x01n\x01hSevere storm warning!");
	debugLog("storm spawned at row " + state.storm.y
		+ " with bias " + state.storm.bias);
}

function spawnRain() {
	state.rain.active = true;
	state.rain.y = -1;
	state.rain.x = 3 + Math.floor(Math.random() * (MAP_WIDTH - 3));
	state.rain.bias = (Math.random() - Math.random()) * 3.0;
	debugLog("rain spawned at column " + state.rain.x
		+ " with bias " + state.rain.bias);
}

function spawnWave() {
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

function avoidStuff(ship, eastward, objects) {
	debugLog((eastward ? "merchant" : "pirate") + " avoiding land from " + xy_str(ship));

	var x = Math.floor(ship.x);
	var y = Math.floor(ship.y);

	ship.blocked[xy_pair(x, y)] = 1;

	var moves = [];
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
		var key = xy_pair(x, y) + move;
		if (ship.moves[key]) { // Don't repeat any move already made
//			debugLog("skipping repeated move: " + key);
			continue;
		}
		var nx = x, ny = y;
		switch(move) {
			case "N":
				--ny;
				break;
			case "S":
				++ny;
				break;
			case "E":
				++nx;
				break;
			case "W":
				--nx;
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
		if (ny < MAP_TOP_ROW || ny > MAP_BOTTOM_ROW)
			continue;
		if (nx < MAP_LEFT_COL || nx > MAP_RIGHT_COL)
			continue;
		debugLog("attempt to avoid stuff with " + key + " move to " + xy_str(nx, ny));
		if (cellIsWater(nx, ny) && (!objects || !objects[ny][nx])) {
			debugLog("successful avoidance via " + key + " move to " + xy_str(nx, ny));
			if (ship.moves[key])
				ship.moves[key]++;
			else
				ship.moves[key] = 1;
			ship.y = ny;
			ship.x = nx;
			return true;
		}
	}
	return false;
}

function compareXY(a, b) {
	return Math.max(Math.abs(Math.floor(a.x) - Math.floor(b.x))
		, Math.abs(Math.floor(a.y) - Math.floor(b.y)));
}

function pathBetween(a, b, ter) {
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
			if (cellOnMap(nx, ny)) {
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

function pathToHomeland(x, y) {
	if (!state.homeland)
		return cellIsLand(x, y);
	if (typeof x != "object")
		x = { x: x, y: y };
	return pathBetween(x, state.homeland, LAND);
}

function pathToDock(x, y) {
	if (typeof x != "object")
		x = { x: x, y: y };
	return pathBetween(x, state.homeland, LAND) && pathBetween(x, ITEM_DOCK);
}

function handleWeather() {
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
		if (sx < MAP_LEFT_COL || sy < MAP_TOP_ROW || sy > MAP_BOTTOM_ROW) {
			state.storm.active = false;
			forceScrub(0, sy, 2);
		}
		else if (itemMap[sy][sx]) {
			announce("\x01h\x01rStorm destroyed " + item_list[itemMap[sy][sx]].name + "!");
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
		// rain storm purposely starts at y=-1, so don't use cellOnMap() here
		if (ry > MAP_BOTTOM_ROW || rx < MAP_LEFT_COL || rx > MAP_RIGHT_COL) {
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
			if (wave.length < rules.wave_min_width || Math.random() < rules.wave_increase_potential)
				wave.length++;
			else if (wave.length > rules.wave_min_width && Math.random() < rules.wave_decrease_potential)
				wave.length--;
		}
		if (wave.length < 1 || wave.x - wave.length >= MAP_RIGHT_COL)
			state.waves.splice(i, 1);
		drawRow(wave.x - (wave.length + 1), wave.y, wave.length + 1);
	}
	if (state.waves.length < 1 || Math.random() < rules.wave_potential)
		spawnWave();
}

function moveFish() {
	// Fish
	// Iterate backwards through the fish pool so we can safely remove caught fish
	for (var i = state.fishPool.length - 1; i >= 0; i--) {
		var f = state.fishPool[i];
		if (f.count < 1) {
			state.fishPool.splice(i, 1); // Fish are all gone
			continue;
		}
		if (cellIsOccupied(f.x, f.y) || Math.random() < rules.fish_move_potential) {
			var oldX = f.x,
				oldY = f.y;
			var nx = f.x + (random(3) - 1);
			var ny = f.y + (random(3) - 1);
			// If fish leaves map, remove it
			if (!cellOnMap(nx, ny)) {
				state.fishPool.splice(i, 1);
				drawCell(oldX, oldY);
				continue;
			}
			// Move if target is water
//			debugLog("trying to move fish to " + xy_str(nx, ny));
			if (cellIsWater(nx, ny)) {
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
				if (cellOnMap(tx, ty)) {
					if (itemAt(tx, ty) === ITEM_TRAWLER && Math.random() < rules.fish_catch_potential) {
						var count = 1 + random(rules.fish_catch_max);
						state.food += Math.floor(rules.fish_food_value * count);
						if (!state.martialLaw)
							state.gold += Math.floor(rules.fish_gold_value * count);
						drawCell(f.x, f.y, BLINK);
						pendingUpdate.push(f);
						alert("\x01h\x01gFish caught!");
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
}

function moveMerchant()
{
	var oldMerc = {
		x: Math.floor(state.merchant.x),
		y: Math.floor(state.merchant.y),
		active: state.merchant.active
	};

	// Merchant (West to East)
	if (state.merchant.active) {
		debugLog("Merchant active at " + xy_str(state.merchant));
		if (!state.martialLaw)
			state.gold += rules.merchant_turn_gold_value;
		var move_key = xy_pair(state.merchant) + "E";
		var nx = state.merchant.x;
		var ny = state.merchant.y;
		if (onWave(state.merchant))
			nx += rules.merchant_speed * 2.0;
		else
			nx += rules.merchant_speed;
		debugLog("Merchant considering move to " + xy_str(nx, ny));
		var mx = Math.floor(nx),
			my = Math.floor(ny);
		if (cellIsSame(state.merchant, {x:mx, y:my})) { // fractional move
			state.merchant.x = nx;
			state.merchant.y = ny;
			debugLog("Merchant fractional move to " + xy_str(state.merchant));
		}
		else if (mx > MAP_RIGHT_COL) {
			state.merchant.active = false;
			if (!state.martialLaw && state.merchant.gold > 0) {
				state.gold += state.merchant.gold;
				announce("\x01h\x01yInternational Trade Successful! +$" + state.merchant.gold);
				state.stats.intlTrades++;
			}
			forceScrub(mx, my, 1);
		}
		else if (cellIsWater(mx, my) && !itemAt(mx, my) && pathToDock(mx, my)) {
			state.merchant.x = nx;
			state.merchant.y = ny;
			debugLog("Merchant moved to dock " + xy_str(state.merchant));
			if (!state.martialLaw && state.merchant.gold && state.turn - state.merchant.last_dock >= rules.merchant_dock_interval) {
				var amt = Math.min(state.merchant.gold, rules.merchant_dock_bonus);
				state.gold += amt;
				announce("\x01h\x01yMerchant Docked and Traded Successfulfly! +$" + amt);
				state.stats.dockTrades++;
				state.merchant.gold -= amt;
				if (state.merchant.gold < 1) {
					state.merchant.active = false;
					forceScrub(mx, my, 1);
				} else {
					drawCell(mx, my, BLINK);
					drawCell(mx - 1, my, BLINK);
					pendingUpdate.push(state.merchant.x, state.merchant.y);
					pendingUpdate.push(state.merchant.x - 1, state.merchant.y);
					state.merchant.last_dock = state.turn;
				}
			}
		}
		else if (cellIsLand(mx, my) || itemAt(mx, my) // Can't use cellIsOccupied here
			|| state.merchant.blocked[xy_pair(mx, my)]
			|| state.merchant.moves[move_key]) { // Don't repeat any move already made
			debugLog("Merchant trying to avoid stuff from " + xy_str(state.merchant));
			// Try to move up or down around a coast line
			if (!avoidStuff(state.merchant, /* eastward: */true, itemMap)) {
				debugLog("MERCHANT BEACHED at " + xy_str(state.merchant));
				alert("Merchant ship run aground and sunk");
				if (debug_enabled) console.getkey();
				state.merchant.active = false;
			} else {+
				debugLog("Merchant successfully avoided stuff by moving from " 
					+ xy_str(state.merchant) + " to " + xy_str(nx, ny));
			}
			forceScrub(mx, my, 1);
			debugLog("Merchant moves: " + JSON.stringify(state.merchant.moves));
		} else {
			state.merchant.moves[move_key] = 1;
			state.merchant.x = nx;
			state.merchant.y = ny;
			debugLog("Merchant successful move to open water at " + xy_str(state.merchant));
		}
	}
	else if (Math.random() < rules.merchant_spawn_potential)
		spawnMerchant();

	if (oldMerc.active) forceScrub(oldMerc.x, oldMerc.y, 1);
	if (state.merchant.active) forceScrub(state.merchant.x, state.merchant.y, 1);
}

function movePirate() {
	if (!state.pirate.active) {
		if (Math.random() < rules.pirate_spawn_potential)
			return spawnPirate();
		return false;
	}

	if (!onWave(state.pirate)) {
		var px = Math.floor(state.pirate.x);
		var py = Math.floor(state.pirate.y);

		// Pirate pursues Merchant ship (to the West, North, or South)
		if (state.merchant.active && state.merchant.x <= px) {
			var mx = Math.floor(state.merchant.x);
			var my = Math.floor(state.merchant.y);
			var dist = Math.sqrt(Math.pow(mx - px, 2) + Math.pow(my - py, 2));
			if (dist < 10) {
				var nx = px + (mx > px ? 1 : (mx < px ? -1 : 0)),
					ny = py + (my > py ? 1 : (my < py ? -1 : 0));
				if (cellOnMap(nx, ny) && !cellIsLand(nx, ny) && !cellIsOccupied(nx, ny)) {
					state.pirate.x = nx;
					state.pirate.y = ny;
					drawCell(nx, ny);
					drawCell(nx + 1, ny);
					drawCell(px, py);
					drawCell(px + 1, py);
					return true;
				}
			}
		}

		// Pirate pursues nearest Trawler (to the West, North, or South)
		var trawlers = getArrayOfItems(ITEM_TRAWLER);
		if (trawlers.length) {
			var target = [];
			for (var t = 0; t < trawlers.length; ++t) {
				var trawler = trawlers[t];
				var dist = objDist(state.pirate, trawler);
				debugLog("Trawler " + xy_str(trawler) + " dist from pirate: " + dist);
				if (trawler.x > px || dist > 15) // too far away
					continue;
				target.push({ x:trawler.x, y:trawler.y, dist: dist });
			}
			if (target.length) {
				target.sort(function (a, b) { return b.dist - a.dist; });
				var t = target.pop();
				var nx = px + (t.x > px ? 1 : (t.x < px ? -1 : 0)),
					ny = py + (t.y > py ? 1 : (t.y < py ? -1 : 0));
				debugLog("Pirate pursuing trawler at " + xy_str(t) + " by moving to " + xy_str(nx, ny));
				if (cellIsWater(nx, ny) && !state.pirate.blocked[xy_pair(nx, ny)] && !itemAt(nx, ny)) {
					state.pirate.x = nx;
					state.pirate.y = ny;
					drawCell(nx, ny);
					drawCell(nx + 1, ny);
					drawCell(px, py);
					drawCell(px + 1, py);
					return true;
				}
				debugLog(xy_str(nx, ny) + " blocking pirate move");
			}
		}
	}

	// Pirate general hunt/escape progress (East to West)

	var oldPirate = {
		x: Math.floor(state.pirate.x),
		y: Math.floor(state.pirate.y),
	};

	var move_key = xy_pair(state.pirate) + "W";
	var nx = state.pirate.x;
	var ny = state.pirate.y;
	if (onWave(state.pirate))
		nx -= rules.pirate_speed / 2;
	else
		nx -= rules.pirate_speed;
	var px = Math.floor(nx),
		py = Math.floor(ny);
	debugLog("trying to move pirate from " + xy_str(state.pirate));
	if (cellIsSame(state.pirate, {x:nx, y:ny})) { // fractional move
		state.pirate.x = nx;
		state.pirate.y = ny;
		return false;
	}
	if (px < MAP_LEFT_COL) {
		state.pirate.active = false;
		if (state.pirate.gold > 0) {
			var penalty = Math.floor(state.pirate.gold * rules.pirate_escape_penalty_multiplier);
			state.gold = Math.max(0, state.gold - penalty);
			announce("\x01h\x01rPirates Escaped with Loot ($" + state.pirate.gold + ") penalized -\x01w$" + penalty);
		}
		forceScrub(0, py, 1);
		return true;
	}
	if (!cellIsWater(px, py)
		|| state.pirate.blocked[xy_pair(px, py)]
		|| state.pirate.moves[move_key]) {
//			debugLog(xy_str(px, py) + " is not water: " + map[py][px]);
		// Try to move up or down around a coast line
		if (!avoidStuff(state.pirate, /* eastward: */false)) {
			debugLog("PIRATE BEACHED at " + xy_str(state.pirate) + " after moves " + JSON.stringify(state.pirate.moves));
			alert("Pirate ship run aground and sunk");
			if (debug_enabled) console.getkey();
			state.pirate.active = false;
			forceScrub(px, py, 1);
		}
		debugLog("Pirate moves: " + JSON.stringify(state.pirate.moves));
	}
	else {
		state.pirate.moves[move_key] = 1;
		state.pirate.x = nx;
		state.pirate.y = ny;
		debugLog("pirate move succesfully to open water at " + xy_str(state.pirate));
	}

	// Redraw
	forceScrub(oldPirate.x, oldPirate.y, 1);
	if (state.pirate.active)
		forceScrub(state.pirate.x, state.pirate.y, 1);
}

function moveEntities() {
	handlePatrols();
	if (!pirateAttack())
		movePirate();
	moveTrawlers();
	moveFish();
	spawnFish(); // Replenish the pool
	moveMerchant();
	checkMutiny(); // Check for Rebel uprisings
};

// --- SCORING & UI ---
function calculateScore() {
	return (state.pop * 10)
			+ Math.floor(state.gold / 10)
			+ Math.floor(state.food) - (state.rebels * 5);
};

function getRank() {
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

function getRetirementTitle(score) {
    for (var i = 0; i < TITLES.length; i++) {
        if (score >= TITLES[i].min)
			return TITLES[i].name;
    }
    return "Absentee Administrator";
};

function cond_cleartoeol() {
	if (console.current_column) console.cleartoeol();
}

var last_gold;
var last_food;
var last_pop;
var last_cap;
var last_reb;

function drawUI(verbose) {
	var lx = 62;
	var ly = 1;

	console.aborted = false;
	var item_key = itemMap[state.cursor.y][state.cursor.x];

	if (verbose > 1) {
		console.gotoxy(lx, ly++);
		console.attributes = LIGHTGRAY;
		console.putmsg("  Items     Health");
		for (var i in item_list) {
			console.gotoxy(lx, ly++);
			var item = item_list[i];
			console.attributes = item.attr || (WHITE | HIGH);
			console.print(item.char);
			if (i == item_key)
				console.attributes = HIGH | WHITE;
			else if (item_key)
				console.attributes = HIGH | BLACK;
			else
				console.attributes = LIGHTGRAY;
			console.print(" " + item.name);
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
		demo_cost = "\x01r\x01h$" + itemDemoCost(item_key);
	} else
		build_attr = "\x01h";
	ly = Object.keys(item_list).length + 2;
	console.gotoxy(lx, ly++);
	if (state.pirate.active && cellIsSame(state.cursor, state.pirate))
		console.putmsg(" \x01r\x01hPirate w/\x01w$" + state.pirate.gold);
	else if (state.merchant.active && cellIsSame(state.cursor, state.merchant))
		console.putmsg(" \x01y\x01hMerchant w/\x01w$" + state.merchant.gold);
	else if (cellContainsFish(state.cursor))
		console.putmsg("  \x01g\x01hSigns of Fish");
	cond_cleartoeol();

	console.gotoxy(lx, ly++);
	if (state.in_progress && item_key)
		console.putmsg("\x01n[" + demo_attr + "Del\x01n] Demolish " + demo_cost);
	else
		console.putmsg("\x01n[" + build_attr + "Enter\x01n] Build Item");
	cond_cleartoeol();
	console.gotoxy(lx, ly++);
	if (state.in_progress && !item_key && state.lastbuilt)
		console.putmsg("\x01n[" + build_attr + "Spc\x01n] " + item_list[state.lastbuilt].name
			+ " \x01y\x01h$" + rules.item[state.lastbuilt].cost);
	cond_cleartoeol();
	console.gotoxy(lx, ly++);

	if (state.in_progress)
		console.putmsg("\x01n[\x01h+\x01n/\x01h-\x01n] Adjust Speed");
	console.cleartoeol();
	console.gotoxy(lx, ly++);
	if (state.log.length)
		console.putmsg("\x01n[\x01hL\x01n]og of News");
	console.cleartoeol();
	console.gotoxy(lx, ly++);
	if (state.in_progress)
		console.putmsg("\x01n[\x01hM\x01n]artial Law: " + (state.martialLaw > 0 ? ("\x01h\x01r" + state.martialLaw) : "\x01nOFF"));
	console.cleartoeol();
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hH\x01n]elp\x01>");
	console.gotoxy(lx, ly++);
	console.putmsg("\x01n[\x01hQ\x01n]uit");
	if (state.in_progress)
		console.putmsg("/Pause Game");
	console.cleartoeol();

	if (verbose) {

		console.gotoxy(1, MAP_HEIGHT + 1);
		console.attributes = BG_BLACK | LIGHTGRAY;
		const up_arrow = "\x01-\x01g\x18";
		const dn_arrow = "\x01-\x01r\x19";
		var gold_dir = " ";
		if (last_gold !== undefined) {
			if (state.gold > last_gold)
				gold_dir = up_arrow;
			else if (state.gold < last_gold)
				gold_dir = dn_arrow;
		}
		var food_dir = " ";
		if (last_food !== undefined) {
			if (state.food > last_food)
				food_dir = up_arrow;
			else if (state.food < last_food)
				food_dir = dn_arrow;
		}
		var pop_dir = " ";
		if (last_pop !== undefined) {
			if (state.pop > last_pop)
				pop_dir = up_arrow;
			else if (state.pop < last_pop)
				pop_dir = dn_arrow;
		}
		var cap_dir = " ";
		if (last_cap !== undefined) {
			if (state.popCap > last_cap)
				cap_dir = up_arrow;
			else if (state.popCap < last_cap)
				cap_dir = dn_arrow;
		}
		var reb_dir = " ";
		if (last_reb !== undefined) {
			if (state.reb > last_reb)
				reb_dir = up_arrow;
			else if (state.reb < last_reb)
				reb_dir = dn_arrow;
		}
		var gold = state.gold > 999 ? format("%1.1fK", state.gold / 1000) : state.gold;
		var food = state.food > 999 ? format("%1.1fK", state.food / 1000) : state.food;
		var pop = state.pop > 999 ? format("%1.1fK", state.pop / 1000) : state.pop;
		var popCap = state.popCap > 999 ? format("%1.1fK", state.popCap / 1000) : state.popCap;
		console.putmsg(format(
			"\x01n\x01h\x01yGold: %s%s%s " +
			"\x01n\x01h\x01gFood: %s%s%s " +
			"\x01n\x01h\x01wPop:%s\x01n%s/%s%s%s " +
			"\x01n\x01h\x01wReb: %s%2d%%%s  " +
			"\x01n\x01h\x01cRnd: %s%4.1f/%d\x01>"
			, state.gold < state.popCap ? "\x01r\x01i" : "\x01n"
			, gold
			, gold_dir
			, state.started && state.food < state.pop ? "\x01r\x01i" : "\x01n"
			, food
			, food_dir
			, pop_dir
			, pop
			, state.started && state.pop >= state.popCap ? "\x01r\x01i" : "\x01n"
			, popCap
			, cap_dir
			, state.rebels >= 50 ? "\x01r\x01i" : "\x01n"
			, state.rebels
			, reb_dir
			, state.turn / 10 >= (rules.max_turns - 1) / 10 ? "\x01r" : "\x01n"
			, state.turn / 10, rules.max_turns / 10));
		console.cleartoeol();
		var msg_index = 0;
		for (var i = 0; i < MAX_MSGS; ++i) {
			console.gotoxy(1, MAP_HEIGHT + 2 + i);
			if (state.msg.length >= MAX_MSGS - i) {
				console.putmsg("\x01n\x01h" + state.msg[msg_index].txt);
				if (state.msg[msg_index].repeat > 0)
					console.print(" [x" + (state.msg[msg_index].repeat + 1) + "]");
				msg_index++;
			}
			console.cleartoeol();
		}
	}
	hideCursor();
};

function buildItem(type) {
	var item = item_list[type];
	if (!item) {
		debugLog("buildItem() called with " + typeof item + ": " + item);
		return false;
	}
	if (cellIsOccupied(state.cursor.x, state.cursor.y)) {
		alert("Space already occupied!");
		return false;
	}
	var ter = map[state.cursor.y][state.cursor.x];
	if (!((ter !== WATER && rules.item[type].land) || (ter === WATER && rules.item[type].sea))) {
		alert("Wrong terrain to build " + item.name);
		return false;
	}
	if (type == ITEM_DOCK) {
		if (map[state.cursor.y][state.cursor.x - 1] != WATER
			&& map[state.cursor.y][state.cursor.x + 1] != WATER
			&& map[state.cursor.y - 1][state.cursor.x] != WATER
			&& map[state.cursor.y + 1][state.cursor.x] != WATER) {
			alert("Docks should be built near water");
			return false;
		}
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
		if (!cellOnMap(state.cursor.x, state.cursor.y)) {
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
	alert(msg);
	state.started = true;
	state.in_progress = true;
	return true;
}

function damageItem(x, y, cause, count) {
	var destroyed = false;
	debugLog(cause + " damaging item at " + xy_str(x, y));
	var item = itemAt(x,y);
	var msg = cause + " damaged ";
	damageMap[y][x] += (count || 1);
	if (damageMap[y][x] > rules.item[item].max_damage) {
		msg += "and destroyed ";
		drawCell(x, y, BLINK);
		pendingUpdate.push({ x: x, y: y});
		destroyed = destroyItem(x, y);
	}
	alert(msg + item_list[item].name + "!");
	return destroyed;
}

function destroyItem(x, y) {
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

function showBuildMenu() {
	var menuX = 6,
		menuY = 3;

	console.aborted = false;
	for (var i = 0; i < 16; i++) {
		console.gotoxy(menuX, menuY + i);
		console.attributes = BG_BLACK | LIGHTGRAY;
		console.write(format("%50s", ""));
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
		console.gotoxy(menuX + 4, ++menuY);
		console.putmsg("Last built: \x01h" + item_list[state.lastbuilt].name);
		console.putmsg("\x01n (SPACE to build again)");
	}
	hideCursor();
	var cmd = console.getkey(K_UPPER | K_NOSPIN);
	for (var y = 0; y < MAP_HEIGHT; y++)
		drawRow(/* starting at x */0, y);
	if (cmd == ' ')
		buildItem(state.lastbuilt);
	else if (item_list[cmd])
		buildItem(cmd);
	drawUI();
};

function saveHighScore(title) {
	var score = calculateScore(),
		scores = [];
	if (file_exists(SCORE_FILE)) {
		var f = new File(SCORE_FILE);
		if (f.open("r")) {
			scores = JSON.parse(f.read());
			f.close();
		}
	}
	scores.push({
		name: user.alias,
		score: score,
		title: title,
		pop: state.pop,
		date: Date.now() / 1000,
		map: state.map_name,
		map_hash: sha1_calc(JSON.stringify(map)),
		rules: rules.name,
		rules_hash: sha1_calc(JSON.stringify(rules)),
		});
	scores.sort(function(a, b) { return b.score - a.score; });
	var f = new File(SCORE_FILE);
	if (f.open("w")) {
		f.write(JSON.stringify(scores.slice(0, 10)));
		f.close();
	}
};

function showHighScores() {
	console.aborted = false;
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
	console.putmsg("\r\nAny key to return to " + system.name + " ...");
	console.getkey();
};

function finishGame() {
	console.aborted = false;
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

function handleOperatorCommand(key) {
	if (debug_enabled) {
		switch (key) {
			case '*':
				rules.max_turns += 100;
				return true;
			case '&':
				state.food += 100;
				return true;
			case '#':
				state.gold += 100;
				return true;
			case '$':
				spawnMerchant();
				refreshMap();
				return true;
			case '!':
				spawnPirate();
				refreshMap();
				return true;
			case '%':
				spawnStorm();
				refreshMap();
				return true;
			case '"':
				spawnRain();
				refreshMap();
				return true;
			case '>':
				spawnWave();
				refreshMap();
				return true;
			case CTRL_S:
				saveGame();
				return true;
			case CTRL_T:
				saveGame('\t');
				return true;
			case CTRL_Z:
				dumpObj(rules, js.exec_dir + "rules.json", null, "\t");
				return true;
		}
	}
	if (state.in_progress) {
		drawUI();
		return false;
	}
	switch (key) {
		case ',':
			map[state.cursor.y][state.cursor.x] = WATER;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case 'N':
			map[state.cursor.y][state.cursor.x] = LAND_N;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case 'S':
			map[state.cursor.y][state.cursor.x] = LAND_S;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case 'E':
			map[state.cursor.y][state.cursor.x] = LAND_E;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case 'W':
			map[state.cursor.y][state.cursor.x] = LAND_W;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case '/':
			map[state.cursor.y][state.cursor.x] = LAND;
			drawCell(state.cursor.x, state.cursor.y);
			state.cursor.x++;
			return true;
		case CTRL_X:
			state.fishPool.length = 0;
			clearMap();
			refreshMap();
			return true;
		case CTRL_O:
			loadMap(MAP_FILE);
			refreshMap();
			return true;
		case CTRL_D:
			dumpObj(map, MAP_FILE);
			return true;
	}
	return false;
}

function saveGame(space) {
	var sf = new File(SAVE_FILE);
	if (sf.open("w")) {
		sf.write(JSON.stringify({
			state: state,
			map: map,
			rules: rules,
			itemMap: itemMap,
			damageMap: damageMap
		}, null, space));
		sf.close();
		debugLog("Saved game data to " + SAVE_FILE);
		alert("Game saved.");
		return true;
	}
	js.global.log(LOG_WARNING, "Failed to save game data to " + SAVE_FILE);
	return false;
}

function dumpObj(obj, filename, replacer, space) {
	var sf = new File(filename);
	if (sf.open("w")) {
		sf.write(JSON.stringify(obj, replacer, space));
		sf.close();
		debugLog("Saved game debug to " + sf.name);
		return true;
	}
	js.global.log(LOG_WARNING, "Failed to save game data to " + sf.name);
	return false;
}

function removeSavedGame() {
	if (file_exists(SAVE_FILE) && file_remove(SAVE_FILE))
		debugLog("Saved game data removed: " + SAVE_FILE);
}

function handleQuit() {
    console.gotoxy(1, 24);
    console.cleartoeol();

    var prompt = "\x01n\x01h\x01rQuit? ";

    if (state.in_progress)
        prompt += "\x01c[\x01wS\x01c]ave Game \x01n\x01r|\x01h "
	if (state.started)
		prompt += "\x01c[\x01wF\x01c]inish Game \x01n\x01r|\x01h ";
	prompt += "\x01r[\x01wA\x01r]bandon Game \x01c\x01n\x01r|\x01h \x01c[\x01wC\x01c]ontinue Game";

    console.putmsg(prompt);
	console.cleartoeol();
    var choice = console.getkey(K_UPPER);

    if (choice === 'S' && state.in_progress) {
		console.putmsg("\r\x01n\x01hSee you next time, Governor!  Returning to " + system.name + "\x01n\x01>");
		return true;
	} else if (choice === 'A') {
		state.in_progress = false;
		console.putmsg("\r\x01n\x01hSorry to see you leave, play again soon!  Returning to " + system.name + "\x01n\x01>");
		return true;
    } else if (choice === 'F' && state.started) {
        finishGame();
		return true;
    } else if (choice === 'V') {
        showHighScores();
        refreshScreen(); // Redraw map after returning from high scores
    } else {
//        pushMsg("Welcome back, Governor!"); // Not an alert
        drawUI();
    }
	return false;
};

function loadMap(filename) {
	var success = false;
	var mapF = new File(filename);
	if (mapF.exists && mapF.open("r")) {
		try {
			map = JSON.parse(mapF.read());
			success = true;
		} catch(e) {
			js.global.log(LOG_WARNING, e + ": " + mapF.name);
		}
		mapF.close();
	} else
		log(LOG_WARNING, "Error opening " + filename);
	return success;
}

function clearMap() {
	for (var y = 0; y < MAP_HEIGHT; y++) {
		map[y] = [];
		for (var x = 0; x < MAP_WIDTH; x++) {
			map[y][x] = WATER;
		}
	}
}

function genMap() {
	clearMap();
	for (var i = 0; i < 3; i++) {
		var cx = 10 + (i * 20)
		var cy = 11;
		for (var n = 0; n < 300; n++) {
			if (cellOnMap(cx, cy))
				map[cy][cx] = LAND;
			cx += Math.floor(Math.random() * 3) - 1;
			cy += Math.floor(Math.random() * 3) - 1;
			if (cx < 2) cx = 2;
			if (cx > MAP_RIGHT_COL - 2 ) cx = MAP_RIGHT_COL - 2;
			if (cy < 2) cy = 2;
			if (cy > MAP_BOTTOM_ROW - 2) cy = MAP_BOTTOM_ROW - 2;
		}
	}

	// Rough up the edges of land masses
	for (var y = 2; y < MAP_BOTTOM_ROW - 1; ++y) {
		for (var x = 2; x < MAP_RIGHT_COL - 1; ++x) {
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
}

function loadRules(filename) {
	var f = new File(js.exec_dir + filename);
	if (!f.open("r"))
		return false;
	var result = false;
	try {
		result = JSON.parse(f.read());
	} catch (e) {
		js.global.log(LOG_WARNING, e + ": " + f.name);
	}
	f.close();
	return result;
}

// --- RUNTIME ---
function main () {
	var settingsF = new File(js.exec_dir + "settings.ini");
	if (settingsF.open("r")) {
		var ini = settingsF.iniGetObject();
		settings.map_list = settingsF.iniGetAllObjects("filename", "map:");
		settings.rules_list = settingsF.iniGetAllObjects("filename", "rules:");
		settingsF.close();
		if (ini.intro_msg)
			settings.intro_msg = js.exec_dir + ini.intro_msg;
		if (ini.tick_interval)
			settings.tick_interval = ini.tick_interval;
	}
	state.tick_interval = settings.tick_interval;

	if (console.optimize_gotoxy !== undefined) {
		js.on_exit("console.optimize_gotoxy = " + console.optimize_gotoxy);
		console.optimize_gotoxy = true;
	}
	var game_restored = false;
	var saveF = new File(SAVE_FILE);
	if (saveF.open("r")) {
		try {
			var s = JSON.parse(saveF.read());
			state = s.state;
			map = s.map;
			rules = s.rules;
			itemMap = s.itemMap;
			damageMap = s.damageMap;
			game_restored = true;
		} catch(e) {
			js.global.log(LOG_WARNING, e + ": " + SAVE_FILE);
		}
		saveF.close();
	}
	if (!game_restored) {
		if (file_exists(settings.intro_msg)) {
			console.clear();
			console.printfile(settings.intro_msg, P_PCBOARD);
		}
		for (var y = 0; y < MAP_HEIGHT; y++) {
			itemMap[y] = [];
			damageMap[y] = [];
			for (var x = 0; x < MAP_WIDTH; x++) {
				itemMap[y][x] = "";
				damageMap[y][x] = 0;
			}
		}
		var map_file;
		var maps = settings.map_list;
		if (maps && maps.length) {
			var i =0;
			for (i = 0; i < maps.length; ++i)
				console.uselect(i, "Map to Play", maps[i].name);
			console.uselect(i, "", "Randomly Generated");
			var choice = console.uselect();
			if (choice < 0)
				return;
			if (choice < maps.length) {
				map_file = js.exec_dir + maps[choice].filename;
				state.map_name = maps[choice].name;
			}
		}
		if (!map_file || !loadMap(map_file)) {
			state.map_name = "Random Islands";
			genMap();
		}
		if (settings.rules_list && settings.rules_list.length) {
			var i =0;
			for (i = 0; i < settings.rules_list.length; ++i)
				console.uselect(i, "Rules to Play By", settings.rules_list[i].name);
			console.uselect(i, "", rules.name);
			var choice = console.uselect();
			if (choice < 0)
				return;
			if (choice < settings.rules_list.length) {
				var new_rules = loadRules(settings.rules_list[choice].filename);
				if (new_rules)
					rules = new_rules;
			}
		}
		spawnFish();
		pushMsg("Welcome to the " + state.map_name + ", Governor.");
		pushMsg("Playing by " + rules.name + " rules...");
		pushMsg("Build to claim your homeland!");
	}
	refreshScreen(true);
	var lastTick = system.timer;
	while (bbs.online && !js.terminated) {
		console.aborted = false;
		if (system.timer - lastTick >= state.tick_interval) {
			if (state.in_progress) {
				if (state.turn < rules.max_turns) {
					handleWeather();
					handleRainfall();
					if (tick % rules.turn_interval == 0) {
						state.turn++;
						handlePendingUpdates();
						handleEconomics();
						moveEntities();
						drawUI(/* verbosity */1);
						last_gold = state.gold;
						last_food = state.food;
						last_pop = state.pop;
						last_cap = state.popCap;
						last_reb = state.rebels;
					} else
						hideCursor();
					tick++;
				}
				else {
					alert("\x01h\x01rYour term as Governor is over, [\x01wQ\x01r]uit to see your score.");
					state.in_progress = false;
					drawUI(/* verbosity */1);
				}
			}
			lastTick = system.timer;
		}
		var k = console.inkey(K_EXTKEYS, 500 * state.tick_interval);
		if (!k)
			continue;
		console.aborted = false;
		var key = k.toUpperCase();
		var cursor = { x: state.cursor.x, y: state.cursor.y };

		switch (key) {
			case KEY_HOME:
				state.cursor.x = MAP_LEFT_COL;
				break;
			case KEY_END:
				state.cursor.x = MAP_RIGHT_COL;
				break;
			case '8':
			case KEY_UP:
				if (state.cursor.y > MAP_TOP_ROW)
					state.cursor.y--;
				break;
			case '2':
			case KEY_DOWN:
				if (state.cursor.y < MAP_BOTTOM_ROW)
					state.cursor.y++;
				break;
			case '4':
			case KEY_LEFT:
				if (state.cursor.x > MAP_TOP_ROW)
					state.cursor.x--;
				break;
			case '6':
			case KEY_RIGHT:
				if (state.cursor.x < MAP_RIGHT_COL)
					state.cursor.x++;
				break;
			case '7':
				if (state.cursor.y > MAP_TOP_ROW && state.cursor.x > MAP_LEFT_COL) {
					state.cursor.y--;
					state.cursor.x--;
				}
				break;
			case '9':
				if (state.cursor.y > MAP_TOP_ROW && state.cursor.x < MAP_RIGHT_COL) {
					state.cursor.y--;
					state.cursor.x++;
				}
				break;
			case '1':
				if (state.cursor.y < MAP_BOTTOM_ROW && state.cursor.x > MAP_LEFT_COL) {
					state.cursor.y++;
					state.cursor.x--;
				}
				break;
			case '3':
				if (state.cursor.y < MAP_BOTTOM_ROW && state.cursor.x < MAP_RIGHT_COL) {
					state.cursor.y++;
					state.cursor.x++;
				}
				break;
			case KEY_PAGEUP:
				state.cursor.y = MAP_TOP_ROW;
				break;
			case KEY_PAGEDN:
				state.cursor.y = MAP_BOTTOM_ROW;
				break;
			case CTRL_R:
				refreshScreen(true);
				break;
			case '0':
			case KEY_INSERT:
			case '\r':
				if (!state.started || state.in_progress)
					showBuildMenu();
				break;
			case ' ':
				if (state.in_progress) {
					if (state.lastbuilt) {
						buildItem(state.lastbuilt)
						drawUI();
						if(state.cursor.x < MAP_RIGHT_COL)
							++state.cursor.x;
					} else
						showBuildMenu();
				}
				break;
			case 'M':
				if (state.in_progress && state.martialLaw === 0) {
					state.martialLaw = rules.martial_law_length;
					announce("MARTIAL LAW!");
				}
				break;
			case 'L':
				console.clear(LIGHTGRAY);
				for (var i = state.log.length - 1; i >= 0  && !console.aborted; --i) {
					var entry = state.log[i];
					console.attributes = LIGHTGRAY;
					console.print(format("R-%4.1f: %s\r\n", entry.turn / 10, entry.msg));
				}
				refreshScreen(true);
				break;
			case '?':
			case 'H':
				if (file_exists(HELP_FILE)) {
					console.clear();
					console.printfile(HELP_FILE, P_SEEK | P_PCBOARD | P_MARKUP | P_HIDEMARKS);
					console.attributes = LIGHTGRAY;
					console.pause();
				}
				refreshScreen(true);
				break;
			case 'Q':
				if (handleQuit())
					return;
				drawUI(2);
				continue;
			case '.':
			case KEY_DEL:
				if (state.in_progress && itemMap[state.cursor.y][state.cursor.x]) {
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
				state.tick_interval /= 2;
				break;
			case '-':
				state.tick_interval *= 2;
				break;
			default:
				if ((!state.started || state.in_progress) && item_list[key]) {
					buildItem(key);
					drawUI();
					break;
				}
				if (user.is_sysop)
					if (handleOperatorCommand(key))
						break;
				if (console.handle_ctrlkey(key)) {
					console.pause();
					refreshScreen(true);
				}
				break;
		}
		if (compareXY(state.cursor, cursor) != 0) {
			drawCell(cursor.x, cursor.y);
			drawCell(state.cursor.x, state.cursor.y);
			drawUI(itemMap[state.cursor.y][state.cursor.x]
				|| itemMap[cursor.y][cursor.x]
				|| (state.pirate.active
					&& (cellIsSame(state.cursor, state.pirate) || cellIsSame(cursor, state.pirate)))
				|| (state.merchant.active
					&& (cellIsSame(state.cursor, state.merchant) || cellIsSame(cursor, state.merchant))));
		}
	}
}

try {
	main();
} catch (e) {
	console.newline();
	console.cleartoeol();
	js.global.alert("Utopia Error: " + e.message + " line " + e.lineNumber)
	js.global.log(LOG_WARNING, e.message + " line " + e.lineNumber);
}
if (state.in_progress)
	saveGame();
else
	removeSavedGame();
