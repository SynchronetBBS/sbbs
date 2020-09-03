// Dependencies
load('sbbsdefs.js');
load('frame.js');
load('tree.js');
load('event-timer.js');
load('json-client.js');
load('sprite.js');
require("mouse_getkey.js", "mouse_getkey");
var ansi = load({}, 'ansiterm_lib.js');

load(js.exec_dir + "defs.js");
load(js.exec_dir + "game.js");
load(js.exec_dir + "level.js");
load(js.exec_dir + "menu.js");
load(js.exec_dir + "help.js");
load(js.exec_dir + "dbhelper.js");

var state,
	frame;

Frame.prototype.drawBorder = function (color) {
	var theColor = color;
	if (Array.isArray(color)) var sectionLength = Math.round(this.width / color.length);
	this.pushxy();
	for (var y = 1; y <= this.height; y++) {
		for (var x = 1; x <= this.width; x++) {
			if (x > 1 && x < this.width && y > 1 && y < this.height) continue;
			var msg;
			this.gotoxy(x, y);
			if (y == 1 && x == 1) {
				msg = ascii(218);
			} else if(y == 1 && x == this.width) {
				msg = ascii(191);
			} else if(y == this.height && x == 1) {
				msg = ascii(192);
			} else if(y == this.height && x == this.width) {
				msg = ascii(217);
			} else if(x == 1 || x == this.width) {
				msg = ascii(179);
			} else {
				msg = ascii(196);
			}
			if (Array.isArray(color)) {
				if (x == 1) {
					theColor = color[0];
				} else if (x % sectionLength == 0 && x < this.width) {
					theColor = color[x / sectionLength];
				} else if (x == this.width) {
					theColor = color[color.length - 1];
				}
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

function PopUp(message) {

	var longestLine = 0;
	for (var m = 0; m < message.length; m++) {
		if (message[m].length > longestLine) longestLine = message[m].length;
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
	for (var m = 0; m < message.length; m++) {
		this.frame.center(message[m] + "\r\n");
	}

	this.close = function () {
		this.frame.delete();
	}

}

function mouse_enable(enable) {
	if (!console.term_supports(USER_ANSI)) return;
	ansi.send('mouse', enable ? 'set' : 'clear', 'x10_compatible');
	ansi.send('mouse', enable ? 'set' : 'clear', 'extended_coord');
}

function init() {
	js.on_exit('frame.delete();');
	js.on_exit('bbs.sys_status = ' + bbs.sys_status + ';');
	js.on_exit('console.clear(' + console.attributes + ');');
	js.on_exit('mouse_enable(false);'); // Should probably depend on whether it's already enabled
	bbs.sys_status|=SS_MOFF; // Turn off node message delivery
	console.clear(BG_BLACK|WHITE);
	frame = new Frame(
		Math.ceil((console.screen_columns - 79) / 2),
		Math.ceil((console.screen_rows - 23) / 2),
		80,
		24,
		BG_BLACK|LIGHTGRAY
	);
	frame.open();
	frame.checkbounds = false;
	state = STATE_MENU;
	mouse_enable(true);
}

function main() {

	// These will always be either 'false' or an instance of a Lemons module
	var game = false,
		menu = false,
		help = false,
		scoreboard = false;

	if (file_exists(js.exec_dir + "server.ini")) {
		var f = new File(js.exec_dir + "server.ini");
		f.open("r");
		var serverIni = f.iniGetObject();
		f.close();
	} else {
		var serverIni = {
			host: "127.0.0.1",
			port: 10088
		};
	}

	while (!js.terminated) {

		if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);

		var userInput = mouse_getkey(K_NONE, 5, true);
		userInput.key = userInput.key.toUpperCase();

		switch (state) {
			case STATE_MENU:
				if (!menu) menu = new Menu();
				menu.getcmd(userInput);
				if (state != STATE_MENU) {
					menu.close();
					menu = false;
				}
				break;
			case STATE_PLAY:
				if (!game) game = new Game(serverIni.host, serverIni.port);
				if (!game.getcmd(userInput)) {
					game.close();
					game = false;
					state = STATE_MENU;
				} else if (!game.cycle()) {
					game.close();
					game = false;
					state = STATE_MENU;
				}
				break;
			case STATE_PAUSE:
				if (!game.paused) game.pause();
				if (game.paused && (userInput.key != "" || userInput.mouse !== null)) {
					game.pause();
					state = STATE_PLAY;
				}
				break;
			case STATE_HELP:
				if (game instanceof Game && !game.paused) game.pause();
				if (!help) help = new Help();
				if (userInput.key != "" && game instanceof Game && game.paused) {
					help.close();
					help = false;
					game.pause();
					state = STATE_PLAY;
				} else if (userInput.key != "") {
					help.close();
					help = false;
					state = STATE_MENU;
				}
				break;
			case STATE_SCORES:
				if (!scoreboard) {
					scoreboard = new DBHelper(serverIni.host, serverIni.port);
					scoreboard.showScores();
				} else if (userInput.key != "") {
					scoreboard.close();
					state = STATE_MENU;
					scoreboard = false;
				}
				break;
			case STATE_EXIT:
				return;
			default:
				break;
		}

	}

}

init();
main();