load("sbbsdefs.js");
load("frame.js");
load("sprite.js");
load("tree.js");
load("json-client.js");
load("event-timer.js");

js.branch_limit = 0;
js.time_limit = 0;

const	LEVEL_CONT = 0,
	 	LEVEL_DEAD = 1,
		LEVEL_NEXT = 2;

var frame,
	popUp,
	attr,
	sysStatus,
	jsonClient,
	state = {
		'menu' : true,
		'help' : false,
		'play' : false,
		'scores' : false,
		'paused' : false,
		'quit' : false
	},
	dbName = "GOOBLE2",
	colours = [BLUE, LIGHTBLUE, CYAN, LIGHTCYAN, WHITE];

var setState = function(s) {
	for(var ss in state)
		state[ss] = (ss == s);
}

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

var PopUp = function(parentFrame, type, message) {
	var yesNoPrompt = "<\1h\1cY\1h\1w/\1h\1cN\1h\1w>";
	var anyKeyPrompt = "<\1h\1cHit any key to continue\1h\1w>";
	var popUpFrame = new Frame(
		parentFrame.x,
		parentFrame.y + 10,
		4,
		4,
		BG_BLACK|WHITE,
		parentFrame
	);
	var subPopUpFrame = new Frame(
		popUpFrame.x + 1,
		popUpFrame.y + 1,
		popUpFrame.width - 2,
		1,
		BG_BLACK|WHITE,
		popUpFrame
	);
	popUpFrame.open();
	popUpFrame.top();
	popUpFrame.gotoxy(1, popUpFrame.height - 1);
	if(type == "yesno") {
		popUpFrame.width = (Math.max(console.strlen(yesNoPrompt), message.length)) + 4;
		popUpFrame.center(yesNoPrompt);
	}
	if(type == "anykey") {
		popUpFrame.width = (Math.max(console.strlen(anyKeyPrompt), message.length)) + 4;
		popUpFrame.center(anyKeyPrompt);
	}
	popUpFrame.moveTo(parentFrame.x + Math.floor((parentFrame.width - popUpFrame.width) / 2), popUpFrame.y);
	popUpFrame.drawBorder(colours);
	subPopUpFrame.width = popUpFrame.width - 2;
	subPopUpFrame.moveTo(popUpFrame.x + 1, popUpFrame.y + 1);
	subPopUpFrame.center(message);
	this.close = function() {
		subPopUpFrame.delete();
		popUpFrame.delete();
	}
}

var Menu = function() {

	var menuFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	menuFrame.open();
	menuFrame.load(js.exec_dir + "splash.bin", 80, 24);

	var treeFrame = new Frame(
		menuFrame.x + 30,
		menuFrame.y + 12,
		20,
		6,
		BG_BLACK|WHITE,
		menuFrame
	);
	treeFrame.drawBorder(colours);

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
	tree.addItem("|Play", setState, "play");
	tree.addItem("|Help", setState, "help");
	tree.addItem("|Scores", setState, "scores");
	tree.addItem("|Quit", setState, "quit");
	tree.open();

	this.getcmd = function(userInput) {
		tree.getcmd(userInput);
	}

	this.close = function() {
		tree.close();
		menuFrame.delete();
	}

}

var Level = function(stats) {

	var gameFrame,
		fieldFrame,
		statsFrame,
		scoreFrame,
		lifeFrame,
		fruit = true,
		warnEvent = {},
		inedibleEvent = {},
		fruitEvent = {},
		fruitRemoveEvent = {},
		timer = new Timer(),
		pelletCount = 0,
		positions = [],
		pelletPoints = 10,
		powerPelletPoints = 50,
		ghostPoints = 200, // Doubles each time a ghost is eaten
		fruitPoints = 100; // Multiplied by level number
		self = this,
		stats = stats, // A is A, bitches
		lifeThreshold = Math.max(10000, Math.ceil(stats.score/10000) * 10000);

	this.__defineGetter__("score", function() {	return stats.score;	});

	this.__defineSetter__(
		"score",
		function(s) {
			stats.score = parseInt(s);
			scoreFrame.clear();
			scoreFrame.putmsg(stats.score);
		}
	);

	this.__defineGetter__("lives", function() { return stats.lives; });

	this.__defineSetter__(
		"lives",
		function(l) {
			stats.lives = parseInt(l);
			lifeFrame.clear();
			lifeFrame.putmsg(stats.lives);
		}
	);

	var initFrames = function() {
		gameFrame = new Frame(
			frame.x,
			frame.y,
			frame.width,
			frame.height,
			BG_BLACK,
			frame
		);
		fieldFrame = new Frame(
			gameFrame.x,
			gameFrame.y,
			gameFrame.width,
			gameFrame.height - 1,
			BG_BLACK,
			gameFrame
		);
		statsFrame = new Frame(
			gameFrame.x,
			gameFrame.y + gameFrame.height - 1,
			gameFrame.width,
			1,
			BG_BLUE|WHITE,
			gameFrame
		);
		statsFrame.putmsg(
			format("Level: %-08s Lives:          Score:", stats.level)
		);
		lifeFrame = new Frame(
			statsFrame.x + 23,
			statsFrame.y,
			8,
			1,
			BG_BLUE|WHITE,
			statsFrame
		);
		lifeFrame.putmsg(stats.lives);
		scoreFrame = new Frame(
			statsFrame.x + 39,
			statsFrame.y,
			statsFrame.width - 40,
			1,
			BG_BLUE|WHITE,
			statsFrame
		);
		scoreFrame.putmsg(stats.score);
		gameFrame.open();
	}

	// Walls, dicks, and balls
	var initMaze = function() {
		var makeWall = function(xOff, yOff, width, height) {
			new Sprite.Platform(
				fieldFrame,
				fieldFrame.x + xOff,
				fieldFrame.y + yOff,
				width,
				height,
				ascii(219),
				LIGHTBLUE
			);
		}
		// Top wall
		makeWall(1, 0, fieldFrame.width - 1, 1);
		// Bottom wall
		makeWall(0, fieldFrame.height - 1, fieldFrame.width, 1);
		// Top left wall
		makeWall(0, 0, 1, Math.floor(fieldFrame.height / 2) - 1);
		// Top left wall's dong
		makeWall(0, 9, 6, 1);
		// Bottom left wall
		makeWall(0, fieldFrame.height - Math.floor(fieldFrame.height / 2) + 1, 1, Math.floor(fieldFrame.height / 2) - 2);
		// Bottom left wall's dong
		makeWall(1, 13, 5, 1);
		// Top right wall
		makeWall(fieldFrame.width - 1, 0, 1, Math.floor(fieldFrame.height / 2) - 2);
		// Top right wall's dong
		makeWall(fieldFrame.width - 6, 9, 6, 1);
		// Bottom right wall
		makeWall(fieldFrame.width - 1, fieldFrame.height - Math.floor(fieldFrame.height / 2) + 1, 1, Math.floor(fieldFrame.height / 2) - 2);
		// Bottom right wall's dong
		makeWall(fieldFrame.width - 6, 13, 5, 1);
		// Left stalactite
		makeWall(23, 0, 2, 6);
		// Right stalactite
		makeWall(fieldFrame.width - 25, 0, 2, 6);
		// Left stalagmite
		makeWall(23, fieldFrame.height - 6, 2, 5);
		// Right stalagmite
		makeWall(fieldFrame.width - 25, fieldFrame.height - 6, 2, 5);
		// Top centre wall
		makeWall(30, 4, 20, 2);
		// Bottom centre wall
		makeWall(30, fieldFrame.height - 6, 20, 2);
		// Ghost-box top-left
		makeWall(25, 9, 12, 1);
		// Ghost-box top-right
		makeWall(26 + 16, 9, 13, 1);
		// Ghost-box bottom-left
		makeWall(25, 13, 12, 1);
		// Ghost-box bottom-right
		makeWall(26 + 16, 13, 13, 1);
		// Ghost-box left wall
		makeWall(25, 10, 2, 3);
		// Ghost-box right-wall
		makeWall(25 + 28, 10, 2, 3);
		// Top-left tee-testicles
		makeWall(6, 4, 12, 2);
		// Top-left tee-dong
		makeWall(11, 5, 2, 5);
		// Top-right tee-testicles
		makeWall(fieldFrame.width - 18, 4, 12, 2);
		// Top-right tee-dong
		makeWall(fieldFrame.width - 13, 5, 2, 5);
		// Bottom-left tee-testicles
		makeWall(6, fieldFrame.height - 6, 12, 2);
		// Bottom-left tee-dong
		makeWall(11, fieldFrame.height - 10, 2, 4);
		// Bottom-right tee-testicles
		makeWall(fieldFrame.width - 18, fieldFrame.height - 6, 12, 2);
		// Bottom-right tee-dong
		makeWall(fieldFrame.width - 13, fieldFrame.height - 10, 2, 4);
		// Left-centre wall
		makeWall(18, 9, 2, 5);
		// Right-centre wall
		makeWall(fieldFrame.width - 20, 9, 2, 5);
		for(var p = 0; p < Sprite.platforms.length; p++)
			Sprite.platforms[p].frame.open();
	}

	var initPellets = function() {
		var pellets = [];
		pellets[2] = [9, 14, 19, 27, 32, 37, 42, 47, 52, 60, 65, 70];
		pellets[7] = [3, 8, 14, 19, 24, 29, 34, 39, 44, 49, 54, 59, 64, 71, 76];
		pellets[11] = [4, 9, 14, 22, 57, 65, 70, 75];
		pellets[15] = pellets[7];
		pellets[20] = pellets[2];
		for(var y = 0; y < pellets.length; y++) {
			if(!Array.isArray(pellets[y]))
				continue;
			for(var x = 0; x < pellets[y].length; x++) {
				new Sprite.Profile(
					"pellet",
					fieldFrame,
					fieldFrame.x + pellets[y][x],
					fieldFrame.y + y,
					"n",
					"normal"
				);
				Sprite.profiles[Sprite.profiles.length - 1].frame.open();
			}
		}
		var powerPellets = [];
		powerPellets[2] = [3, 76];
		powerPellets[20] = [3, 76];
		for(var y = 0; y < powerPellets.length; y++) {
			if(!Array.isArray(powerPellets[y]))
				continue;
			for(var x = 0; x < powerPellets[y].length; x++) {
				new Sprite.Profile(
					"powerpellet",
					fieldFrame,
					fieldFrame.x + powerPellets[y][x],
					fieldFrame.y + y,
					"n",
					"normal"
				);
				Sprite.profiles[Sprite.profiles.length - 1].frame.open();
			}
		}
		pelletCount = Sprite.profiles.length;
	}

	var initSprites = function() {
		new Sprite.Aerial(
			"gooble",
			fieldFrame,
			fieldFrame.x + 38,
			fieldFrame.y + 14,
			"e",
			"normal"
		);
		Sprite.aerials[Sprite.aerials.length - 1].frame.open();
		positions.push(
			{	'x' : Sprite.aerials[Sprite.aerials.length - 1].x,
				'y' : Sprite.aerials[Sprite.aerials.length - 1].y
			}
		);
		for(var g = 1; g < 5; g++) {
			new Sprite.Aerial(
				"ghost0" + g,
				fieldFrame,
				fieldFrame.x + 21 + (g * 6),
				fieldFrame.y + 10,
				(g > 2) ? "w" : "e",
				"inedible"
			);
			Sprite.aerials[Sprite.aerials.length - 1].frame.open();
			Sprite.aerials[Sprite.aerials.length - 1].freeRange = false;
			positions.push(
				{	'x' : Sprite.aerials[Sprite.aerials.length - 1].x,
					'y' : Sprite.aerials[Sprite.aerials.length - 1].y
				}
			);
			Sprite.aerials[Sprite.aerials.length - 1].ini.constantmotion = 0;
			timer.addEvent(
				2000,
				false,
				function() {
					for(var g = 1; g < 5; g++)
						Sprite.aerials[g].ini.constantmotion = 1;
				}
			);
		}
		if(fruit instanceof Sprite.Aerial)
			return;
		fruitEvent = timer.addEvent(
			15000,
			false,
			function() {
				fruit = new Sprite.Aerial(
					"fruit",
					fieldFrame,
					fieldFrame.x + 37,
					fieldFrame.y + 10,
					"n",
					"strawberry"
				);
				var pos = [];
				for(var p in fruit.positions)
					pos.push(p);
				fruit.position = pos[(stats.level - 1 % pos.length)];
				fruit.frame.open();
				fruitRemoveEvent = timer.addEvent(
					15000,
					false,
					function() {
						if(fruit instanceof Sprite.Aerial && fruit.open)
							fruit.remove();
					}
				);
			}
		)
	}

	var init = function() {
		initFrames();
		initMaze();
		initPellets();
		initSprites();
	}

	this.getcmd = function(userInput) {
		if(userInput == "")
			return;
		if(userInput == "Q") {
			setState("paused");
			return;
		}
		var wasStopped = false;
		if(Sprite.aerials[0].ini.constantmotion == 0) {
			wasStopped = true;
			if(userInput == KEY_RIGHT && Sprite.aerials[0].bearing == "e")
				return;
			if(userInput == KEY_LEFT && Sprite.aerials[0].bearing == "w")
				return;
			if(userInput == KEY_UP && Sprite.aerials[0].bearing == "n")
				return;
			if(userInput == KEY_DOWN && Sprite.aerials[0].bearing == "s")
				return;
		}
		Sprite.aerials[0].getcmd(userInput);
		if(wasStopped && Sprite.aerials[0].bearing != Sprite.aerials[0].lastBearing)
			Sprite.aerials[0].ini.constantmotion = 1;
	}

	var checkSide = function(side) {
		if(!side)
			return false;
		for(var s = 0; s < side.length; s++) {
			if(side[s] instanceof Sprite.Platform)
				return true;
		}
		return false;
	}

	var eat = function(sprite) {
		if(!sprite.open)
			return;
		switch(sprite.ini.type) {
			case "enemy":
				self.score = self.score + ghostPoints;
				ghostPoints = ghostPoints * 2;
				if(ghostPoints > 1600)
					ghostPoints = 200;
				sprite.position = "inedible";
				sprite.turnTo("e");
				sprite.freeRange = false;
				sprite.moveTo(fieldFrame.x + 27, fieldFrame.y + 10);
				sprite.ini.constantmotion = 0;
				timer.addEvent(
					2000,
					false,
					function(s) {
						s.ini.constantmotion = 1;
					},
					[sprite]
				);
				break;
			case "pellet":
				self.score = self.score + pelletPoints;
				pelletCount--;
				sprite.remove();
				break;
			case "powerpellet":
				self.score = self.score + powerPelletPoints;
				pelletCount--;
				sprite.remove();
				for(var s = 1; s < 5; s++)
					Sprite.aerials[s].position = "edible";
				warnEvent.abort = true;
				inedibleEvent.abort = true;
				warnEvent = timer.addEvent(
					Math.max(1000, (11000 - (stats.level * 1000))),
					false,
					function() {
						for(var s = 1; s < 5; s++) {
							if(Sprite.aerials[s].position == "edible")
								Sprite.aerials[s].position = "warning";
						}
						inedibleEvent = timer.addEvent(
							Math.max(1000, (6000 - (stats.level * 500))),
							false,
							function() {
								for(var s = 1; s < 5; s++)
									Sprite.aerials[s].position = "inedible";
							}
						);
					}
				);
				break;
			case "fruit":
				self.score = self.score + (fruitPoints * stats.level);
				sprite.remove();
				fruitRemoveEvent.abort = true;
				break;
			default:
				break;
		}
		if(stats.score >= lifeThreshold) {
			self.lives++;
			lifeThreshold += 10000;
			timer.addEvent(
				250,
				20,
				function() {
					lifeFrame.clear();
					lifeFrame.attr = ((lifeFrame.attr == (BG_BLUE|WHITE)) ? (BG_BLUE|LIGHTRED) : (BG_BLUE|WHITE));
					lifeFrame.putmsg(self.lives);
				}
			)
		}
	}

	this.reset = function() {
		for(var a = 0; a < Sprite.aerials.length; a++)
			Sprite.aerials[a].remove();
		Sprite.aerials = [];
		positions = [];
		initSprites();
		statsFrame.clear();
		statsFrame.putmsg(
			format(
				"Level: %-08s Lives:          Score:",
				stats.level, stats.lives
			)
		);
	}

	this.cycle = function() {
		timer.cycle();
		for(var a = 0; a < 5; a++) {
			if(	positions[a].x == Sprite.aerials[a].x
				&&
				positions[a].y == Sprite.aerials[a].y
				&&
				Sprite.aerials[a].lastBearing == Sprite.aerials[a].bearing
			) {
				continue;
			}
			positions[a] = {
				'x' : Sprite.aerials[a].x,
				'y' : Sprite.aerials[a].y
			};
			if(Sprite.aerials[a].x + Sprite.aerials[a].frame.width < fieldFrame.x + 1)
				Sprite.aerials[a].moveTo(fieldFrame.x + fieldFrame.width - 2, Sprite.aerials[a].y);
			else if(Sprite.aerials[a].x > fieldFrame.x + fieldFrame.width - 1)
				Sprite.aerials[a].moveTo(fieldFrame.x + 2, Sprite.aerials[a].y);
			if(Sprite.aerials[a].ini.type == "enemy") {
				if(Sprite.doubleCheckOverlap(Sprite.aerials[0], Sprite.aerials[a])) {
					if(	Sprite.aerials[a].position == "edible"
						||
						Sprite.aerials[a].position == "warning"
					) {
						eat(Sprite.aerials[a]);
					} else {
						return LEVEL_DEAD;
					}
				}
				if(!Sprite.aerials[a].freeRange) {
					if(	Sprite.aerials[a].y == fieldFrame.y + 10
						&&
						Sprite.aerials[a].x > fieldFrame.x + 36
						&&
						Sprite.aerials[a].x  + Sprite.aerials[a].frame.width - 1 < fieldFrame.x + 42
						&&
						Sprite.aerials[a].bearing.match(/n|s/) === null
					) {
						Sprite.aerials[a].turnTo(
							(Sprite.aerials[a].bearing == "e") ? "n" : "s"
						);
					} else if(
						(	Sprite.aerials[a].bearing == "n"
							&&
							Sprite.aerials[a].y <= fieldFrame.y + 7
						)
						||
						(	Sprite.aerials[a].bearing == "s"
							&&
							Sprite.aerials[a].y >= fieldFrame.y + 13
						)
					) {
						Sprite.aerials[a].freeRange = true;
					}
					continue;
				}
				// If Math.random() * 1 is greater than prob, ghost will turn
				// at an intersection (when it could just keep moving.)
				var prob = .6;
				var above = checkSide(Sprite.checkAbove(Sprite.aerials[a]));
				var below = checkSide(Sprite.checkBelow(Sprite.aerials[a]));
				var left = checkSide(Sprite.checkLeft(Sprite.aerials[a]));
				var right = checkSide(Sprite.checkRight(Sprite.aerials[a]));
				// Handle ghosts encountering obstacles or intersections in
				// the maze. Pretty sucky and random, but good enough for now.
				// Should try to make the ghosts smarter at some point.
				switch(Sprite.aerials[a].bearing) {
					case 'n':
						// Comments for this case apply to the others, with
						// appropriate directional substitutions.
						if(above) {
							// Must turn w, e, or s
							// Favour options other than reversing course
							var options = [];
							if(!left)
								options.push("w", "w");
							if(!right)
								options.push("e", "e");
							if(!below)
								options.push("s");
							Sprite.aerials[a].turnTo(
								options[
									Math.floor(Math.random() * options.length)
								]
							);
						} else if(!left && !right) {
							// Have the option of turning e or w
							// Decide whether or not to turn at all
							// If turning, it's a 50/50 shot which way
							if(Math.random() * 1 > prob)
								Sprite.aerials[a].turnTo((Math.random() * 1 > .5) ? "w" : "e");
						// Have the option of turning w
						// Decide whether or not to do so:
						} else if(!left && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("w");
						// Have the option of turning e
						// Decide whether or not to do so:
						} else if(!right && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("e");
						}
						break;
					case 's':
						if(below) {
							var options = [];
							if(!left)
								options.push("w", "w");
							if(!right)
								options.push("e", "e");
							if(!above)
								options.push("n");
							Sprite.aerials[a].turnTo(
								options[
									Math.floor(Math.random() * options.length)
								]
							);
						} else if(!left && !right) {
							if(Math.random() * 1 > prob)
								Sprite.aerials[a].turnTo((Math.random() * 1 > .5) ? "w" : "e");
						} else if(!left && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("w");
						} else if(!right && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("e");
						}
						break;
					case 'e':
						if(right) {
							var options = [];
							if(!left)
								options.push("w");
							if(!above)
								options.push("n", "n");
							if(!below)
								options.push("s", "s");
							Sprite.aerials[a].turnTo(
								options[
									Math.floor(Math.random() * options.length)
								]
							);
						} else if(!above && !below) {
							if(Math.random() * 1 > prob)
								Sprite.aerials[a].turnTo((Math.random() * 1 > .5) ? "n" : "s");
						} else if(!above && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("n");
						} else if(!below && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("s");
						}
						break;
					case 'w':
						if(left) {
							var options = [];
							if(!right)
								options.push("e");
							if(!above)
								options.push("n", "n");
							if(!below)
								options.push("s", "s");
							Sprite.aerials[a].turnTo(
								options[
									Math.floor(Math.random() * options.length)
								]
							);
						} else if(!above && !below) {
							if(Math.random() * 1 > prob)
								Sprite.aerials[a].turnTo((Math.random() * 1 > .5) ? "n" : "s");
						} else if(!above && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("n");
						} else if(!below && Math.random() * 1 > prob) {
							Sprite.aerials[a].turnTo("s");
						}
						break;
					default:
						break;
				}
			} else if(Sprite.aerials[a].ini.type == "player") {
				switch(Sprite.aerials[a].bearing) {
					case 'n':
						Sprite.aerials[a].position = (Sprite.aerials[a].y % 2 == 0) ? "normal" : "closed";
						if(checkSide(Sprite.checkAbove(Sprite.aerials[a])))
							Sprite.aerials[a].ini.constantmotion = 0;
						break;
					case 's':
						Sprite.aerials[a].position = (Sprite.aerials[a].y % 2 == 0) ? "normal" : "closed";
						if(checkSide(Sprite.checkBelow(Sprite.aerials[a])))
							Sprite.aerials[a].ini.constantmotion = 0;
						break;
					case 'e':
						Sprite.aerials[a].position = (Sprite.aerials[a].x % 2 == 0) ? "normal" : "closed";
						if(checkSide(Sprite.checkRight(Sprite.aerials[a])))
							Sprite.aerials[a].ini.constantmotion = 0;
						break;
					case 'w':
						Sprite.aerials[a].position = (Sprite.aerials[a].x % 2 == 0) ? "normal" : "closed";
						if(checkSide(Sprite.checkLeft(Sprite.aerials[a])))
							Sprite.aerials[a].ini.constantmotion = 0;
						break;
					default:
						break;
				}
				var collisions = Sprite.checkOverlap(Sprite.aerials[a]);
				if(!collisions)
					continue;
				for(var c = 0; c < collisions.length; c++) {
					if(collisions[c].ini.type == "enemy" && collisions[c].position == "inedible") {
						warnEvent.abort = true;
						inedibleEvent.abort = true;
						fruitEvent.abort = true;
						fruitRemoveEvent.abort = true;
						return LEVEL_DEAD;
					}
					eat(collisions[c]);
				}
				if(pelletCount == 0)
					return LEVEL_NEXT;
			}
		}
		return LEVEL_CONT;
	}

	this.close = function() {
		warnEvent.abort = true;
		inedibleEvent.abort = true;
		fruitEvent.abort = true;
		fruitRemoveEvent.abort = true;
		for(var s = 0; s < Sprite.platforms.length; s++)
			Sprite.platforms[s].remove();
		for(var s = 0; s < Sprite.profiles.length; s++)
			Sprite.profiles[s].remove();
		for(var s = 0; s < Sprite.aerials.length; s++)
			Sprite.aerials[s].remove();
		Sprite.platforms = [];
		Sprite.profiles = [];
		Sprite.aerials = [];
		scoreFrame.delete();
		statsFrame.delete();
		fieldFrame.delete();
		gameFrame.delete();
	}

	init();

}

var pushScore = function(stats) {
	try {
		jsonClient.write(
			dbName,
			"SCORES.LATEST",
			{	'user' : user.alias,
				'system' : system.name,
				'level' : stats.level,
				'score' : stats.score,
				'date' : time()
			},
			2
		);
	} catch(err) {
		log(LOG_ERR, err);
	}
}

var highScores = function() {
	var scoreFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);
	var scoreSubFrame = new Frame(
		scoreFrame.x + 1,
		scoreFrame.y + 2,
		scoreFrame.width - 2,
		scoreFrame.height - 4,
		BG_BLACK|WHITE,
		scoreFrame
	);
	scoreFrame.drawBorder(colours);
	scoreFrame.gotoxy(1, 2);
	scoreFrame.center("Gooble Gooble: High Scores");
	scoreFrame.gotoxy(1, scoreFrame.height - 1);
	scoreFrame.center("< Press any key to continue >");
	scoreFrame.open();
	scoreSubFrame.putmsg(format("\1h\1c%-55s Level %16s\r\n\1h\1w", "Player", "Score"));
	var hs = jsonClient.read(dbName, "SCORES.HIGH", 1);
	for(var s in hs) {
		scoreSubFrame.putmsg(
			format(
				"%-55s %5s %16s\r\n",
				hs[s].user + "@" + hs[s].system,
				hs[s].level,
				hs[s].score
			)
		);
	}
	if(scoreSubFrame.data_height > scoreSubFrame.height)
		scoreSubFrame.scrollTo(0, 0);
	frame.cycle();
	console.getkey(K_NOCRLF|K_NOECHO);
	scoreFrame.delete();
}

var help = function() {
	var helpFrame = new Frame(
		frame.x + 4,
		frame.y + 2,
		71,
		19,
		BG_BLACK|WHITE,
		frame
	);
	var helpSubFrame = new Frame(
		helpFrame.x + 1,
		helpFrame.y + 1,
		helpFrame.width - 2,
		helpFrame.height - 2,
		BG_BLACK|WHITE,
		helpFrame
	);
	helpFrame.drawBorder(colours);
	helpSubFrame.load(js.exec_dir + "help.bin", 69, 17);
	helpFrame.open();
	frame.cycle();
	console.getkey(K_NOCRLF|K_NOECHO);
	helpFrame.delete();
}

var initDisplay = function() {
	attr = console.attributes;
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
	sysStatus = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;
	initDisplay();
	try {
		initJSON();
	} catch(err) {
		log(LOG_ERR, err);
	}
}

var cleanUp = function() {
	frame.close();
	frame.delete();
	console.attributes = attr;
	bbs.sys_status = sysStatus;
	console.clear();
}

var main = function() {
	var stats = {
		'level' : 1,
		'lives' : 3,
		'score' : 0
	};
	while(!js.terminated) {
		var userInput = console.inkey(K_NONE|K_UPPER, 5);
		if(state.menu) {
			if(typeof menu == "undefined")
				var menu = new Menu();
			menu.getcmd(userInput);
			if(!state.menu) {
				menu.close();
				menu = undefined;
			}
		} else if(state.help) {
			help();
			setState("menu");
		} else if(state.play) {
			if(typeof level == "undefined")
				var level = new Level(stats);
			level.getcmd(userInput);
			var result = level.cycle();
			switch(result) {
				case LEVEL_DEAD:
					stats.score = level.score;
					level.lives--;
					if(level.lives < 0) {
						pushScore(stats);
						stats.level = 1;
						stats.lives = 3;
						stats.score = 0;
						setState("menu");
						popUp = new PopUp(frame, "anykey", "Game over!");
						frame.cycle();
						console.getkey(K_NOCRLF|K_NOECHO);
						popUp.close();
						popUp = undefined;
						level.close();
						level = undefined;
					} else {
						popUp = new PopUp(frame, "anykey", "You died!");
						frame.cycle();
						console.getkey(K_NOCRLF|K_NOECHO);
						popUp.close();
						popUp = undefined;
						level.reset();
					}
					break;
				case LEVEL_NEXT:
					stats.level++;
					stats.score = level.score;
					stats.lives = level.lives;
					level.close();
					level = new Level(stats);
					break;
				default:
					break;
			}
			Sprite.cycle();
		} else if(state.scores) {
			try{
				highScores();
			} catch(err) {
				log(LOG_ERR, err);
			}
			setState("menu");
		} else if(state.paused) {
			if(typeof popUp == "undefined")
				popUp = new PopUp(frame, "yesno", "Quit the game?");
			if(userInput == "Y") {
				stats.score = level.score;
				level.close();
				level = undefined;
				pushScore(stats);
				stats.level = 1;
				stats.lives = 3;
				stats.score = 0;
				setState("menu");
				popUp.close();
				popUp = undefined;
			} else if(userInput == "N") {
				setState("play");
				popUp.close();
				popUp = undefined;
			}
		} else if(state.quit) {
			break;
		}
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
	}
}

try {
	init();
	main();
	cleanUp();
} catch(err) {
	log(LOG_ERR, err);
}