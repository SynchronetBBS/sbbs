'use strict';

require('sbbsdefs.js', 'K_NONE');
require('mouse_getkey.js', 'mouse_getkey');

const answer_offset_x = 4;
const answer_origin = { x: 4, y: 3 };
const author = 'Ree';
const colour_offset_x = 4;
const colour_origin = { x: 23, y: 20 };
const colour_width = 2;
const debug = true;
const level_names = ['Unknown', 'Easy', 'Normal', 'Hard'];
const message_origin = { x: 21, y: 24 };
const message_width = 79 - message_origin.y + 1;
const peg_colours = [BLACK, BLACK, WHITE];
const peg_origin = { x: 4, y: 23 };
const piece_colours = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, BLACK, WHITE, LIGHTMAGENTA];
const piece_offset_x = 2;
const piece_origin = { x: 11, y: 23 };   
const program_name = 'Mastermind';
const program_version = '23.09.14';
const row_offset_y = 2;
const tear_line = '\r\n--- ' + js.exec_file + ' v' + program_version + '\r\n';
const winner_subject = program_name + ' Winner';
const winner_to = js.exec_file;
const winners_list = js.exec_dir + 'winners' + (debug ? '-debug' : '') + '.jsonl';

var answer;
var current_colour;
var current_column;
var game = { version: program_version };
var game_over;
var guesses;
var last_message = '';

var data_sub = load({}, 'syncdata.js').find(debug ? 'localdata' : 'syncdata');

if (js.global.bbs === undefined)
	var json_lines = load({}, 'json_lines.js');
else {
	var json_lines = bbs.mods.json_lines;
	if (!json_lines) {
		json_lines = load(bbs.mods.json_lines = {}, 'json_lines.js');
    }
}

function add_hotspots() {
    console.clear_hotspots();
    console.add_hotspot('H', false, 51, 69, 16); // High Scores List
    console.add_hotspot('N', false, 51, 61, 17); // New Game
    console.add_hotspot('Q', false, 63, 69, 17); // Quit
}

// Check to see if we ran out of guesses
function check_lost() {
    if (game.row === 9) {
        game_over = true;
        set_message('Game Over, You Lose!');
        draw_answer();
    }
}

// Check to see if all four pegs are black
function check_won() {
    for (var i = 0; i < 4; i++) {
        if (guesses[game.row].peg[i] !== 1) {
            return false;
        }
    }

    game.end = time();
    game_over = true;
    set_message('Congratulations, You Win!');
    draw_answer();

    // Save high score to data_sub
    if (data_sub) {
        var msgbase = new MsgBase(data_sub);
        var hdr = { 
            to: winner_to,
            from: user.alias,
            subject: winner_subject
        };
        game.name = user.alias;
        game.md5 = md5_calc(JSON.stringify(game));
        game.name = undefined;
        var body = lfexpand(JSON.stringify(game, null, 1));
        body += tear_line;
        if(!msgbase.save_msg(hdr, body)) {
			msg = 'Error saving exception-message to: ' + data_sub;
            alert(msg);
            log(LOG_ERR, msg);
        }
        msgbase.close();
    }

    // Save high score to filesystem
    game.name = user.alias;
    var result = json_lines.add(winners_list, game);
    if (result !== true) {
        alert(result);
    }
    
    return true;
}

function compare_won_game(g1, g2) {
	var level = g2.level - g1.level;
	if (level) {
		return level;
    }

    var row = g1.row - g2.row;
	if (row) {
		return row;
    }

	return (g1.end - g1.start) - (g2.end - g2.start);
}

// A non-destructive console.center() replacement
function console_center(text) {
	console.right((console.screen_columns - console.strlen(text)) / 2);
	console.print(text);
	console.crlf();
}

function display_high_scores() {
	console.attributes = LIGHTGRAY;
	console.clear();
	console.aborted = false;
	console.attributes = WHITE|BG_BLUE;
	console_center(' ' + program_name + ' Top 20 Winners ');
	console.attributes = LIGHTGRAY;

	var list = get_winners();
	if (!list.length) {
		alert('No winners yet!');
	} else {
        console.attributes = WHITE;
        console.print(' ##  User                     System          Level  Row  Time       Date       \r\n');
        for (var i = 0; i < list.length && i < 20 && !console.aborted; i++) {
            var game = list[i];
            if (i & 1) {
                console.attributes = YELLOW;
            } else {
                console.attributes = BG_BROWN;
            }
            
            var duration = game.end - game.start;
            var endDate = new Date(game.end * 1000);

            // Games before the level system was added were equivalent to Easy
            if (game.level === undefined) {
                game.level = 1;
            }
            
            console.print(format(' %02u  %-25.25s%-15.15s %s   %02u   %s  %04u/%02u/%02u\x01>\r\n'
                ,i + 1
                ,game.name
                ,game.net_addr ? ('@'+game.net_addr) : 'Local'
                ,level_names[game.level].substring(0, 4)
                ,game.row + 1
                ,format('%02u:%06.3f', Math.floor(duration / 60), duration % 60)
                ,endDate.getFullYear()
                ,endDate.getMonth() + 1
                ,endDate.getDate()
            ));
        }
        console.attributes = LIGHTGRAY;
    }

    console.pause();
    console.aborted = false;
}

// Draw the answer line
function draw_answer() {
    for (var i = 0; i < 4; i++) {
        console.gotoxy(answer_origin.x + (answer_offset_x * i), answer_origin.y);
        console.attributes = piece_colours[answer[i]];
        console.write('\xDB\xDB');
    }
}

// Highlight the currently selected colour
function draw_colour(highlight) {
    console.gotoxy(colour_origin.x + (colour_offset_x * current_colour) - 1, colour_origin.y);
    console.attributes = LIGHTGRAY;
    console.write(highlight ? '[' : ' ');
    console.right(colour_width);
    console.write(highlight ? ']' : ' ');
}

// Draw the pegs for the current line
function draw_pegs() {
    for (var i = 0; i < 4; i++) {
        console.gotoxy(peg_origin.x + i, peg_origin.y - (game.row * row_offset_y));
        console.attributes = peg_colours[guesses[game.row].peg[i]] | BG_BROWN;
        if (guesses[game.row].peg[i] === null) {
            console.write('\xFA');
        } else {
            console.write('\xFE');
        }
    }
}

// Highlight the currently selected piece
function draw_piece(highlight) {
    console.gotoxy(piece_origin.x + (piece_offset_x * current_column) - 1, piece_origin.y - (game.row * row_offset_y));
    console.attributes = (highlight ? LIGHTGRAY : BLACK) | BG_BROWN;
    console.write(highlight ? '[ ]' : '   ');
    console.gotoxy(piece_origin.x + (piece_offset_x * current_column), piece_origin.y - (game.row * row_offset_y));
    console.attributes = piece_colours[guesses[game.row].piece[current_column]] | BG_BROWN;
    if (guesses[game.row].piece[current_column] === null) {
        console.write('\xF9');
    } else {
        console.write('\xDB');
    }
}

// Generate a random answer
function generate_answer(level) {
    // Pick four colours
    // Level 1 requires unique colours (360 permutations)
    // Level 2 allows duplicates (1296 permutations)
    // Level 3 adds a seventh colour (2401 permutations)

    var available_colours = [0, 1, 2, 3, 4, 5];
    if (level === 3) {
        available_colours.push(6);
    }

    answer = [];
    for (var i = 0; i < 4; i++) {
        var next_colour = -1;
        while (next_colour === -1) {
            var j = random(available_colours.length);
            next_colour = available_colours[j];
            
            // Level 1 does not allow duplicates
            if (level === 1) {
                if (answer.indexOf(next_colour) >= 0) {
                    next_colour = -1;
                }
            }
        }
        answer[i] = next_colour;
    }
}

// Check temp_line against the answer, looking for right colour in right spot
// Returns the number of black/blue pegs that should be placed
function get_black_pegs(temp_line) {
    var result = 0;
    for (var i = 0; i < 4; i++) {
        if (temp_line.piece[i] === answer[i]) {
            temp_line.peg[result++] = 1;
        }
    }
    return result;
}

// Check temp_line against the answer, looking for right colour in wrong spot
// Returns the number of white pegs that should be placed
function get_white_pegs(temp_line, black) {
    var result = 0;
    if (black < 4) {
        for (var i = 0; i < 4; i++) {
            for (var j = 0; j < 4; j++) {
                if ((temp_line.piece[i] === answer[j]) && (i !== j)) {
                    temp_line.peg[black + result++] = 2;
                }
            }
        }
    }
    return result;
}

function get_winners() {
	var list = json_lines.get(winners_list);
	if (typeof list != 'object') {
		list = [];
    }

	if (data_sub) {
		var msgbase = new MsgBase(data_sub);
		if (msgbase.get_index !== undefined && msgbase.open()) {
			var to_crc = crc16_calc(winner_to.toLowerCase());
			var winner_crc = crc16_calc(winner_subject.toLowerCase());
			var index = msgbase.get_index();
			for (var i = 0; index && i < index.length; i++) {
				var idx = index[i];
				if ((idx.attr & MSG_DELETE) || (idx.to !== to_crc) || (idx.subject !== winner_crc)) {
					continue;
                }
				var hdr = msgbase.get_msg_header(true, idx.offset);
				if (!hdr || (!hdr.from_net_type && !debug) || (hdr.to !== winner_to) || (hdr.subject !== winner_subject)) {
					continue;
                }
				var body = msgbase.get_msg_body(hdr, false, false, false);
				if (!body) {
					continue;
                }
				body = body.split('\n===', 1)[0];
				body = body.split('\n---', 1)[0];
				var obj;
				try {
					obj = JSON.parse(strip_ctrl(body));
				} catch (e) {
					log(LOG_INFO, program_name + ' ' + e + ': '  + data_sub + ' msg ' + hdr.number);
					continue;
				}
				obj.name = hdr.from;
				var md5 = obj.md5;
				obj.md5 = undefined;
                var calced = md5_calc(JSON.stringify(obj));
				if (calced === md5) {
                    obj.net_addr = hdr.from_net_addr;	// Not included in MD5 sum
                    if (!list_contains(list, obj)) {
                        list.push(obj);
                    }
				} else {
					log(LOG_INFO, program_name + ' MD5 mismatch, got ' + calced + ', expected ' + md5 + ', in: '  + data_sub + ' msg ' + hdr.number);
				}
			}
			msgbase.close();
		}
	}

	list.sort(compare_won_game);
			
	return list;
}

// TODOX New locations for mouse clicks
function handle_board_click(x, y) {
    // First/bottom row is 4x22, 6x22, 8x22, 10x22
    // Then each subsequent row is two rows up

    // Confirm x and y coordinates correspond to a board position on the current row
    var new_column = [4, 6, 8, 10].indexOf(x);
    if (new_column === -1) {
        return false;
    }
    if (y !== 22 - (game.row * 2)) {
        return false;
    }

    if (new_column !== current_column) {
        move_piece(new_column - current_column);
    }

    return true;
}

// TODOX New locations for mouse clicks
function handle_colour_click(x, y) {
    // 32x9 and 33x9 are the cells for green
    // Then each subsequent colour is one row down

    // Confirm x and y coordinate correspond to a colour
    if ((x < 32) || (x > 33)) {
        return false;
    }
    if ((y < 9) || (y > 14)) {
        return false;
    }
    
    var new_colour = y - 8; // For example 9 = green, minus 8 = 1, which is the piece_colours index for green
    if (new_colour !== current_colour) {
        move_colour(new_colour - current_colour);
    }

    return true;
}

function list_contains(list, obj)
{
	var match = false;
	for (var i = 0; i < list.length && !match; i++) {
		match = true;
		for (var p in obj) {
			if (list[i][p] !== obj[p]) {
				match = false;
				break;
			}
		}
	}
	return match;
}

function main() {
    // Call new_game to draw screen and init variables, but then flag as game_over so user has to hit N to start (and we get an accurate game duration)
    new_game(0);
    draw_colour(false);
    draw_piece(false);
    set_message('Welcome to ' + program_name + ' v' + program_version + '.  Press N to begin.');
    game_over = true;

    mouse_enable(true);

    do {
        var mk = mouse_getkey(K_NONE, 1000, true);
        var ch = mk.key.toUpperCase();

        if (mk.mouse !== null) {
			if (!mk.mouse.release || mk.mouse.motion) {
                // Ignore keys if it's not a release event, or it's a motion event
				ch = '';
			} else {
				switch(mk.mouse.button) {
					case 0:
                        // Left click on colour or row should set colour or move position
                        if (handle_colour_click(mk.mouse.x, mk.mouse.y)) {
                            // Clicking a colour should set a piece of that colour in the current position
                            // handle_colour_click will set the new colour, so we just need to fake an ENTER keypress to place it
                            ch = '\r';
                        } else if (handle_board_click(mk.mouse.x, mk.mouse.y)) {
                            // Clicking the board should move to the new position
                            // handle_board_click will set the new position, so nothing to do here
                        }
						break;
					case 1:
                    case 2:
                        // Middle or right click should submit the guess, so we fake a SPACE keypress to do that
                        ch = ' ';
                        break;
					default:
                        // Any other button should do nothing
						ch = '';
                        break;
				}
			}
		}

        // If game is over, only High Scores, New, and Quit are allowed
        if (game_over && (ch !== 'H') && (ch !== 'N') && (ch !== 'Q')) {
            continue;
        }

        switch (ch) {
            case '':
                // Must have a case for '' or ' is not a valid choice!' will be displayed
                break;

            case KEY_DOWN:
            case '2':
                move_colour(+1);
                break;

            case KEY_LEFT:
            case '4':
                move_piece(-1);
                break;

            case KEY_RIGHT:
            case '6':
                move_piece(+1);
                break;

            case KEY_UP:
            case '8':
                move_colour(-1);
                break;

            case 'H':
                mouse_enable(false);
                display_high_scores();
                redraw_screen();
                mouse_enable(true);
                break;

            case 'N':
                var saved_message = last_message;

                if (!game_over) { 
                    set_message('Are you sure you want to start a new game? [y/N] ');
                    ch = console.inkey(K_NONE, 5000);
                    if (ch.toUpperCase() === 'Y') {
                        ch = 'N'; // Restore the N for New Game
                    } else {
                        ch = '';
                        set_message(saved_message);
                    }
                }

                // If we still want to start a new game, prompt for level
                if (ch === 'N') {
                    set_message('What level?  1) Easy, 2) Normal, 3) Hard  [123] ');
                    ch = console.inkey(K_NONE, 10000);
                    switch (ch) {
                        case '1':
                        case '2':
                        case '3':
                            var level = parseInt(ch);
                            new_game(level);
                            set_message('New ' + level_names[level] + ' game started.  Good luck!');
                            break;

                        default:
                            set_message(saved_message);
                            break;
                    }
                }
            break;

            case 'Q':
                // Prompt to quit if in middle of game
                if (!game_over) {
                    var saved_message = last_message;
                    set_message('Are you sure you want to quit? [y/N] ');
                    ch = console.inkey(K_NONE, 5000);
                    if (ch.toUpperCase() === 'Y') {
                        ch = 'Q';
                    } else {
                        ch = '';
                        set_message(saved_message);
                    }
                }
                break; 

            case ' ':
                submit_guess();
                break;

            case '\r':
                place_piece();
                break;

            default:
                set_message(ch + ' Is Not A Valid Choice!');
                break;
        }
    } while (bbs.online && (ch !== 'Q'));

    mouse_enable(false);
    display_high_scores();
}

function mouse_enable(enable) {
	const mouse_passthru = CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_REL_PASSTHRU;
	if (enable) {
		console.status |= mouse_passthru;
    } else {
		console.status &= ~mouse_passthru;
    }
}

// Move the current colour up or down (negative = up, positive = down)
function move_colour(offset) {
    var max_colour = game.level === 3 ? 6 : 5;

    draw_colour(false);
    current_colour = current_colour + offset;
    if (current_colour > max_colour) {
        current_colour = 0;
    }
    if (current_colour < 0) {
        current_colour = max_colour;
    }
    draw_colour(true);
}

// Move the current piece left or right (negative = left, positive = right)
function move_piece(offset) {
    draw_piece(false);
    current_column = current_column + offset;
    if (current_column > 3) {
        current_column = 0;
    }
    if (current_column < 0) {
        current_column = 3;
    }
    draw_piece(true);
}

function new_game(level) {
    current_colour = 0;
    current_column = 0;
    game.level = level;
    game.row = 0;
    game.start = time();
    game_over = false;
    guesses = [];
    for (var i = 0; i < 10; i++) {
        guesses[i] = { peg: [null, null, null, null], piece: [null, null, null, null] };
    }

    redraw_screen();
    generate_answer(level);
    if (debug && (level > 0)) {
        draw_answer();
    }
}

// Place the currently selected colour at the current place on the board
function place_piece() {
    guesses[game.row].piece[current_column] = current_colour;
    move_piece(+1);
}

function redraw_guesses() {
    for (var y = 0; y < 10; y++) {
        for (var x = 0; x < 4; x++) {
            // Draw piece
            console.gotoxy(piece_origin.x + (piece_offset_x * current_column), piece_origin.y - (row_offset_y * y));
            console.attributes = piece_colours[guesses[y].piece[x]] | BG_BROWN;
            if (guesses[y].piece[x] === null) {
                console.write('\xF9');
            } else {
                console.write('\xDB');
            }

            // Draw peg
            console.gotoxy(peg_origin.x + x, peg_origin.y - (row_offset_y * y));
            console.attributes = peg_colours[guesses[y].peg[x]] | BG_BROWN;
            if (guesses[y].peg[x] === null) {
                console.write('\xFA');
            } else {
                console.write('\xFE');
            }
        }
    }
}

function redraw_screen() {
    console.printfile(js.exec_dir + 'main.ans');
    add_hotspots();
    redraw_guesses();
    draw_piece(!game_over);
    draw_colour(!game_over);
    set_message(last_message);
}

function set_message(message) {
    last_message = message;

    message = ' ' + message;
    if (message.length > message_width) {
        message = message.substring(0, message_width);
    } else {
        while (message.length < message_width) {
            message += ' ';
        }
    }

    console.attributes = BLACK | BG_LIGHTGRAY;
    console.gotoxy(message_origin.x, message_origin.y);
    console.write(message);
    console.attributes = LIGHTGRAY;
}

// Submit the current line for validation/scoring
function submit_guess() {
    if (validate_guess()) {
        var temp_line = guesses[game.row];
        var black = get_black_pegs(temp_line);
        var white = get_white_pegs(temp_line, black);

        guesses[game.row].peg = temp_line.peg;
        draw_pegs();
        
        if (!check_won()) {
            check_lost();
        }

        if (game_over) {
            draw_colour(false);
            draw_piece(false);
        } else {
            set_message('You Scored ' + black.toString() + ' Black Peg' + (black === 1 ? '' : 's') + ' and ' + white.toString() + ' White Peg' + (white === 1 ? '' : 's'));
            draw_piece(false);
            current_column = 0;
            game.row++;
            draw_piece(true);
        }
    }
}

// Check to see that the current guess is valid
// It isn't if there are duplicate or blank pieces
function validate_guess() {
    for (var i = 0; i < 4; i++) {
        // Ensure four colours were selected
        if (guesses[game.row].piece[i] === null) {
            set_message('You Must Pick Four Colours');
            return false;
        }

        // For level 1, ensure four unique colours were selected
        if (game.level === 1) {
            for (var j = 0; j < 4; j++) {
                if ((guesses[game.row].piece[i] === guesses[game.row].piece[j]) && (i !== j)) {
                    set_message('You Must Pick Four Unique Colours');
                    return false;
                }
            }
        }
    }
    return true;
}

// Start of program
try {
    main();
} catch (e) {
    var msg = file_getname(e.fileName) + ' line ' + e.lineNumber + ': ' + e.message;
    if (js.global.console) {
        console.crlf();
    }
    alert(msg);
    log(LOG_ERR, msg);

    if (data_sub && (user.alias != author)) {
		var msgbase = new MsgBase(data_sub);
		var hdr = { 
			to: author,
			from: user.alias || system.operator,
			subject: program_name + ' v' + program_version,
		};
		msg += tear_line;
		if(!msgbase.save_msg(hdr, msg)) {
			msg = 'Error saving exception-message to: ' + data_sub;
            alert(msg);
            log(LOG_ERR, msg);
        }
		msgbase.close();
	}    
}
