"use strict";

require("sbbsdefs.js", "K_NONE");
require("mouse_getkey.js", "mouse_getkey");

const debug = true;
const peg_colours = [DARKGRAY, LIGHTBLUE, WHITE];
const piece_colours = [DARKGRAY, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE];
const program_version = 'Mastermind v23.09.12';

var answer;
var current_colour;
var current_column;
var current_row;
var game_over;
var guesses;
var start_time;

if (js.global.bbs === undefined)
	json_lines = load({}, "json_lines.js");
else {
	var json_lines = bbs.mods.json_lines;
	if (!json_lines) {
		json_lines = load(bbs.mods.json_lines = {}, "json_lines.js");
    }
}

// Check to see if we ran out of guesses
function check_lost() {
    if (current_row === 9) {
        game_over = true;
        set_message('Game Over, You Lose!');
        draw_answer();
    }
}

// Check to see if all four pegs are black
function check_won() {
    for (var i = 0; i < 4; i++) {
        if (guesses[current_row].peg[i] !== 1) {
            return false;
        }
    }

    game_over = true;
    set_message('Congratulations, You Win!');
    draw_answer();

    // Save the high score
    var end_time = time()
    var duration = end_time - start_time;

    // Save high score
    if (!debug) {
        // TODOX
        //using (RMSQLiteConnection DB = new RMSQLiteConnection("HighScores.sqlite", false)) {
        //    string SQL = "";
        //    SQL = "INSERT INTO HighScores (PlayerName, Seconds, RecordDate) VALUES (";
        //    SQL += DB.AddVarCharParameter(Door.DropInfo.Alias) + ", ";
        //    SQL += DB.AddIntParameter((int)TS.TotalSeconds) + ", ";
        //    SQL += DB.AddDateTimeParameter(StartTime) + ") ";
        //    DB.ExecuteNonQuery(SQL);
        //}
    }

    return true;
}

function display_high_scores() {
    // TODOX Display the high scores
}

// Draw the answer line
function draw_answer() {
    for (var i = 0; i < 4; i++) {
        console.gotoxy(4 + (i * 4), 2);
        console.attributes = piece_colours[answer[i]];
        console.write('\xDB\xDB');
    }
}

// Highlight the currently selected colour
function draw_colour(highlight) {
    console.gotoxy(32, 8 + current_colour);
    console.attributes = piece_colours[current_colour];
    if (highlight) {
        console.attributes |= BG_LIGHTGRAY;
    }
    console.write('\xFE\xFE');
}

// Draw the pegs for the current line }
function draw_pegs() {
    for (var i = 0; i < 4; i++) {
        console.gotoxy(14 + i, 22 - (current_row * 2));
        console.attributes = peg_colours[guesses[current_row].peg[i]];
        console.write('\xFE');
    }
}

// Highlight the currently selected piece
function draw_piece(highlight) {
    console.gotoxy(3 + (current_column * 2), 22 - (current_row * 2));
    console.attributes = highlight ? LIGHTGRAY : BLACK;
    console.write(highlight ? '[ ]' : '   ');
    console.gotoxy(4 + (current_column * 2), 22 - (current_row * 2));
    console.attributes = piece_colours[guesses[current_row].piece[current_column]];
    console.write('\xDB');
}

// Generate a random answer
function generate_answer() {
    var available_colours = [ 0, 1, 2, 3, 4, 5, 6 ];

    // Pick four unique colours (no repeats)
    answer = [];
    for (var i = 0; i < 4; i++) {
        do {
            answer[i] = random(available_colours.length);
        } while (available_colours[answer[i]] === 0);
        
        available_colours[answer[i]] = 0;
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

function handle_board_click(x, y) {
    // First/bottom row is 4x22, 6x22, 8x22, 10x22
    // Then each subsequent row is two rows up

    // Confirm x and y coordinates correspond to a board position on the current row
    var new_column = [4, 6, 8, 10].indexOf(x);
    if (new_column === -1) {
        return false;
    }
    if (y !== 22 - (current_row * 2)) {
        return false;
    }

    if (new_column !== current_column) {
        move_piece(new_column - current_column);
    }

    return true;
}

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

function main() {
    new_game();
    set_message('Welcome to ' + program_version);

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

        // If game is over, only New and Quit are allowed
        if (game_over && (ch !== 'N') && (ch !== 'Q')) {
            continue;
        }

        switch (ch) {
            case '':
                // Must have a case for '' or " is not a valid choice!" will be displayed
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

            case 'A':
                if (debug) {
                    draw_answer();
                    set_message('Cheater!!!');
                }
                break;

            case 'H':
                mouse_enable(false);
                display_high_scores();
                redraw_screen();
                mouse_enable(true);
                break;

            case 'N':
                if (game_over) { 
                    new_game(); 
                } else { 
                    set_message('Finish This Game First!'); 
                }
                break;

            case 'Q':
                // Must have a case for 'Q' or "Q is not a valid choice!" will be displayed
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

function mouse_enable(enable)
{
	const mouse_passthru = CON_MOUSE_CLK_PASSTHRU | CON_MOUSE_REL_PASSTHRU;
	if (enable) {
		console.status |= mouse_passthru;
    } else {
		console.status &= ~mouse_passthru;
    }
}

// Move the current colour up or down (negative = up, positive = down)
function move_colour(offset) {
    draw_colour(false);
    current_colour = current_colour + offset;
    if (current_colour > 6) {
        current_colour = 1;
    }
    if (current_colour < 1) {
        current_colour = 6;
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

function new_game() {
    current_colour = 1;
    current_column = 0;
    current_row = 0;
    game_over = false;
    guesses = [];
    for (var i = 0; i < 10; i++) {
        guesses[i] = { peg: [0, 0, 0, 0], piece: [0, 0, 0, 0] };
    }
    start_time = time();

    redraw_screen();
    generate_answer();
}

// Place the currently selected colour at the current place on the board
function place_piece() {
    guesses[current_row].piece[current_column] = current_colour;
    move_piece(+1);
}

function redraw_guesses() {
    for (var y = 0; y < 10; y++) {
        for (var x = 0; x < 4; x++) {
            console.gotoxy(4 + (x * 2), 22 - (y * 2));
            console.attributes = piece_colours[guesses[y].piece[x]];
            console.write('\xDB');
            console.gotoxy(14 + x, 22 - (y * 2));
            console.attributes = peg_colours[guesses[y].peg[x]];
            console.write('\xFE');
        }
    }
}

function redraw_screen() {
    console.printfile(js.exec_dir + 'main.ans');
    redraw_guesses();
    draw_piece(true);
    draw_colour(true);
}

function set_message(message) {
    if (message.length > 54) {
        message = message.substring(0, 54);
    }

    console.attributes = BG_BLUE | WHITE;
    console.gotoxy(26, 22);
    console.write('                                                      ');
    console.gotoxy(26, 22);
    console.write(message);
}

// Submit the current line for validation/scoring
function submit_guess() {
    if (validate_guess()) {
        var temp_line = guesses[current_row];
        var black = get_black_pegs(temp_line);
        var white = get_white_pegs(temp_line, black);

        guesses[current_row].peg = temp_line.peg;
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
            current_row++;
            draw_piece(true);
        }
    }
}

// Check to see that the current guess is valid
// It isn't if there are duplicate or blank pieces
function validate_guess() {
    for (var i = 0; i < 4; i++) {
        if (guesses[current_row].piece[i] === 0) {
            set_message('You Must Pick Four Colours');
            return false;
        }
        for (var j = 0; j < 4; j++) {
            if ((guesses[current_row].piece[i] === guesses[current_row].piece[j]) && (i !== j)) {
                set_message('You Must Pick Four Unique Colours');
                return false;
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
}
