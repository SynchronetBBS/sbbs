// $Id$

// Minesweeper, the game

// See readme.txt for instructions on installation, configuration, and use

"use strict";

const title = "Synchronet Minesweeper";
const ini_section = "minesweeper";
const REVISION = "$Revision$".split(' ')[1];
const author = "Digital Man";
const header_height = 4;
const winners_list = js.exec_dir + "winners.jsonl";
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
const selectors = ["()", "[]", "<>", "{}", "--", "  "];

require("sbbsdefs.js", "K_NONE");

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
if(!options.image_delay)
	options.image_delay = 1500;
if(!options.sub)
    options.sub = load({}, "syncdata.js").find();

var userprops = bbs.mods.userprops;
if(!userprops)
	userprops = load(bbs.mods.userprops = {}, "userprops.js");
var json_lines = bbs.mods.json_lines;
if(!json_lines)
	json_lines = load(bbs.mods.json_lines = {}, "json_lines.js");
var game = {};
var board = [];
var selected = {x:0, y:0};
var gamewon = false;
var gameover = false;
var cell_width;	// either 3 or 2
var selector = userprops.get(ini_section, "selector", options.selector);
var highlight = userprops.get(ini_section, "highlight", options.highlight);
var difficulty = userprops.get(ini_section, "difficulty", options.difficulty);

log(LOG_DEBUG, title + " options: " + JSON.stringify(options));

function show_image(filename, fx)
{
	var dir = directory(filename);
	filename = dir[random(dir.length)];
	var Graphic = load({}, "graphic.js");
	var sauce_lib = load({}, "sauce_lib.js");
	var sauce = sauce_lib.read(filename);
	if(sauce && sauce.datatype == sauce_lib.defs.datatype.bin) {
		try {
			var graphic = new Graphic(sauce.cols, sauce.rows);
			graphic.load(filename);
			if(fx && graphic.revision >= 1.82)
				graphic.drawfx('center', 'center');
			else
				graphic.draw('center', 'center');
			sleep(options.image_delay);
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
			body += "\r\n--- " + js.exec_file + " " + REVISION + "\r\n";
			if(!msgbase.save_msg(hdr, body))
				alert("Error saving message to: " + options.sub);
			msgbase.close();
			game.md5 = undefined;
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
		show_image(winner_image, true);
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
	show_image(boom_image, false);
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

function secondstr(t)
{
	return format("%2u:%02u", Math.floor(t/60), Math.floor(t%60));
}

function show_winners(level)
{
	console.clear();
	console.aborted = false;
	console.attributes = YELLOW|BG_BLUE|BG_HIGH;
	console_center(" " + title + " Top " + options.winners + " Winners ");
	console.attributes = LIGHTGRAY;

	var list = json_lines.get(winners_list);
	if(typeof list != 'object')
		list = [];

	if(options.sub) {
		var msgbase = new MsgBase(options.sub);
		if(msgbase.get_index !== undefined && msgbase.open()) {
			var to_crc = crc16_calc(title.toLowerCase());
			var subj_crc = crc16_calc(winner_subject.toLowerCase());
			var index = msgbase.get_index();
			for(var i = 0; i < index.length; i++) {
				var idx = index[i];
				if((idx.attr&MSG_DELETE)
					|| idx.to != to_crc || idx.subject != subj_crc)
					continue;
				var hdr = msgbase.get_msg_header(true, idx.offset);
				if(!hdr)
					continue;
				if(!hdr.from_net_type || hdr.to != title || hdr.subject != winner_subject)
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
				obj.name = hdr.from;
				var md5 = obj.md5;
				obj.md5 = undefined;
				var calced = md5_calc(JSON.stringify(obj));
				if(calced == md5) {
					obj.net_addr = hdr.from_net_addr;	// Not included in MD5 sum
					list.push(obj);
				} else {
					log(LOG_INFO, title +
						" MD5 not " + calced +
						" in: "  + options.sub +
						" msg " + hdr.number);
				}
			}
			msgbase.close();
		}
	}

	if(level)
		list = list.filter(function (obj) { var difficulty = calc_difficulty(obj);
			return (difficulty <= level && difficulty > level - 1); });

	if(!list.length) {
		alert("No " + (level ? ("level " + level) : "") + " winners yet!");
		return;
	}
	console.attributes = WHITE;
	console.print(format("    %-25s%-15s Lvl   Time  WxHxMines   Date   Rev\r\n", "User", ""));

	list.sort(compare_won_game);

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
		console.print(format("%3u %-25.25s%-15.15s %1.2f %s %3ux%2ux%-3u %s %s\x01>\r\n"
			,count + 1
			,game.name
			,game.net_addr ? ('@'+game.net_addr) : ''
			,difficulty
			,secondstr(game.end - game.start)
			,game.width
			,game.height
			,game.mines
			,system.datestr(game.end)
			,game.rev ? game.rev : ''
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
	console.print(format("Date      %-25s Lvl   Time  WxHxMines Rev  Result\r\n", "User", ""));
	
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
			,secondstr(game.end - game.start)
			,game.width
			,game.height
			,game.mines
			,game.rev ? game.rev : ''
			,game.cause ? ("Lost: " + game.cause) : "Won"
			));
	}
	console.attributes = LIGHTGRAY;
}

function cell_val(x, y)
{
	if(gameover && board[y][x].mine) {
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
	if(!gameover && board[y][x].covered)
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
	console.print(left + val + right);
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
	const margin = Math.floor((console.screen_columns - (game.width * cell_width)) / 2);
	
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
		console.right((game.width * cell_width) + !(cell_width&1));
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
	const margin = Math.floor((console.screen_columns - (game.width * cell_width)) / 2);
	top = Math.floor(Math.max(0, (console.screen_rows - (header_height + game.height)) - 1) / 2);
	console.line_counter = 0;
	console.home();
	if(full) {
		console.down(top);
		console.right(margin - 1);
		console.attributes = BG_BLUE|BG_HIGH;
		console.print('\xDF');
		var width = (game.width * cell_width) + 1 + !(cell_width&1);
		for(var x = 1; x < width; x++)
			console.print(' ');
		console.print('\xDF');
		console.creturn();
		show_title();
		draw_border();
	} else
		console.down(top + 1);
	if(gamewon) {
		console.attributes = YELLOW|BLINK;
		console_center("Winner! Completed in " + secondstr(game.end - game.start).trim());
	} else if(gameover) {
		console.attributes = RED|HIGH|BLINK;
		console_center(((game.end - game.start) < options.timelimit * 60
			? "" : "Time-out: ") + "Game Over");
	} else {
		var elapsed = 0;
		if(game.start)
			elapsed = (Date.now() / 1000) - game.start;
		var timeleft = Math.max(0, (options.timelimit * 60) - elapsed);
		console.attributes = LIGHTCYAN;
		console_center(format("%2d Mines  Lvl %1.2f  %s%s "
			, game.mines - totalflags(), calc_difficulty(game)
			, game.start && !gameover && (timeleft / 60) <= options.timewarn ? "\x01r\x01h\x01i" : ""
			, secondstr(timeleft)
			));
	}
	var cmds = "";
	if(!gameover) {
		if(!board[selected.y][selected.x].covered) {
			if(can_chord(selected.x, selected.y))
				cmds += "\x01h\x01kDig  \x01n\x01hC\x01nhord  ";
			else
				cmds += "\x01h\x01kDig  Flag  ";
		}
		else 
			cmds += "\x01hD\x01nig  \x01hF\x01nlag  ";
	}
	cmds += "\x01n\x01hN\x01new  \x01hQ\x01nuit";
	if(full || cmds !== cmds_shown) {
		draw_border();
		console.attributes = LIGHTGRAY;		
		console_center(cmds);
		draw_border();
		console_center("\x01hW\x01ninners  \x01hL\x01nog  \x01hH\x01nelp");
		cmds_shown = cmds;
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
				if(console.term_supports(USER_ANSI))
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
		if(game.height + header_height < console.screen_rows - 1) {
			height++;
			console.down();
			console.creturn();
			console.right(margin - 1);
			console.attributes = BG_BLUE|BG_HIGH;
			console.print('\xDC');
			for(var x = 0; x < (game.width * cell_width) + !(cell_width&1); x++)
				console.print(' ');
			console.print('\xDC');
		}
		console.attributes = LIGHTGRAY;
	}
	if(redraw_selection) { // We need to draw/redraw the selected cell last in this case
		if(console.term_supports(USER_ANSI))
			console.gotoxy(margin + (selected.x * cell_width) + 1, header_height + selected.y + top + 1);
		else {
			console.up(height - (selected.y + 1));
			console.creturn();
			console.right(margin + (selected.x * cell_width));
		}
		draw_cell(selected.x, selected.y);
		console.left(2);
	}
	console.gotoxy(margin + (selected.x * cell_width) + 2, header_height + selected.y + top + 1);
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

function get_difficulty()
{
	console.creturn();
	console.cleartoeol();
	draw_border();
	console.attributes = WHITE;
	console.right((console.screen_columns - 24) / 2);
	console.print(format("Difficulty Level (1-%u): ", max_difficulty));
	return console.getnum(max_difficulty);
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
	game = { rev: REVISION };
	game.height = target_height(difficulty);
	game.width = game.height;
	game.height = Math.min(game.height, console.screen_rows - header_height);
	game.width += game.width - game.height;
	if(game.width > 10 && (game.width * 3) + 2 > (console.screen_columns - 20))
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

function play()
{
	console.clear();
	show_image(welcome_image);
	show_image(mine_image, true);
	sleep(500);
	init_game(difficulty);
	draw_board(true);
	var full_redraw = false;
	while(bbs.online) {
		if(!gameover && game.start
			&& Date.now() - (game.start * 1000) >= options.timelimit * 60 * 1000) {
			lostgame("timeout");
			draw_board(true);
		}
		var key = console.inkey(K_NONE, 1000);
		if(!key) {
			if(game.start && !gameover)
				draw_board(false);	// update the time display
			continue;
		}
		change(selected.x, selected.y);
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
			case 'D':	// Dig
			case ' ':
				if(!gameover
					&& board[selected.y][selected.x].covered
					&& !board[selected.y][selected.x].flagged) {
					if(uncover(selected.x, selected.y))
						lostgame("mine");
					else
						isgamewon();
					full_redraw = gameover;
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
				if(game.start && !gameover) {
					console.cleartoeol();
					draw_border();
					console.attributes = LIGHTRED;
					console.right((console.screen_columns - 15) / 2);
					console.print("New Game (Y/N) ?");
					if(console.getkey(K_UPPER) != 'Y')
						break;
				}
				var new_difficulty = get_difficulty();
				if(new_difficulty > 0)
					difficulty = init_game(new_difficulty);
				full_redraw = true;
				break;
			}
			case 'W':
				console.home();
				console.down(top + 1);
				var level = get_difficulty();
				if(level >= 1) {
					console.line_counter = 0;
					show_winners(level);
					console.pause();
					console.clear();
					console.aborted = false;
					full_redraw = true;
				}
				break
			case 'L':
				console.line_counter = 0;
				show_log();
				console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				break
			case '?':
			case 'H':
				console.line_counter = 0;
				console.clear();
				console.printfile(help_file);
				console.clear();
				console.aborted = false;
				full_redraw = true;
				break;
			case '\t':
				highlight = !highlight;
				break;
			case CTRL_R:
				console.clear();
				full_redraw = true;
				break;
			case CTRL_S:
				selector++;
				break;
			case 'Q':
				if(game.start && !gameover) {
					console.home();
					console.down(top + 1);
					console.cleartoeol();
					draw_border();
					console.attributes = LIGHTRED;
					console.right((console.screen_columns - 16) / 2);
					console.print("Quit Game (Y/N) ?");
					if(console.getkey(K_UPPER) != 'Y')
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
	
	if(argv.indexOf("nocls") < 0)
		js.on_exit("console.clear()");
	
	js.on_exit("console.attributes = LIGHTGRAY");

	if(argv.indexOf("winners") >= 0) {
		if(!isNaN(numval) && numval > 0)
			options.winners = numval;
		show_winners();
		exit();
	}

	js.on_exit("console.line_counter = 0");
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru = "KOPTUZ";

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
	console.crlf();
	alert(msg);
	if(options.sub && user.alias != author) {
		var msgbase = new MsgBase(options.sub);
		var hdr = { 
			to: author,
			from: user.alias,
			subject: title
		};
		msg += "\r\n--- " + js.exec_file + " " + REVISION + "\r\n";		
		if(!msgbase.save_msg(hdr, msg))
			alert("Error saving exception-message to: " + options.sub);
		msgbase.close();
	}
}
