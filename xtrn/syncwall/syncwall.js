// SyncWall InterBBS Graffiti Wall
// by echicken -at- bbs.electronicchicken.com

load("ansiedit.js");
load("json-client.js");

js.branch_limit = 0;

var chDelay = 15;
var server = "bbs.electronicchicken.com"
var port = 10088;

var monthYear = system.datestr().split("/");
monthYear.splice(1, 1);
monthYear = monthYear.join("");
var ch;
var pName;
var lastUser = "";
var lastSystem = "";
var userInput = "";
var bgColours = [ BG_BLUE, BG_CYAN, BG_GREEN, BG_MAGENTA, BG_RED, BG_BROWN ];

var ansiClient = new JSONClient(server, port);
ansiClient.callback = drawStuff;
ansiClient.subscribe("syncwall", "canvas." + monthYear);

var frame = new Frame(1, 1, 80, 24, BG_BLACK);
var topBar = new Frame(1, 1, 80, 1, BG_BLUE|WHITE, frame);
var bottomBar = new Frame(1, 24, 80, 1, BG_BLUE|WHITE, frame);
topBar.putmsg(".: SyncWall InterBBS Graffiti Wall : Hit [ESC] to Exit :.");
topBar.gotoxy(69, 1);
topBar.putmsg("\1h\1bby echicken");
frame.open();
frame.cycle();
var ansi = new ansiEdit(1, 2, 80, 22, BG_BLACK|LIGHTGRAY);

function drawStuff(update) {
	if(update.oper.toUpperCase() == "SUBSCRIBE" || update.oper.toUpperCase() == "UNSUBSCRIBE") return;
	var location = update.location.split(".").shift().toUpperCase();
	if(location != "CANVAS") return;
	putCh(update.data);
	return;
}

function putCh(ch) {
	ansi.putChar(ch);
	if(ch.userAlias != lastUser || ch.system != lastSystem) {
		var randomAttr = bgColours[Math.floor(Math.random()*bgColours.length)]|WHITE;
		bottomBar.attr = randomAttr;
		bottomBar.clear();
		bottomBar.putmsg(ch.userAlias + "@" + ch.system + " is drawing");
		bottomBar.cycle();	
		lastUser = ch.userAlias;
		lastSystem = ch.system;
	}
	return;
}

// This may be needed if canvas data exceed's json-sock.js' receive limit
/*
var canvas = ansiClient.keys("syncwall", "canvas." + monthYear, 1);
if(canvas !== undefined) {
	for(var c = 0; c < canvas.length; c++) {
		var ch = ansiClient.read("syncwall", "canvas." + monthYear + "." + canvas[c], 1);
		putCh(ch);
		mswait(chDelay);
	}
}
*/
// So if you uncomment the above section, comment out this next bit:
var canvas = ansiClient.read("syncwall", "canvas." + monthYear, 1);
if(canvas !== undefined) {
	for(var c in canvas) {
		putCh(canvas[c]);
		mswait(chDelay);
	}
}
// And stop commenting here.

while(ascii(userInput) != 27) {
	ansiClient.cycle();
	userInput = console.inkey(K_NONE, 5);
	if(userInput == "") continue;
	ch = ansi.cycle(userInput);
	if(!ch.ch) continue;
	if(lastUser != user.alias || lastSystem != system.name) {
		bottomBar.attr = BG_BLUE|WHITE;
		bottomBar.clear();
		bottomBar.putmsg("You are drawing");
		bottomBar.cycle();
		lastUser = user.alias;
		lastSystem = system.name;
	}
	ch.userAlias = user.alias;
	ch.system = system.name;
	pName = base64_encode(user.alias) + system.timer.toString().split(".").join("");
	ansiClient.write("syncwall", "canvas." + monthYear + "." + pName, ch, 2);
}

ansi.close();
ansiClient.unsubscribe();
ansiClient.disconnect();
