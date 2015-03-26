// Dependencies
load('sbbsdefs.js');
load('frame.js');
load('tree.js');
load('event-timer.js');
load('json-client.js');
load('sprite.js');

// Lemons modules
load(js.exec_dir + "defs.js");
load(js.exec_dir + "game.js");
load(js.exec_dir + "level.js");
load(js.exec_dir + "menu.js");
load(js.exec_dir + "help.js");
load(js.exec_dir + "dbhelper.js");

var	status,		// A place to store bbs.sys_status until exit
	attributes;	// A place to store console attributes until exit

// The Lemons modules expect these globals:
var state, // What we're currently doing
	frame; // The parent frame

/*	Draw a border of colour 'color' inside of a frame.
	'color' can be a number or an array of colours. */
Frame.prototype.drawBorder = function(color) {
	var theColor = color;
	if(Array.isArray(color));
		var sectionLength = Math.round(this.width / color.length);
	this.pushxy();
	for(var y = 1; y <= this.height; y++) {
		for(var x = 1; x <= this.width; x++) {
			if(x > 1 && x < this.width && y > 1 && y < this.height)
				continue;
			var msg;
			this.gotoxy(x, y);
			if(y == 1 && x == 1)
				msg = ascii(218);
			else if(y == 1 && x == this.width)
				msg = ascii(191);
			else if(y == this.height && x == 1)
				msg = ascii(192);
			else if(y == this.height && x == this.width)
				msg = ascii(217);
			else if(x == 1 || x == this.width)
				msg = ascii(179);
			else
				msg = ascii(196);
			if(Array.isArray(color)) {
				if(x == 1)
					theColor = color[0];
				else if(x % sectionLength == 0 && x < this.width)
					theColor = color[x / sectionLength];
				else if(x == this.width)
					theColor = color[color.length - 1];
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

/*	Pop a message up near the centre of frame 'frame':
	var popUp = new PopUp(["Line 1", "Line 2" ...]);
	console.getkey();
	popUp.close(); */
var PopUp = function(message) {

	var longestLine = 0;
	for(var m = 0; m < message.length; m++) {
		if(message[m].length > longestLine)
			longestLine = message[m].length;
	}

	this.frame = new Frame(
		frame.x + Math.ceil((frame.width - longestLine - 4) / 2),
		frame.y + Math.ceil((frame.height - message.length - 2) / 2),
		longestLine + 4,
		message.length + 2,
		BG_BLACK|WHITE,
		frame
	);

	this.frame.open();
	this.frame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	this.frame.gotoxy(1, 2);
	for(var m = 0; m < message.length; m++)
		this.frame.center(message[m] + "\r\n");

	this.close = function() {
		this.frame.delete();
	}

}

// Prepare the display, set the initial state
var init = function() {
	
	status = bbs.sys_status; // We'll restore this on cleanup
	bbs.sys_status|=SS_MOFF; // Turn off node message delivery

	attributes = console.attributes; // We'll restore this on cleanup
	console.clear(BG_BLACK|WHITE); // Wipe away any poops
	
	// Set up the root frame
	frame = new Frame(
		Math.ceil((console.screen_columns - 79) / 2),
		Math.ceil((console.screen_rows - 23) / 2),
		80,
		24,
		BG_BLACK|LIGHTGRAY
	);
	frame.open();
	frame.checkbounds = false;

	// We start off at the menu screen
	state = STATE_MENU;

}

// Return the display & system status to normal
var cleanUp = function() {
	frame.delete();
	bbs.sys_status = status;
	console.clear(attributes);
}

// Get input from the user, make things happen
var main = function() {

	// These will always be either 'false' or an instance of a Lemons module
	var game = false,
		menu = false,
		help = false,
		scoreboard = false;

	if(file_exists(js.exec_dir + "server.ini")) {
		var f = new File(js.exec_dir + "server.ini");
		f.open("r");
		var serverIni = f.iniGetObject();
		f.close();
	} else {
		var serverIni = { 'host' : "127.0.0.1", 'port' : 10088 };
	}

	while(!js.terminated) {

		// Refresh the user's terminal and bury the (real) cursor if necessary
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);

		/*	This is the only place where we take input from the user.
			Input is passed to the current (as per state) module from here. */
		var userInput = console.inkey(K_UPPER, 5);

		switch(state) {

			case STATE_MENU:
				// Load the menu if it isn't already showing
				if(!menu)
					menu = new Menu();
				// Pass user input to the menu
				menu.getcmd(userInput);
				// If we've left the menu, close and falsify it
				if(state != STATE_MENU) {
					menu.close();
					menu = false;
				}
				break;

			case STATE_PLAY:
				// Create a new Game if we're not already in one
				if(!game)
					game = new Game(serverIni.host, serverIni.port);
				// Pass user input to the game module
				if(!game.getcmd(userInput)) {
					// If Game.getcmd returns false, the user has hit 'Q'
					game.close();
					game = false;
					// Return to the menu on the next loop
					state = STATE_MENU;
				} else if(!game.cycle()) {
					/*	If Game.cycle returns false, the user is out of turns
						or has beat the last level. */
					game.close();
					game = false;
					// Return to the menu on the next loop
					state = STATE_MENU;
				}
				break;

			case STATE_PAUSE:
				// If the game isn't paused, pause it
				if(!game.paused)
					game.pause();
				// If the user hit a key and the game is paused, unpause it
				if(userInput != "" && game.paused) {
					game.pause();
					// Return to gameplay on the next loop
					state = STATE_PLAY;
				}
				break;

			case STATE_HELP:
				// If a game is in progress, pause it
				if(game instanceof Game && !game.paused)
					game.pause();
				// If the help screen isn't showing, load it
				if(!help)
					help = new Help();
				/*	If a game is in progress and the user hit a key, unpause
					the game, remove the help screen, and return to gameplay. */
				if(	userInput != ""
					&&
					game instanceof Game && game.paused
				) {
					help.close();
					help = false;
					game.pause();
					state = STATE_PLAY;
				/*	If there's no game in progress, close the help screen and
					return to the menu.	*/
				} else if(userInput != "") {
					help.close();
					help = false;
					state = STATE_MENU;
				}
				break;

			case STATE_SCORES:
				// Bring up the scoreboard if it isn't showing already
				if(!scoreboard) {
					scoreboard = new DBHelper(serverIni.host, serverIni.port);
					scoreboard.showScores();
				// Remove the scoreboard when the user hits a key
				} else if(userInput != "") {
					scoreboard.close();
					state = STATE_MENU;
					scoreboard = false;
				}
				break;

			case STATE_EXIT:
				// Break the main loop
				return;
				break; // I just can't not.  :|

			default:
				break;

		}

	}

}

// Prepare the things
init();
// Do the things
main();
// Remove the things
cleanUp();
// We're done
