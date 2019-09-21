// $Id$

// Minesweeper, the game

"use strict";

const title = "Synchronet Minesweeper";
const REVISION = "$Revision$".split(' ')[1];
const header_height = 4;
const cell_width = 3;
const winners_list = system.data_dir + "minesweeper.jsonl";
const help_file = system.text_dir + "minesweeper.hlp";
const max_level = 5;
const char_flag = '\x01r\x01hF';
const char_badflag = '\x01r\x01h!';
const char_empty = ' '; 
const char_covered = '\xFA';
const char_mine = '\x01r\x01h\xEB';

require("sbbsdefs.js", "K_NONE");

var json_lines = load({}, "json_lines.js");
var game = {};
var board = [];
var selected = {x:0, y:0};
var gamewon = false;
var gameover = false;

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

function init_board()
{
	var mined = new Array(game.height * game.width);
	for(var i = 0; i < game.mines; i++)
		mined[i] = true;

	for(var i = 0; i < game.mines; i++) {
		var r = random(game.height * game.width);
		var tmp = mined[i];
		mined[i] = mined[r];
		mined[r] = tmp;
	}
	
	for(var y = 0; y < game.height; y++) {
		board[y] = new Array(game.width);
		for(var x = 0; x < game.width; x++) {
			board[y][x] = { covered: true };
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
		game.user = { alias: user.alias, number: user.number };
		var result = json_lines.add(winners_list, game);
		if(result !== true) {
			alert(result);
			console.pause();
		}
		return true;
	}
	return false;
}

function show_winners()
{
	console.attributes = YELLOW|BG_BLUE;
	console.print(" " + title + " Winners ");
	console.attributes = LIGHTGRAY;
	console.crlf();
	
	var list = json_lines.get(winners_list);
	if(typeof list != 'object' || !list.length)	{
		alert("No winners yet!");
		return;
	}
	console.attributes = WHITE;
	console.print(format("Date     %-25s Time  WxHxMines\r\n", "Name"));
	for(var i = 0; i < list.length; i++) {
		var game = list[i];
		if(i&1)
			console.attributes = LIGHTGRAY;
		else
			console.attributes = BG_CYAN;
		console.print(format("%s %-25s %s %ux%ux%u\r\n"
			,system.datestr(game.end)
			,game.user.alias
			,system.secondstr(game.end - game.start).slice(-5)
			,game.width
			,game.height
			,game.mines
			));
	}
	console.attributes = LIGHTGRAY;
}

function cell_val(x, y)
{
	if(gameover && board[y][x].mine)
		return char_mine;
	if(board[y][x].flagged) {
		if(gameover && !board[y][x].mine)
			return char_badflag;
		return char_flag;
	}
	if(!gameover && board[y][x].covered)
		return char_covered;
	if(board[y][x].mine)
		return char_mine;
	if(board[y][x].count)
		return board[y][x].count;
	return char_empty;
}

function draw_cell(x, y)
{
	console.attributes = LIGHTGRAY;
	var val = cell_val(x, y);
	var left = " ";
	var right = " ";
	if(!gameover) {
		if(selected.x == x && selected.y == y)
			left = "\x01n\x01h<", right = "\x01n\x01h>";
	}
	console.print(left + val + right);
}

function countflags()
{
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
	console.attributes = YELLOW|BG_BLUE;
	console.cleartoeol();
	console.center(title + " " + REVISION);
}

function draw_border()
{
	console.creturn();
	console.attributes = LIGHTGRAY;
	console.cleartoeol();
	console.attributes = YELLOW|BG_BLUE;
	console.print(' ');
	console.right(console.screen_columns - 1);
	console.cleartoeol();
	console.creturn();
	console.attributes = LIGHTGRAY;
}
	
function draw_board(full)
{
	if(gameover)
		full = true;
	console.line_counter = 0;
	console.home();
	if(full) {
		show_title();
		draw_border();
	} else
		console.down();
	if(gamewon) {
		console.attributes = YELLOW|BLINK;
		console.center("Game Won!");
	} else if(gameover) {
		console.attributes = RED|HIGH|BLINK;
		console.center("Game Over!");
	} else {
		console.attributes = WHITE;
		console.center(format("Mines: %-3u Flags: %-3u %s"
			, game.mines, countflags(), system.secondstr(time() - game.start).slice(-5))
			);
	}
	if(full) {
		draw_border();
		console.attributes = LIGHTGRAY;		
		var cmds = "";
		if(!gameover)
			cmds += "\x01n\x01hR\x01neveal  \x01hF\x01nlag  ";
		cmds += "\x01hN\x01new  \x01hQ\x01nuit";
		console.center(cmds);
		draw_border();
		console.center("\x01n\x01hW\x01ninners  \x01hH\x01nelp");
	} else if(!console.term_supports(USER_ANSI)) {
		console.creturn();
		console.down(2);
	}
	const margin = (console.screen_columns - (game.width * cell_width)) / 2;
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
			}
			board[y][x].changed = false;
		}
		if(full || !console.term_supports(USER_ANSI))
			console.down();
		console.line_counter = 0;
	}
	if(full) {
		if(game.height + header_height < console.screen_rows - 1) {
			console.creturn();
			console.attributes = YELLOW|BG_BLUE;
			console.cleartoeol();
		}
		console.attributes = LIGHTGRAY;
	}
}

function mined(x, y)
{
	return board[y][x].mine;
}

function uncover(x, y)
{
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

function get_level()
{
	console.creturn();
	if(game.level) {
		console.cleartoeol();
		draw_border();
	}
	console.attributes = WHITE;
	console.right((console.screen_columns - 24) / 2);
	console.print(format("Difficulty Level (1-%u): ", max_level));
	var result = console.getnum(max_level);
	if(result < 1)
		return false;
	game.level = Math.max(1, result);
	return true;
}

function init_game()
{
	console.line_counter = 0;
	console.clear();
	selected = {x:0, y:0};
	gamewon = false;
	gameover = false;
	game.height = 5 + (game.level * 5);
	game.width = game.height;
	game.width = Math.min(game.width, Math.floor((console.screen_columns - 1) / cell_width));
	game.height = Math.min(game.height, console.screen_rows - header_height);
	game.mines = Math.floor((game.height * game.width) / (11 - game.level))
	game.start = time();
	init_board();
}

function play() {
	
	draw_board(true);
	var full_redraw = false;
	while(bbs.online && !console.aborted) {
		var key = console.inkey(K_NONE, 1000);
		if(!key) {
			if(!gameover)
				draw_board(false);
			continue;
		}
		board[selected.y][selected.x].changed = true;		
		switch(key.toUpperCase()) {
			case KEY_HOME:
				selected.x = 0;
				break;
			case KEY_END:
				selected.x = game.width - 1;
				break;
			case KEY_PAGEUP:
				selected.y = 0;
				break;
			case KEY_PAGEDN:
				selected.y = game.height -1;
				break;
			case '7':
				if(selected.y && selected.x) {
					selected.y--;
					selected.x--;
				}
				break;
			case '8':
			case KEY_UP:
				if(selected.y)
					selected.y--;
				break;
			case '9':
				if(selected.y && selected.x < game.width - 1) {
					selected.y--;
					selected.x++;
				}
				break;
			case '2':
			case KEY_DOWN:
				if(selected.y < game.height - 1)
					selected.y++;
				break;
			case '1':
				if(selected.y < game.height -1 && selected.x) {
					selected.y++;
					selected.x--;
				}
				break;
			case '3':
				if(selected.x < game.width - 1&& selected.y < game.height - 1) {
					selected.x++;
					selected.y++;
				}
				break;
			case '4':
			case KEY_LEFT:
				if(selected.x)
					selected.x--;
				break;
			case '6':
			case KEY_RIGHT:
				if(selected.x < game.width - 1)
					selected.x++;
				break;
			case 'R':
			case ' ':
				if(board[selected.y][selected.x].covered
					&& !board[selected.y][selected.x].flagged) {
					if(board[selected.y][selected.x].mine)
						gameover = true;
					else
						uncover(selected.x, selected.y);
					if(isgamewon()) {
						gamewon = true;
						gameover = true;
					}
				}
				break;
			case 'F':
				if(board[selected.y][selected.x].covered) {
					board[selected.y][selected.x].flagged = !board[selected.y][selected.x].flagged;
				}
				break;
			case 'N':
				console.home();
				console.down();
				if(get_level())
					init_game();
				full_redraw = true;
				break;
			case 'W':
				console.line_counter = 0;
				console.clear();
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
				console.pause();
				console.clear();
				console.aborted = false;
				full_redraw = true;
				break;
			case '\x12': // Ctrl-R
				full_redraw = true;
				break;
			case 'Q':
				return;
		}
		board[selected.y][selected.x].changed = true;
		draw_board(full_redraw);
		full_redraw = false;
	}
}

js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
console.ctrlkey_passthru = "TPU";

// Parse cmd-line options here:
var val = parseInt(argv[0]);
if(!isNaN(val) && val > 0 && val < max_level)
	game.level = val;
else {
	show_title();
	if(!get_level())
		exit();
}
init_game();
play();
console.attributes = LIGHTGRAY;
console.clear();