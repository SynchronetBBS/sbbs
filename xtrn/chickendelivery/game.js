load("frame.js");
load("json-client.js");

client.subscribe("chickendelivery","chickenScores");
// The values of score, lives, level, and mileStone should match on every BBS
// that is using the same json-db server (as configured in server.ini.) Don't
// change these values without consulting the json-db server admin first. 
var score = 0; // The player's starting score
var lives = 5; // The number of lives the player starts with
var level = 1; // The level that the player starts at
var mileStone = 2500; // How many points must a player earn to win a free life

// Edit below this line at your own peril.
var quitNow = false; // Try this shit
var collision = false; // Dim c as collision, that is cool
var levelCompleted = false; // Your iQ is at the level of 10

var frame = new Frame(1, 1, 80, 24, BG_BLACK); // Parent of all frames
frame.open();

var splashScreen = new Frame(1, 1, 80, 24, BG_BLACK, frame);
var netScoreFrame = new Frame(3, 3, 75, 21, BG_BLACK, frame);
var instructionFrame = new Frame(8, 3, 68, 20, BG_BLACK, frame);
var menuFrame = new Frame(2, 10, 16, 6, BG_BLACK, frame);
var deathFrame = new Frame(29, 10, 21, 4, BG_BLUE, frame);
var quitFrame = new Frame(29, 10, 21, 4, BG_BLUE, frame);
var statusBar = new Frame(1, 1, 80, 1, BG_BLACK, frame);
var lifeBox = new Frame(60, 1, 10, 1, BG_BLACK, statusBar);
var timeBox = new Frame(70, 1, 10, 1, BG_BLACK, statusBar);
var player = new Frame(1, 1, 5, 4, BG_BLACK, frame);


splashScreen.load(js.exec_dir + "ckndlvry.ans");
splashScreen.draw();

mswait(500);

menuFrame.load(js.exec_dir + "menu.ans");
menuFrame.draw();

var userInput = '';
while(userInput != 'P') {
	frame.cycle();
	userInput = console.getkey(K_NOSPIN|K_NOECHO|K_NOCRLF).toUpperCase();
	switch(userInput) {
		case 'I':	instructionFrame.open();
			instructionFrame.load(js.exec_dir + "instruct.ans");
			frame.cycle();
			console.getkey(K_NOSPIN|K_NOECHO|K_NOCRLF);
			instructionFrame.close();
			continue; // Instructions
		case 'S':	showNetScores();
			continue; // Scores
		case 'Q': 	frame.close();
			client.unsubscribe("chickendelivery","chickenScores");
			exit();
	}
}

menuFrame.close();
splashScreen.close();

while(!js.terminated) {

	var f = new File(js.exec_dir + "levels/" + level + ".ini");
	f.open("r");
	var iniPlatforms = f.iniGetObject("Platforms");
	var iniEnemies = f.iniGetObject("Enemies");
	var levelName = f.iniGetValue(null, 'name');
	var door = f.iniGetValue(null, 'door').split(",");
	var timeLimit = f.iniGetValue(null, 'time');
	var timeBonus = f.iniGetValue(null, 'timeBonus');
	var entry = f.iniGetValue(null, 'entry').split(",");
	var entryDirection = f.iniGetValue(null, 'entryDirection');
	f.close();

	var timeStart = system.timer;
	var timeElapsed = 0;
	var clockChange = timeStart;
	var lastClockChange = timeStart;
	
	statusBar.open();
	statusBar.clear();
	statusBar.putmsg("\1h\1wScore: " + score + "      Level " + level + ": " + levelName);
	lifeBox.open();
	lifeBox.clear();
	lifeBox.putmsg("\1h\1wLives: " + lives);
	timeBox.open();
	timeBox.clear();
	timeBox.putmsg("\1h\1wTime: " + timeLimit);
	
	player.open();
	player.clear();
	player.moveTo(parseInt(entry[0]), parseInt(entry[1]));
	player.direction = entryDirection;
	if(entryDirection == 'r') {
		player.load(js.exec_dir + "sprites/player-r.ans");
	} else {
		player.load(js.exec_dir + "sprites/player-l.ans");
	}

	var platforms = new Array();
	for(var p in iniPlatforms) {
		var thisPlatform = iniPlatforms[p].split(",");
		platforms[p] = new Frame(parseInt(thisPlatform[0]), parseInt(thisPlatform[1]), parseInt(thisPlatform[2]), parseInt(thisPlatform[3]), eval(thisPlatform[4]), frame);
		platforms[p].open();
	}

	var doorFrame = new Frame(parseInt(door[0]), parseInt(door[1]), 5, 4, BG_BLACK, frame);
	doorFrame.load(js.exec_dir + "sprites/" + door[2].toString());
	doorFrame.score = door[3];
	doorFrame.open();

	var enemies = new Array();
	for(var e in iniEnemies) {
		var thisEnemy = iniEnemies[e].split(",");
		enemies[e] = new Frame(parseInt(thisEnemy[0]), parseInt(thisEnemy[1]), 5, 4, BG_BLACK, frame);
		enemies[e].load(js.exec_dir + "sprites/" + thisEnemy[2].toString());
		enemies[e].direction = thisEnemy[5];
		enemies[e].lastStep = system.timer;
		enemies[e].lastX = parseInt(thisEnemy[0]);
		enemies[e].borderL = parseInt(thisEnemy[3]);
		enemies[e].borderR = parseInt(thisEnemy[4]);
		enemies[e].stepInterval = parseFloat(thisEnemy[6]);
		enemies[e].open();
	}

	frame.cycle();
	fall(player, frame);

	do {
		userInput = ascii(console.inkey(K_NOSPIN|K_NOECHO|K_NOCRLF));
		switch(userInput) {
			case 6: 	right(player, frame, 1, false, false);
					break;
			case 29: 	left(player, frame, 1, false, false);
					break;
			case 30:	jump(player, frame, 5);
					break;
			case 27:	if(quitPrompt()) var quitNow = true;
					break;
			default:	doCycle(frame);
					break;
		}
		timeElapsed = system.timer - timeStart;
	} while(!quitNow && player.y + player.height <= 24 && !collision && !levelCompleted && (timeElapsed <= timeLimit));

	if(quitNow) quitGame();

	if(collision || player.y + player.height > 24 || (timeElapsed > timeLimit)) {
		lives = lives - 1;
		deathFrame.open();
		deathFrame.clear();
		deathFrame.crlf();
		if(lives >= 0) {
			if(timeElapsed > timeLimit) {
				deathFrame.center("\1h\1wOut of Time");
			} else {
				deathFrame.center("\1h\1wYou died!");
			}
			deathFrame.crlf();
			deathFrame.center("\1h\1w< Press Enter >");
			deathFrame.draw();
			while(console.getkey(K_NOECHO) !== "\r");
			collision = false;
		} else {
			deathFrame.center("\1h\1wGame over");
			deathFrame.crlf();
			deathFrame.center("\1h\1w< Press Enter >");
			deathFrame.draw();
			while(console.getkey(K_NOECHO) !== "\r");
			addNetScore();
			quitGame();
		}
		deathFrame.close();
		if(levelCompleted) levelCompleted = false; // Sometimes you can overlap the door and an enemy
	}

	if(levelCompleted) {
		levelCompleted = false;
		var timeScore = parseInt((timeLimit - timeElapsed) * timeBonus);
		var lastScore = score;
		score = score + parseInt(door[3]) + timeScore;
		if(lastScore < mileStone && score >= mileStone) {
			lives++;
			mileStone = mileStone + mileStone;
			var nlFrame = new Frame(29,10,21,8,BG_BLUE,frame);
			nlFrame.gotoxy(1,6);
			nlFrame.center("\1h\1w1UP!");
			nlFrame.gotoxy(1,1);
		} else {
			var nlFrame = new Frame(29, 10, 21, 6, BG_BLUE, frame);
		}
		nlFrame.open();
		nlFrame.crlf();
		nlFrame.center("\1h\1wLevel " + level + " complete!");
		nlFrame.crlf();
		nlFrame.center("\1h\1wPoints: " + door[3]);
		nlFrame.crlf();
		nlFrame.center("\1h\1wTime Bonus: " + timeScore);
		nlFrame.crlf();
		nlFrame.center("\1h\1wScore: " + score);
		nlFrame.gotoxy(1,7);
		nlFrame.center("\1h\1w< Press Enter >");
		nlFrame.cycle();
		while(console.getkey(K_NOECHO) !== "\r");
		if(!file_exists(js.exec_dir + "levels/" + (level + 1) + ".ini")) {
			nlFrame.close();
			winScreen();
			addNetScore();
			quitGame();
		}
		level++;
		nlFrame.close();
	}

	frame.close();

}

function right(player, frame, steps, nofall, nocycle) {
	if(player.direction == 'l' && !player.hasOwnProperty("borderL")) {
		player.clear();
		player.load(js.exec_dir + "sprites/player-r.ans");
		player.direction = 'r';
		return;
	}
	for(var y = 1; y <= steps; y++) {
		if(checkRight(player)) return;
		player.move(1, 0);
		if(!nofall) fall(player, frame);
		if(!nocycle) doCycle(frame);
	}
	return;
}

function left(player, frame, steps, nofall, nocycle) {
	if(player.direction == 'r' && !player.hasOwnProperty("borderL")) {
		player.clear();
		player.load(js.exec_dir + "sprites/player-l.ans");
		player.direction = 'l';
		return;
	}
	for(var y = 1; y <= steps; y++) {
		if(checkLeft(player)) return;
		player.move(-1, 0);
		if(!nofall) fall(player, frame);
		if(!nocycle) doCycle(frame);
	}
	return;
}

function jump(player, frame, steps) {
	var timeOut = 10;
	for(var y = 1; y <= steps; y++) {
		if(checkTop(player)) break;
		player.move(0, -1);
		doCycle(frame);
		if(y == steps) timeOut = 250;
		userInput = ascii(console.inkey(K_NOSPIN|K_NOECHO|K_NOCRLF, timeOut));
		if(userInput == 6) right(player, frame, 5, true);
		if(userInput == 29) left(player, frame, 5, true);
		if(userInput == 32 && y == steps) {
			var hoverStart = system.timer;
			while(system.timer - hoverStart <= 1) {
				userInput = ascii(console.inkey(K_NOSPIN|K_NOECHO|K_NOCRLF, 10));
				if(userInput == 6) right(player, frame, 1, true, false);
				if(userInput == 29) left(player, frame, 1, true, false);
				if(userInput == 10) break;
			}
		}
	}
	fall(player, frame);
	return;
}

function fall(player, frame) {
	while(!checkBottom(player)) {
		player.move(0, 1);
		if(!player.hasOwnProperty("borderR")) {
			userInput = ascii(console.inkey(K_NOSPIN|K_NOECHO|K_NOCRLF, 5));
			if(userInput == 6) right(player, frame, 1, true);
			if(userInput == 29) left(player, frame, 1, true);
		}
		doCycle(frame);
	}
	return;
}

function checkBottom(sprite) {
	var retVal = false;
	sprite.move(0, 1);
	for(var p in platforms) {
		if(checkOverlap(sprite, platforms[p])) {
			retVal = true;
			break;
		}
	}
	if(sprite.y + sprite.height > 24 && !retVal) return true;
	sprite.move(0, -1);
	return retVal;
}

function checkTop(sprite) {
	var retVal = false;
	sprite.move(0, -1);
	if(sprite.y < 1) retVal = true;
	for(var p in platforms) {
		if(checkOverlap(sprite, platforms[p]) || retVal) {
			retVal = true;
			break;
		}
	}
	sprite.move(0, 1);
	return retVal;
}

function checkRight(sprite) {
	var retVal = false;
	sprite.move(1, 0);
	if(sprite.x + sprite.width > 81) retVal = true;
	for(var p in platforms) {
		if(checkOverlap(sprite, platforms[p]) || retVal) {
			retVal = true;
			break;
		}
	}
	sprite.move(-1, 0);
	return retVal;
}

function checkLeft(sprite) {
	var retVal = false;
	sprite.move(-1,0);
	if(sprite.x < 1) retVal = true;
	for(var p in platforms) {
		if(checkOverlap(sprite, platforms[p]) || retVal) {
			retVal = true;
			break;
		}
	}
	sprite.move(1, 0);
	return retVal;
}

function checkOverlap(sprite1, sprite2) {
	if(sprite1.x >= sprite2.x + sprite2.width || sprite1.x + sprite1.width <= sprite2.x) return false; // There is no x overlap
	if(sprite1.y >= sprite2.y + sprite2.height || sprite1.y + sprite1.height <= sprite2.y) return false; // There is no y overlap
	return true;
}

function doCycle(frame) {
	for(var e in enemies) {
		var thisTime = system.timer;
		if(thisTime - enemies[e].lastStep >= enemies[e].stepInterval) {
			enemies[e].lastStep = thisTime;
			enemies[e].lastX = enemies[e].x;
			if(enemies[e].direction == 'l') {
				if(enemies[e].x != enemies[e].borderL) left(enemies[e], frame, 1, false, true);
				if(enemies[e].lastX == enemies[e].x) enemies[e].direction = 'r';
			} else {
				if(enemies[e].x != enemies[e].borderR) right(enemies[e], frame, 1, false, true);
				if(enemies[e].lastX == enemies[e].x) enemies[e].direction = 'l';
			}
		}
		if(checkOverlap(player, enemies[e])) collision = true;
	}
	if(checkOverlap(player, doorFrame)) levelCompleted = true;
	var clockChange = system.timer;
	if(clockChange - lastClockChange >= 1) {
		timeBox.clear();
		timeBox.putmsg("\1h\1wTime: " + parseInt(timeLimit - timeElapsed));
		lastClockChange = clockChange;
	}
	if(frame.cycle()) console.gotoxy(80,24);
	return;
}

function winScreen() {
	var winFrame = new Frame(1, 1, 80, 24, BG_BLACK, frame);
	winFrame.open();
	winFrame.gotoxy(1, 23);
	winFrame.center("\1h\1wYou win!");
	winFrame.crlf();
	winFrame.center("\1h\1w< Press any key >");
	frame.cycle();
	var winner = new Frame(1, 10, 5, 4, BG_BLACK, winFrame);
	winner.open();
	winner.load(js.exec_dir + "sprites/player-r.ans");
	winFrame.cycle();
	for(var m = 1; m <= 38; m++) {
		winner.move(1,0);
		if(m % 5 == 0) winner.move(0,1);
		if(m % 10 == 0) { 
			winner.move(0,-1);
			winner.move(0,-1);
		}
		winFrame.cycle();
		mswait(100);
	}
	var n = 0;
	var starFrames = new Object();
	while(console.inkey(K_NOSPIN|K_NOECHO|K_NOCRLF) == '') {
		if(n % 20 == 0 && n <= 200) {
			var rx = Math.floor(Math.random()*75) + 1;
			var ry = Math.floor(Math.random()*20) + 1;
			starFrames[n] = new Frame(rx, ry, 5, 3, winFrame);
			starFrames[n].open();
			starFrames[n].stage = 1;
		}
		for(s in starFrames) {
			if(checkOverlap(winner, starFrames[s])) continue;
			if(starFrames[s].stage == 5) starFrames[s].stage = 1;
			starFrames[s].clear();
			starFrames[s].load(js.exec_dir + "sprites/star-" + starFrames[s].stage + ".ans");
			starFrames[s].stage++;
			starFrames[s].cycle();
		}
		mswait(100);
		n++;
	}
}

function showNetScores() {
	var scores = client.read("chickendelivery", "chickenScores", 1);
	var onlinePlayers = client.who("chickendelivery","chickenScores");
	netScoreFrame.open();
	netScoreFrame.clear();
	netScoreFrame.load(js.exec_dir + "netscore.ans");
	netScoreFrame.gotoxy(3, 6);

	var highScores = [];
	for each(var s in scores) 
		highScores.push(s);
	highScores.sort(function(a, b) { return b.score - a.score });

	// for each(var score in list) {
		// if(player.score == 0)
			// continue;
		// if(count > 0 && count%scores_per_page == 0) {
			// scoreFrame.center("\1r\1h<SPACE to continue>");
			// scoreFrame.draw();
			// while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
			// scoreFrame.clear();
		// }
		// if(count++%scores_per_page == 0) {
			// scoreFrame.crlf();
			// scoreFrame.putmsg("\1w\1h" + format(" %3s %-25s %4s %6s %6s %6s",
				// "###","NAME","WINS","LEVEL","SCORE","LINES") + "\r\n");
		// }
		// scoreFrame.putmsg("\1w\1y" + formatScore(count,	player.name,
			// player.wins,player.level,player.score,player.lines) + "\r\n");
	// }
	// scoreFrame.end();
	// scoreFrame.center("\1r\1h<SPACE to continue>");
	// scoreFrame.cycle();
	// while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
	// scoreFrame.delete();
	
	var count=0;
	var scores_per_page=14;
	for(var h in highScores) {
	
		if(count > 0 && count%scores_per_page == 0) {
			netScoreFrame.center("\1r\1h<SPACE to continue>");
			netScoreFrame.draw();
			while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
			netScoreFrame.clear();
		}
		
		if(count++%scores_per_page == 0) {
			netScoreFrame.clear();
			netScoreFrame.load(js.exec_dir + "netscore.ans");
			netScoreFrame.gotoxy(3, 6);
		}
		
		var online = " ";
		for each(var u in onlinePlayers) {
			if(u.nick == highScores[h].name) {
				online = "\1y\1h*";
				break;
			}
		}
	
		netScoreFrame.gotoxy(2, netScoreFrame.getxy().y);
		netScoreFrame.putmsg(online + "\1h\1w" + highScores[h].name);
		netScoreFrame.gotoxy(25, netScoreFrame.getxy().y);
		netScoreFrame.putmsg("\1h\1w" + highScores[h].level);
		netScoreFrame.gotoxy(32, netScoreFrame.getxy().y);
		netScoreFrame.putmsg("\1h\1w" + highScores[h].score);
		netScoreFrame.gotoxy(47, netScoreFrame.getxy().y);
		netScoreFrame.putmsg("\1h\1w" + highScores[h].bbsName);
		netScoreFrame.crlf();			
	
	}
	
	netScoreFrame.center("\1r\1h<SPACE to continue>");
	frame.cycle();
	while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
	netScoreFrame.close();
}

function netScore(name, bbsName, level, score) {
	this.name = name;
	this.bbsName = bbsName;
	this.level = level;
	this.score = score;
}

function addNetScore() {
	var s = new netScore(user.alias,system.name,level,score);
	client.write("chickendelivery", "chickenScores." + user.alias, s, 2);
}

function quitPrompt() {
	quitFrame.open();
	quitFrame.clear();
	quitFrame.crlf();
	quitFrame.center("\1h\1wQuit the game?");
	quitFrame.crlf();
	quitFrame.center("\1h\1wY/N");
	frame.cycle();
	var userInput = console.getkey(K_NOSPIN|K_NOECHO|K_NOCRLF);
	timeStart = time() - timeElapsed;
	if(userInput.toUpperCase() == "Y") return true;
	quitFrame.close();
	frame.cycle();
	return false;
}

function quitGame() {
	frame.clear();
	frame.cycle();
	splashScreen.open();
	splashScreen.gotoxy(1, 24);
	splashScreen.center("\1h\1w< Press any key to return to " + system.name + " >");
	showNetScores();
	splashScreen.close();
	frame.close();
	console.clear(BG_BLACK);
	client.unsubscribe("chickendelivery","chickenScores");
	exit();
}
