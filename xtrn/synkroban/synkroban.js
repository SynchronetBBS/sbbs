/* Synkroban
 *
 * COPYRIGHT:
 * All sources from Synkroban are not to be modified or used in any way
 * without explicit consent from the author. You have consent from the 
 * author to run Synkroban as documented on your BBS.
 * The only exception to this restriction is that you may modify the
 * sources as appropriate for the purposes of configuration as part of
 * the documented setup process.
 *
 * AUTHOR: ART, at FATCATS BBS dot COM.
 */

load("frame.js");

var skb_def = {
	UP: 1,
	DOWN: 2,
	LEFT: 3,
	RIGHT: 4,
}

var skb_config = {
	colour_status_bg: BG_MAGENTA,
	colour_textpane_bg: BG_MAGENTA,
	JSON_PORT: 10088,
	JSON_SERVER: "romulusbbs.com",
	PATH_SYNKROBAN: "/sbbs/xtrn/synkroban/",
	USE_JSON: true,
	USE_GLOBAL_SERVER: false,
	level_set: "Arfonzo",	
}

var skb_state = {
	frame: new Frame(1, 1, console.screen_columns, console.screen_rows, BG_LIGHTGRAY),
	frame_items: undefined,
  frame_items_last: undefined,
	frame_status: undefined,
	frame_level: undefined,
	frame_player: undefined,
	frame_textpane: undefined,
	json: undefined,
	level: 1,	// Player's current level.
	level_description: new Array(),
	levels_raw: new Array(),
	popup_data: new Array(),
	push_lock: false,
	push_x_current: undefined,
	push_y_current: undefined,
	push_x_preserved: undefined,
	push_y_preserved: undefined,
	set_complete: false,
	steps: 0,
  undoable: false,
	x: 0,
	x_last: undefined,
	y: 0,
	y_last: undefined,
}

var skb_symbols = {
	WALL: '#',
	PLAYER: '@',
	PLAYER_GOAL: '+',
	BOX: '$',
	BOX_GOAL: '*',
	GOAL: '.',
	FLOOR: ' ',
}

// Object SkbLevel.
function SkbLevel() {
	this.frame = undefined;
}

function SkbLevelSet(level_set) {
	this.description = new Array();
	this.levels = new Array();

	this.init = function () {
		/* Load level file. */
		var filename = "/sbbs/xtrn/synkroban/levels/" + level_set + ".txt";
		var file = new File(filename);
		file.open("r");
		try {
			var c = file.readAll();
			file.close();
			//log("Level file read: " + c.length + " lines.");

			levels_parse(c, this);
		} catch (e) {
			log("Error loading level set file.");
			return;
		}
	}
	this.init();
}

// Boolean: has the level been completed?
function check_level_completed() {
	if (!skb_state.set_complete) {

		for (var a = 0; a < skb_state.frame_level.height; a++) {
			// Don't count comments.
			if (skb_state.frame_level.getData(0, a).ch == ';') {
				// Comment lines.
				//log("Ignoring commented line " + a);
			} else {
				for (var b = 0; b < skb_state.frame_level.width; b++) {
					if (skb_state.frame_level.getData(b, a).ch == '.') {
						//log("Goal found at " + (b + 1) + ", " + (a + 1));

						if (skb_state.frame_items.getData(b, a).ch != skb_symbols.BOX) {
							//log("Goal found at " + (b + 1) + ", " + (a + 1) + " with NO box on top.");
							return false;
						}
					}
				}
			}
		}

		textpane_update_steps();

		skb_state.popup_data = [
			"Level complete!",
			"Steps taken: " + skb_state.steps,
		];

		// Write score to JSON.
		if (skb_config.USE_JSON) {
			try {
				// Check if score is lower than last value.
				var overwrite_json = false;
				var json_step = skb_state.json.read("synkroban", "SCORES." + system.qwk_id + "." + user.alias + "." + skb_config.level_set + "." + skb_state.level, 1);
				var commentary = "";

				if (json_step == undefined) {
					// No previous score/some mumbo jumbo.
					overwrite_json = true;
					commentary = "This was your first try.";
				} else {
					log("I found in JSON: " + json_step);
					if (skb_state.steps < json_step) {
						// New better score.
						overwrite_json = true;
						commentary = "Your previous best was " + json_step + " steps.";
					}

				}

				if (overwrite_json) {
					skb_state.json.write("synkroban", "SCORES." + system.qwk_id + "." + user.alias + "." + skb_config.level_set + "." + skb_state.level, skb_state.steps, 2);
					skb_state.popup_data.push("Submitted to global scoreboard. " + commentary);
				} else {
					commentary = "Not submitted to global scoreboard: that doesn't beat your previous best of " + json_step + " steps."
					skb_state.popup_data.push(commentary);
				}
			} catch (e) {
				skb_state.popup_data.push("Error: could not submit score on global scoreboard - " + e);
			}
		}

		popup();

		log("Level completed!");
		return true;
	}
}

// Check x,y is moveable.
function check_moveable(x, y) {
	//log("check_moveable()");

	var moveable = true;

	// Check the player frame.
	if (x >= skb_state.frame_player.width || y >= skb_state.frame_player.height || x < 0 || y < 0) {
		//log("check_moveable() Nae: that would put me outside frame_player.");
		moveable = false;
	}

	// Check the level frame.
	//log("checking level frame");
	if (!is_walkable(x, y, skb_state.frame_level.getData(x, y).ch)) {
		moveable = false;
	}

	// Check the items frame.
	//log("checking items frame");
	if (!is_walkable(x, y, skb_state.frame_items.getData(x, y).ch)) {
		moveable = false;
	}

	//log("returning: " + moveable);
	return moveable;
}

function get_level_name(filepath) {
	return file_getname(filepath).replace(file_getext(filepath), '');
}

function init() {
	log("Synkroban started.");

	skb_state.frame_status = new Frame(1, 1, console.screen_columns, 1, BG_MAGENTA, skb_state.frame);
	skb_state.frame_level = new Frame(1, 2, console.screen_columns - 20, console.screen_rows - 1, BG_LIGHTGRAY, skb_state.frame);
	skb_state.frame_items = new Frame(1, 2, console.screen_columns - 20, console.screen_rows - 1, BG_LIGHTGRAY, skb_state.frame);
	skb_state.frame_items.transparent = true;
	skb_state.frame_player = new Frame(1, 2, console.screen_columns - 20, console.screen_rows - 1, WHITE | BG_LIGHTGRAY, skb_state.frame);
	skb_state.frame_player.transparent = true;
	skb_state.frame_textpane = new Frame(console.screen_columns - 19, 2, 20, console.screen_rows - 1, BG_MAGENTA, skb_state.frame);

	//skb_state.frame_status.putmsg(" Welcome to Synkroban, by art@FATCATS");
	set_default_status();

	/* JSON initialize client connection to server. */
	if (skb_config.USE_JSON) {
		load("json-client.js");
		
		try {
			if (!skb_config.USE_GLOBAL_SERVER) {
				var server_file = new File(file_cfgname(skb_config.PATH_SYNKROBAN, "server.ini"));
				server_file.open('r',true);
				skb_config.JSON_SERVER = server_file.iniGetValue(null,"host","localhost");
				skb_config.JSON_PORT = server_file.iniGetValue(null,"port",10088);
				server_file.close();
			}

			/* JSON client object. */
			skb_state.json = new JSONClient(skb_config.JSON_SERVER, skb_config.JSON_PORT);
		} catch (e) {
			console.writeln("Couldn't connect to JSON service: " + e.message);
			console.pause();
		}

	}

	/* Load level. */
	levels_load();
	level_load(skb_state.level);

	// Draw initial screen.
	skb_state.frame.draw();
}

// Return true if char c is a walkable tile.
function is_walkable(x, y, c) {
	//log("is_walkable(" + c.toSource() + ")");
	switch (c) {
		case '$':
		case '*':
			push_box(x, y);
			break;

		case '#':		
			return false;
			break;

		default:
			return true;
			break;
	}
}

function level_load(lvl) {
	//log("level_load(" + lvl + ")");

	//log("Loading raw data: " + skb_state.levels_raw[level - 1]);

	var level = skb_state.levels_raw[lvl - 1];

	// Draw the frame.
	skb_state.frame_level.clear();
	level_parse(level);

	skb_state.steps = 0;

	/* Load player. */
	player_load();

	skb_state.frame_textpane.clear();
	skb_state.frame_textpane.putmsg(" Level: " + skb_state.level + '/' + skb_state.levels_raw.length, WHITE | BG_MAGENTA);	

	// Check previous best and display if found.
	if (skb_config.USE_JSON) {
		try {
			var json_step = skb_state.json.read("synkroban", "SCORES." + system.qwk_id + "." + user.alias + "." + skb_config.level_set + "." + lvl, 1);

			if (json_step) {
				skb_state.frame_textpane.gotoxy(1, 3);
				skb_state.frame_textpane.putmsg(" Best: " + json_step, LIGHTGRAY | BG_MAGENTA);
			}
		} catch (e) {
			log("Exception: " + e);
		}
	}	

	skb_state.frame_textpane.crlf(); skb_state.frame_textpane.crlf();
	skb_state.frame_textpane.putmsg(skb_state.level_description, HIGH | MAGENTA | BG_MAGENTA);

	set_default_status();
}

function level_parse(raw) {
	/*	
	Level element			Char	ASCII Code
	Wall					#		0x23
	Player					@		0x40
	Player on goal square	+		0x2b
	Box						$		0x24
	Box on goal square		*		0x2a
	Goal square				.		0x2e
	Floor					(Space)	0x20
	*/

	// Clear old frames.
	skb_state.frame_level.clear();
	skb_state.frame_items.clear();
	skb_state.frame_player.clear();
	skb_state.frame_textpane.clear();

	for (var y = 0; y < raw.length; y++) {
		for (var x = 0; x < raw[y].length; x++) {
			switch (raw[y][x]) {
				case '@':
					// Player on floor.

					// Put a floor on the level frame.
					skb_state.frame_level.setData(x, y, skb_symbols.FLOOR);

					// Place the player.
					skb_state.x = x;
					skb_state.y = y;
					break;

				case '+':
					// Player on goal square.

					// Put a goal on the level frame.
					skb_state.frame_level.setData(x, y, skb_symbols.GOAL);

					// Place the player.
					skb_state.x = x;
					skb_state.y = y;

					break;

				case '$':
					// Box.

					// Put a floor on the level frame.
					skb_state.frame_level.setData(x, y, skb_symbols.FLOOR);
					
					// Put a box on the items frame.
					skb_state.frame_items.setData(x, y, skb_symbols.BOX, YELLOW | BG_LIGHTGRAY);
					break;

				case '*':
					// Box on goal square

					// Put a goal on the level frame.
					skb_state.frame_level.setData(x, y, skb_symbols.GOAL);

					// Put a box on the items frame.
					skb_state.frame_items.setData(x, y, skb_symbols.BOX, YELLOW | BG_GREEN);

					break;

				case '.':
				case ' ':
					skb_state.frame_level.setData(x, y, raw[y][x]);
					break;
				case '#':
					skb_state.frame_level.setData(x, y, skb_symbols.WALL, RED | BG_BROWN);
					break;
				default:
					skb_state.frame_level.setData(x, y, raw[y][x]);
					break;
			}


				
		}
	}
}

function levels_load() {
	skb_state.level_description = new Array();
	var level_set = new SkbLevelSet(skb_config.level_set);
}

function levels_parse(str_arr, levelset) {
	//log("levels_parse(): " + str_arr.toSource());

	skb_state.levels_raw = new Array();

	skb_state.set_complete = false;

	set_default_status();

	var level_raw = new Array();

	var is_header = true;

	for (var a = 0; a < str_arr.length; a++) {

		if (str_arr[a].trim() == "" || (a == str_arr.length - 1) ) {
			// End of level.

			//log("I got this level: " + level_raw.toSource());

			// Ignore header information.
			//	- Do all the rows start with ';'? If so, ignore this block.
			var all_comments = true;

			for (var b = 0; b < level_raw.length; b++) {
				if (level_raw[b].trim()[0] != ';') {
					all_comments = false;
				}
			}

			is_header = all_comments;

			if (is_header) {
				
				for (var z = 0; z < level_raw.length; z++) {
					// - Strip the leading comment ;
					level_raw[z] = level_raw[z].replace(/\;/g, "").trim();
				}

				skb_state.level_description += "\r\n" + level_raw.join("\r\n");
				//log("level desc: " + skb_state.level_description);
			} else {
				// Add raw level to the array.
				skb_state.levels_raw.push(level_raw);
			}

			level_raw = new Array();

		} else {
			level_raw.push(str_arr[a]);
		}
	}

	//log("Loaded " + skb_state.levels_raw.length + " raw levels into skb_state.");
}

function main() {
	var is_playing = true;

	while (is_playing) {
		textpane_update_steps();

		skb_state.frame.cycle();
		console.gotoxy(console.screen_columns, console.screen_rows);

		var c = console.inkey(250);

		switch (c) {
			case KEY_DOWN:
				player_move(skb_def.DOWN);
				break;

			case KEY_UP:
				player_move(skb_def.UP);
				break;

			case KEY_LEFT:
				player_move(skb_def.LEFT);
				break;

			case KEY_RIGHT:
				player_move(skb_def.RIGHT);
				break;

			case "l":
			case "L":
				pick_level();
				break;

			case "P":
			case "p":
				pick_level_set();
				break;

			case "q":
			case "Q":
				is_playing = false;
				break;

			case "R":
				level_load(skb_state.level);
				break;

		  case "U":
		    // Undo one previous step.
		    undo();
		    break;

			case "$":
				show_scores();
				break;

			case "?":
				show_help();
				break;
		}

	}

}

function next_level() {
	if (skb_state.level == skb_state.levels_raw.length) {
		//log("Error: no more levels.");
		skb_state.frame_status.clear();
		skb_state.frame_status.putmsg(" Set complete! Why not 'P'ick a new set/another 'L'evel?", YELLOW | BG_MAGENTA);
		skb_state.set_complete = true;
	} else {
		skb_state.level++;
		log("Loading level " + skb_state.level);
		level_load(skb_state.level);
	}
}

function pick_level() {
	// Let the user pick any level in the set.
	var frame_chooser = new Frame(Number(skb_state.frame_level.width / 2) + 1, 2, Number(skb_state.frame_level.width / 2), skb_state.frame_level.height, LIGHTBLUE | BG_BLUE, skb_state.frame);

	frame_chooser.putmsg(" Pick a level (1 to " + skb_state.levels_raw.length + "): ");

	frame_chooser.open();
	frame_chooser.draw();

	var tmp_last_level = skb_state.level;

	var is_inputting = true;

	var lvl = "";

	while (is_inputting) {
		frame_chooser.cycle();
		var clvl = console.inkey(250, K_NOECHO);

		switch (clvl) {
			case "\r":
				is_inputting = false;
				break;

			default:
				lvl += clvl;
				frame_chooser.putmsg(clvl, WHITE | BG_BLUE);
				break;
		}
	}

	lvl = Number(lvl);

	if (lvl != NaN && 0 < lvl && lvl <= skb_state.levels_raw.length) {

		try {
			skb_state.level = lvl;
			level_load(skb_state.level);

			skb_state.frame.draw();

		} catch (e) {
			log("Exception loading level: " + e);
			frame_chooser.crlf(); frame_chooser.crlf();
			frame_chooser.putmsg(" Couldn't load level: reloading the previous one. Darkest condolences. :|", LIGHTRED | BG_BLUE);
			set_pause_status();

			skb_state.frame.draw();

			console.getkey();

			skb_state.level = tmp_last_level;
			level_load(skb_state.level);
		}
	} else {
		frame_chooser.crlf(); frame_chooser.crlf();
		frame_chooser.putmsg(" That was not a valid level, my dear puke. >:C", LIGHTRED | BG_BLUE);
		set_pause_status();
		skb_state.frame.draw();

		console.getkey();
	}

	frame_chooser.delete();

	set_default_status();
}

function pick_level_set() {
	try {
		var dir = directory("/sbbs/xtrn/synkroban/levels/*.txt");	

		var frame_popup = new Frame(Number(skb_state.frame_level.width / 2) + 1, 2, Number(skb_state.frame_level.width / 2), 3, HIGH | BLUE | BG_BLUE, skb_state.frame);
		frame_popup.v_scroll = true;

		frame_popup.open();
		frame_popup.top();
		frame_popup.draw();

		set_status("Up/Down to select. RETURN to pick.");

		var is_picking = true;
		var a = 0;
		// TODO: Match index a to current set.

		var redraw = true;

		while (is_picking) {
			frame_popup.cycle();			

			if (redraw) {
				frame_popup.clear();

				// Item before.
				if (a >= 1) {
					frame_popup.center(" " + get_level_name(dir[a - 1]), BG_BLUE | DARKGRAY);
					frame_popup.crlf();
					
				} else {
					// Display last item.
					frame_popup.center(" " + get_level_name(dir[dir.length - 1]), BG_BLUE | DARKGRAY);
					frame_popup.crlf();
				}

				frame_popup.center(" " + get_level_name(dir[a]));
				frame_popup.crlf();

				// Item after.
				if (a < dir.length - 1) {
					frame_popup.center(" " + get_level_name(dir[a + 1]), BG_BLUE | DARKGRAY);
					frame_popup.crlf();
				} else {
					// Display first item.
					frame_popup.center(" " + get_level_name(dir[0]), BG_BLUE | DARKGRAY);
					frame_popup.crlf();
				}

				console.gotoxy(console.screen_columns, console.screen_rows);

				redraw = false;
			}

			var c = console.inkey(250);

			switch (c.toLowerCase()) {
				case KEY_UP:
					if (a == 0) {
						a = dir.length - 1;
					} else {
						a--;
					}
					redraw = true;
					break;

				case KEY_DOWN:
					if (a == (dir.length - 1)) {
						a = 0;
					} else {
						a++;
					}
					redraw = true;
					break;

				case "q":
					is_picking = false;					
					break;

				case "\r":
					is_picking = false;
					break;

				default:
					redraw = false;
					break;
			}
		}

		var levelset_all_good = false;

		if (a != NaN) {
			levelset_all_good = true;
		}

		frame_popup.clear();
		frame_popup.center("Loading Level Set:", WHITE | BG_BLUE);
		frame_popup.crlf();
		frame_popup.center(get_level_name(dir[a]), YELLOW | BG_BLUE);
		frame_popup.crlf();

		frame_popup.draw();

		if (levelset_all_good) {
			skb_state.frame.clear();
			skb_config.level_set = get_level_name(dir[a]);

			skb_state.level = 1;

			levels_load();
			level_load(skb_state.level);

			frame_popup.center("Loaded successfully.", LIGHTGREEN | BG_BLUE);
		}
		

	} catch (e) {
		log("pick_level_set() exception: " + e);
		frame_popup.center("Error loading.", LIGHTRED| BG_BLUE);
	}

	set_pause_status();

	skb_state.frame.invalidate();
	skb_state.frame.draw();

	console.home();
	console.getkey();
	frame_popup.delete();

	skb_state.frame.invalidate();

	set_default_status();

	
}

function player_load() {
	skb_state.frame_player.clear();
	skb_state.frame_player.setData(skb_state.x, skb_state.y, '@');
}

function player_move(direction) {
	var x_proposed = skb_state.x;
	var y_proposed = skb_state.y;

	switch (direction) {
		case skb_def.DOWN:
			y_proposed++;
			break;

		case skb_def.UP:
			y_proposed--;
			break;

		case skb_def.LEFT:
			x_proposed--;
			break;

		case skb_def.RIGHT:
			x_proposed++;
			break;
	}

	if (check_moveable(x_proposed, y_proposed)) {
		skb_state.x = x_proposed;
		skb_state.y = y_proposed;

		skb_state.frame_player.clear();
		skb_state.frame_player.setData(skb_state.x, skb_state.y, '@');

		skb_state.steps++;

		// Check win state.
		if (check_level_completed()) {
			next_level();
		}
	}
}

function player_move_to(x, y) {
	skb_state.x = x;
	skb_state.y = y;

	skb_state.frame_player.clear();
	skb_state.frame_player.setData(skb_state.x, skb_state.y, '@');

	skb_state.steps++;

	// Check win state.
	if (check_level_completed()) {
		next_level();
	}

	//log("player_move_to(): check undoable: " + skb_state.undoable);
}

function popup() {
	var popup_frame = new Frame(1, 2, skb_state.frame_level.width, Number(skb_state.frame_level.height/2), LIGHTGREEN | BG_GREEN, skb_state.frame);

	for (var a = 0; a < skb_state.popup_data.length; a++) {
		popup_frame.putmsg(skb_state.popup_data[a]);
		popup_frame.crlf();
	}

	set_pause_status();

	popup_frame.draw();

	var hasinput = false;

	while (!hasinput) {
		var c = console.inkey(1000);

		if (c != "") {
			hasinput = true;
		}
	}	

	// Clear popup state data.
	skb_state.popup_data = new Array();

	popup_frame.delete();

	set_default_status();

}

function push_box(x, y) {

	if (!skb_state.push_lock) {
		skb_state.push_lock = true;
		var x_diff = -(skb_state.x - x);
		var y_diff = -(skb_state.y - y);

		var x_proposed = x + x_diff;
		var y_proposed = y + y_diff;


		// Check diff is moveable.
		if (check_moveable(x_proposed, y_proposed)) {
		  //
		  // Push the box.
		  //

		  // Preserve undo state.
		  undo_preserve(x,y, x_proposed, y_proposed);

			// Remove item from old location.
			skb_state.frame_items.clearData(x, y);

			// Add to new location.		

			// Check if new spot is a goal, and colour.
			if (skb_state.frame_level.getData(x_proposed, y_proposed).ch == '.')
				skb_state.frame_items.setData(x_proposed, y_proposed, skb_symbols.BOX, YELLOW | BG_GREEN);
			else
				skb_state.frame_items.setData(x_proposed, y_proposed, skb_symbols.BOX, YELLOW | BG_LIGHTGRAY);

			// Move the player.
			player_move_to(x, y);

		} /*else {
			log("Can't push: something's already there.");
		}*/

		skb_state.push_lock = false;
	}

	//log("end push_box()");
}

function set_default_status() {
	skb_state.frame_status.clear();
	skb_state.frame_status.putmsg(" Welcome to Synkroban");

	if (skb_config.USE_JSON) {
		skb_state.frame_status.putmsg(" (InterBBS mode)");
	}

	skb_state.frame_status.putmsg("! Press ");
	skb_state.frame_status.putmsg("?", YELLOW | BG_MAGENTA);
	skb_state.frame_status.putmsg(" for help.");
	skb_state.frame_status.putmsg(" By ");
	skb_state.frame_status.putmsg("art", LIGHTGREEN | BG_MAGENTA);
	skb_state.frame_status.putmsg("@FATCATS", GREEN | BG_MAGENTA);
}

function set_pause_status() {
	skb_state.frame_status.clear();
	skb_state.frame_status.center("Press any key to continue.", YELLOW | BG_MAGENTA);
}

function set_status(msg, attr) {
	skb_state.frame_status.clear();
	skb_state.frame_status.center(msg, attr);
}

function show_help() {
	var frame_help = new Frame(1, 2, console.screen_columns - 20, console.screen_rows - 1, YELLOW | BG_BROWN, skb_state.frame);

	frame_help.putmsg("Synkroban Help", WHITE | BG_BROWN);
	frame_help.crlf();
	frame_help.putmsg("  Commands are case-insensitive except where denoted.");
	frame_help.crlf(); frame_help.crlf();
	frame_help.putmsg("  ?    This help.");
	frame_help.crlf();

	if (skb_config.USE_JSON) {
		frame_help.putmsg("  $    InterBBS scores.");
		frame_help.crlf();
	}

	frame_help.putmsg("  l    Pick a level to play.");
	frame_help.crlf();
	frame_help.putmsg("  p    Pick a level set to play.");
	frame_help.crlf();
	frame_help.putmsg("  q    Quit.");
	frame_help.crlf();
	frame_help.putmsg("  R    Restart level (case-sensitive).");
	frame_help.crlf();
	frame_help.putmsg("  U    Undo last box push (step penalty, case-sensitive).");
	frame_help.crlf();

	frame_help.crlf();
	frame_help.putmsg("Legend", WHITE | BG_BROWN);
	frame_help.crlf();
	frame_help.putmsg("  @    You.", BLACK | BG_BROWN);
	frame_help.crlf();
	frame_help.putmsg("  $    Package.", BLACK | BG_BROWN);
	frame_help.crlf();
	frame_help.putmsg("  .    Goal.", BLACK | BG_BROWN);
	frame_help.crlf();
	frame_help.putmsg("  #    Wall.", BLACK | BG_BROWN);

	frame_help.crlf(); frame_help.crlf();
	frame_help.putmsg("Movement", WHITE | BG_BROWN);
	frame_help.crlf(); 
	frame_help.putmsg("  Use the arrow keys to move.");
	frame_help.crlf();
	frame_help.putmsg("  Push the packages onto the goals.");

	// Display frame.

	set_pause_status();

	frame_help.open();

	skb_state.frame.draw();

	console.getkey();
	frame_help.delete();

	set_default_status();

	skb_state.frame.invalidate();
}

function show_progress() {
}

function show_scores() {
	frame_scores = new Frame(1, 2, console.screen_columns - 20, console.screen_rows - 1, LIGHTCYAN | BG_CYAN, skb_state.frame);

	frame_scores.v_scroll = true;

	frame_scores.putmsg("\r\n");
	frame_scores.center("InterBBS Scores:", YELLOW | BG_CYAN);
	frame_scores.putmsg("\r\n");
	frame_scores.center("Lower scores are better. ", BLACK | BG_CYAN);
	frame_scores.putmsg("\r\n");
	frame_scores.putmsg("\r\n");

	var scores = skb_state.json.read("synkroban", "SCORES", 1);

	var bbses = Object.keys(scores);
	
	for (var b = 0; b < bbses.length; b++) {		

		var tmp_bbs = bbses[b];

		var tmp_players = Object.keys(scores[bbses[b]]);
		
		for (var u = 0; u < tmp_players.length; u++) {
			
			var tmp_player = scores[bbses[b]][tmp_players[u]];

			var tmp_sets = Object.keys(tmp_player);

			var has_scores = false;

			// Only check if the user has level scores in this set.
			for (var r = 0; r < tmp_sets.length; r++) {
				// The same Level Set as we're playing.
				if (tmp_sets[r] == skb_config.level_set) {
					has_scores = true;
				}
			}

			if (has_scores) {
				log("Player: " + tmp_players[u]);
				frame_scores.putmsg(tmp_players[u], BG_CYAN | WHITE);
				frame_scores.putmsg('@', BG_CYAN | BLACK);
				frame_scores.putmsg(tmp_bbs, BG_CYAN | DARKGRAY);
				frame_scores.putmsg(":\r\n");

				for (var s = 0; s < tmp_sets.length; s++) {

					// Only display the same Level Set as we're playing.
					if (tmp_sets[s] == skb_config.level_set) {
						frame_scores.putmsg("  Level set: " + tmp_sets[s]);
						frame_scores.putmsg("\r\n");

						var tmp_set = scores[bbses[b]][tmp_players[u]][tmp_sets[s]];

						var tmp_scores = Object.keys(tmp_set);

						for (var t = 0; t < tmp_scores.length; t++) {
							var tmp_score = scores[bbses[b]][tmp_players[u]][tmp_sets[s]][tmp_scores[t]];
							frame_scores.putmsg("    Level " + tmp_scores[t] + ": " + tmp_score + "\r\n", BG_CYAN | BLACK);
						}

						frame_scores.putmsg("\r\n");
					} // End if (level set matches).

				} // End for()

			} // End if

			
		}

		frame_scores.putmsg("\r\n");

	} // end for each bbs.

	// Display frame.
	set_status("Up/Down - scroll, CR/Q - quit", HIGH | GREEN | BG_MAGENTA);

	
	frame_scores.open();

	//Scroll frame.

	frame_scores.offset.y = 0;

	var is_scrolling = true;

	while (is_scrolling) {
		frame_scores.cycle();
		var cmd = console.inkey(1000);
		switch (cmd.toLowerCase()) {
			case KEY_DOWN:
				frame_scores.pagedown();
				break;
			case KEY_UP:
				frame_scores.pageup();
				break;
			case "q":
			case "\r":
				is_scrolling = false;
				break;
		} // end switch()
	} // end while()


	frame_scores.delete();
	set_default_status();

	skb_state.frame.invalidate();
}

function textpane_update_steps() {
	skb_state.frame_textpane.gotoxy(1, 2);
	skb_state.frame_textpane.putmsg(" Steps: " + skb_state.steps, WHITE | BG_MAGENTA);
}

function undo() {
  //
  // Undo the last move.
  //
  //log("Undoing...");
  if (skb_state.undoable) {
    //log("- Undoable.");

    // Player movement.
    player_move_to(skb_state.x_last, skb_state.y_last);

    // Items frame.
    skb_state.frame_items.clearData(skb_state.push_x_current, skb_state.push_y_current);
    if (skb_state.frame_level.getData(skb_state.push_x_preserved, skb_state.push_y_preserved).ch == '.')
      skb_state.frame_items.setData(skb_state.push_x_preserved, skb_state.push_y_preserved, skb_symbols.BOX, YELLOW | BG_GREEN);
    else
      skb_state.frame_items.setData(skb_state.push_x_preserved, skb_state.push_y_preserved, skb_symbols.BOX, YELLOW | BG_LIGHTGRAY);

    // We only allow a single step for undo. Less cheaty. :|
    skb_state.undoable = false;

    // Apply step penalty for undo.
    penalty = Math.max(1, Math.round((skb_state.steps - 1)*0.1));
    log("Penalty applied: +" + penalty);
    skb_state.steps += penalty;
  } else {
    //log("- Not undoable.");
  }
}

function undo_preserve(x_preserved, y_preserved, x_current, y_current) {
  //
  // Preserve last state for undo.
  //

  // Player.
  skb_state.x_last = skb_state.x;
  skb_state.y_last = skb_state.y;
  
  // Items.
  skb_state.push_x_preserved = x_preserved;
  skb_state.push_y_preserved = y_preserved;
  skb_state.push_x_current = x_current;
  skb_state.push_y_current = y_current;

  skb_state.undoable = true;
}

/*
 * Main program code.
 */
init();
main();
console.write(console.ansi(ANSI_NORMAL));
console.clear();
log("Synkroban ended.");
