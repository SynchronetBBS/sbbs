load("sbbsdefs.js");
load("json-client.js");
load("event-timer.js");
load("frame.js");
load("layout.js");
load("sprite.js");

var maxEnemies = 5;

var sysStatus, userInput, player, timer, waveTimer, jsonClient, serverIni;
var frame, fieldFrame, statusFrame, clockFrame, clockSubFrame, uhOhFrame;

var player = {
	round : 0,
	score : 0,
	shipName : "",
	sprite : {}
};
var enemySprites = [];
var enemies = [
	"romulan",
	"klingon",
	"borg",
	"cardassian",
	"gorn",
	"pakled"
];
var ships = [
	"galaxy",
	"intrepid",
	"defiant"
];

function cycle() {
	Sprite.cycle();
	timer.cycle();
	if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

function splash() {
	var splashFrame = new Frame(1, 1, 80, 24, 0, frame);
	splashFrame.load(js.exec_dir + "startrek.bin", 80, 24);
	splashFrame.top();
	splashFrame.open();
	cycle();
	console.getkey();
	splashFrame.close();
	splashFrame.delete();
}

function init() {
	js.branch_limit = 0;
	sysStatus = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;
	KEY_WEAPON = " ";

	frame = new Frame(1, 1, 80, 24, 0);
	fieldFrame = new Frame(1, 1, 80, 24, 0, frame);
	statusFrame = new Frame(1, 24, 66, 1, BG_BLUE|WHITE, frame);
	clockFrame = new Frame(67, 24, 14, 1, BG_BLUE|WHITE, frame);
	clockSubFrame = new Frame(78, 24, 2, 1, BG_BLUE|WHITE, clockFrame);
	uhOhFrame = new Frame(1, 12, 80, 1, 0, frame);
	uhOhFrame.transparent = true;
	fieldFrame.load(js.exec_dir + "starfield.bin", 80, 24);

	timer = new Timer();

	if (file_exists(js.exec_dir + "server.ini")) {
		var f = new File(js.exec_dir + "server.ini");
		f.open("r");
		serverIni = f.iniGetObject();
		f.close();
	} else {
		serverIni = {
			host: 'localhost',
			port: 10088
		};
	}
	
	frame.open();	
	statusFrame.top();
	frame.draw();
}

function removeShip(s) {
	Sprite.aerials[s].remove();
	Sprite.aerials[s].frame.delete();
}

function rotateSprites(sprites) {
	for (var s in sprites) {
		sprites[s].turn("cw");
	}
}

function ucfl(str) {
	str = str[0].toUpperCase() + str.substr(1);
	return str;
}

function dots(d) {
	var str = "";
	for (var dd = 0; dd < d; dd++) {
		str += ascii(254);
	}
	return str;
}

function setup() {
	var setupFrame = new Frame(1, 1, 80, 24, 0, frame);
	setupFrame.load(js.exec_dir + "starfield.bin", 80, 24);
	
	var layout = new Layout(setupFrame);
	
	var shipSprites = [];
	var viewWidth = 13;
	var x = Math.floor((80 - (ships.length * viewWidth)) / 2);
	for (var s = 0; s < ships.length; s++) {
		var view = layout.addView(ucfl(ships[s]), x, 5, viewWidth - 1, 16);
		view.show_tabs = false;
		var tab = view.addTab("frame", "frame");
		var sprite = new Sprite.Aerial(ships[s], tab.frame, tab.frame.x, tab.frame.y, 'n');
		var weapon = new Sprite.Aerial(sprite.ini.weapon, tab.frame, tab.frame.x, tab.frame.y, 'n');
		sprite.ini.constantmotion = 0;
		shipSprites.push(sprite);
		weapon.remove();
		weapon.frame.delete();
		tab.frame.gotoxy(1, 7);
		tab.frame.putmsg(format(
			"Speed\r\n\1h\1g%s\r\n\r\n\1h\1wShields\r\n\1h\1c%s\r\n\r\n\1h\1wWeapons\r\n\1h\1r%s",
			dots(Math.floor((1 - sprite.ini.maximumspeed) * 10)),
			dots(sprite.ini.health - 1),
			dots(weapon.ini.damage * 3)
		), WHITE);
		x = x + viewWidth;
	}
	timer.addEvent(1000, true, rotateSprites, [shipSprites]);
	
	layout.open();
	setupFrame.open();
	setupFrame.gotoxy(1, 2);
	setupFrame.center(
		"What class of ship do you want to command, Captain " + ucfl(user.alias) + "?",
		WHITE
	);
	setupFrame.gotoxy(1, 3);
	setupFrame.center("Press [TAB] to change ships, [Enter] to select.", CYAN);
	setupFrame.draw();

	var userInput = "";
	setupFrame.top();
	while (ascii(userInput) != 13) {
		userInput = console.inkey(K_NONE, 5);
		layout.getcmd(userInput);
		layout.cycle();
		cycle();
	}
	
	setupFrame.gotoxy(1, 21);
	setupFrame.center("What is the name of your " + layout.current.title + "-class starship?", WHITE);
	cycle();
	while (player.shipName == "") {
		console.gotoxy(25, 23);
		player.shipName = console.getstr("USS ", 30, K_LINE|K_EDIT);
	}

	for (var s in Sprite.aerials) {
		removeShip(s);
	}

	player.sprite = new Sprite.Aerial(layout.current.title.toLowerCase(), fieldFrame, 40, 10, 'n', 'normal');
	layout.close();
	setupFrame.close();
	setupFrame.delete();
	frame.invalidate();
	timer.events = [];
}

function showHelp() {
	var helpFrame = new Frame(8, 4, 64, 16, 0, frame);
	helpFrame.load(js.exec_dir + "help.bin", 64, 16);
	helpFrame.top();
	helpFrame.open();
	cycle();
	console.getkey();
	helpFrame.close();
	helpFrame.delete();
}

function checkScreenEdge(sprite) {
	if (sprite.x < -3) {
		sprite.moveTo(77, sprite.y);
	} else if (sprite.x > 77) {
		sprite.moveTo(-3, sprite.y);
	}
	if (sprite.y < -2) {
		sprite.moveTo(sprite.x, 23);
	} else if (sprite.y > 23) {
		sprite.moveTo(sprite.x, -2);
	}
}

function putEnemy(re) {
	var rx = Math.floor(Math.random() * 73) + 1;
	var ry = Math.floor(Math.random() * 20) + 1;
	var s = new Sprite.Aerial(enemies[re], fieldFrame, rx, ry, 'n', 'normal');
	if (Sprite.checkOverlap(s).length > 0) {
		s.remove();
		return false;
	}
	s.frame.draw();
	return true;
}

function spawnEnemies() {
	var re = Math.floor(Math.random() * enemies.length);
	var roundMax = Math.floor((player.round + 1) / 2);
	var _enemies = Math.min(roundMax, maxEnemies);
	for (var x = 0; x < _enemies; x++) {
		while(!putEnemy(re)) {
			mswait(1);
		}
	}
	if (player.round == 1) {
		timer.addEvent(1000, true, countdown);
		clockFrame.putmsg("Next wave: ");
		countdown();
	} else {
		player.sprite.ini.health = player.sprite.ini.health + 5;
	}
	player.round++;
	putStats();
	uhOhFrame.top();
	statusFrame.top();
	timer.addEvent(250, 12, cycleWarning, ["Oh no! " + Sprite.aerials[Sprite.aerials.length - 1].ini.name + "!"]);
	timer.addEvent(3000, false, removeWarning);
	if (player.round % 5 == 0) waveTimer.interval = waveTimer.interval - 5000;
}

function countdown() {
	clockSubFrame.clear();
	clockSubFrame.putmsg(parseInt(waveTimer.nextrun * .001));
}

function cycleWarning(warning) {
	var c = Math.floor(Math.random() * 15) + 1;
	uhOhFrame.clear();
	uhOhFrame.center(warning, c);
}

function removeWarning() {
	uhOhFrame.clear();
	uhOhFrame.bottom();
}

function putStats() {
	statusFrame.clear();
	statusFrame.putmsg(format(
		"[Q]uit [H]elp [N]ext | Round: %s  Shields: %s  Score: %s",
		player.round - 1, player.sprite.ini.health - 1, player.score
	));
}

function deathKnell() {
	uhOhFrame.top();
	for (var n = 0; n < 20; n++) {
		cycleWarning("You done goofed!");
		cycle();
		mswait(200);
	}
	while (console.input_buffer_level > 0) {
		console.inkey();
	}
}

function gamePlay() {
	player.round = 1;
	player.sprite.frame.draw();
	timer.addEvent(2000, 1, spawnEnemies);
	waveTimer = timer.addEvent(30000, true, spawnEnemies);
	putStats();
	statusFrame.top();
	while (!js.terminated) {
		if (player.sprite.ini.health < 1) break;
		userInput = console.inkey(K_NONE, 5).toUpperCase();
		if (userInput == '\x1B' || userInput == 'Q') break;
		if (userInput == "H") {
			showHelp();
		} else if (userInput == 'N') {
			waveTimer.run();
		} else {
			player.sprite.getcmd(userInput);
		}
		for (var s = 0; s < Sprite.aerials.length; s++) {
			if (!Sprite.aerials[s].open) continue;
			checkScreenEdge(Sprite.aerials[s]);
			if (Sprite.aerials[s].ini.type == "ship" &&
				Sprite.aerials[s] != player.sprite &&
				system.timer - Sprite.aerials[s].lastMove > Sprite.aerials[s].ini.speed &&
				Sprite.aerials[s].pursue(player.sprite)
			) {
				Sprite.aerials[s].getcmd(KEY_WEAPON);
			}
			var o = Sprite.checkOverlap(Sprite.aerials[s]);
			if (!o) continue;
			for (var oo in o) {
				if (!o[oo].open) continue;
				if (o[oo].ini.type == "weapon" && Sprite.aerials[s].ini.type == "weapon") {
					Sprite.aerials[s].remove();
					o[oo].remove();
				} else if (
					(o[oo].ini.type == "weapon" && Sprite.aerials[s].ini.type == "ship") &&
					(o[oo].owner != Sprite.aerials[s])
				) {
					Sprite.aerials[s].ini.health = Sprite.aerials[s].ini.health - parseInt(o[oo].ini.damage);
					o[oo].remove();
				} else if (
					o[oo].ini.type == "ship" && Sprite.aerials[s].ini.type == "ship" &&
					o[oo].position != "destroyed" && Sprite.aerials[s].position != "destroyed"
				) {
					Sprite.aerials[s].ini.health = 0;
					o[oo].ini.health = 0;
				}
				if (Sprite.aerials[s] == player.sprite || o[oo] == player.sprite) putStats();
			}
			if (Sprite.aerials[s].ini.hasOwnProperty("health") && Sprite.aerials[s].ini.health < 1) {
				Sprite.aerials[s].position = "destroyed";
				Sprite.aerials[s].open = false;
				player.score = player.score + ((Sprite.aerials[s].ini.hasOwnProperty("points")) ? parseInt(Sprite.aerials[s].ini.points) * player.round : 0);
				timer.addEvent(500, false, removeShip, [s]);
				putStats();
			}
		}
		cycle();
	}
	if (player.sprite.ini.health < 1) deathKnell();
	timer.events = [];
}

function scoreBoard() {
	var firstRun = false;
	var scoreObj = {
		alias: user.alias,
		system: system.name,
		shipName: player.shipName,
		round: player.round - 1,
		score: player.score,
		when: time()
	};
	try {
		jsonClient = new JSONClient(serverIni.host, serverIni.port);
		var scores = jsonClient.read("STARTREK", "STARTREK.HIGHSCORES", 1);
		if (scores === undefined) {
			firstRun = true;
			scores = [scoreObj];
			jsonClient.write("STARTREK", "STARTREK.HIGHSCORES", scores, 2);
		}
	} catch(err) {
		log(LOG_ERR, "JSON client error: " + err);
		return false;
	}
	var scoreFrame = new Frame(1, 1, 80, 24, BG_BLACK|WHITE, frame);
	scoreFrame.open();
	scoreFrame.center("High scores");
	scoreFrame.putmsg("\r\n\r\n");
	scoreFrame.putmsg(format("%-30s%-30s%-10s%s\r\n\r\n", "Player", "Ship", "Round", "Score"), LIGHTCYAN);
	var writeBack = false;
	for (var s = 0; s < scores.length; s++) {
		if (player.score > scores[s].score && !writeBack) {
			writeBack = true;
			scores.splice(s, 0, scoreObj);
		}
		scoreFrame.putmsg(format(
			"%-30s%-30s%-10s%s\r\n",
			scores[s].alias, scores[s].shipName, scores[s].round, scores[s].score
		), WHITE);
	}
	if (!writeBack && scores.length < 18) {
		writeBack = true;
		if (!firstRun) scores.push(scoreObj);
		scoreFrame.putmsg(format(
			"%-30s%-30s%-10s%s\r\n",
			scores[s].alias, scores[s].shipName, scores[s].round, scores[s].score
		), WHITE);
	}
	if (writeBack) {
		while (scores.length > 18) {
			scores.pop();
		}
		jsonClient.write("STARTREK", "STARTREK.HIGHSCORES", scores, 2);
	}
	jsonClient.disconnect();
	
	scoreFrame.crlf();
	scoreFrame.center("<Press any key>", LIGHTRED);
	scoreFrame.top();
	cycle();
	console.getkey();
}

function main() {
	splash();
	setup();
	gamePlay();
	scoreBoard();
}

function cleanUp() {
	frame.close();
	console.clear();
	bbs.sys_status = sysStatus;
}

init();
main();
cleanUp();
exit();
