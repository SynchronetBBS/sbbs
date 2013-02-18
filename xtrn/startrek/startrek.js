load("sbbsdefs.js");
load("json-client.js");
load("event-timer.js");
load("frame.js");
load("sprite.js");

var sysStatus, userInput, player, timer, waveTimer, jsonClient, serverIni;
var frame, splashFrame, fieldFrame, statusFrame, clockFrame, clockSubFrame, uhOhFrame, scoreFrame, helpFrame;
var player = {
	round : 0,
	score : 0,
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

var checkScreenEdge = function(sprite) {
	if(sprite.x < -3)
		sprite.moveTo(77, sprite.y);
	if(sprite.y < -2)
		sprite.moveTo(sprite.x, 23);
	if(sprite.x > 77)
		sprite.moveTo(-3, sprite.y);
	if(sprite.y > 23)
		sprite.moveTo(sprite.x, -2);
}

var putEnemy = function(re) {
	var rx = Math.floor(Math.random() * 73) + 1;
	var ry = Math.floor(Math.random() * 20) + 1;
	var s = new Sprite.Aerial(enemies[re], fieldFrame, rx, ry, 'n', 'normal');
	if(Sprite.checkOverlap(s).length > 0) {
		s.remove();
		return false;
	}
	s.frame.draw();
	return true;
}

var spawnEnemies = function() {
	var re = Math.floor(Math.random() * enemies.length);
	for(var x = 0; x < player.round; x++) {
		while(!putEnemy(re)) {
		}
	}
	if(player.round == 1) {
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
	timer.addEvent(250, 12, cycleWarning, [Sprite.aerials[Sprite.aerials.length - 1].ini.name]);
	timer.addEvent(3000, false, removeWarning);
}

var removeShip = function(s) {
	Sprite.aerials[s].remove();
}

var cycleWarning = function(enemy) {
	var c = Math.floor(Math.random() * 15) + 1;
	uhOhFrame.clear();
	uhOhFrame.center("Oh no! " + enemy + "!", c);
}

var removeWarning = function() {
	uhOhFrame.clear();
	uhOhFrame.bottom();
}

var putStats = function() {
	statusFrame.clear();
	statusFrame.putmsg(
		format(
			"[ESC] quit  [H]elp | Round: %s  Shields: %s  Score: %s",
			player.round - 1, player.sprite.ini.health - 1, player.score
		)
	);
}

var countdown = function() {
	clockSubFrame.clear();
	clockSubFrame.putmsg(parseInt(waveTimer.nextrun * .001));
}

var init = function() {
	js.branch_limit = 0;
	sysStatus = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;
	KEY_WEAPON = " ";

	frame = new Frame(1, 1, 80, 24, 0);
	splashFrame = new Frame(1, 1, 80, 24, 0, frame);
	fieldFrame = new Frame(1, 1, 80, 24, 0, frame);
	statusFrame = new Frame(1, 24, 66, 1, BG_BLUE|WHITE, frame);
	clockFrame = new Frame(67, 24, 14, 1, BG_BLUE|WHITE, frame);
	clockSubFrame = new Frame(78, 24, 2, 1, BG_BLUE|WHITE, clockFrame);
	uhOhFrame = new Frame(1, 12, 80, 1, 0, frame);
	scoreFrame = new Frame(1, 1, 80, 24, BG_BLACK|WHITE, frame);
	helpFrame = new Frame(8, 4, 64, 16, 0, frame);

	uhOhFrame.transparent = true;
	splashFrame.load(js.exec_dir + "startrek.bin", 80, 24);
	fieldFrame.load(js.exec_dir + "starfield.bin", 80, 24);
	helpFrame.load(js.exec_dir + "help.bin", 64, 16);

	player.sprite = new Sprite.Aerial("enterprise", fieldFrame, 40, 10, 'n', 'normal');

	timer = new Timer();
	
	var f = new File(js.exec_dir + "server.ini");
	f.open("r");
	serverIni = f.iniGetObject();
	f.close();

	frame.open();	
	scoreFrame.bottom();
	helpFrame.bottom();
	statusFrame.top();
	splashFrame.top();
	frame.draw();
}

var showHelp = function() {
	helpFrame.top();
	frame.cycle();
	console.getkey();
	helpFrame.bottom();
}

var gamePlay = function() {
	player.round = 1;
	timer.addEvent(5000, 1, spawnEnemies);
	waveTimer = timer.addEvent(30000, true, spawnEnemies);
	putStats();
	while(!js.terminated) {
		if(player.sprite.ini.health < 1) {
			deathKnell();
			break;
		}
		userInput = console.inkey(K_NONE, 5);
		if(ascii(userInput) == 27)
			break;
		else if(userInput.toUpperCase() == "H")
			showHelp();
		player.sprite.getcmd(userInput);
		for(var s = 0; s < Sprite.aerials.length; s++) {
			if(!Sprite.aerials[s].open)
				continue;
			checkScreenEdge(Sprite.aerials[s]);
			if(
				Sprite.aerials[s].ini.type == "ship"
				&&
				Sprite.aerials[s] != player.sprite
				&&
				system.timer - Sprite.aerials[s].lastMove > Sprite.aerials[s].ini.speed
				&&
				Sprite.aerials[s].pursue(player.sprite)
			) {
				Sprite.aerials[s].getcmd(KEY_WEAPON);
			}
			var o = Sprite.checkOverlap(Sprite.aerials[s]);
			if(!o)
				continue;
			for(var oo in o) {
				if(!o[oo].open)
					continue;
				if(o[oo].ini.type == "weapon" && Sprite.aerials[s].ini.type == "weapon") {
					Sprite.aerials[s].remove();
					o[oo].remove();
				} else if(
					(o[oo].ini.type == "weapon" && Sprite.aerials[s].ini.type == "ship")
					&&
					(o[oo].owner != Sprite.aerials[s])
				) {
					Sprite.aerials[s].ini.health = Sprite.aerials[s].ini.health - 1;
					o[oo].remove();
				} else if(
					o[oo].ini.type == "ship" && Sprite.aerials[s].ini.type == "ship"
					&&
					o[oo].position != "destroyed" && Sprite.aerials[s].position != "destroyed"
				) {
					Sprite.aerials[s].ini.health = 0;
					o[oo].ini.health = 0;
				}
				if(Sprite.aerials[s] == player.sprite || o[oo] == player.sprite)
					putStats();
			}
			if(Sprite.aerials[s].ini.hasOwnProperty("health") && Sprite.aerials[s].ini.health < 1) {
				Sprite.aerials[s].position = "destroyed";
				Sprite.aerials[s].open = false;
				player.score = player.score + ((Sprite.aerials[s].ini.hasOwnProperty("points")) ? parseInt(Sprite.aerials[s].ini.points) * player.round : 0);
				timer.addEvent(500, false, removeShip, [s]);
				putStats();
			}
		}
		timer.cycle();
		Sprite.cycle();
		if(frame.cycle())
			console.gotoxy(80, 24);
	}
	waveTimer.abort = true;
}

var deathKnell = function() {
	uhOhFrame.top();
	for(var n = 0; n < 20; n++) {
		cycleWarning("You done goofed");
		frame.cycle();
		mswait(200);
	}
}

var scoreBoard = function() {
	var scoreObj = {
		'alias' : user.alias,
		'system' : system.name,
		'round' : player.round - 1,
		'score' : player.score,
		'when' : time()
	};
	try {
		jsonClient = new JSONClient(serverIni.host, serverIni.port);
		var scores = jsonClient.read("STARTREK", "STARTREK.SCORES", 1);
		if(scores === undefined) {
			scores = [];
			jsonClient.write("STARTREK", "STARTREK.SCORES", scores, 2);
		}
		jsonClient.push("STARTREK", "STARTREK.SCORES", scoreObj, 2);
		jsonClient.disconnect();
	} catch(err) {
		log(LOG_ERR, "JSON client error: " + err);
		return false;
	}
	scoreFrame.center("Recent scores");
	scoreFrame.putmsg("\r\n\r\n");
	scoreFrame.putmsg(
		format(
			"%-30s%-30s%-10s%s\r\n\r\n",
			"Player", "System", "Round", "Score"
		),
		LIGHTCYAN
	);
	scores.push(scoreObj);
	shownUsers = [];
	for(var s = scores.length - 1; s > (scores.length >= 19) ? scores.length - 20 : 0; s = s - 1) {
		if(shownUsers.indexOf(scores[s].alias) >= 0)
			continue;
		shownUsers.push(scores[s].alias);
		scoreFrame.putmsg(
			format(
				"%-30s%-30s%-10s%s\r\n",
				scores[s].alias, scores[s].system, scores[s].round, scores[s].score
			)
		);
	}
	scoreFrame.crlf();
	scoreFrame.center("<Press any key>", LIGHTRED);
	scoreFrame.top();
	frame.cycle();
	mswait(1000);
	while(console.input_buffer_level > 0) {
		console.inkey();
	}
	console.getkey();
}

var main = function() {
	console.getkey();
	splashFrame.close();
	gamePlay();
	scoreBoard();
}

var cleanUp = function() {
	frame.close();
	console.clear();
	bbs.sys_status = sysStatus;
}

init();
main();
cleanUp();
exit();