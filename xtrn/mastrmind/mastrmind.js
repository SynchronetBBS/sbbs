// Add to credits portion:
//  - Minesweeper (lots of code copied from it)
//  - http://patorjk.com/software/taag/#p=display&f=ANSI%20Shadow&t=Mastrmind (for the fonts)
//  - http://codelobe.com/tools/cp437-converter (for converting the fonts from unicode to cp437)
// TODO:
//  - Initial message saying Press N to begin should be clickable
'use strict';

require('sbbsdefs.js', 'K_NONE');
require('mouse_getkey.js', 'mouse_getkey');

const answer_offset = { x: 0, y: 3 };
const answer_origin = { x: 70, y: 9 };
const author = 'Ree';
const colour_offset = { x: 0, y: 2 };
const colour_origin = { x: 3, y: 9 };
const colour_width = 2;
const debug = true;
const level_names = ['Unknown', 'Easy', 'Normal', 'Hard'];
const max_guesses = 12;
const message_origin = { x: 7, y: 7 };
const message_width = 70;
const peg_colours = [BLACK, BLACK, WHITE];
const peg_offset = { x: 5, y: 0 };
const peg_origin = { x: 9, y: 21 };
const piece_colours = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, DARKGRAY, WHITE, LIGHTMAGENTA];
const piece_names = ['Red', 'Yellow', 'Green', 'Blue', 'Black', 'White', 'Magenta'];
const piece_offset = { x: 0, y: 3 };
const piece_origin = { x: 9, y: 9 };   
const program_name = 'Mastrmind';
const program_version = '23.09.26';
const row_offset_x = 5;
const tear_line = '\r\n--- ' + js.exec_file + ' v' + program_version + '\r\n';
const winner_subject = program_name + ' Winner' + (debug ? ' (debug mode)' : '');
const winner_to = js.exec_file;
const winners_list = js.exec_dir + 'winners' + (debug ? '-debug' : '') + '.jsonl';

var answer;
var current_colour;
var current_column;
var game = { version: program_version };
var game_over;
var guesses;
var last_message = '';

var data_sub = load({}, 'syncdata.js').find();

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
    console.add_hotspot('H', false, 1, 4, 23); // Help
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
        if (debug) {
            log(LOG_DEBUG, JSON.stringify(game));
        }
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
    game.md5 = undefined;
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
        console.attributes = piece_colours[answer[i]] | BG_BROWN;
        console.gotoxy(answer_origin.x + (answer_offset.x * i), answer_origin.y + (answer_offset.y * i));
        console.write(' \xDC\xDC ');
        console.gotoxy(answer_origin.x + (answer_offset.x * i), answer_origin.y + (answer_offset.y * i) + 1);
        console.write(' \xDF\xDF ');
    }
}

// Highlight the currently selected colour
function draw_colour(highlight) {
    console.attributes = piece_colours[current_colour] | (highlight ? BG_LIGHTGRAY : 0);
    console.gotoxy(colour_origin.x + (colour_offset.x * current_colour) - 1, colour_origin.y + (colour_offset.y * current_colour));
    console.write(' \xDC\xDC ');
    console.gotoxy(colour_origin.x + (colour_offset.x * current_colour) - 1, colour_origin.y + (colour_offset.y * current_colour) + 1);
    console.write(' \xDF\xDF ');
}

// Draw the pegs for the current line
function draw_pegs() {
    draw_pegs_at(game.row);
}

function draw_pegs_at(row) {
    for (var i = 0; i < 4; i++) {
        var x_offset = i % 2;
        var y_offset = Math.floor(i / 2);

        console.attributes = peg_colours[guesses[row].peg[i]] | BG_BROWN;
        console.gotoxy(peg_origin.x + x_offset + (peg_offset.x * row), peg_origin.y + y_offset + (peg_offset.y * row));
        if (guesses[row].peg[i] === null) {
            console.write('\xFA');
        } else {
            console.write('\xFE');
        }
    }
}

// Highlight the currently selected piece
function draw_piece(highlight) {
    draw_piece_at(current_column, game.row, highlight);
}

function draw_piece_at(column, row, highlight) {
    var isEmpty = guesses[row].piece[column] === null;
    console.attributes = (isEmpty ? BLACK : piece_colours[guesses[row].piece[column]]) | (highlight ? BG_LIGHTGRAY : BG_BROWN);
    console.gotoxy(piece_origin.x + (piece_offset.x * column) + (row * row_offset_x) - 1, piece_origin.y + (piece_offset.y * column));
    console.write(isEmpty ? ' \xC9\xBB ' : ' \xDC\xDC ');
    console.gotoxy(piece_origin.x + (piece_offset.x * column) + (row * row_offset_x) - 1, piece_origin.y + (piece_offset.y * column) + 1);
    console.write(isEmpty ? ' \xC8\xBC ' : ' \xDF\xDF ');
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

// Check temp_line against temp_answer, looking for right colour in right spot
// Returns the number of black/blue pegs that should be placed
function get_black_pegs(temp_line, temp_answer) {
    var result = 0;
    for (var i = 0; i < 4; i++) {
        if (temp_line.piece[i] === temp_answer[i]) {
            temp_line.peg[result++] = 1;
            temp_answer[i] = -1;
        }
    }
    return result;
}

// Check temp_line against temp_answer, looking for right colour in wrong spot
// Returns the number of white pegs that should be placed
function get_white_pegs(temp_line, temp_answer, black) {
    var result = 0;
    if (black < 4) {
        for (var i = 0; i < 4; i++) {
            for (var j = 0; j < 4; j++) {
                if ((temp_line.piece[i] === temp_answer[j]) && (i !== j)) {
                    temp_line.peg[black + result++] = 2;
                    temp_answer[j] = -1;
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

function handle_board_click(column, row) {
    // Confirm click was on current row
    var minX = piece_origin.y + (game.row * row_offset_x);
    var maxX = minX + 1;
    if ((column !== minX) && (column !== maxX)) {
        return false;
    }

    // Confirm click was on a piece position in the row
    var piece_columns = [];
    for (var i = 0; i < 4; i++) {
        piece_columns[i] = piece_origin.y + (piece_offset.y * i);
    }

    // Confirm click was in a piece column
    var new_column = piece_columns.indexOf(row);
    if (new_column === -1) {
        new_column = piece_columns.indexOf(row - 1);
        if (new_column === -1) {
            return false;
        }
    }

    if (new_column !== current_column) {
        move_piece(new_column - current_column);
    }

    return true;
}

function handle_colour_click(x, y) {
    // Easy check, must be on the right column to be a colour click
    if ((x !== colour_origin.x) && (x !== colour_origin.x + 1)) {
        return false;
    }

    // Harder check, must be on a colour row
    for (var i = 0; i < piece_colours.length; i++) {
        var colour_y1 = colour_origin.y + (colour_offset.y * i);
        var colour_y2 = colour_y1 + 1;
        if ((y >= colour_y1) && (y <= colour_y2)) {
            if ((i == piece_colours.length - 1) && game.level !== 3) {
                set_message(piece_names[i] + ' is only available in Hard mode');
                return false;
            } else {
                move_colour(i - current_colour);
                return true;
            }
        }
    }

    // Must not have been on a colour
    return false;
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
    game_over = true;
    
    mouse_enable(true);

    console.add_hotspot('N', false, 49, 65, 6);
    set_message('Welcome to ' + program_name + ' v' + program_version + '.  Press N to begin.');

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
                        if (!game_over) {
                            // Left click on colour or row should set colour or move position
                            if (handle_colour_click(mk.mouse.x, mk.mouse.y)) {
                                // Clicking a colour should set a piece of that colour in the current position
                                // handle_colour_click will set the new colour, so we just need to fake an ENTER keypress to place it
                                ch = '\r';
                            } else if (handle_board_click(mk.mouse.x, mk.mouse.y)) {
                                // Clicking the board should move to the new position
                                // handle_board_click will set the new position, so nothing to do here
                            }
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
                    console.clear_hotspots();
                    console.add_hotspot('Y', false, 58, 60, 6);
                    console.add_hotspot('N', false, 64, 65, 6);

                    set_message('Are you sure you want to start a new game? [ Yes / No ] ');
                    ch = mouse_getkeys('NY');
                    if (ch === 'Y') {
                        ch = 'N'; // Restore the N for New Game
                    } else {
                        ch = '';
                        set_message(saved_message);
                    }

                    add_hotspots();
                }

                // If we still want to start a new game, prompt for level
                if (ch === 'N') {
                    console.clear_hotspots();
                    console.add_hotspot('1', false, 29, 35, 6);
                    console.add_hotspot('2', false, 38, 46, 6);
                    console.add_hotspot('3', false, 49, 55, 6);
                    console.add_hotspot('A', false, 58, 65, 6);

                    set_message('What level?  1) Easy, 2) Normal, 3) Hard, A) Abort ');
                    ch = mouse_getkeys('123A');
                    switch (ch) {
                        case '1':
                        case '2':
                        case '3':
                            var level = parseInt(ch);
                            new_game(level);
                            set_message('New ' + level_names[level] + ' game started.  Good luck!');
                            last_message = ''; // We don't want to re-draw this message on a re-paint (ie if you hit Quit then No it would be weird to see "New game started" restored)
                            break;

                        default:
                            set_message(saved_message);
                            break;
                    }

                    add_hotspots();
                }
                break;

            case 'Q':
                // Prompt to quit if in middle of game
                if (!game_over) {
                    console.clear_hotspots();
                    console.add_hotspot('Y', false, 52, 54, 6);
                    console.add_hotspot('N', false, 58, 59, 6);

                    var saved_message = last_message;
                    set_message('Are you sure you want to quit? [ Yes / No ] ');
                    ch = mouse_getkeys('NY');
                    if (ch === 'Y') {
                        ch = 'Q';
                    } else {
                        ch = '';
                        set_message(saved_message);
                    }

                    add_hotspots();
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

function mouse_getkeys(keys) {
    console.clearkeybuffer();

    while (bbs.online) {
        var mk = mouse_getkey(K_NONE, 1000, true);
        var ch = mk.key.toUpperCase();
        if ((ch !== '') && (keys.indexOf(ch) !== -1)) {
            return ch;
        }
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
    for (var i = 0; i < max_guesses; i++) {
        guesses[i] = { peg: [null, null, null, null], piece: [null, null, null, null] };
    }

    generate_answer(level);
    redraw_screen();
}

// Place the currently selected colour at the current place on the board
function place_piece() {
    guesses[game.row].piece[current_column] = current_colour;
    move_piece(+1);
}

function redraw_guesses() {
    for (var row = 0; row < max_guesses; row++) {
        for (var column = 0; column < 4; column++) {
            // Draw piece
            draw_piece_at(column, row, false);
        }
        draw_pegs_at(row);
    }
}

function redraw_screen() {
    console.printfile(js.exec_dir + 'main.ans');
    add_hotspots();
    redraw_guesses();
    if (game.level > 0) {
        if (game_over || debug) {
            draw_answer();
        }
        if (!game_over) {
            draw_piece(true);
            draw_colour(true);
        }
    }
    set_message(last_message);
}

function set_message(message) {
    last_message = message;

    while (message.length < message_width) {
        message = ' ' + message + ' ';
    }

    if (message.length > message_width) {
        message = message.substring(0, message_width);
    }

    console.attributes = YELLOW;
    console.gotoxy(message_origin.x, message_origin.y);
    console.write(message);
    console.attributes = LIGHTGRAY;
}

// Submit the current line for validation/scoring
function submit_guess() {
    if (validate_guess()) {
        var temp_line = { piece: [null, null, null, null], peg: [null, null, null, null] };
        var temp_answer = [];
        for (var i = 0; i < 4; i++) {
            temp_line.piece[i] = guesses[game.row].piece[i];
            temp_answer[i] = answer[i];
        }
        var black = get_black_pegs(temp_line, temp_answer);
        if ((game.row === 0) && (black === 4)) {
            // Don't allow winning on the first row -- we want high scores with skill, not dumb luck
            generate_answer(game.level);
            if (debug) {
                draw_answer();
            }
            submit_guess();
            return;
        }
        var white = get_white_pegs(temp_line, temp_answer, black);

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
