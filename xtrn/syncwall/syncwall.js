// SyncWall InterBBS Graffiti Wall
// by echicken -at- bbs.electronicchicken.com

load("ansiedit.js");
load("json-client.js");

js.branch_limit = 0;

var chDelay = 10; // Milliseconds between characters when loading the buffer
var server = "bbs.electronicchicken.com"; // IP or hostname of the JSON server
var port = 10088; // Port that "server" is listening on
var chBuffer = 100; // Max character history to read at once 

var monthYear = strftime("%m%y", time());

var ch;
var lastUser = "";
var lastSystem = "";
var userInput = "";

var bgColours = [ 
	BG_BLUE,
	BG_CYAN,
	BG_GREEN,
	BG_MAGENTA,
	BG_RED,
	BG_BROWN
];

var ansiClient = new JSONClient(server, port);
if(!ansiClient.socket.is_connected)
	exit();

var frame, topBar, bottomBar, ansi;

function init() {
	frame = new Frame(1, 1, 80, 24, BG_BLACK);
	topBar = new Frame(1, 1, 80, 1, BG_BLUE|WHITE, frame);
	bottomBar = new Frame(1, 24, 80, 1, BG_BLUE|WHITE, frame);
	ansiClient.callback = drawStuff;
	ansiClient.subscribe("syncwall", "canvas." + monthYear + ".ch");
	topBar.putmsg(".: SyncWall InterBBS Graffiti Wall : Hit [ESC] to Exit :.");
	topBar.gotoxy(70, 1);
	topBar.putmsg("\1h\1bby echicken");
	frame.open();
	ansi = new ansiEdit(1, 2, 80, 22, BG_BLACK|LIGHTGRAY, frame);
	frame.cycle();
}

function drawStuff(update) {
	if(update.oper.toUpperCase() == "SUBSCRIBE" || update.oper.toUpperCase() == "UNSUBSCRIBE")
		return;
	var location = update.location.split(".").shift().toUpperCase();
	if(location != "CANVAS")
		return;
	putCh(update.data, false);
	return;
}

function putCh(ch, skipMsg) {
	ansi.putChar(ch);
	if(ch.userAlias != lastUser || ch.system != lastSystem) {
		var randomAttr = bgColours[Math.floor(Math.random()*bgColours.length)]|WHITE;
		bottomBar.attr = randomAttr;
		bottomBar.clear();
		bottomBar.putmsg(ch.userAlias + "@" + ch.system + " is drawing");
		if(skipMsg) {
			bottomBar.gotoxy(60, 1);
			bottomBar.putmsg("<Hit [SPACE] to skip>");
		}
		lastUser = ch.userAlias;
		lastSystem = ch.system;
	}
	return;
}

function cleanUp() {
	frame.close();
	ansi.close();
	ansiClient.unsubscribe("syncwall", "canvas." + monthYear + ".ch");
	ansiClient.disconnect();
	console.clear();
}

function loadHistory() {
	var index = 0; 
	var endex = chBuffer;
	var canvasLength = ansiClient.read("syncwall", "canvas." + monthYear + ".history.length", 1);
	if(canvasLength === undefined) {
		ansiClient.write("syncwall", "canvas." + monthYear, { history : [] }, 2);
	} else {
		ansi.showUI(false);
		while(index < canvasLength) {
			var canvas = ansiClient.slice("syncwall", "canvas." + monthYear + ".history", index, endex, 1);
			index += chBuffer;
			endex += chBuffer;
			while(canvas.length > 0) {
				if(frame.cycle())
					console.gotoxy(80, 24);
				putCh(canvas.shift(), true);
				var cmd = console.inkey(K_NONE, chDelay);
				if(ascii(cmd) == 27) {
					cleanUp();
					exit();
				}
				else if(cmd == " ") {
					var canvas = ansiClient.read("syncwall", "canvas." + monthYear + ".data", 1);
					ansi.putChar({ x : 1, y: 1, ch : "CLEAR", attr : 0 });
					for(var c in canvas) {
						for(var y in canvas[c])
							ansi.putChar(canvas[c][y]);
					}
					index = canvasLength;
					break;
				}
			}
		}
		ansi.showUI(true);
	}
	bottomBar.clear();
	lastUser = "";
	lastSystem = "";
	console.gotoxy(80, 24);
}

function main() {
	while(ascii(userInput) != 27) {
		if(frame.cycle())
			console.gotoxy(80, 24);
		ansiClient.cycle();
		userInput = console.inkey(K_NONE, 5);
		if(userInput == "")
			continue;
		ch = ansi.getcmd(userInput);
		if(!ch.ch)
			continue;
		if(lastUser != user.alias || lastSystem != system.name) {
			bottomBar.attr = BG_BLUE|WHITE;
			bottomBar.clear();
			bottomBar.putmsg("You are drawing");
			lastUser = user.alias;
			lastSystem = system.name;
		}
		ch.userAlias = user.alias;
		ch.system = system.name;
		if(ch.ch == "CLEAR")
			ansiClient.write("syncwall", "canvas." + monthYear + ".data", {}, 2);
		else
			ansiClient.write("syncwall", "canvas." + monthYear + ".data." + ch.x + "." + ch.y, { x : ch.x, y : ch.y, ch : ch.ch, attr: ch.attr }, 2);
		ansiClient.write("syncwall", "canvas." + monthYear + ".ch", ch, 2);
		ansiClient.push("syncwall", "canvas." + monthYear + ".history", ch, 2);
	}
}

init();
loadHistory();
main();
cleanUp();