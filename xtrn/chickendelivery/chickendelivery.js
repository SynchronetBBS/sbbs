load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("sprite.js");
load("json-client.js");
load("event-timer.js");

const LEVEL_CONT = 0;
const LEVEL_DEAD = 1;
const LEVEL_NEXT = 2;
const LEVEL_TIME = 3;
const LEVEL_QUIT = 4;

var state = {
	'menu' : true,
	'play' : false,
	'help' : false,
	'scores' : false,
	'quit' : false
};

var	attr,
	sys_status,
	frame,
	helpFrame,
	scoreFrame,
	popUpFrame,
	subPopUpFrame,
	treeItems = [],
	jsonClient,
	dbName = "CHICKENDELIVERY2",
	uid = base64_encode(user.alias + "@" + system.name, true),
	initLevel = 0;

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

var getLevels = function() {
	var f = new File(js.exec_dir + "levels.json");
	f.open("r");
	var levels = JSON.parse(f.read());
	f.close();
	return levels;
}

var initFrames = function() {
	console.clear(LIGHTGRAY);
	frame = new Frame(
		Math.ceil((console.screen_columns - 79) / 2),
		Math.ceil((console.screen_rows - 23) / 2),
		80,
		24,
		WHITE
	);
	frame.open();
}

var initJSON = function() {
	if(file_exists(js.exec_dir + "server.ini")) {
		var f = new File(js.exec_dir + "server.ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
	} else {
		var ini = { 'host' : 'localhost', 'port' : 10088 };
	}
	jsonClient = new JSONClient(ini.host, ini.port);
}

var init = function() {
	attr = console.attributes;
	sys_status = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;
	initFrames();
	initJSON();
}

var cleanUp = function() {
	frame.close();
	console.clear(attr);
	bbs.sys_status = sys_status;
}

var popUp = function() {
	popUpFrame = new Frame(
		frame.x + 10,
		frame.y + 10,
		60,
		4,
		BG_BLACK|WHITE,
		frame
	);
	subPopUpFrame = new Frame(
		popUpFrame.x + 1,
		popUpFrame.y + 1,
		popUpFrame.width - 2,
		1,
		BG_BLACK|WHITE,
		popUpFrame
	);
	popUpFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	popUpFrame.gotoxy(1, popUpFrame.height - 1);
	popUpFrame.open();
	popUpFrame.top();
	frame.cycle();
}

var popUpMessage = function(msg) {
	popUp();
	popUpFrame.center("<\1h\1cPress [\1h\1wenter\1h\1c] to continue\1h\1w>");
	subPopUpFrame.center(msg);
	frame.cycle();
	console.gotoxy(console.screen_columns, console.screen_rows);
	console.getstr("", 128, K_NOCRLF);
	popDown();
}

var popUpYesNo = function(question) {
	popUp();
	popUpFrame.center("<\1h\1cY\1h\1w/\1h\1cN\1h\1w>");
	subPopUpFrame.center(question + "?");
	frame.cycle();
	console.gotoxy(console.screen_columns, console.screen_rows);
	var ret = console.getkeys("YN");
	popDown();
	return (ret == "Y");
}

var popUpNumber = function(msg, maxnum) {
	popUp();
	subPopUpFrame.center(msg);
	frame.cycle();
	console.attributes = LIGHTGRAY;
	console.gotoxy(popUpFrame.x + subPopUpFrame.cursor.x + 1, popUpFrame.y + subPopUpFrame.cursor.y + 1);
	var num = console.getnum(maxnum, 1);
	popDown();
	return (num < 1) ? 1 : num; // The docs is false!
}

var popDown = function() {
	subPopUpFrame.delete();
	popUpFrame.delete();
	frame.invalidate();
	console.clear();
}

var Menu = function() {

	state.menu = true;

	var menuFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	menuFrame.open();
	menuFrame.load(js.exec_dir + "ckndlvry.bin", 80, 24);

	var titleFrame = new Frame(
		frame.x + frame.width - 22,
		frame.y + 1,
		20,
		4,
		BG_BLACK|WHITE,
		menuFrame
	);
	titleFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	titleFrame.gotoxy(1, 2);
	titleFrame.center("chicken delivery");
	titleFrame.gotoxy(titleFrame.width - 12, 3);
	titleFrame.putmsg("\1n\1cby echicken");
	titleFrame.open();

	var treeFrame = new Frame(
		menuFrame.x + 26,
		menuFrame.y + 5,
		20,
		6,
		BG_BLACK|WHITE,
		menuFrame
	);
	treeFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);

	var treeSubFrame = new Frame(
		treeFrame.x + 1,
		treeFrame.y + 1,
		treeFrame.width - 2,
		treeFrame.height - 2,
		BG_BLACK|WHITE,
		treeFrame
	);

	treeFrame.open();

	var tree = new Tree(treeSubFrame);
	tree.colors.fg = LIGHTGRAY;
	tree.colors.bg = BG_BLACK;
	tree.colors.lfg = WHITE;
	tree.colors.lbg = BG_BLUE;
	tree.colors.kfg = LIGHTCYAN;
	tree.addItem("|Play", startGame);
	tree.addItem("|Help", help);
	tree.addItem("|Scores", highScores);
	tree.addItem("|Quit", endGame);
	tree.open();

	this.getcmd = function(userInput) {
		tree.getcmd(userInput);
	}

	this.close = function() {
		tree.close();
		menuFrame.delete();
	}

}

var endGame = function() {
	for(var s in state)
		state[s] = (s == "quit");
}

var startGame = function() {
	for(var s in state)
		state[s] = (s == "play");
	var player = jsonClient.read(dbName, "PLAYERS." + uid, 1);
	if(player)
		initLevel = popUpNumber("Start at level 1 - " + player.level + ": ", player.level) - 1;
	return true;
}

var help = function() {
	for(var s in state)
		state[s] = (s == "help");
	helpFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	helpSubFrame = new Frame(
		helpFrame.x + 1,
		helpFrame.y + 1,
		helpFrame.width - 2,
		helpFrame.height - 2,
		BG_BLACK|WHITE,
		helpFrame
	);
	helpFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	helpFrame.open();
	helpSubFrame.load(js.exec_dir + "instruct.ans");
	return true;
}

var highScores = function() {
	for(var s in state)
		state[s] = (s == "scores");
	scoreFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	scoreSubFrame = new Frame(
		scoreFrame.x + 1,
		scoreFrame.y + 2,
		scoreFrame.width - 2,
		scoreFrame.height - 4,
		BG_BLACK|WHITE,
		scoreFrame
	);
	scoreFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	scoreFrame.gotoxy(1, 2);
	scoreFrame.center("Chicken Delivery: High Scores");
	scoreFrame.gotoxy(1, scoreFrame.height - 1);
	scoreFrame.center("< Press any key to continue >");
	scoreFrame.open();
	scoreSubFrame.putmsg(format("\1h\1c%-55s Level %16s\r\n\1h\1w", "Player", "Score"));
	var hs = jsonClient.read(dbName, "SCORES.HIGH", 1);
	for(var s in hs) {
		scoreSubFrame.putmsg(
			format(
				"%-55s %5s %16s\r\n",
				base64_decode(hs[s].uid),
				hs[s].level,
				hs[s].score
			)
		);
	}
	scoreSubFrame.scrollTo(0, 0);
	return true;
}

var Level = function(level, stats) {

	var	player,
		door,
		time = 0,
		flight = 1,
		flightTimer = 0,
		quit = false, // heh :|
		self = this;

	this.score = stats.score;

	var headFrame, fieldFrame, footFrame, timeFrame, scoreFrame;

	var timer = new Timer();

	var updateTime = function() {
		time--;
		timeFrame.clear();
		timeFrame.putmsg("Time: " + time);
	}

	var loadLevel = function(level, stats) {

		time = level.time;

		headFrame = new Frame(
			frame.x,
			frame.y,
			frame.width,
			1,
			BG_BLUE|WHITE,
			frame
		);
		headFrame.open();
		headFrame.putmsg("Level " + (stats.level + 1) + ": " + level.name);

		footFrame = new Frame(
			frame.x,
			frame.y + frame.height - 1,
			frame.width,
			1,
			BG_BLUE|WHITE,
			frame
		);
		footFrame.open();
		footFrame.putmsg("Lives: " + stats.lives + ", Score: ");

		scoreFrame = new Frame(
			footFrame.x + footFrame.cursor.x,
			footFrame.y,
			12,
			1,
			BG_BLUE|WHITE,
			footFrame
		);
		scoreFrame.open();
		scoreFrame.putmsg(self.score);

		timeFrame = new Frame(
			footFrame.x + footFrame.width - 10,
			footFrame.y,
			9,
			1,
			BG_BLUE|WHITE,
			footFrame
		);
		timeFrame.open();
		timer.addEvent(1000, true, updateTime);
		updateTime();

		fieldFrame = new Frame(
			frame.x,
			frame.y + 1,
			frame.width,
			frame.height - headFrame.height - footFrame.height,
			BG_BLACK|WHITE,
			frame
		);
		fieldFrame.open();

		player = new Sprite.Profile(
			"chicken",
			fieldFrame,
			fieldFrame.x + level.player.x - 1,
			fieldFrame.y + level.player.y - 2,
			"e",
			"normal"
		);
		player.frame.open();

		door = new Sprite.Profile(
			"door",
			fieldFrame,
			fieldFrame.x + level.door.x - 1,
			fieldFrame.y + level.door.y - 2,
			"e",
			"normal"
		);
		door.frame.open();

		for(var e = 0; e < level.enemies.length; e++) {
			new Sprite.Profile(
				level.enemies[e].type,
				fieldFrame,
				fieldFrame.x + level.enemies[e].x - 1,
				fieldFrame.y + level.enemies[e].y - 2,
				"w",
				"normal"
			);
			Sprite.profiles[Sprite.profiles.length - 1].frame.open();
		}

		for(var p = 0; p < level.platforms.length; p++) {
			new Sprite.Platform(
				fieldFrame,
				fieldFrame.x + level.platforms[p].x - 1,
				fieldFrame.y + level.platforms[p].y - 2,
				level.platforms[p].width,
				level.platforms[p].height,
				level.platforms[p].ch,
				level.platforms[p].attr,
				level.platforms[p].upDown,
				level.platforms[p].leftRight,
				level.platforms[p].speed
			);
			Sprite.platforms[Sprite.platforms.length - 1].ini = { 'type' : "platform" };
			Sprite.platforms[Sprite.platforms.length - 1].frame.open();
		}

		for(var p = 0; p < level.powerUps.length; p++) {
			new Sprite.Profile(
				level.powerUps[p].type,
				fieldFrame,
				fieldFrame.x + level.powerUps[p].x - 1,
				fieldFrame.y + level.powerUps[p].y - 2,
				"e",
				"normal"
			);
			Sprite.profiles[Sprite.profiles.length - 1].frame.open();
			Sprite.profiles[Sprite.profiles.length - 1].ini.constantmotion = 0;
		}

	}

	this.powerUp = function(item) {
		item.remove();
		switch(item.ini.powerUpName) {
			case "flightTime":
				flight = flight + Number(item.ini.powerUpValue);
				break;
			case "extraTime":
				time = time + parseInt(item.ini.powerUpValue);
				break;
			default:
				break;
		}
		this.score += ((stats.level + 1) * 10);
		return item;
	}

	this.getcmd = function(userInput) {
		if(ascii(userInput) == 27 && popUpYesNo("Quit the game"))
			quit = true;
		if(userInput == KEY_JUMP) {
			if(player.inJump || player.inFall) {
				player.inJump = false;
				player.inFall = true;
				flightTimer = 0;
				return;
			} else {
				var above = Sprite.checkAbove(player);
				if(above) {
					for(var a in above) {
						if(above[a].ini.type == "platform") {
							return;
						} else {
							player.inJump = true;
							flightTimer = system.timer;
						}
					}
				}
			}
		}
		player.getcmd(userInput);
	}

	this.cycle = function() {

		if(quit)
			return LEVEL_QUIT;

		if(time < 0)
			return LEVEL_TIME;

		timer.cycle();
		Sprite.cycle();

		for(var p = 0; p < Sprite.platforms.length; p++) {

			if(!Sprite.platforms[p].moved)
				continue;

			var n = (Sprite.platforms[p].bearing.match(/n/) !== null);
			var s = (Sprite.platforms[p].bearing.match(/s/) !== null);
			var e = (Sprite.platforms[p].bearing.match(/e/) !== null);
			var w = (Sprite.platforms[p].bearing.match(/w/) !== null);

			if(Sprite.platforms[p].upDown) {

				if(n && Sprite.platforms[p].y <= fieldFrame.y)
					Sprite.platforms[p].reverse();
				else if(s && Sprite.platforms[p].y + Sprite.platforms[p].frame.height >= fieldFrame.y + fieldFrame.height)
					Sprite.platforms[p].reverse();

				if(typeof Sprite.platforms[p].lastAbove != "undefined" && Sprite.platforms[p].lastAbove) {
					for(var a = 0; a < Sprite.platforms[p].lastAbove.length; a++) {
						if(!(Sprite.platforms[p].lastAbove[a] instanceof Sprite.Profile))
							continue;
						if(	!Sprite.platforms[p].lastAbove[a].inJump
							&&
							Sprite.platforms[p].lastAbove[a].x >= Sprite.platforms[p].x
							&&
							Sprite.platforms[p].lastAbove[a].x < Sprite.platforms[p].x + Sprite.platforms[p].frame.width
							&&
							Sprite.platforms[p].lastAbove[a].ini.gravity == 1
							&&
							!(Sprite.checkBelow(Sprite.platforms[p].lastAbove[a]))
						) {
							Sprite.platforms[p].lastAbove[a].moveTo(
								Sprite.platforms[p].lastAbove[a].x,
								Sprite.platforms[p].y - Sprite.platforms[p].lastAbove[a].frame.height
							);
						}
					}
				}
				var above = Sprite.checkAbove(Sprite.platforms[p]);
				if(above && n) {
					for(var a = 0; a < above.length; a++) {
						if(above[a].ini.type == "platform") {
							Sprite.platforms[p].reverse();
							if(above[a].upDown)
								above[a].reverse();
						}
					}
				}
				Sprite.platforms[p].lastAbove = above;

				var below = Sprite.checkBelow(Sprite.platforms[p]);
				if(below && s) {
					for(var b = 0; b < below.length; b++) {
						if(below[b].ini.type == "platform") {
							Sprite.platforms[p].reverse();
							if(below[b].upDown)
								below[b].reverse();
						}
					}
				}

			}

			if(Sprite.platforms[p].leftRight) {

				if(Sprite.platforms[p].x <= fieldFrame.x && w) {
					Sprite.platforms[p].reverse();
				} else if(Sprite.platforms[p].x + Sprite.platforms[p].frame.width >= fieldFrame.x + fieldFrame.width - 1 && !w) {
					Sprite.platforms[p].reverse();
				} else {
					var above = Sprite.checkAbove(Sprite.platforms[p]);
					if(above) {
						for(var a = 0; a < above.length; a++) {
							if(!(above[a] instanceof Sprite.Profile) || above[a].ini.gravity == 0)
								continue;
							if(w)
								above[a].move((above[a].bearing.match(/w/) !== null) ? "forward" : "reverse", true);
							else
								above[a].move((above[a].bearing.match(/e/) !== null) ? "forward" : "reverse", true);
						}
					}
				}

				if(e) {
					var right = Sprite.checkRight(Sprite.platforms[p]);
					if(right) {
						for(var r = 0; r < right.length; r++) {
							if(right[r].ini.type == "platform") {
								Sprite.platforms[p].reverse();
								if(right[r].leftRight)
									right[r].reverse();
							}
						}
					}
				} else {
					var left = Sprite.checkLeft(Sprite.platforms[p]);
					if(left) {
						for(var l = 0; l < left.length; l++) {
							if(left[l].ini.type == "platform") {
								Sprite.platforms[p].reverse();
								if(left[l].leftRight)
									left[l].reverse();
							}
						}
					}
				}

			}

			var collisions = Sprite.checkOverlap(Sprite.platforms[p]);
			if(collisions) {
				for(var c = 0; c < collisions.length; c++) {
					if(collisions[c].ini.type == "platform") {
						Sprite.platforms[p].reverse();
						collisions[c].reverse();
						continue;
					}
					if(n && Sprite.platforms[p].upDown && collisions[c].y < Sprite.platforms[p].y && (collisions[c].ini.type == "player" || collisions[c].ini.gravity == 1))
						collisions[c].moveTo(collisions[c].x, Sprite.platforms[p].y - collisions[c].frame.height);
					else if(s && Sprite.platforms[p].upDown && collisions[c].ini.type != "enemy")
						collisions[c].moveTo(collisions[c].x, Sprite.platforms[p].y + 1);
					if(w && Sprite.platforms[p].leftRight && collisions[c].x < Sprite.platforms[p].x)
						collisions[c].moveTo(Sprite.platforms[p].x - collisions[c].frame.width, collisions[c].y);
					else if(e && Sprite.platforms[p].leftRight && collisions[c].x > Sprite.platforms[p].x)
						collisions[c].moveTo(Sprite.platforms[p].x + Sprite.platforms[p].frame.width, collisions[c].y);
				}
			}

			Sprite.platforms[p].moved = false;

		}

		for(var p = 0; p < Sprite.profiles.length; p++) {

			if(!Sprite.profiles[p].open || Sprite.profiles[p].ini.type != "enemy")
				continue;

			if(Sprite.profiles[p].x <= fieldFrame.x)
				Sprite.profiles[p].turnTo("e");

			if(Sprite.profiles[p].x + Sprite.profiles[p].frame.width >= fieldFrame.x + fieldFrame.width)
				Sprite.profiles[p].turnTo("w");

			if(Sprite.profiles[p].y >= fieldFrame.y + fieldFrame.height)
				Sprite.profiles[p].remove();

			if(Sprite.profiles[p].ini.gravity == 1 && system.timer -  Sprite.profiles[p].lastYMove >= (Sprite.profiles[p].ini.speed * 2)) {
				var below = Sprite.checkBelow(Sprite.profiles[p]);
				if(below) {
					for(var b = 0; b < below.length; b++) {
						if(below[b].ini.type != "player")
							continue;
						Sprite.profiles[p].moveTo(Sprite.profiles[p].x, Sprite.profiles[p].y + 1);
						break;
					}
				}
			}

			var collisions = Sprite.checkOverlap(Sprite.profiles[p]);
			if(collisions) {
				for(var c = 0; c < collisions.length; c++) {
					if(collisions[c].ini.type != "platform")
						continue;
					if(!collisions[c].upDown && !collisions[c].leftRight) {
						Sprite.profiles[p].moveTo(
							(Sprite.profiles[p].bearing.match(/e/) !== null) ? collisions[c].x - Sprite.profiles[p].frame.width : collisions[c].x + collisions[c].frame.width,
							Sprite.profiles[p].y
						);
						Sprite.profiles[p].turn("cw");
						break;
					}
				}
			}

		}

		if(player.x < fieldFrame.x)
			player.move("reverse", true);

		if(player.x + player.frame.width > fieldFrame.x + fieldFrame.width)
			player.move("reverse", true);

		if(player.y >= fieldFrame.y + fieldFrame.height) {
			player.remove();
			return LEVEL_DEAD;
		}

		if(player.inJump && player.y > player.jumpStart - player.ini.jumpheight)
			flightTimer = system.timer;

		if(system.timer - flightTimer < flight && player.ini.gravity == 1)
			player.ini.gravity = 0;
		else if(system.timer - flightTimer > flight && player.ini.gravity == 0)
			player.ini.gravity = 1;

		// Force an overlap if an item directly below is not a platform
		var below = Sprite.checkBelow(player);
		if(below && system.timer - flightTimer > flight) {
			for(var b = 0; b < below.length; b++) {
				if(below[b].ini.type == "platform")
					continue;
				player.moveTo(player.x, player.y + 1);
				break;
			}
		}

		var collisions = Sprite.checkOverlap(player);
		if(collisions) {
			for(var c = 0; c < collisions.length; c++) {
				if(collisions[c].ini.type == "platform") {
					if(player.inJump || player.inFall) {
						player.inJump = false;
						player.inFall = true;
						flightTimer = 0;
						player.moveTo(
							player.x,
							collisions[c].y + 1
						);
					} else {
						return LEVEL_DEAD;
					}
				} else if(collisions[c].ini.type == "enemy" && Sprite.doubleCheckOverlap(collisions[c], player)) {
					return LEVEL_DEAD;
				} else if(collisions[c].ini.type == "door") {
					this.score += ((time + stats.lives + Sprite.profiles.length - 1) * (stats.level + 1));
					return LEVEL_NEXT;
				} else if(collisions[c].ini.type == "powerup") {
					this.powerUp(collisions[c]);
				}
			}
		}

		return LEVEL_CONT;
	}

	this.close = function() {

		for(var s = 0; s < Sprite.platforms.length; s++)
			Sprite.platforms[s].remove();

		for(var s = 0; s < Sprite.profiles.length; s++)
			Sprite.profiles[s].remove();

		Sprite.platforms = [];
		Sprite.profiles = [];

		headFrame.delete();
		fieldFrame.delete();
		footFrame.delete();

	}

	loadLevel(level, stats);

}

var Game = function(firstLevel) {

	var stats = {
		'level' : (typeof firstLevel == "undefined") ? 0 : firstLevel,
		'lives' : 5,
		'score' : 0,
		'powerUps' : []
	};

	var pushScore = function() {
		jsonClient.write(
			dbName,
			"SCORES.LATEST",
			{	"uid" : uid,
				"level" : stats.level,
				"score" : stats.score,
				"date" : time()
			},
			2
		);
	}

	var level;
	var levels = getLevels();
	this.getcmd = function(userInput) {
		if(!(level instanceof Level)) {
			level = new Level(levels[stats.level], stats);
			level.score = stats.score;
			for(var p in stats.powerUps)
				level.powerUp(stats.powerUps[p]);
		}
		level.getcmd(userInput);
		var result = level.cycle();
		var ret = true;
		if(result instanceof Sprite.Profile) { // powerUp
			stats.powerUps.push(result);
		} else if(result == LEVEL_DEAD || result == LEVEL_TIME) {
			stats.powerUps = [];
			stats.lives--;
			if(stats.lives < 1) {
				ret = false;
				stats.score = level.score;
				stats.level++;
				pushScore();
			}
			msg = (result == LEVEL_DEAD) ? "You done died!" : "You done ran out of time!";
			popUpMessage(msg);
			level.close();
			level = false;
		} else if(result == LEVEL_NEXT) {
			stats.score = level.score;
			level.close();
			level = false;
			stats.level++;
			if(stats.level % 5 == 0) {
				var extra = 1 + Math.floor(stats.score / 10000);
				popUpMessage(
					format(
						"You got %s extra %s!",
						extra, (extra > 1) ? "lives" : "life"
					)
				);
				stats.lives += extra;
			}
			if(stats.level >= levels.length) {
				pushScore();
				popUpMessage("Congratulations - the chicken has been delivered!");
				victoryLap();
				ret = false;
			}
		} else if(result == LEVEL_QUIT) {
			stats.score = level.score;
			level.close();
			level = false;
			stats.level++;
			pushScore();
			ret = false;
		}
		return ret;
	}

}

var victoryLap = function() {

	var timer = new Timer();

	var vFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	vFrame.open();

	var enemies = [
		"ghost-blue",
		"robot-cyan",
		"dwer",
		"lester",
		"telegard-gray",
		"jerf",
		"alien-green",
		"demon-red"
	];
	for(var e = 0; e < enemies.length; e++) {
		enemies[e] = new Sprite.Profile(
			enemies[e],
			vFrame,
			vFrame.x + ((e + 1) * 6),
			vFrame.y,
			"e",
			"normal"
		);
		enemies[e].frame.open();
		enemies[e].ini.gravity = 0;
	}

	var player = new Sprite.Profile(
		"chicken",
		vFrame,
		enemies[enemies.length - 1].x + 6,
		vFrame.y,
		"e",
		"normal"
	);
	player.frame.open();
	player.ini.constantmotion = 1;
	player.ini.gravity = 0;

	for(var y = 4, e = true; y < vFrame.height; y = y + 5) {
		var p = new Sprite.Platform(
			vFrame,
			((e) ? vFrame.x : vFrame.x + 5),
			vFrame.y + y,
			vFrame.width - 5,
			1,
			ascii(219),
			DARKGRAY,
			false,
			false,
			0
		);
		p.frame.open();
		e = (e) ? false : true;
	}

	var door = new Sprite.Profile(
		"door",
		vFrame,
		vFrame.x + 5,
		Sprite.platforms[Sprite.platforms.length - 1].y - 4,
		"e",
		"normal"
	);
	door.frame.open();
	door.ini.gravity = 0;

	var changeColour = function() {
		for(var p in Sprite.platforms) {
			Sprite.platforms[p].attr = Math.floor((Math.random() * 15) + 1);
			Sprite.platforms[p].frame.draw();
		}
	}
	timer.addEvent(500, true, changeColour);

	while(enemies[0].open) {
		var overlap = Sprite.checkOverlap(door);
		if(overlap) {
			for(var o in overlap)
				overlap[o].remove();
			if(enemies[1].open && !enemies[2].open) {
				door.ini.constantmotion = 1;
				door.ini.speed = enemies[0].ini.speed;
			}
		}
		if(ascii(console.inkey(K_NONE, 1)) == 27)
			break;
		timer.cycle();
		Sprite.cycle();
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
		for(var p = 0; p < Sprite.profiles.length; p++) {
			if(Sprite.profiles[p].ini.type == "door" || !Sprite.profiles[p].open)
				continue;
			if(Sprite.profiles[p].bearing == "e" && Sprite.profiles[p].x == vFrame.x + vFrame.width - 5) {
				Sprite.profiles[p].turnTo("w");
				Sprite.profiles[p].moveTo(Sprite.profiles[p].x, Sprite.profiles[p].y + 5);
			} else if(Sprite.profiles[p].bearing == "w" && Sprite.profiles[p].x == vFrame.x) {
				Sprite.profiles[p].turnTo("e");
				Sprite.profiles[p].moveTo(Sprite.profiles[p].x, Sprite.profiles[p].y + 5);
			}
		}
		mswait(5);
	}

	for(var s in Sprite.profiles)
		Sprite.profiles[s].remove();
	Sprite.profiles = [];

	for(var s in Sprite.platforms)
		Sprite.platforms[s].remove();
	Sprite.platforms = [];
}

var main = function() {

	var menu, game;
	while(!js.terminated) {

		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);

		var userInput = console.inkey(K_NONE, 5);
		if(state.menu) {
			if(!(menu instanceof Menu))
				menu = new Menu();
			menu.getcmd(userInput);
			if(!state.menu) {
				menu.close();
				menu = false;
			}
		} else if(state.play) {
			if(!(game instanceof Game))
				game = new Game(initLevel);
			if(!game.getcmd(userInput)) {
				state.menu = true;
				state.play = false;
				game = false;
			}
		} else if(state.help) {
			if(userInput == "")
				continue;
			helpFrame.delete();
			state.help = false;
			state.menu = true;
		} else if(state.scores) {
			if(userInput == "")
				continue;
			scoreFrame.delete();
			state.scores = false;
			state.menu = true;
		} else if(state.quit) {
			return;
		}

	}
}

try {
	init();
	main();
	cleanUp();
} catch(err) {
	log(LOG_ERR, err);
}
exit();
