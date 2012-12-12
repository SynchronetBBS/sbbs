// Gooble Gooble
// A Pac-Man knock-off for Synchronet BBS
// echicken -at- bbs.electronicchicken.com

/*	This game is a bit slow, but it makes for a good demo of both the
	frame.js and sprite.js libraries. */

load("sbbsdefs.js");
load("frame.js");
load("sprite.js");
load("event-timer.js");
load("json-client.js");

var level = 1;
var lives = 5;
var score = 0;
var pellets = 0;
var userInput = "";
var edible = false;
var fruitPos = 0;

var frame = new Frame(1, 1, 80, 24, BG_BLACK);
var splash = new Frame(1, 1, 80, 24, BG_BLACK, frame);
var maze = new Frame(1, 1, 80, 23, BG_BLACK, frame);
var statFrame = new Frame(21, 24, 60, 1, BG_BLUE|WHITE, frame);
var scoreFrame1 = new Frame(1, 24, 7, 1, BG_BLUE|WHITE, frame);
var scoreFrame2 = new Frame(8, 24, 13, 1, BG_BLUE|WHITE, frame);
var readyFrame1 = new Frame(35, 11, 13, 3, BG_BLUE|WHITE, frame);
var readyFrame2 = new Frame(36, 12, 11, 1, BG_BLACK|WHITE, readyFrame1);

splash.load(js.exec_dir + "splash.bin", 80, 24);
readyFrame2.putmsg("\1H\1I\1W   READY");

var	gooble = new Sprite.Aerial("gooble", maze, 39, 16, "w", "inedible");
var	ghosts = [
	new Sprite.Aerial("ghost01", maze, 28, 11, "e", "inedible"),
	new Sprite.Aerial("ghost02", maze, 34, 11, "e", "inedible"),
	new Sprite.Aerial("ghost03", maze, 44, 11, "e", "inedible"),
	new Sprite.Aerial("ghost04", maze, 50, 11, "e", "inedible")
];
var fruit = new Sprite.Aerial("fruit", maze, 39, 11, "n", "fruit");
var timer = new Timer();

var scoreClient = new JSONClient("bbs.electronicchicken.com", 10088);

var newLevel = function() {
	statFrame.clear();
	statFrame.putmsg("\1h\1w" + format("%-15s %-25s %s", "Level: " + level, "Lives: " + lives, "Hit [ESC] to quit"));
	scoreFrame1.putmsg("Score: ");
	scoreFrame2.clear();
	scoreFrame2.putmsg(score);
	maze.clear();
	maze.load(js.exec_dir + "sprites/maze.bin", 80, 23);
	gooble.moveTo(39, 16);
	gooble.turnTo("w");
	gooble.frame.draw();
	ghosts[0].moveTo(28, 11);
	ghosts[1].moveTo(34, 11);
	ghosts[2].moveTo(44, 11);
	ghosts[3].moveTo(50, 11);
	for(var g in ghosts) {
		ghosts[g].open = true;
		ghosts[g].frame.draw();
	}
	edible = false;
	pellets = 0;
	readyFrame1.top();
	if(frame.cycle())
		console.gotoxy(80, 24);
	mswait(3000);
	readyFrame1.bottom();
	timer.events = [];
	timer.addEvent(30000, false, addFruit);
}

var checkAhead = function(frame, sprite, attr) {
	if(sprite.bearing == "n") {
		for(var x = sprite.x; x < sprite.x + sprite.ini.width; x++) {
			var a = frame.getData(x - 1, sprite.y - 2);
			if(a.attr == attr)
				return { 
					'x' : x - 1,
					'y' : sprite.y - 2,
					'ch' : a.ch
				};
		}
	} else if(sprite.bearing == "s") {
		for(var x = sprite.x; x < sprite.x + sprite.ini.width; x++) {
			var a = frame.getData(x - 1, sprite.y + sprite.ini.height - 1);
			if(a.attr == attr)
				return {
					'x' : x - 1,
					'y' : sprite.y + sprite.ini.height - 1,
					'ch' : a.ch
				};
		}
	} else if(sprite.bearing == "e") {
		for(var y = sprite.y; y < sprite.y + sprite.ini.height; y++) {
			var a = frame.getData(sprite.x + sprite.ini.width - 1, y - 1);
			if(a.attr == attr)
				return {
					'x' : sprite.x + sprite.ini.width - 1,
					'y' : y - 1,
					'ch' : a.ch
				};
		}
	} else if(sprite.bearing == "w") {
		for(var y = sprite.y; y < sprite.y + sprite.ini.height; y++) {
			var a = frame.getData(sprite.x - 2, y - 1);
			if(a.attr == attr)
				return {
					'x' : sprite.x - 2,
					'y' : y - 1,
					'ch' : a.ch
				};
		}	
	}
	return false;
}

var addFruit = function() {
	fruit.open = true;
	fruit.turnTo(fruitPos);
	fruit.frame.draw();
	fruitPos++;
	if(fruitPos >= fruit.ini.bearings.length)
		fruitPos = 0;
	timer.addEvent(20000 - (level * 1000), false, removeFruit);
}

var removeFruit = function() {
	fruit.remove();
}

var play = function() {
	newLevel();
	while(ascii(userInput) != 27) {
		for(var s = 0; s < Sprite.aerials.length; s++) {
			if(!Sprite.aerials[s].open)
				continue;
			if(Sprite.aerials[s].x == 1) {
				Sprite.aerials[s].moveTo(75, Sprite.aerials[s].y);
				continue;
			}
			if(Sprite.aerials[s].x == 76)
				Sprite.aerials[s].moveTo(2, Sprite.aerials[s].y);
		}
		userInput = console.inkey(K_NONE, 5);
		gooble.getcmd(userInput);
		if(checkAhead(maze, gooble, 9))
			gooble.ini.speed = 0;
		else
			gooble.ini.speed = .25;
		var p = checkAhead(maze, gooble, 15);
		if(p && p.ch == ascii(254)) {
			maze.setData(p.x, p.y, " ", 0);
			score = score + 10;
			pellets++;
			scoreFrame2.clear();
			scoreFrame2.putmsg(score);
		}
		var p = checkAhead(maze, gooble, 14);
		if(p && p.ch == ascii(219)) {
			maze.setData(p.x, p.y, " ", 0);
			score = score + 100;
			pellets++;
			scoreFrame2.clear();
			scoreFrame2.putmsg(score);
			for(var g = 0; g < ghosts.length; g++)
				ghosts[g].position = "edible";
			edible = system.timer;
		}
		for(var g = 0; g < ghosts.length; g++) {
			if(!ghosts[g].open)
				continue;
			if(system.timer - edible >= 15 && ghosts[g].position == "edible")
				ghosts[g].position = "warning";
			if(system.timer - edible > (21 - level) && ghosts[g].position == "warning")
				ghosts[g].position = "inedible";
			if(checkAhead(maze, ghosts[g], 9))
				ghosts[g].turn("cw");
			var rr = 200 - (level * 10);
			if(rr < 4)
				rr = 4;
			var r = Math.floor(Math.random() * rr) + 1;
			if(r != g)
				continue;
			ghosts[g].turn("cw");
			if(checkAhead(maze, ghosts[g], 9))
				ghosts[g].turn("ccw");
		}
		var o = Sprite.checkOverlap(gooble);
		if(o) {
			for(var i = 0; i < o.length; i++) {
				if(!o[i].open)
					continue;
				if(o[i].position == "fruit") {
					o[i].remove();
					score = score + (2000 * level);
					scoreFrame2.clear();
					scoreFrame2.putmsg(score);
				} else if(o[i].position == "edible") {
					o[i].remove();
					score = score + 1000;
					scoreFrame2.clear();
					scoreFrame2.putmsg(score);
				} else {
					lives = lives - 1;
					newLevel();
					break;
				}
			}
		}
		if(pellets == 37) {
			level++;
			newLevel();
			continue;
		}
		if(lives < 1)
			break;
		timer.cycle();
		Sprite.cycle();
		if(frame.cycle())
			console.gotoxy(80, 24);
	}
}

var addScore = function() {
	var scoreObj = {
		'alias' : user.alias,
		'system' : system.name,
		'level' : level,
		'score' : score
	};
	var scores = scoreClient.read("gooble", "scores", 1);
	if(scores === undefined) {
		scoreClient.write("gooble", "scores.0", scoreObj, 2);
	} else {
		var tempScores = [];
		for(var s = 0; s < 18; s++) {
			if(!scores.hasOwnProperty(s))
				break;
			tempScores.push(scores[s]);
		}
		for(var t = 0; t < tempScores.length; t++) {
			if(score > tempScores[t].score) {
				if(tempScores.length > 18)
					tempScores.pop();
				tempScores.splice(t, 0, scoreObj);
				break;
			} else if(tempScores.length < 18) {
				tempScores.push(scoreObj);
				break;
			}
		}
		for(var t = 0; t < tempScores.length; t++)
			scoreClient.write("gooble", "scores." + t, tempScores[t], 2);
	}
}

var highScores = function() {
	var scores = scoreClient.read("gooble", "scores", 1);
	if(scores === undefined)
		return;
	splash.open();
	splash.top();
	splash.clear();
	splash.load(js.exec_dir + "scores.bin", 80, 24);
	splash.gotoxy(1, 5);
	for(var s = 0; s < 18; s++) {
		if(scores[s] === undefined)
			break;
		splash.putmsg("\1h\1w" + format("%-23s %-25s %-18s %s", scores[s].alias, scores[s].system, scores[s].level, scores[s].score) + "\r\n");
	}
	frame.cycle();
	console.getkey();
}

console.clear(0);
frame.open();
splash.top();
frame.cycle();
console.getkey();
splash.close();
fruit.remove();
play();
addScore();
highScores();
frame.close();