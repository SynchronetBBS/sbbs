load("ansiedit.js"); // Loads frame.js, tree.js, funclib.js, sbbsdefs.js
load("json-client.js");
load("event-timer.js");
load("http.js");

var delay = 10,
	page = 100,
	dbName = "SYNCWALL2";

var	attr, systemStatus, users, systems, months,
	frame, titleFrame, editorFrame, statusFrame, promptFrame,
	editor, jsonClient, timer, timedEvent,
	lastUser = -1, lastSystem = -1;

function updateStatus(u, s, altUsers, altSystems) {

	if (u == lastUser && s == lastSystem) return;

	// Derp, how does I scope?  Too late, too much drinkings yesno?
	var usrs = (typeof altUsers == "undefined") ? users : altUsers;
	var systms = (typeof altSystems == "undefined") ? systems : altSystems;

	var rbg = [BG_BLUE, BG_CYAN, BG_GREEN, BG_MAGENTA, BG_RED, BG_BROWN];
	statusFrame.attr = rbg[Math.floor(Math.random()*rbg.length)]|WHITE
	statusFrame.clear();
	statusFrame.putmsg(format("%s@%s is drawing", usrs[u], systms[s]));
	lastUser = u;
	lastSystem = s;
	editor.cycle(true);
	if (typeof timedEvent != "undefined") timedEvent.abort = true;
	timedEvent = timer.addEvent(
		5000, false, function () {
			statusFrame.clear();
			lastUser = -1;
			lastSystem = -1;
			editor.cycle(true);
		}
	);

}

function fetchHistory(month) {
	try {
		var historyDir = js.exec_dir + "history";
		if (!file_exists(historyDir) || !file_isdir(historyDir)) {
			mkdir(historyDir);
		}
		var fn = month.split("/");
		fn = historyDir + "/" + fn[fn.length - 1];
		if (!file_exists(fn)) {
			var http = new HTTPRequest();
			var body = http.Get(month);
			var f = new File(fn);
			f.open("wb");
			f.write(body);
			f.close();
		}
		loadLocalHistory(fn);
	} catch (err) {
		log(LOG_ERR, "SyncWall error: " + err);
	}
}

function loadLocalHistory(fn) {
	var f = new File(fn);
	f.open("r");
	var history = JSON.parse(f.read());
	f.close();
	editor.getcmd("\x09");
	editor.clear();
	promptFrame.top();
	promptFrame.putmsg(" Hit <SPACE> to skip");
	lastUser = -1;
	lastSystem = -1;
	for (var h = 0; h < history.SEQUENCE.length; h++) {
		if (history.SEQUENCE[h].c == "CLEAR") {
			editor.clear();
		} else {
			editor.putChar(
				{	'x' : history.SEQUENCE[h].x,
					'y' : history.SEQUENCE[h].y,
					'ch' : history.SEQUENCE[h].c,
					'attr' : history.SEQUENCE[h].a
				}
			);
			updateStatus(
				history.SEQUENCE[h].u,
				history.SEQUENCE[h].s,
				history.USERS,
				history.SYSTEMS
			);
			editor.cycle();
			frame.cycle();
			if (console.inkey(K_NONE) == " ") {
				loadState(history.STATE);
				break;
			}
			if (h < history.SEQUENCE.length - 1) mswait(delay);
		}
	}
	promptFrame.bottom();
	statusFrame.clear();
	statusFrame.putmsg("Hit any key to continue");
	editor.cycle(true);
	frame.cycle();
	console.getkey();
	statusFrame.clear();
	loadState();
}

function loadHistory() {
	promptFrame.putmsg(" Hit <SPACE> to skip");
	var len = jsonClient.read(dbName, "SEQUENCE.length", 1);
	pageLoop:
	for (var i = 0; i < len; i = i + ((len - i > page) ? page : len - i)) {
		var history = jsonClient.slice(
			dbName,	"SEQUENCE",	i, ((len - i > page) ? i + page : len),	1
		);
		for (var h = 0; h < history.length; h++) {
			if (history[h].c == "CLEAR") {
				editor.clear();
			} else {
				editor.putChar(
					{	'x' : history[h].x,
						'y' : history[h].y,
						'ch' : history[h].c,
						'attr' : history[h].a
					}
				);
				updateStatus(history[h].u, history[h].s);
			}
			editor.cycle();
			frame.cycle();
			if (console.inkey(K_NONE) == " ") {
				loadState();
				break pageLoop;
			}
			// Loading the next 'page' history elements will delay us anyhow.
			if (h < history.length - 1) mswait(delay);
		}
	}
	promptFrame.bottom();
	statusFrame.clear();
	editor.cycle(true);
	lastUser = -1;
	lastSystem = -1;
}

function loadState(state) {
	if (typeof state == "undefined") {
		var state = jsonClient.read(dbName, "STATE", 1);
	}
	editor.clear();
	for(var y in state) {
		for(var x in state[y]) {
			editor.putChar(
				{	'x' : parseInt(x),
					'y' : parseInt(y),
					'ch' : state[y][x].c,
					'attr' : state[y][x].a
				}
			);
		}
	}
}

function sendUpdate(ch) {
	jsonClient.write(
		dbName,	"LATEST", {
			'x' : ch.x,
			'y' : ch.y,
			'c' : ch.ch,
			'a' : ch.attr,
			'u' : users.indexOf(user.alias),
			's' : systems.indexOf(system.name)
		}, 2
	);
}

function processUpdate(update) {

	if (update.oper != "WRITE") return;

	var location = update.location.split(".");
	if (location[0] == "USERS") {
		users.push(update.data[update.data.length - 1]);
	} else if (location[0] == "SYSTEMS") {
		systems.push(update.data);
	} else if (location[0] == "LATEST") {
		if (update.data.c == "CLEAR") {
			editor.clear();
		} else {
			editor.putChar(
				{	'x' : update.data.x,
					'y' : update.data.y,
					'ch' : update.data.c,
					'attr' : update.data.a
				}
			);
		}
		updateStatus(update.data.u, update.data.s);
	}

}

function initDisplay() {
	attr = console.attributes;
	systemStatus = bbs.sys_status;
	bbs.sys_status |= SS_MOFF;
	console.clear(LIGHTGRAY);
}

function initFrames() {

	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		LIGHTGRAY
	);

	titleFrame = new Frame(
		frame.x, frame.y, frame.width, 1, BG_BLUE|WHITE, frame
	);
	titleFrame.putmsg(
		"\1h\1w.: SyncWall InterBBS Graffiti Wall :. Hit [ESC] to exit"
	);
	titleFrame.gotoxy(titleFrame.width - 10, 1);
	titleFrame.putmsg("\1h\1bby echicken");

	statusFrame = new Frame(
		frame.x, frame.y + frame.height - 1,
		frame.width, 1, BG_BLUE|WHITE, frame
	);

	promptFrame = new Frame(
		statusFrame.width - 20, statusFrame.y, 21, 1, BG_BLUE|WHITE, statusFrame
	);

	editorFrame = new Frame(
		frame.x, titleFrame.y + titleFrame.height,
		frame.width, frame.height - titleFrame.height - statusFrame.height,
		LIGHTGRAY, frame
	);

	frame.open();

}

function initEditor() {

	editor = new ANSIEdit(
		{	'x' : editorFrame.x,
			'y' : editorFrame.y,
			'width' : editorFrame.width,
			'height' : editorFrame.height,
			'attr' : LIGHTGRAY,
			'parentFrame' : frame,
			'menuHeading' : "SyncWall Menu"
		}
	);

	editor.open();
	var historyTree = editor.menu.addTree("View previous month");
	for (var m in months) {
		month = months[m].split("/");
		month = month[month.length - 1].replace(".json", "");
		month = format(" %s/%s", month.substr(0, 2), month.substr(2));
		historyTree.addItem(month, fetchHistory, months[m]);
	}

}

function initUsers() {
	users = jsonClient.read(dbName, "USERS", 1);
	if (!users || users.indexOf(user.alias) < 0) {
		jsonClient.push(dbName, "USERS", user.alias, 2);
		users = jsonClient.read(dbName, "USERS", 1);
	}
	jsonClient.subscribe(dbName, "USERS");
}

function initSystems() {
	systems = jsonClient.read(dbName, "SYSTEMS", 1);
	if (!systems || systems.indexOf(system.name) < 0) {
		jsonClient.push(dbName, "SYSTEMS", system.name, 2);
		systems = jsonClient.read(dbName, "SYSTEMS", 1);
	}
	jsonClient.subscribe(dbName, "SYSTEMS");
}

function initJSON() {

	if (file_exists(js.exec_dir + "server.ini")) {
		var f = new File(js.exec_dir + "server.ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
		var host = ini.host;
		var port = ini.port;
	} else {
		var f = new File(system.ctrl_dir + "services.ini");
		f.open("r");
		var ini = f.iniGetObject("JSON");
		f.close();
		// services.ini normally uses "Port" rather than "port", but it's actually not case sensitive
		if (ini.port == undefined && ini.Port != undefined) ini.port = ini.Port;
		var host = "localhost";
		var port = (ini === null) ? 10088 : Number(ini.port);
	}
	jsonClient = new JSONClient(host, port);
	jsonClient.callback = processUpdate;
	jsonClient.subscribe(dbName, "LATEST");
	months = jsonClient.read(dbName, "MONTHS", 1);
	if (!months) months = [];

}

function init() {
	timer = new Timer();
	initDisplay();
	initJSON();
	initFrames();
	initEditor();
	initUsers();
	initSystems();
	loadHistory();
}

function cycle() {
	editor.cycle();
	jsonClient.cycle();
	timer.cycle();
	if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

function input() {
	var userInput = console.inkey(K_NONE, 5);
	if (userInput == "") return true;
	if (ascii(userInput) == 27) return false;
	var ret = editor.getcmd(userInput);
	if (ret.ch != false || ascii(ret.ch) == 32) {
		updateStatus(users.indexOf(user.alias), systems.indexOf(system.name));
		sendUpdate(ret);
	}
	return true;
}

function main() {
	while(!js.terminated) {
		if(!input()) break;
		cycle();
	}
}

function cleanUp() {
	jsonClient.disconnect();
	editor.close();
	frame.close();
	console.clear(attr);
	bbs.sys_status = systemStatus;
}

try {
	init();
	main();
	cleanUp();
	exit();
} catch (err) {
	log(LOG_ERR, "SyncWall: " + err);
}
