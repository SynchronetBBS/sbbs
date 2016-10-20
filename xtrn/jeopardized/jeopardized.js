load('sbbsdefs.js');
load('nodedefs.js');
load('http.js');
load('frame.js');
load('tree.js');
load('layout.js');
load('event-timer.js');
load('json-client.js');
load('typeahead.js');
load('scrollbar.js');

load(js.exec_dir + 'lib/defs.js');
load(js.exec_dir + 'lib/func.js');
load(js.exec_dir + 'lib/frame-ext.js');
load(js.exec_dir + 'lib/database.js');
load(js.exec_dir + 'views/messages.js');
load(js.exec_dir + 'views/menu.js');
load(js.exec_dir + 'views/game.js');
load(js.exec_dir + 'views/board.js');
load(js.exec_dir + 'views/wager.js');
load(js.exec_dir + 'views/clue.js');
load(js.exec_dir + 'views/answer.js');
load(js.exec_dir + 'views/round.js');
load(js.exec_dir + 'views/popup.js');
load(js.exec_dir + 'views/scoreboard.js');
load(js.exec_dir + 'views/news.js');
load(js.exec_dir + 'views/bin-scroller.js');

var database,
	frame,
	menu,
	game,
	news,
	help,
	credits,
	settings,
	consoleAttr,
	sysStatus,
	popUp,
	scoreboard,
	buryCursor = false;

var state = STATE_MENU;

function loadSettings() {
	settings = {};
	var f = new File(js.exec_dir + 'settings.ini');
	f.open('r');
	settings.JSONDB = f.iniGetObject('JSONDB');
	settings.WebAPI = f.iniGetObject('WebAPI');
	f.close();
}

function initDatabase() {

	database = new Database(settings.JSONDB);

	database.on(
		'state',
		function (update) {
			if (update.data.locked) state = STATE_MAINTENANCE;
		}
	);

	if (database.getState().locked) {
		console.clear(WHITE);
		console.putmsg('Maintenance in progress.  Exiting ...\r\n');
		console.pause();
		state = STATE_MAINTENANCE;
	} else {
		database.notify('\1n\1c' + user.alias + ' is here.', false);
	}

}

function initDisplay() {
	console.clear(WHITE);
	frame = new Frame(1, 1, 80, 24, WHITE);
	frame.checkbounds = false;
	frame.centralize();
	frame.open();
	menu = new Menu(frame);	
}

function init() {
	consoleAttr = console.attributes;
	sysStatus = bbs.sys_status;
	loadSettings();
	initDatabase();
	initDisplay();
}

function main() {

	while (state !== STATE_QUIT && state !== STATE_MAINTENANCE) {

		database.cycle();

		var cmd = console.inkey(K_NONE, 5);

		switch (state) {

			case STATE_MENU:
				var ret = menu.getcmd(cmd);
				menu.cycle();
				if (typeof ret !== 'boolean') {
					state = ret;
				} else if (typeof ret === 'boolean' && ret) {
					buryCursor = true;
				}
				break;

			case STATE_GAME:
				if (typeof game === 'undefined') game = new Game(frame);
				var ret = game.getcmd(cmd);
				game.cycle();
				if (typeof ret === 'boolean' && !ret) {
					state = STATE_MENU;
					game.close();
					game = undefined;
				}
				break;

			case STATE_SCORE:
				if (typeof scoreboard === 'undefined') {
					scoreboard = new Scoreboard(frame);
				}
				var ret = scoreboard.getcmd(cmd);
				scoreboard.cycle();
				if (!ret) {
					state = STATE_MENU;
					scoreboard.close();
					scoreboard = undefined;
				}
				break;

			case STATE_NEWS:
				if (typeof news === 'undefined') news = new News(frame);
				var ret = news.getcmd(cmd);
				news.cycle();
				if (!ret) {
					state = STATE_MENU;
					news.close();
					news = undefined;
				}
				break;

			case STATE_HELP:
				if (typeof help === 'undefined') {
					help = new BinScroller(
						frame,
						js.exec_dir + 'views/help.bin',
						77,
						53
					);
				}
				var ret = help.getcmd(cmd);
				help.cycle();
				if (!ret) {
					state = STATE_MENU;
					help.close();
					help = undefined;
				}
				break;

			case STATE_CREDIT:
				if (typeof credits === 'undefined') {
					credits = new BinScroller(
						frame,
						js.exec_dir + 'views/credits.bin',
						77,
						20
					);
				}
				var ret = credits.getcmd(cmd);
				credits.cycle();
				if (!ret) {
					state = STATE_MENU;
					credits.close();
					credits = undefined;
				}
				break;

			case STATE_MAINTENANCE:
				state = STATE_POPUP;
				popUp = new PopUp(
					frame,
					'Maintenance is in progress.  Exiting ...',
					'[Press any key to continue]',
					PROMPT_ANY,
					STATE_MAINTENANCE
				);
				break;

			case STATE_POPUP:
				popUp.cycle();
				if (typeof popUp.getcmd(cmd) !== 'undefined') {
					state = popUp.close();
				}
				break;

			default:
				break;

		}

		if (frame.cycle() || buryCursor) {
			console.gotoxy(console.screen_columns, console.screen_rows);
			buryCursor = false;
		}

	}

}

function cleanUp() {
	if (typeof database !== 'undefined') {
		database.notify('\1n\1c' + user.alias + ' has left.', false);
		database.close();
	}
	if (typeof frame !== 'undefined') frame.close();
	console.clear(consoleAttr);
	bbs.sys_status = sysStatus;
}

try {
	init();
	main();
} catch (err) {
	log(LOG_ERR, err);
} finally {
	cleanUp();
}
