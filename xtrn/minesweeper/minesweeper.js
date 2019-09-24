// $Id$

// Minesweeper, the game

// Configure in SCFG->External Programs->Online Programs->Games->
// Available Online Programs...
//
// Name                       Synchronet Minesweeper
// Internal Code              MSWEEPER
// Command Line               ?minesweeper
// Multiple Concurrent Users  Yes

// Optionally, if you want the top-X winners displayed after exiting game, set:
// Clean-up Command Line      ?minesweeper winners

// If you want the top-x winners displayed during logon:
//
// Name                       Synchronet Minesweeper Winners
// Internal Code              MSWINNER
// Command Line               ?minesweeper winners
// Multiple Concurrent Users  Yes
// Execute on Event           Logon, Only

// Command-line arguments supported:
//
// "winners" - display list of top-X winners and exit
// "nocls"   - don't clear the screen upon exit
// <level>   - set the initial game difficulty level (1-5, don't prompt)

// ctrl/modopts.ini [minesweeper] options (with default values):
// sub = syncdata
// timelimit = 60
// winners = 20

"use strict";

const title = "Synchronet Minesweeper";
const REVISION = "$Revision$".split(' ')[1];
const author = "Digital Man";
const header_height = 4;
const winners_list = system.data_dir + "minesweeper.jsonl";
const help_file = system.text_dir + "minesweeper.hlp";
const max_difficulty = 5;
const min_mine_density = 0.10;
const mine_density_multiplier = 0.025;
const char_flag = '\x01rF';
const char_badflag = '\x01r\x01h!';
const attr_empty = '\x01b'; //\x01h';
const char_empty = attr_empty + '\xFA'; 
const char_covered = attr_empty +'\xFE';
const char_mine = '\x01r\x01h\xEB';
const char_detonated_mine = '\x01r\x01h\x01i\*';
const attr_count = "\x01c";
const winner_subject = "Winner";

require("sbbsdefs.js", "K_NONE");

var options=load({}, "modopts.js", "minesweeper");
if(!options)
	options = {};
if(!options.timelimit)
	options.timelimit = 60;	// minutes
if(!options.winners)
	options.winners = 20;
if(!options.sub)
    options.sub = load({}, "syncdata.js").find();

var json_lines = load({}, "json_lines.js");
var game = { rev: REVISION };
var board = [];
var selected = {x:0, y:0};
var gamewon = false;
var gameover = false;
var cell_width;	// either 3 or 2

function reach(x, y)
{
	var count = 0;
	if(y) {
		if(x && board[y-1][x-1].mine)
			count++;
		if(board[y-1][x].mine)
			count++;
		if(x + 1 < game.width && board[y-1][x+1].mine)
			count++;
	}
	if(x && board[y][x-1].mine)
		count++;
	if(x + 1 < game.width && board[y][x+1].mine)
		count++;
	if(y + 1 < game.height) {
		if(x && board[y+1][x-1].mine)
			count++;
		if(board[y+1][x].mine)
			count++;
		if(x + 1 < game.width && board[y + 1][x + 1].mine)
			count++;
	}
	return count;
}

function place_mines()
{
	var mined = new Array(game.height * game.width);
	for(var i = 0; i < game.mines; i++)
		mined[i] = true;

	for(var i = 0; i < game.mines; i++) {
		var r = random(game.height * game.width);
		if(r == (selected.y * game.width) + selected.x)
			continue;
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
			board[y][x].count = reach(x, y);
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
		game.end = time();
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
		return true;
	}
	return false;
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

function compare_game(g1, g2)
{
	var diff = calc_difficulty(g2) - calc_difficulty(g1);
	if(diff)
		return diff;
	return calc_time(g1) - calc_time(g2);
}

function secondstr(t)
{
	return format("%2u:%02u", t/60, t%60);
}

function show_winners()
{
	console.clear();
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
	
	if(!list.length) {
		alert("No winners yet!");
		return;
	}
	console.attributes = WHITE;
	console.print(format("Rank %-25s%-15s Lvl  Time  WxHxMines   Date   Rev\r\n", "User", ""));
	
	list.sort(compare_game);
	
	for(var i = 0; i < list.length && i < options.winners && !console.aborted; i++) {
		var game = list[i];
		if(i&1)
			console.attributes = LIGHTCYAN;
		else
			console.attributes = BG_CYAN;
		console.print(format("%4u %-25s%-15s %1.1f %s %3ux%2ux%-3u %s %s\x01>\r\n"
			,i + 1
			,game.name
			,game.net_addr ? ('@'+game.net_addr) : ''
			,calc_difficulty(game)
			,secondstr(game.end - game.start)
			,game.width
			,game.height
			,game.mines
			,system.datestr(game.end)
			,game.rev ? game.rev : ''
			));
	}
	console.attributes = LIGHTGRAY;
}

function cell_val(x, y)
{
	if(gameover && board[y][x].mine) {
		if(selected.x == x && selected.y == y)
			return char_detonated_mine;
		return char_mine;
	}
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

function draw_cell(x, y)
{
	console.attributes = LIGHTGRAY;
	var val = cell_val(x, y);
	var left = " ";
	var right = " ";
	if(game.start && !gameover
		&& !board[selected.y][selected.x].covered
		&& board[selected.y][selected.x].count
		&& (selected.x == x - 1 || selected.x == x || selected.x == x + 1)
		&& (selected.y == y -1 || selected.y == y || selected.y == y + 1))
		console.attributes |= HIGH;
	if(selected.x == x && selected.y == y)
		left = "<", right = "\x01n\x01h>"; //left = "\x01n\x01h<"
	console.print(left + val + right);
}

function countflags()
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

function draw_board(full)
{
	const margin = Math.floor((console.screen_columns - (game.width * cell_width)) / 2);
	console.line_counter = 0;
	console.home();
	if(full) {
		console.right(margin - 1);
		console.attributes = BG_BLUE|BG_HIGH;
		console.print('\xDF');
		var width = (game.width * cell_width) + 1 + !(cell_width&1);
		for(var x = 1; x < width; x++)
			console.print(' ');
		console.print('\xDF');
		console.creturn();
		console.home();
		show_title();
		draw_border();
	} else
		console.down();
	if(gamewon) {
		console.attributes = YELLOW|BLINK;
		console_center("Winner! Completed in " + secondstr(game.end - game.start).trim());
	} else if(gameover) {
		console.attributes = RED|HIGH|BLINK;
		console_center("Game Over!");
	} else {
		var elapsed = 0;
		if(game.start)
			elapsed = time() - game.start;
		console.attributes = LIGHTCYAN;
		console_center(format("Mines: %-3d Lvl: %1.1f  %s"
			, game.mines - countflags(), calc_difficulty(game)
			, secondstr((options.timelimit * 60) - elapsed))
			);
	}
	if(full) {
		draw_border();
		console.attributes = LIGHTGRAY;		
		var cmds = "";
		if(!gameover)
			cmds += "\x01hR\x01neveal  \x01hF\x01nlag  ";
		cmds += "\x01hN\x01new  \x01hQ\x01nuit";
		console_center(cmds);
		draw_border();
		console_center("\x01hW\x01ninners  \x01hH\x01nelp");
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
					console.gotoxy((x * cell_width) + margin + 1, header_height + y + 1);
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
			console.gotoxy(margin + (selected.x * cell_width) + 1, header_height + selected.y + 1);
		else {
			console.up(height - (selected.y + 1));
			console.creturn();
			console.right(margin + (selected.x * cell_width));
		}
		draw_cell(selected.x, selected.y);
		console.left(2);
	}
	console.gotoxy(margin + (selected.x * cell_width) + 2, header_height + selected.y + 1);
}

function mined(x, y)
{
	return board[y][x].mine;
}

function start_game()
{
	place_mines();
	game.start = time();
}

function uncover(x, y)
{
	if(!game.start)
		start_game();
	
	if(board[y][x].flagged || !board[y][x].covered)
		return;
	board[y][x].covered = false;
	board[y][x].changed = true;
	if(board[y][x].count)
		return;
	if(y) {
		if(x) {
			if(!mined(x - 1, y - 1))
				uncover(x - 1, y - 1);
		}
		if(!mined(x, y - 1))
			uncover(x, y - 1);
		if(x + 1 < game.width) {
			if(!mined(x + 1, y - 1))
				uncover(x + 1, y - 1);
		}
	}
	if(x) {
		if(!mined(x - 1, y))
			uncover(x - 1, y);
	}
	if(x + 1 < game.width) {
		if(!mined(x + 1, y))
			uncover(x + 1, y);
	}
	if(y + 1 < game.height) {
		if(x) {
			if(!mined(x - 1, y + 1))
				uncover(x - 1, y + 1);
		}
		if(!mined(x, y + 1))
			uncover(x, y + 1);
		if(x + 1 < game.width) {
			if(!mined(x + 1, y + 1))
				uncover(x + 1, y + 1);
		}
	}
}

function get_difficulty()
{
	console.creturn();
	if(options.difficulty) {
		console.cleartoeol();
		draw_border();
	}
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
	for(var y = 0; y < game.height; y++) {
		board[y] = new Array(game.width);
		for(var x = 0; x < game.width; x++) {
			board[y][x] = { covered: true };
		}
	}
	select_middle();
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

function play() {
	
	init_game(options.difficulty);
	draw_board(true);
	var full_redraw = false;
	while(bbs.online) {
		if(!gameover && game.start
			&& time() - game.start >= options.timelimit * 60) {
			gameover = true;
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
			case 'R':
			case ' ':
				if(!gameover
					&& board[selected.y][selected.x].covered
					&& !board[selected.y][selected.x].flagged) {
					if(board[selected.y][selected.x].mine)
						gameover = true;
					else
						uncover(selected.x, selected.y);
					if(isgamewon()) {
						gamewon = true;
						gameover = true;
					}
					full_redraw = gameover;
				}
				break;
			case 'F':
				if(!gameover && board[selected.y][selected.x].covered) {
					board[selected.y][selected.x].flagged = !board[selected.y][selected.x].flagged;
					if(!game.start)
						start_game();
				}
				break;
			case 'N':
			{
				console.home();
				console.down();
				if(game.start && !gameover) {
					console.cleartoeol();
					draw_border();
					console.attributes = LIGHTRED;
					console.right((console.screen_columns - 15) / 2);
					console.print("New Game (Y/N) ?");
					if(console.getkey(K_UPPER) != 'Y')
						break;
				}
				var difficulty = get_difficulty();
				if(difficulty > 0)
					init_game(difficulty);
				full_redraw = true;
				break;
			}
			case 'W':
				console.line_counter = 0;
				show_winners();
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
			case CTRL_R:
				full_redraw = true;
				break;
			case 'Q':
				if(game.start && !gameover) {
					console.home();
					console.down();
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
	console.ctrlkey_passthru = "TPU";

	if(!isNaN(numval) && numval > 0 && numval < max_difficulty)
		options.difficulty = numval;

	if(!options.difficulty) {
		show_title();
		options.difficulty = get_difficulty();
		if(options.difficulty < 1)
			exit();
	}
	play();
	
} catch(e) {
	
	var msg = file_getname(e.fileName) + 
		" line " + e.lineNumber + 
		": " + e.message;
	console.crlf();
	alert(msg);
	if(options.sub) {
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
