// Minesweeper, the game

// See readme.txt for instructions on installation, configuration, and use

"use strict";

const title = "Synchronet Minesweeper";
const ini_section = "minesweeper";
const REVISION = "3.10";
const author = "Digital Man";
const header_height = 4;
const winners_list = js.exec_dir + "winners.jsonl";
const netwins_list = js.exec_dir + "netwins.jsonl";
const losers_list = js.exec_dir + "losers.jsonl";
const help_file = js.exec_dir + "minesweeper.hlp";
const welcome_image = js.exec_dir + "welcome.bin";
const mine_image = js.exec_dir + "mine.bin";
const winner_image = js.exec_dir + "winner.bin";
const boom_image = js.exec_dir + "boom?.bin";
const loser_image = js.exec_dir + "loser.bin";
const max_difficulty = 5;
const min_mine_density = 0.10;
const mine_density_multiplier = 0.025;
const char_flag = '\x01r\x9f';
const char_badflag = '\x01r\x01h!';
const char_unsure = '\x01r?';
const attr_empty = '\x01b'; //\x01h';
const char_empty = attr_empty + '\xFA';
const char_covered = attr_empty +'\xFE';
const char_mine = '\x01r\x01h\xEB';
const char_detonated_mine = '\x01r\x01h\x01i\*';
const attr_count = "\x01c";
const winner_subject = "Winner";
const highscores_subject = "High Scores";
const tear_line = "\r\n--- " + js.exec_file + " " + REVISION + "\r\n";
const selectors = ["()", "[]", "<>", "{}", "--", "  "];
const gfile = "graphics.ppm";
const mfile = "selmask.pbm";
const image = {
	'selected': 208
};
image[attr_count + '1'] = 0;
image[attr_count + '2'] = 16;
image[attr_count + '3'] = 32;
image[attr_count + '4'] = 48;
image[attr_count + '5'] = 64;
image[attr_count + '6'] = 80;
image[attr_count + '7'] = 96;
image[attr_count + '8'] = 112;
image[char_empty] = 128;
image[char_covered] = 240;
image[char_mine] = 144;
image[char_flag] = 160;
image[char_unsure] = 176;
image[char_badflag] = 192;
image[char_detonated_mine] = 224;

const orig_mouse = console.mouse_mode;
const orig_misc = user.misc;
const orig_autoterm = console.autoterm;

require("sbbsdefs.js", "K_NONE");
require("mouse_getkey.js", "mouse_getkey");

if(BG_HIGH === undefined)
	BG_HIGH = 0x400;

var options=load({}, "modopts.js", ini_section);
if(!options)
	options = {};
if(!options.timelimit)
	options.timelimit = 60;	// minutes
if(!options.timewarn)
	options.timewarn = 5;
if(!options.winners)
	options.winners = 20;
if(!options.selector)
	options.selector = 0;
if(!options.highlight)
	options.highlight = true;
if(!options.boom_delay)
	options.boom_delay = 1000;
if(!options.image_delay)
	options.image_delay = 1500;
if(!options.splash_delay)
	options.splash_delay = 500;
if(!options.sub)
    options.sub = load({}, "syncdata.js").find();
if(js.global.bbs === undefined)
	json_lines = load({}, "json_lines.js");
else {
	var userprops = bbs.mods.userprops;
	if(!userprops)
		userprops = load(bbs.mods.userprops = {}, "userprops.js");
	var json_lines = bbs.mods.json_lines;
	if(!json_lines)
		json_lines = load(bbs.mods.json_lines = {}, "json_lines.js");
	var selector = userprops.get(ini_section, "selector", options.selector);
	var highlight = userprops.get(ini_section, "highlight", options.highlight);
	var difficulty = userprops.get(ini_section, "difficulty", options.difficulty);
	var ansiterm = bbs.mods.ansiterm_lib;
	if(!ansiterm)
		ansiterm = bbs.mods.ansiterm_lib = load({}, "ansiterm_lib.js");
	var cterm = bbs.mods.cterm_lib;
	if(!cterm)
		cterm = bbs.mods.cterm_lib = load({}, "cterm_lib.js");
}
var game = {};
var board = [];
var selected = {x:0, y:0};
var gamewon = false;
var gameover = false;
var new_best = false;
var win_rank = false;
var view_details = false;
var cell_width;	// either 3 or 2
var best = null;
var graph = false;

log(LOG_DEBUG, title + " options: " + JSON.stringify(options));

function mouse_enable(enable)
{
	if (graph) {
		ansiterm.send('mouse', enable ? 'set' : 'clear', ['any_events', 'extended_coord']);
	}
	const mouse_passthru = (CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_REL_PASSTHRU);
	if(enable)
		console.status |= mouse_passthru;
	else
		console.status &= ~mouse_passthru;
}

function show_image(filename, fx, delay)
{
	var dir = directory(filename);
	filename = dir[random(dir.length)];
	var Graphic = load({}, "graphic.js");
	var sauce_lib = load({}, "sauce_lib.js");
	var sauce = sauce_lib.read(filename);
	if(delay === undefined)
		delay = options.image_delay;
	if(sauce && sauce.datatype == sauce_lib.defs.datatype.bin) {
		try {
			var graphic = new Graphic(sauce.cols, sauce.rows);
			graphic.load(filename);
			if(fx && graphic.revision >= 1.82)
				graphic.drawfx('center', 'center');
			else
				graphic.draw('center', 'center');
			sleep(delay);
		} catch(e) { 
			log(LOG_DEBUG, e);
		}
	}
}

function countmines(x, y)
{
	var count = 0;

	for(var yi = y - 1; yi <= y + 1; yi++)
		for(var xi = x - 1; xi <= x + 1; xi++)
			if((yi != y || xi != x) && mined(xi, yi))
				count++;
	return count;
}

function place_mines()
{
	var mined = new Array(game.height * game.width);
	for(var i = 0; i < game.mines; i++)
		mined[i] = true;

	for(var i = 0; i < game.mines; i++) {
		var r;
		do {
			r = random(game.height * game.width);
		} while (r == (selected.y * game.width) + selected.x);
		var tmp = mined[i];
		mined[i] = mined[r];
		mined[r] = tmp;
	}

	for(var y = 0; y < game.height; y++) {
		for(var x = 0; x < game.width; x++) {
			if(mined[(y * game.width) + x])
				board[y][x].mine = true;
		}
	}
	for(var y = 0; y < game.height; y++) {
		for(var x = 0; x < game.width; x++) {
			board[y][x].count = countmines(x, y);
		}
	}
}

function isgamewon()
{
	var covered = 0;
	for(var y = 0; y < game.height; y++) {
		for(var x = 0; x < game.width; x++) {
			if(board[y][x].covered && !board[y][x].mine)
				covered++;
		}
	}
	if(covered === 0) { 
		game.end = Date.now() / 1000;
		if(options.sub) {
			var msgbase = new MsgBase(options.sub);
			var hdr = { 
				to: title,
				from: user.alias,
				subject: winner_subject
			};
			game.name = user.alias;
			game.md5 = md5_calc(JSON.stringify(game));
			game.name = undefined;
			var body = lfexpand(JSON.stringify(game, null, 1));
			body += tear_line;
			if(!msgbase.save_msg(hdr, body))
				alert("Error saving message to: " + options.sub);
			msgbase.close();
			game.md5 = undefined;
		}
		var level = calc_difficulty(game);
		if(!best || !best[level] || calc_time(game) < calc_time(best[level])) {
			new_best = true;
			if(!best)
				best = {};
			delete best[level];
			best[level] = game;
		}
		game.name = user.alias;
		var result = json_lines.add(winners_list, game);
		if(result !== true) {
			alert(result);
			console.pause();
		}
		gamewon = true;
		gameover = true;
		draw_board(false);
		show_image(winner_image, true, /* delay: */0);
		var start = Date.now();
		var winners = get_winners(Math.ceil(level));
		for(var i = 0; i < options.winners; i++) {
			if(winners[i].name == user.alias && winners[i].end == game.end) {
				win_rank = i + 1;
				break;
			}
		}
		var now = Date.now();
		if(now - start < options.image_delay)
			sleep(options.image_delay - (now - start));
		return true;
	}
	return false;
}

function lostgame(cause)
{
	gameover = true;
	game.end = Date.now() / 1000;
	game.name = user.alias;
	game.cause = cause;
	json_lines.add(losers_list, game);
	draw_board(true);
	show_image(boom_image, false, options.boom_delay);
	show_image(loser_image, true);
}

function calc_difficulty(game)
{
	const game_cells = game.height * game.width;
	const mine_density = game.mines / game_cells;
	const level = 1 + Math.ceil((mine_density - min_mine_density) / mine_density_multiplier);
	const target = target_height(level);
	const target_cells = target * target;
	const bias = (1 - (game_cells / target_cells)) * 1.5;
	return level - bias;
}

function calc_time(game)
{
	return game.end - game.start;
}

function compare_won_game(g1, g2)
{
	var diff = calc_difficulty(g2) - calc_difficulty(g1);
	if(diff)
		return diff;
	return calc_time(g1) - calc_time(g2);
}

function secondstr(t, frac)
{
	if(frac)
		return format("%2u:%06.3f", Math.floor(t/60), t%60);
	return format("%2u:%02u", Math.floor(t/60), Math.floor(t%60));
}

function list_contains(list, obj)
{
	var match = false;
	for(var i = 0; i < list.length && !match; i++) {
		match = true;
		for(var p in obj) {
			if(list[i][p] != obj[p]) {
				match = false;
				break;
			}
		}
	}
	return match;
}

function get_winners(level)
{
	var list = json_lines.get(winners_list);
	if(typeof list != 'object')
		list = [];

	if(options.sub) {
		var net_list = json_lines.get(netwins_list);
		if(typeof net_list != 'object')
			net_list = [];
		var msgbase = new MsgBase(options.sub);
		if(msgbase.get_index !== undefined && msgbase.open()) {
			var file = new File(msgbase.cfg.data_dir + msgbase.cfg.code + ".ini");
			var ptr = 0;
			if(file.open("r")) {
				ptr = file.iniGetValue(ini_section, "import_ptr", 0);
				file.close();
			}
			if(msgbase.last_msg > ptr) {
				var to_crc = crc16_calc(title.toLowerCase());
				var winner_crc = crc16_calc(winner_subject.toLowerCase());
				var highscores_crc = crc16_calc(highscores_subject.toLowerCase());
				var index = msgbase.get_index();
				for(var i = 0; index && i < index.length; i++) {
					var idx = index[i];
					if(idx.number <= ptr)
						continue;
					if((idx.attr&MSG_DELETE) || idx.to != to_crc)
						continue;
					if(idx.subject != winner_crc && idx.subject != highscores_crc)
						continue;
					var hdr = msgbase.get_msg_header(true, idx.offset);
					if(!hdr)
						continue;
					if(!hdr.from_net_type || hdr.to != title)
						continue;
					if(hdr.subject != winner_subject && hdr.subject != highscores_subject)
						continue;
					var body = msgbase.get_msg_body(hdr, false, false, false);
					if(!body)
						continue;
					body = body.split("\n===", 1)[0];
					body = body.split("\n---", 1)[0];
					var obj;
					try {
						obj = JSON.parse(strip_ctrl(body));
					} catch(e) {
						log(LOG_INFO, title + " " + e + ": "  + options.sub + " msg " + hdr.number);
						continue;
					}
					if(!obj.md5)	// Ignore old test messages
						continue;
					if(idx.subject == highscores_crc && !obj.game)
						continue;
					obj.name = hdr.from;
					var md5 = obj.md5;
					obj.md5 = undefined;
					var calced = md5_calc(JSON.stringify(idx.subject == winner_crc ? obj : obj.game));
					if(calced == md5) {
						if(idx.subject == winner_crc) {
							obj.net_addr = hdr.from_net_addr;	// Not included in MD5 sum
							if(!list_contains(net_list, obj)) {
								net_list.push(obj);
								json_lines.add(netwins_list, obj);
							}
						} else {
							for(var j = 0; j < obj.game.length; j++) {
								var game = obj.game[j];
								game.net_addr = hdr.from_net_addr;
								if(!list_contains(net_list, game)) {
									net_list.push(game);
									json_lines.add(netwins_list, game);
								}
							}
						}
					} else {
						log(LOG_INFO, title +
							" MD5 not " + calced +
							" in: "  + options.sub +
							" msg " + hdr.number);
					}
				}
				if(file.open(file.exists ? "r+" : "w+")) {
					file.iniSetValue(ini_section, "import_ptr", msgbase.last_msg);
					file.close();
				}
			}
			msgbase.close();
		}
		list = list.concat(net_list);
	}

	if(level)
		list = list.filter(function (obj) { var difficulty = calc_difficulty(obj);
			return (difficulty <= level && difficulty > level - 1); });

	list.sort(compare_won_game);

	return list;
}

function show_winners(level)
{
	console.clear();
	console.aborted = false;
	console.attributes = YELLOW|BG_BLUE|BG_HIGH;
	var str = " " + title + " Top " + options.winners;
	if(level)
		str += " Level " + level;
	str += " Winners ";
	console_center(str);
	console.attributes = LIGHTGRAY;

	var list = get_winners(level);
	if(!list.length) {
		alert("No " + (level ? ("level " + level + " ") : "") + "winners yet!");
		return;
	}
	console.attributes = WHITE;
	console.print(format("    %-25s%-15s Lvl     Time    WxHxMines   Date\r\n", "User", ""));

	var count = 0;
	var displayed = 0;
	var last_level = 0;
	for(var i = 0; i < list.length && displayed < options.winners && !console.aborted; i++) {
		var game = list[i];
		var difficulty = calc_difficulty(game);
		if(Math.ceil(difficulty) != Math.ceil(last_level)) {
			last_level = difficulty;
			count = 0;
		} else {
			if(!level && difficulty > 1.0 && count >= options.winners / max_difficulty)
				continue;
		}
		if(displayed&1)
			console.attributes = LIGHTCYAN;
		else
			console.attributes = BG_CYAN;
		console.print(format("%3u %-25.25s%-15.15s %1.2f %s %3ux%2ux%-3u %s\x01>\r\n"
			,count + 1
			,game.name
			,game.net_addr ? ('@'+game.net_addr) : ''
			,difficulty
			,secondstr(calc_time(game), true)
			,game.width
			,game.height
			,game.mines
			,system.datestr(game.end)
			));
		count++;
		displayed++;
	}
	console.attributes = LIGHTGRAY;
}

function compare_game(g1, g2)
{
	return g2.start - g1.start;
}

function show_log()
{
	console.clear();
	console.attributes = YELLOW|BG_BLUE|BG_HIGH;
	console_center(" " + title + " Log ");
	console.attributes = LIGHTGRAY;

	var winners = json_lines.get(winners_list);
	if(typeof winners != 'object')
		winners = [];

	var losers = json_lines.get(losers_list);
	if(typeof losers != 'object')
		losers = [];

	var list = losers.concat(winners);

	if(!list.length) {
		alert("No winners or losers yet!");
		return;
	}
	console.attributes = WHITE;
	console.print(format("Date      %-25s Lvl     Time    WxHxMines Rev  Result\r\n", "User", ""));

	list.sort(compare_game);

	for(var i = 0; i < list.length && !console.aborted; i++) {
		var game = list[i];
		if(i&1)
			console.attributes = LIGHTCYAN;
		else
			console.attributes = BG_CYAN;
		console.print(format("%s  %-25s %1.2f %s %3ux%2ux%-3u %3s  %s\x01>\x01n\r\n"
			,system.datestr(game.end)
			,game.name
			,calc_difficulty(game)
			,secondstr(calc_time(game), true)
			,game.width
			,game.height
			,game.mines
			,game.rev ? game.rev : ''
			,game.cause ? ("Lost: " + format("%.4s", game.cause)) : "Won"
			));
	}
	console.attributes = LIGHTGRAY;
}

function show_best()
{
	console.clear(LIGHTGRAY);
	console.attributes = YELLOW|BG_BLUE|BG_HIGH;
	console_center(" Your " + title + " Personal Best Wins ");
	console.attributes = LIGHTGRAY;

	var wins = [];
	for(var i in best)
		wins.push(best[i]);
	wins.reverse();	// Display newest first

	console.attributes = WHITE;
	console.print("Date       Lvl     Time    WxHxMines  Rev\r\n");

	for(var i in wins) {
		var game = wins[i];
		if(i&1)
			console.attributes = LIGHTCYAN;
		else
			console.attributes = BG_CYAN;
		console.print(format("%s  %1.2f  %s  %3ux%2ux%-3u %s\x01>\x01n\r\n"
			,system.datestr(game.end)
			,calc_difficulty(game)
			,secondstr(calc_time(game), true)
			,game.width, game.height, game.mines
			,game.rev ? game.rev : ''));
	}
}

function cell_val(x, y)
{
	if(gameover && board[y][x].mine && (!view_details || board[y][x].detonated)) {
		if(board[y][x].detonated)
			return char_detonated_mine;
		return char_mine;
	}
	if(board[y][x].unsure)
		return char_unsure;
	if(board[y][x].flagged) {
		if(gameover && !board[y][x].mine)
			return char_badflag;
		return char_flag;
	}
	if((view_details || !gameover) && board[y][x].covered)
		return char_covered;
	if(board[y][x].count)
		return attr_count + board[y][x].count;
	return char_empty;
}

function highlighted(x, y)
{
	if(selected.x == x && selected.y == y)
		return true;
	if(!highlight)
		return false;
	return (selected.x == x - 1 || selected.x == x || selected.x == x + 1)
		&& (selected.y == y -1 || selected.y == y || selected.y == y + 1);
}

function board_width()
{
	var width;

	if (graph) {
		width = game.width * cell_width;
		if (width < (title + " " + REVISION).length + 2)
			width = (title + " " + REVISION).length + 2;
	}
	else
		width = (game.width * cell_width) + !(cell_width&1);
	return width;
}

function get_margin()
{
	var width = board_width();
	if ((cell_width & 1) || graph)
		width--;
	var margin = Math.floor((console.screen_columns - width) / 2);

	return margin;
}

function get_board_margin()
{
	var margin = get_margin();
	if (graph) {
		var width = board_width();
		// Correct for level 1...
		if (width != game.width * cell_width) {
			margin += Math.floor((width - (game.width * cell_width)) / 2);
		}
	}

	return margin;
}

function get_top()
{
	return Math.floor(Math.max(0, (console.screen_rows - (header_height + game.height)) - 1) / 2);
}

function draw_cell(x, y)
{
	console.attributes = LIGHTGRAY;
	var val = cell_val(x, y);
	var left = " ";
	var right = " ";
	if(game.start && !gameover
		&& !board[selected.y][selected.x].covered
		&& board[selected.y][selected.x].count
		&& highlighted(x, y))
		console.attributes |= HIGH;
	if(selected.x == x && selected.y == y) {
		left = "\x01n\x01h" + selectors[selector%selectors.length][0];
		right = "\x01n\x01h" + selectors[selector%selectors.length][1];
	}
	if (graph) {
		if (image[val] !== undefined) {
			const margin = get_board_margin();
			const width = board_width();
			var xpos = (x * cell_width + margin) * 8;
			var ypos = (header_height + y + top) * 16;
			console.write('\x1b_SyncTERM:P;Paste;SX='+image[val]+';SY=0;SW=16;SH=16;DX='+xpos+';DY='+ypos+';B=0\x1b\\');
			if (selected.x == x && selected.y == y) {
				console.write('\x1b_SyncTERM:P;Paste;SX='+image['selected']+';SY=0;SW=16;SH=16;DX='+xpos+';DY='+ypos+';MBUF;B=0\x1b\\');
			}
			else if(console.attributes & HIGH) {
				console.write('\x1b_SyncTERM:P;Paste;SX='+image['selected']+';SY=0;SW=16;SH=16;DX='+xpos+';DY='+ypos+';MBUF;MX=16;B=0\x1b\\');
			}
		}
		else {
			log(LOG_DEBUG, "Undefined cell: "+val.toSource());
			console.print(left + val + right);
		}
	}
	else {
		console.print(left + val + right);
	}
}

// Return total number of surrounding flags
function countflagged(x, y)
{
	var count = 0;

	for(var yi = y - 1; yi <= y + 1; yi++)
		for(var xi = x - 1; xi <= x + 1; xi++)
			if((yi != y || xi != x) && flagged(xi, yi))
				count++;

	return count;
}

// Return total number of surrounding unflagged-covered cells
function countunflagged(x, y)
{
	var count = 0;

	for(var yi = y - 1; yi <= y + 1; yi++)
		for(var xi = x - 1; xi <= x + 1; xi++)
			if((yi != y || xi != x) && unflagged(xi, yi))
				count++;

	return count;
}

function totalflags()
{
	if(!game.start)
		return 0;
	var flags = 0;
	for(var y = 0; y < game.height; y++) {
		for(var x = 0; x < game.width; x++) {
			if(board[y][x].flagged)
				flags++;
		}
	}
	return flags;
}

function show_title()
{
	console.attributes = YELLOW|BG_BLUE|BG_HIGH;
	console_center(title + " " + REVISION);
}

function draw_border()
{
	const margin = get_margin();

	console.creturn();
	console.attributes = LIGHTGRAY;
	console.cleartoeol();
	console.attributes = BG_BLUE|BG_HIGH;
	console.right(margin - 1);
	console.print(' ');
	if(game.width * cell_width >= console.screen_columns - 3) {
		console.attributes = BG_BLUE;
		console.cleartoeol();
	} else {
		console.right(board_width());
		console.print(' ');
	}
	console.creturn();
	console.attributes = LIGHTGRAY;
}

// A non-destructive console.center() replacement
function console_center(text)
{
	console.right((console.screen_columns - console.strlen(text)) / 2);
	console.print(text);
	console.crlf();
}

// global state variable used by draw_board()
var cmds_shown;
var top;

function draw_board(full)
{
	var width;
	const margin = get_margin();
	const bmargin = get_board_margin();
	top = get_top();
	console.line_counter = 0;
	console.home();
	if(full) {
		console.down(top);
		console.right(margin - 1);
		console.attributes = BG_BLUE|BG_HIGH;
		console.print('\xDF');
		width = board_width();
		for(var x = 0; x < width; x++)
			console.print(' ');
		console.print('\xDF');
		console.creturn();
		show_title();
		draw_border();
	} else
		console.down(top + 1);
	if(gamewon) {
		console.attributes = YELLOW|BLINK;
		var blurb = "Winner! Cleared in";
		if(win_rank)
			blurb = "Rank " + win_rank + " Winner in";
		else if(new_best)
			blurb = "Personal Best Time";
		console_center(blurb + " " + secondstr(calc_time(game), true).trim());
	} else if(gameover && !view_details) {
		console.attributes = RED|HIGH|BLINK;
		console_center((calc_time(game) < options.timelimit * 60
			? "" : "Time-out: ") + "Game Over");
	} else {
		var elapsed = 0;
		if(game.start) {
			if(gameover)
				elapsed = game.end - game.start;
			else
				elapsed = (Date.now() / 1000) - game.start;
		}
		var timeleft = Math.max(0, (options.timelimit * 60) - elapsed);
		console.attributes = LIGHTCYAN;
		console_center(format("%2d Mines  Lvl %1.2f  %s%s "
			, game.mines - totalflags(), calc_difficulty(game)
			, game.start && !gameover && (timeleft / 60) <= options.timewarn ? "\x01r\x01h\x01i" : ""
			, secondstr(timeleft)
			));
	}
	var cmds = "";
	if(gameover) {
		if(!gamewon)
			cmds += "\x01n\x01h\x01~D\x01nisplay  ";
	} else {
		if(!board[selected.y][selected.x].covered) {
			if(can_chord(selected.x, selected.y))
				cmds += "\x01h\x01k\x01~Dig  \x01n\x01h\x01~C\x01nhord  ";
			else
				cmds += "\x01h\x01k\x01~Dig  Flag  ";
		}
		else
			cmds += "\x01h\x01~D\x01nig  \x01h\x01~F\x01nlag  ";
	}
	cmds += "\x01n\x01h\x01~N\x01new  \x01h\x01~Q\x01nuit";
	if(full || cmds !== cmds_shown) {
		console.clear_hotspots();
		draw_border();
		console.attributes = LIGHTGRAY;
		console_center(cmds);
		cmds_shown = cmds;
		draw_border();
		cmds = "\x01h\x01~W\x01ninners  \x01h\x01~L\x01nog  ";
		if(best)
			cmds += "\x01h\x01~B\x01nest  ";
		cmds += "\x01h\x01~H\x01nelp"
		console_center(cmds);
	} else if(!console.term_supports(USER_ANSI)) {
		console.creturn();
		console.down(2);
	}
	var redraw_selection = false;
	for(var y = 0; y < game.height; y++) {
		if(full)
			draw_border();
		for(var x = 0; x < game.width; x++) {
			if(full || board[y][x].changed !== false) {
				if (graph) {
				}
				else if(console.term_supports(USER_ANSI))
					console.gotoxy((x * cell_width) + margin + 1, header_height + y + top + 1);
				else {
					console.creturn();
					console.right((x * cell_width) +  margin);
				}
				draw_cell(x, y);
				if(cell_width < 3)
					redraw_selection = true;
			}
			board[y][x].changed = false;
		}
		if(y + 1 < game.height && (full || !console.term_supports(USER_ANSI)))
			console.down();
		console.line_counter = 0;
	}
	var height = game.height;
	if(full) {
		if(game.height + header_height < console.screen_rows) {
			height++;
			console.down();
			console.creturn();
			console.right(margin - 1);
			console.attributes = BG_BLUE|BG_HIGH;
			console.print('\xDC');
			width = board_width();
			for(var x = 0; x < width; x++)
				console.print(' ');
			console.print('\xDC');
		}
		console.attributes = LIGHTGRAY;
	}
	if(redraw_selection) { // We need to draw/redraw the selected cell last in this case
		if (graph) {
		}
		else if(console.term_supports(USER_ANSI))
			console.gotoxy(margin + (selected.x * cell_width) + 1, header_height + selected.y + top + 1);
		else {
			console.up(height - (selected.y + 1));
			console.creturn();
			console.right(margin + (selected.x * cell_width));
		}
		draw_cell(selected.x, selected.y);
		console.left(2);
	}
	console.gotoxy(bmargin + (selected.x * cell_width) + 2, header_height + selected.y + top + 1);
}

function mined(x, y)
{
	return board[y] && board[y][x] && board[y][x].mine;
}

function start_game()
{
	place_mines();
	game.start = Date.now() / 1000;
}

function uncover_cell(x, y)
{
	if(!board[y] || !board[y][x])
		return false;
	if(board[y][x].flagged)
		return false;

	board[y][x].covered = false;
	board[y][x].unsure = false;
	board[y][x].changed = true;

	if(!mined(x, y))
		return false;
	board[y][x].detonated = true;
	return true;
}

function flagged(x, y)
{
	return board[y] && board[y][x] && board[y][x].flagged;
}

function unflagged(x, y)
{
	return board[y] && board[y][x] && board[y][x].covered && !board[y][x].flagged;
}

// Returns true if mined (game over)
function uncover(x, y)
{
	if(!game.start)
		start_game();

	if(!board[y] || !board[y][x] || board[y][x].flagged || !board[y][x].covered)
		return;
	if(uncover_cell(x, y))
		return true;
	if(board[y][x].count)
		return false;
	for(var yi = y - 1; yi <= y + 1; yi++)
		for(var xi = x - 1; xi <= x + 1; xi++)
			if((yi != y || xi != x) && !mined(xi, yi))
				uncover(xi, yi);
	return false;
}

function can_chord(x, y)
{
	return !board[y][x].covered
		&& board[y][x].count
		&& board[y][x].count == countflagged(x, y)
		&& countunflagged(x, y);
}

// Return true if mine denotated
function chord(x, y)
{
	for(var yi = y - 1; yi <= y + 1; yi++)
		for(var xi = x - 1; xi <= x + 1; xi++)
			if((yi != y || xi != x) && uncover(xi, yi))
				return true;
	return false;
}

function get_difficulty(all)
{
	console.creturn();
	console.cleartoeol();
	draw_border();
	console.attributes = WHITE;
	console.clear_hotspots();
	mouse_enable(false);
	var lvls = "";
	for(var i = 1; i <= max_difficulty; i++)
		lvls += "\x01~" + i;
	if(all) {
		console.right((console.screen_columns - 20) / 2);
		console.print(format("Level (%s) [\x01~All]: ", lvls));
		var key = console.getkeys("QA", max_difficulty);
		if(key == 'A')
			return 0;
		if(key == 'Q')
			return -1;
		return key;
	}
	console.right((console.screen_columns - 24) / 2);
	console.print(format("Difficulty Level (%s): ", lvls));
	var result = console.getnum(max_difficulty);
	mouse_enable(true);
	return result;
}

function target_height(difficulty)
{
	return 5 + (difficulty * 5);
}

function select_middle()
{
	selected.x = Math.floor(game.width / 2) - !(game.width&1);
	selected.y = Math.floor(game.height / 2) - !(game.height&1);
}

function init_game(difficulty)
{
	console.line_counter = 0;
	console.clear(LIGHTGRAY);

	gamewon = false;
	gameover = false;
	new_best = false;
	win_rank = false;
	view_details = false;
	game = { rev: REVISION };
	game.height = target_height(difficulty);
	game.width = game.height;
	game.height = Math.min(game.height, console.screen_rows - header_height);
	game.width += game.width - game.height;
	if(graph || (game.width > 10 && (game.width * 3) + 2 > (console.screen_columns - 20)))
		cell_width = 2;
	else
		cell_width = 3;
	game.width = Math.min(game.width, Math.floor((console.screen_columns - 5) / cell_width));
	game.mines = Math.floor((game.height * game.width) 
		* (min_mine_density + ((difficulty - 1) * mine_density_multiplier)));
	log(LOG_INFO, title + " new level " + difficulty + " board WxHxM: " 
		+ format("%u x %u x %u", game.width, game.height, game.mines));
	game.start = 0;
	// init board:
	board = [];
	for(var y = 0; y < game.height; y++) {
		board[y] = new Array(game.width);
		for(var x = 0; x < game.width; x++) {
			board[y][x] = { covered: true };
		}
	}
	select_middle();
	return difficulty;
}

function change(x, y)
{
	if(y) {
		if(x)
			board[y - 1][x - 1].changed = true;
		board[y - 1][x].changed = true;
		if(board[y - 1][x + 1])
			board[y - 1][x + 1].changed = true;
	}
	if(x)
		board[y][x - 1].changed = true;
	board[y][x].changed = true;
	if(board[y][x + 1])
		board[y][x + 1].changed = true;
	if(board[y + 1]) {
		if(x)
			board[y + 1][x - 1].changed = true;
		board[y + 1][x].changed = true;
		if(board[y + 1][x + 1])
			board[y + 1][x + 1].changed = true;
	}
}

function screen_to_board(mouse)
{
	const margin = get_board_margin();
	top = get_top();

	var x = (mouse.x - margin + (cell_width - 2)) / cell_width;
	if (graph) {
		x = Math.floor(x + 0.5);
	}
	if (Math.floor(x) !== x)
		return false;
	mouse.x = x;
	mouse.y = (mouse.y - top - header_height);
	if (mouse.x < 1 || mouse.y < 1 ||
	    mouse.x > game.width || mouse.y > game.height)
		return false;
	return true;
}

function read_apc()
{
	var ret = '';
	var ch;
	var state = 0;

	for(;;) {
		ch = console.getbyte(1000);
		if (ch === null)
			return undefined;
		switch(state) {
			case 0:
				if (ch == 0x1b) {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 95) {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 0x1b) {
					state++;
					break;
				}
				ret += ascii(ch);
				break;
			case 3:
				if (ch == 92) {
					return ret;
				}
				return undefined;
		}
	}
	return undefined;
}

function detect_graphics()
{
	var tmpckpt;
	var f;
	var md5;
	var lst;
	var m;
	var b;
	var sent;

	// Detect PPM graphics and load the cache
	graph = false;
	tmpckpt = console.ctrlkey_passthru;
	if (console.cterm_version >= cterm.cterm_version_supports_copy_buffers) {
		graph = cterm.query_ctda(cterm.cterm_device_attributes.pixelops_supported);
	}

	if (graph) {
		// Load up cache...
		f = new File(js.exec_dir+gfile);
		if (f.open('rb')) {
			sent = false;
			md5 = f.md5_hex;
			console.write('\x1b_SyncTERM:C;L;minesweeper/'+gfile+'\x1b\\');
			lst = read_apc();
			m = lst.match(/\ngraphics.ppm\t([0-9a-f]+)\n/);
			if (m == null || m[1] !== md5) {
				// Store in cache...
				console.write('\x1b_SyncTERM:C;S;minesweeper/'+gfile+';');
				f.base64 = true;
				console.write(f.read());
				console.write('\x1b\\');
				sent = true;
			}
			if (sent) {
				console.write('\x1b_SyncTERM:C;L;minesweeper/'+gfile+'\x1b\\');
				lst = read_apc();
				m = lst.match(/\ngraphics.ppm\t([0-9a-f]+)\n/);
				if (m == null || m[1] !== md5) {
					graph = false;
				}
			}
			f.close();
			if (graph) {
				sent = false;
				f = new File(js.exec_dir+mfile);
				if (f.open('rb')) {
					md5 = f.md5_hex;
					console.write('\x1b_SyncTERM:C;L;minesweeper/'+mfile+'\x1b\\');
					lst = read_apc();
					m = lst.match(/\nselmask.pbm\t([0-9a-f]+)\n/);
					if (m == null || m[1] !== md5) {
						// Store in cache...
						console.write('\x1b_SyncTERM:C;S;minesweeper/'+mfile+';');
						f.base64 = true;
						console.write(f.read());
						console.write('\x1b\\');
						sent = true;
					}
					console.write('\x1b_SyncTERM:C;L;minesweeper/'+mfile+'\x1b\\');
					lst = read_apc();
					m = lst.match(/\nselmask.pbm\t([0-9a-f]+)\n/);
					if (m == null || m[1] !== md5) {
						graph = false;
					}
					f.close();
				}
				else {
					graph = false;
				}
			}
		}
		else {
			graph = false;
		}
	}

	if (graph) {
		console.write('\x1b_SyncTERM:C;LoadPPM;B=0;minesweeper/'+gfile+'\x1b\\');
		console.write('\x1b_SyncTERM:C;LoadPBM;minesweeper/'+mfile+'\x1b\\');
	}

	console.ctrlkey_passthru = tmpckpt;
	if (graph) {
		console.mouse_mode = false;
		ansiterm.send('mouse', 'clear', 'all');
		ansiterm.send('mouse', 'set', ['any_events', 'extended_coord']);
		js.on_exit("console.write('\x1b[?1003;1006l;'); console.mouse_mode = orig_mouse; user.misc = orig_misc; console.autoterm = orig_autoterm;");
	}
}

function play()
{
	if (graph)
		detect_graphics();
	console.clear();
	var start = Date.now();
	show_image(welcome_image, /* fx: */false, /* delay: */0);

	// Find "personal best" wins
	var winners = json_lines.get(winners_list);
	for(var i in winners) {
		var win = winners[i];
		if(win.name !== user.alias)
			continue;
		var level = calc_difficulty(win);
		if(best && best[level] && calc_time(best[level]) < calc_time(win))
			continue;
		if(!best)
			best = {};
		delete best[level];
		best[level] = win;
	}
	var now = Date.now();
	if(now - start < options.splash_delay)
		sleep(options.splash_delay - (now - start));

	show_image(mine_image, true);
	sleep(options.splash_delay);
	init_game(difficulty);
	draw_board(true);
	var full_redraw = false;
	mouse_enable(true);
	while(bbs.online) {
		if(!gameover && game.start
			&& Date.now() - (game.start * 1000) >= options.timelimit * 60 * 1000) {
			lostgame("timeout");
			draw_board(true);
		}
		var mk = mouse_getkey(K_NONE, 1000, true);
		var key = mk.key;
		if (mk.mouse !== null) {
			var ignore = true;
			if (graph && mk.mouse.button == 64 && mk.mouse.motion == 32 && mk.mouse.press == true && mk.mouse.release == false)
				ignore = false;
			if (((!mk.mouse.release) && ignore) || (mk.mouse.motion && ignore) || !screen_to_board(mk.mouse)) {
				key = null;
			}
			else {
				switch(mk.mouse.button) {
					case 0:
						key = 'D';
						break;
					case 1:
						key = 'C';
						break;
					case 2:
						key = 'F';
						break;
					case 64:
						if (graph) {
							if (selected.x != mk.mouse.x - 1 || selected.y != mk.mouse.y - 1) {
								change(selected.x, selected.y);
								selected.x = mk.mouse.x - 1;
								selected.y = mk.mouse.y - 1;
								change(selected.x, selected.y);
							}
							else {
								// Skip time update...
								continue;
							}
						}
						key = null;
						break;
					default:
						key = null;
				}
			}
		}
		if(key === '' || key === null) {
			if(game.start && !gameover)
				draw_board(false);	// update the time display
			continue;
		}
		change(selected.x, selected.y);
		if (mk.mouse !== null) {
			selected.x = mk.mouse.x - 1;
			selected.y = mk.mouse.y - 1;
		}
		switch(key.toUpperCase()) {
			case KEY_HOME:
				if(!gameover)
					selected.x = 0;
				break;
			case KEY_END:
				if(!gameover)
					selected.x = game.width - 1;
				break;
			case KEY_PAGEUP:
				if(!gameover)
					selected.y = 0;
				break;
			case KEY_PAGEDN:
				if(!gameover)
					selected.y = game.height -1;
				break;
			case '7':
				if(!gameover && selected.y && selected.x) {
					selected.y--;
					selected.x--;
				}
				break;
			case '8':
			case KEY_UP:
				if(!gameover && selected.y)
					selected.y--;
				break;
			case '9':
				if(!gameover && selected.y && selected.x < game.width - 1) {
					selected.y--;
					selected.x++;
				}
				break;
			case '2':
			case KEY_DOWN:
				if(!gameover && selected.y < game.height - 1)
					selected.y++;
				break;
			case '1':
				if(!gameover && selected.y < game.height -1 && selected.x) {
					selected.y++;
					selected.x--;
				}
				break;
			case '3':
				if(!gameover && selected.x < game.width - 1&& selected.y < game.height - 1) {
					selected.x++;
					selected.y++;
				}
				break;
			case '4':
			case KEY_LEFT:
				if(!gameover && selected.x)
					selected.x--;
				break;
			case '5':
				if(!gameover)
					select_middle();
				break;
			case '6':
			case KEY_RIGHT:
				if(!gameover && selected.x < game.width - 1)
					selected.x++;
				break;
			case 'D':	// Dig (or Details)
			case ' ':
				if(gameover) {
					if(!gamewon) {
						view_details = !view_details;
						full_redraw = true;
					}
				} else {
					if(board[selected.y][selected.x].covered
						&& !board[selected.y][selected.x].flagged) {
						if(uncover(selected.x, selected.y))
							lostgame("mine");
						else
							isgamewon();
						full_redraw = gameover;
					}
				}
				break;
			case 'C':	// Chord
				if(!gameover && can_chord(selected.x, selected.y)) {
					if(chord(selected.x, selected.y))
						lostgame("mine");
					else 
						isgamewon();
					full_redraw = gameover;
				}
				break;
			case 'F':	// Flag
				if(!gameover && board[selected.y][selected.x].covered) {
					if(board[selected.y][selected.x].flagged) {
						board[selected.y][selected.x].unsure = true;
						board[selected.y][selected.x].flagged = false;
					} else if(board[selected.y][selected.x].unsure) {
						board[selected.y][selected.x].unsure = false;
					} else {
						board[selected.y][selected.x].flagged = true;
					}
					if(!game.start)
						start_game();
					full_redraw = gameover;
				}
				break;
			case 'N':
			{
				console.home();
				console.down(top + 1);
				full_redraw = true;
				if(game.start && !gameover) {
					console.cleartoeol();
					draw_border();
					console.attributes = LIGHTRED;
					console.right((console.screen_columns - 15) / 2);
					mouse_enable(false);
					console.clear_hotspots();
					console.print("New Game (\x01~Y/\x01~N) ?");
					var key = console.getkey(K_UPPER);
					mouse_enable(true);
					if(key != 'Y')
						break;
				}
				var new_difficulty = get_difficulty();
				if(new_difficulty > 0)
					difficulty = init_game(new_difficulty);
				break;
			}
			case 'W':
				full_redraw = true;
				mouse_enable(false);
				console.home();
				console.down(top + 1);
				var level = get_difficulty(true);
				if(level >= 0) {
					console.line_counter = 0;
					show_winners(level);
					console.pause();
					console.clear();
					console.aborted = false;
				}
				mouse_enable(true);
				break
			case 'T':
				mouse_enable(false);
				show_winners();
				console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				mouse_enable(true);
				break;
			case 'L':
				mouse_enable(false);
				console.line_counter = 0;
				show_log();
				console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				mouse_enable(true);
				break
			case 'B':
				if(!best)
					break;
				console.line_counter = 0;
				mouse_enable(false);
				show_best();
				console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				mouse_enable(true);
				break;
			case '?':
			case 'H':
				mouse_enable(false);
				console.line_counter = 0;
				console.clear();
				console.printfile(help_file, P_SEEK);
				if(console.line_counter)
					console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				mouse_enable(true);
				break;
			case '\t':
				highlight = !highlight;
				break;
			case CTRL_G:	// graphics toggle/re-detect
				graph = !graph;
				if(graph)
					graph = cterm.query_ctda(cterm.cterm_device_attributes.pixelops_supported);
				// Fall-through
			case CTRL_R:
				console.clear();
				full_redraw = true;
				break;
			case CTRL_S:
				selector++;
				break;
			case 'Q':
				if(game.start && !gameover) {
					full_redraw = true;
					console.home();
					console.down(top + 1);
					console.cleartoeol();
					draw_border();
					console.attributes = LIGHTRED;
					console.right((console.screen_columns - 16) / 2);
					mouse_enable(false);
					console.clear_hotspots();
					console.print("Quit Game (\x01~Y/\x01~N) ?");
					var key = console.getkey(K_UPPER);
					mouse_enable(true);
					if(key != 'Y')
						break;
				}
				return;
		}
		change(selected.x, selected.y);
		draw_board(full_redraw);
		full_redraw = false;
	}
}

try {

	// Parse cmd-line options here:
	var numval;
	for(var i = 0; i < argv.length; i++) {
		numval = parseInt(argv[i]);
		if(!isNaN(numval))
			break;
	}

	if(js.global.console) {
		if(argv.indexOf("nocls") < 0)
			js.on_exit("console.clear()");

		js.on_exit("console.attributes = LIGHTGRAY");
	}
	if(argv.indexOf("graphics") >= 0) {
		graph = true;
	}
	if(argv.indexOf("winners") >= 0) {
		if(!isNaN(numval) && numval > 0)
			options.winners = numval;
		show_winners();
		exit();
	}

	if(argv.indexOf("export") >= 0) {
		if(!options.sub) {
			alert("Sub-board not defined");
			exit(1);
		}
		var count = 20;
		if(!isNaN(numval) && numval > 0)
			count = numval;
		var list = json_lines.get(winners_list);
		if(typeof list != 'object') {
			alert("No winners yet: " + list);
			exit(0);
		}
		list.sort(compare_won_game);
		var obj = { date: Date(), game: [] };
		for(var i = 0; i < list.length && i < count; i++)
			obj.game.push(list[i]);
		obj.md5 = md5_calc(JSON.stringify(obj.game));
		var msgbase = new MsgBase(options.sub);
		var hdr = {
			to: title,
			from: system.operator,
			subject: highscores_subject
		};
		var body = lfexpand(JSON.stringify(obj, null, 1));
		body += tear_line;
		if(!msgbase.save_msg(hdr, body)) {
			alert("Error saving message to: " + options.sub);
			exit(2);
		}
		msgbase.close();
		exit(0);
	}

	if(js.global.console) {
		js.on_exit("console.line_counter = 0");
		js.on_exit("console.status = " + console.status);
		js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
		console.ctrlkey_passthru = "KOPTUZ";
	}
	if(!isNaN(numval) && numval > 0 && numval < max_difficulty)
		difficulty = numval;

	if(!difficulty)
		difficulty = 1;

	play();
	userprops.set(ini_section, "selector", selector%selectors.length);
	userprops.set(ini_section, "highlight", highlight);
	userprops.set(ini_section, "difficulty", difficulty);

} catch(e) {

	var msg = file_getname(e.fileName) +
		" line " + e.lineNumber +
		": " + e.message;
	if(js.global.console)
		console.crlf();
	alert(msg);
	if(options.sub && user.alias != author) {
		var msgbase = new MsgBase(options.sub);
		var hdr = {
			to: author,
			from: user.alias || system.operator,
			subject: title
		};
		msg += tear_line;
		if(!msgbase.save_msg(hdr, msg))
			alert("Error saving exception-message to: " + options.sub);
		msgbase.close();
	}
}
