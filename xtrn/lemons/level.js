// This is the actual game.  It's a disaster, but it gets the job done.
var Level = function(l, n) {

	// Scope some variables for use by functions and methods in this object
	var timer = new Timer(), // A Timer
		// A place to organize this level's frames
		frames = {
			'counters' : {}
		},
		cursor,		// The 1x1 cursor frame
		countDown,	// Level timeout
		lost = 0,	// Lemons lost
		saved = 0,	// Lemons saved
		total = 0,	// Total lemon count for this level
		// Placeholder values for how many of each skill may be assigned
		quotas = {
			'basher' : 0,
			'blocker' : 0,
			'bomber' : 0,
			'builder' : 0,
			'climber' : 0,
			'digger' : 0
		},
		nuked = false;

	// The parent Game object will want to read this
	this.score = 0;

	/*	Change the colour of a lemon's peel without altering the colour of its
		shoes or 'nuked' position. */
	var colorize = function(sprite, colour) {
		var fgmask = (1<<0)|(1<<1)|(1<<2)|(1<<3);
		for(var y = 0; y < sprite.frame.data.length; y++) {
			for(var x = 0; x < 9; x++) {
				sprite.frame.data[y][x].attr&=~fgmask;
				if((y == 2 || y == 5) && (x == 0 || x == 2 || x == 3 || x == 5 || x == 6 || x == 8))
					sprite.frame.data[y][x].attr=BG_BLACK|BROWN;
				else
					sprite.frame.data[y][x].attr|=colour;
			}
		}
		sprite.frame.invalidate();
	}

	//	Turn skilled lemon back into an ordinary zombie-lemon.
	var lemonize = function(sprite) {
		colorize(sprite, COLOUR_LEMON);
		sprite.ini.constantmotion = 1;
		sprite.ini.gravity = 1;
		sprite.ini.speed = .25;
		sprite.ini.skill = "lemon";
		if(typeof sprite.ini.buildCount != "undefined")
			delete sprite.ini.buildCount;
		if(typeof sprite.ini.lastBuild != "undefined")
			delete sprite.ini.lastBuild;
		if(typeof sprite.ini.climbStart != "undefined")
			delete sprite.ini.climbStart;
		if(typeof sprite.ini.lastDig != "undefined")
			delete sprite.ini.lastDig;
	}

	// Remove the lemon from the screen
	var remove = function(sprite) {
		if(!sprite.open)
			return;
		sprite.remove();
	}

	/*	If a lemon encounters blocks stacked >= 2 characters high it
		will turn and start walking the other way.
		If a lemon encounters a block 1 character high, it will climb
		on top of it (stairs, etc.)
		Normal, bomber, and builder lemons should be passed to this function.
	*/
	var turnOrClimbIfObstacle = function(sprite) {

		var beside = (sprite.bearing == "w") ? Sprite.checkLeft(sprite) : Sprite.checkRight(sprite);
		if(!beside)
			beside = Sprite.checkOverlap(sprite);
		if(!beside)
			return;

		for(var b = 0; b < beside.length; b++) {

			if(	beside[b].ini.type != "block"
				&&
				beside[b].ini.type != "lemon"
				&&
				beside[b].ini.type != "hazard"
			) {
				continue;
			}

			if(	beside[b].ini.type == "block"
				&&
				beside[b].y == sprite.y + sprite.ini.height - 1
			) {
				sprite.moveTo(
					(sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1),
					sprite.y - 1
				);
				if(Sprite.checkOverlap(sprite)) {
					sprite.moveTo(
						(sprite.bearing == "w") ? (sprite.x + 1) : (sprite.x - 1),
						sprite.y + 1
					);
					sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
					if(sprite.ini.skill == "builder")
						lemonize(sprite);
				}
				sprite.lastMove = system.timer;
				break;

			} else {
				sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
				sprite.lastMove = system.timer;
				var overlaps = Sprite.checkOverlap(sprite);
				if(overlaps) {
					for(var o = 0; o < overlaps.length; o++) {
						if(	overlaps[o].ini.type != "block"
							&&
							overlaps[o].ini.type != "lemon"
							&&
							overlaps[o].ini.type != "hazard"
						) {
							continue;
						}
						if(sprite.bearing == "w") {
							while(sprite.x < overlaps[o].x + overlaps[o].frame.width) {
								sprite.move("reverse");
							}
						} else if(sprite.bearing == "e") {
							while(sprite.x + sprite.ini.width > overlaps[o].x) {
								sprite.move("reverse");
							}
						}
					}
				}
				if(sprite.ini.skill == "builder")
					lemonize(sprite);
				break;
			}

		}

	}

	/*	This function is strictly for climber lemons.  The lemon keeps
		scaling the wall until it is able to continue moving along its
		original bearing.  If the climb is only one cell in height, this
		was just a single block (stairs, perhaps) and the 'climber' skill
		is retained for later use.  Otherwise convert back to normal lemon
		status. */
	var climbIfObstacle = function(sprite) {

		if(system.timer - sprite.lastYMove < sprite.ini.speed)
			return;

		if(Sprite.checkAbove(sprite)) {
			lemonize(sprite);
			sprite.lastMove = system.timer;
			return;
		}

		var beside = (sprite.bearing == "w") ? Sprite.checkLeft(sprite) : Sprite.checkRight(sprite);
		if(!beside)
			beside = Sprite.checkOverlap(sprite);
		if(!beside)
			return;

		if(typeof sprite.ini.climbStart == "undefined")
			sprite.ini.climbStart = sprite.y;

		sprite.ini.gravity = 0;
		sprite.ini.constantmotion = 0;

		for(var b = 0; b < beside.length; b++) {

			if(	beside[b].ini.type == "entrance"
				||
				beside[b].ini.type == "exit"
				||
				beside[b].ini.type == "projectile"
			) {
				lemonize(sprite);
				return;
			} else if(beside[b].ini.type == "lemon") {
				sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
				sprite.ini.gravity = 1;
				sprite.ini.constantmotion = 1;
				return;
			}

		}

		sprite.moveTo(
			(sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1),
			sprite.y - 1
		);
		sprite.lastYMove = system.timer;
		var overlaps = Sprite.checkOverlap(sprite);
		if(overlaps) {
			for(var o = 0; o < overlaps.length; o++) {
				if(overlaps[o].y != sprite.y + sprite.ini.height - 1)
					continue;
				if(sprite.bearing == "w") {
					while(sprite.x < overlaps[o].x + overlaps[o].frame.width) {
						sprite.move("reverse");
					}
				} else if(sprite.bearing == "e") {
					while(sprite.x + sprite.ini.width > overlaps[o].x) {
						sprite.move("reverse");
					}
				}
				break;
			}
		} else if(
			sprite.ini.climbStart > sprite.y
			&&
			sprite.ini.climbStart - sprite.y > 1
		) {
			lemonize(sprite);
		} else {
			sprite.ini.constantmotion = 1;
			sprite.ini.gravity = 1;
			delete sprite.ini.climbStart;
		}

	}

	/*	The Sprite class actually takes care of most aspects of falling
		for us.  We just need to enforce straight vertical drops, and make
		sure certain types don't start moving again once they land. 
		All lemons should be passed to this function.  */
	var fallIfNoFloor = function(sprite) {

		if(sprite.inFall && sprite.ini.constantmotion == 1) {
			// Stop lemons from moving horizontally when falling
			sprite.ini.constantmotion = 0;
		} else if(
			!sprite.inFall
			&&
			sprite.ini.constantmotion == 0
			&&
			sprite.ini.gravity == 1
			&&
			sprite.ini.skill != "blocker"
			&&
			sprite.ini.skill != "digger"
			&&
			sprite.ini.skill != "basher"
			&&
			sprite.ini.skill != "builder"
			&&
			sprite.ini.skill != "dying"
		) {

			if(typeof sprite.ini.forceDrop == "boolean")
				delete sprite.ini.forceDrop;

			if(	typeof sprite.ini.ticker == "undefined"
				&&
				sprite.ini.skill != "climber"
			) {
				lemonize(sprite);
			} else {
				sprite.ini.constantmotion = 1;
			}
		}
	
	}

	/*	If a lemon has reached the exit, remove it and update counters
		accordingly.
		All but blocker lemons should be passed to this function. */
	var removeIfAtExit = function(sprite) {
		var overlaps = Sprite.checkOverlap(sprite);
		if(!overlaps)
			return;
		for(var o = 0; o < overlaps.length; o++) {
			if(overlaps[o].ini.type != "exit")
				continue;
			sprite.remove();
			saved++;
			frames.counters.lostSaved.clear();
			frames.counters.lostSaved.putmsg(lost + "/" + saved);
			break;
		}
	}

	/*	If any blocks get in the way of a 'basher' lemon, it will bash
		through them until it reaches free space again, at which point
		it loses its 'basher' skill.
		It's too easy to just remove an entire block from the screen,
		so we will produce a nice effect by dismantling the block by
		one half-cell every ~ .5 seconds. */
	var bashersGonnaBash = function(sprite) {

		if(typeof sprite.ini.lastDig != "undefined" && system.timer - sprite.ini.lastDig < .5)
			return;

		var beside = (sprite.bearing == "e") ? Sprite.checkRight(sprite) : Sprite.checkLeft(sprite);
		if(!beside)
			beside = Sprite.checkOverlap(sprite);
		if(!beside) {
			sprite.ini.constantmotion = 1;
			return;
		}

		sprite.ini.constantmotion = 0;

		var blockFound = false;
		var columnClear = false;
		for(var b = 0; b < beside.length; b++) {

			if(beside[b].ini.type == "lemon") {
				sprite.turnTo((sprite.bearing == "e") ? "w" : "e");
				return;
			}

			if(	beside[b].ini.type != "block"
				||
				!beside[b].open
			) {
				continue;
			}

			if(beside[b].y == sprite.y + sprite.ini.height - 1) {
				sprite.moveTo(
					(sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1),
					sprite.y - 1
				);
				if(!Sprite.checkOverlap(sprite)) {
					sprite.ini.constantmotion = 1;
					return;
				}
				sprite.moveTo(
					(sprite.bearing == "w") ? (sprite.x + 1) : (sprite.x - 1),
					sprite.y + 1
				);
			}

			blockFound = true;

			if(beside[b].ini.material == "metal") {
				lemonize(sprite);
				sprite.turnTo((sprite.bearing == "e") ? "w" : "e");
				return;
			}

			if(sprite.bearing == "e")
				var x = (beside[b].x - sprite.x == 3) ? 0 : ((beside[b].x - sprite.x == 2) ? 1 : 2);
			else
				var x = (sprite.x - beside[b].x == 3) ? 2 : ((sprite.x - beside[b].x == 2) ? 1 : 0);

			beside[b].frame.invalidate();
			sprite.ini.lastDig = system.timer;

			if(beside[b].frame.data[0][x].ch == ascii(220)) {
				beside[b].frame.data[0][x].ch = " ";
				beside[b].frame.data[0][x].attr = BG_BLACK;
				if(	(sprite.bearing == "e" && x == 2)
					||
					(sprite.bearing == "w" && x == 0)
				) {
					beside[b].remove();
				}
				columnClear = (b == beside.length - 1);
				break;
			}

			if(beside[b].frame.data[0][x].ch == ascii(223)) {
				beside[b].frame.data[0][x].ch = ascii(220);
				beside[b].frame.data[0][x].attr = BG_BLACK|BROWN;
				break;
			}

			if(beside[b].frame.data[0][x].ch == ascii(219)) {
				beside[b].frame.data[0][x].ch = ascii(220);
				beside[b].frame.data[0][x].attr = BG_BLACK|RED;
				break;
			}

		}

		if(columnClear) {
			sprite.move("forward");
			if(	((sprite.bearing == "e" && !Sprite.checkRight(sprite))
				||
				(sprite.bearing == "w" && !Sprite.checkLeft(sprite)))
				&&
				!Sprite.checkOverlap(sprite)
			) {
				lemonize(sprite);
				return;
			}
		} else if(!blockFound) {
			lemonize(sprite);
		}

	}

	//	Like bashersGonnaBash, but for digging downward.
	var diggersGonnaDig = function(sprite) {

		if(typeof sprite.ini.lastDig != "undefined" && system.timer - sprite.ini.lastDig < .5)
			return;

		var below = Sprite.checkBelow(sprite);
		if(!below)
			below = Sprite.checkOverlap(sprite);
		if(!below)
			return;

		sprite.ini.constantmotion = 0;

		var clear = below.length;
		for(var b = 0; b < below.length; b++) {

			if(	below[b].ini.type != "block"
				||
				!below[b].open
			) {
				clear--;
				continue;
			}

			if(below[b].ini.material == "metal") {
				lemonize(sprite);
				return;
			}

			var cleared = true;
			for(var c = 0; c < 3; c++) {
				if(below[b].frame.data[0][c].ch != " ")
					cleared = false;
			}				
			if(cleared) {
				below[b].remove();
				clear--;
				continue;
			}

			below[b].frame.invalidate();
			sprite.ini.lastDig = system.timer;

			for(var c = 0; c < 3; c++) {

				if(below[b].frame.data[0][c].ch == ascii(220)) {
					below[b].frame.data[0][c].ch = " ";
					below[b].frame.data[0][c].attr = 0;
					break;
				}

				if(below[b].frame.data[0][c].ch == ascii(223)) {
					below[b].frame.data[0][c].ch = ascii(220);
					below[b].frame.data[0][c].attr = BG_BLACK|BROWN;
					break;
				}

				if(below[b].frame.data[0][c].ch == ascii(219)) {
					below[b].frame.data[0][c].ch = ascii(220);
					below[b].frame.data[0][c].attr = BG_BLACK|RED;
					break;
				}

			}

		}

		if(clear == 0) {
			sprite.moveTo(sprite.x, sprite.y + 1);
			if(!Sprite.checkBelow(sprite)) {
				lemonize(sprite);
				sprite.lastMove = system.timer;
			}
		}

	}

	// Build a staircase four blocks high, or until an obstacle is encountered
	var buildersGonnaBuild = function(sprite) {

		if(typeof sprite.ini.lastBuild == "undefined") {
			sprite.ini.lastBuild = system.timer;
			sprite.ini.buildCount = 0;
			sprite.ini.constantmotion = 0;
		}
		if(system.timer - sprite.ini.lastBuild < .5)
			return;

		if(sprite.ini.buildCount == 4) {
			lemonize(sprite);
			return;
		}

		sprite.ini.buildCount++;
		sprite.ini.lastBuild = system.timer;

		var block = new Sprite.Profile(
			"brick",
			frames.field,
			(sprite.bearing == "e") ? (sprite.x + sprite.ini.width) : (sprite.x - 4),
			sprite.y + sprite.ini.height - 1,
			"e",
			"normal"
		);
		var overlap = Sprite.checkOverlap(block);
		if(overlap) {
			block.remove();
			Sprite.profiles.splice(block.index, 1);
			lemonize(sprite);
			return;
		}
		block.frame.open();
		sprite.moveTo(
			(sprite.bearing == "e") ? (sprite.x + 3) : (sprite.x - 3),
			sprite.y - 1
		);

	}

	// Draw the time left until explosion at the centre of each of a lemon's positions
	var ticker = function(sprite) {
		sprite.ini.ticker--;
		sprite.frame.data[1][1].ch = sprite.ini.ticker;
		sprite.frame.data[1][4].ch = sprite.ini.ticker;
		sprite.frame.data[1][7].ch = sprite.ini.ticker;
		sprite.frame.data[4][1].ch = sprite.ini.ticker;
		sprite.frame.data[4][4].ch = sprite.ini.ticker;
		sprite.frame.data[4][7].ch = sprite.ini.ticker;
	}

	// Make the lemon look explodey, cause some damage
	var explode = function(sprite) {
		if(!sprite.open)
			return;
		sprite.changePosition("nuked");
		sprite.ini.speed = 2000;
		var overlap = Sprite.checkOverlap(sprite, 1);
		for(var o = 0; overlap && o < overlap.length; o++) {
			if(overlap[o].ini.type == "lemon")
				continue;
			overlap[o].remove();
		}
		lost++;
		frames.counters.lostSaved.clear();
		frames.counters.lostSaved.putmsg(lost + "/" + saved);
	}

	// Redden a lemon, and prepare it for obliteration
	var nuke = function(sprite) {
		sprite.ini.ticker = 5;
		timer.addEvent(5000, false, explode, [sprite]);
		timer.addEvent(6000, false, remove, [sprite]);
		timer.addEvent(1000, 5, ticker, [sprite]);
		colorize(sprite, COLOUR_NUKED);
		ticker(sprite);
	}

	// Make a lemon tap its foot
	var tapFoot = function(sprite) {
		sprite.changePosition(
			(sprite.position == "normal") ? "normal2" : "normal"
		);
	}

	/*	Populate the terminal with a status bar, the level's static sprites,
		and set up timed events for releasing lemons and level timeout.	*/
	var loadLevel = function(level, frame) {

		// The parent frame for all sprites & frames created by Level
		frames.game = new Frame(
			frame.x,
			frame.y,
			frame.width,
			frame.height,
			BG_BLACK|LIGHTGRAY,
			frame
		);

		// The gameplay area
		frames.field = new Frame(
			frames.game.x,
			frames.game.y,
			frames.game.width,
			frames.game.height - 2,
			BG_BLACK|LIGHTGRAY,
			frames.game
		);

		// The status bar and its various subframes
		frames.statusBar = new Frame(
			frames.game.x,
			frames.game.y + frames.game.height - 2,
			frames.game.width,
			2,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.game
		);

		frames.counters.basher = new Frame(
			frames.statusBar.x + 10,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);
		
		frames.counters.bomber = new Frame(
			frames.statusBar.x + 24,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);
		
		frames.counters.climber = new Frame(
			frames.statusBar.x + 38,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);
		
		frames.counters.blocker = new Frame(
			frames.statusBar.x + 10,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);
		
		frames.counters.builder = new Frame(
			frames.statusBar.x + 24,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);
		
		frames.counters.digger = new Frame(
			frames.statusBar.x + 38,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.counters.remaining = new Frame(
			frames.statusBar.x + 69,
			frames.statusBar.y,
			5,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.counters.time = new Frame(
			frames.statusBar.x + 76,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.counters.lostSaved = new Frame(
			frames.statusBar.x + 69,
			frames.statusBar.y + 1,
			4,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);		
		
		// Populate the status bar's static text fields
		frames.statusBar.putmsg(KEY_BASH + ") Bash :     ", COLOUR_BASHER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg(KEY_BOMB + ") Bomb :     ", COLOUR_BOMBER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg(KEY_CLIMB + ") Climb:     ", COLOUR_CLIMBER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("N)uke   ", COLOUR_NUKED|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("H)elp  ", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("  Released:       Time:\r\n", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg(KEY_BLOCK + ") Block:     ", COLOUR_BLOCKER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg(KEY_BUILD + ") Build:     ", COLOUR_BUILDER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg(KEY_DIG + ") Dig  :     ", COLOUR_DIGGER|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("P)ause  ", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("Q)uit  ", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.putmsg("Lost/Saved: ", WHITE|COLOUR_STATUSBAR_BG);

		// Display the initial lost/saved value
		frames.counters.lostSaved.putmsg(lost + "/" + saved);

		// Set the quota values
		if(typeof level.quotas == "undefined")
			level.quotas = {};
		for(var q in level.quotas) {
			quotas[q] = level.quotas[q];
			frames.counters[q].putmsg(quotas[q]);
		}

		// Add bricks, etc. to the screen
		for(var b = 0; b < level.blocks.length; b++) {
			new Sprite.Profile(
				level.blocks[b].type,
				frames.field,
				frames.field.x + level.blocks[b].x - 1,
				frames.field.y + level.blocks[b].y - 1,
				"e",
				"normal"
			);
		}

		// Add hazards (water, slime, lava) to the screen
		for(var h = 0; h < level.hazards.length; h++) {
			new Sprite.Profile(
				level.hazards[h].type,
				frames.field,
				frames.field.x + level.hazards[h].x - 1,
				frames.field.y + level.hazards[h].y - 1,
				"e",
				"normal"
			);
		}

		// Add any shooters
		for(var s = 0; s < level.shooters.length; s++) {
			new Sprite.Profile(
				level.shooters[s].type,
				frames.field,
				frames.field.x + level.shooters[s].x - 1,
				frames.field.y + level.shooters[s].y - 1,
				(level.shooters[s].type == "shooter-e") ? "e" : "w",
				"normal"
			);
		}

		// Create the cursor "sprite"
		cursor = new Sprite.Platform(
			frames.field,
			frames.field.x + 39,
			frames.field.y + 10,
			1,
			1,
			ascii(219),
			WHITE,
			false,
			false,
			0
		);
		cursor.ini = {};
		cursor.open = false; // Exempt it from collision-detection

		// Add the in & out doors to the screen
		new Sprite.Profile(
			"entrance",
			frames.field,
			frames.field.x + level.entrance.x - 1,
			frames.field.y + level.entrance.y - 1,
			"e",
			"normal"
		);

		new Sprite.Profile(
			"exit",
			frames.field,
			frames.field.x + level.exit.x - 1,
			frames.field.y + level.exit.y - 1,
			"e",
			"normal"
		);

		// Set up a timed lemon-release event and "remaining lemons" counter
		var remaining = level.lemons;
		var releaseLemon = function(x, y) {
			new Sprite.Profile(
				"lemon",
				frames.field,
				x,
				y,
				"e",
				"normal"
			);
			Sprite.profiles[Sprite.profiles.length - 1].frame.open();
			Sprite.profiles[Sprite.profiles.length - 1].ini.skill = "lemon";
			remaining--;
			frames.counters.remaining.clear();
			frames.counters.remaining.putmsg((level.lemons - remaining) + "/" + level.lemons);
			if(nuked)
				nuke(Sprite.profiles[Sprite.profiles.length - 1]);
		}
		frames.counters.remaining.putmsg(remaining);

		timer.addEvent(
			3000,
			level.lemons,
			releaseLemon,
			[	frames.field.x + level.entrance.x - 1,
				frames.field.y + level.entrance.y - 1
			]
		);
		total = level.lemons; // When lost + saved == total, the level is done

		// Set up a timed event to update the clock
		countDown = level.time + 3; // Don't penalize users for delayed release
		var tickTock = function() {
			countDown--;
			frames.counters.time.clear();
			frames.counters.time.putmsg(countDown);
		}
		timer.addEvent(1000, level.time, tickTock);

		// Open the top frame and all its chilluns
		frames.game.open();

		// Display the level number and name for a few sex
		var popUp = new PopUp(["Level " + (n + 1) + ": " + level.name]);
		timer.addEvent(3000, false, popUp.close, [], popUp);

	}

	/*	Make the lemons behave according to skillset, or die if necessary.
		Return a LEVEL_ value to the parent script indicating whether the level
		is complete (and why) or if it is in progress. */
	this.cycle = function() {

		/*	Record the index of the first lemon that overlaps with the cursor,
			if any, for use when assigning skills during this.getcmd.
			Make the cursor red if there's an overlap, white if not. */
		cursor.frame.top();
		var overlaps = Sprite.checkOverlap(cursor);
		if(overlaps) {
			for(var o = 0; o < overlaps.length; o++) {
				if(overlaps[o].ini.type != "lemon")
					continue;
				cursor.frame.data[0][0].attr = LIGHTRED;
				cursor.ini.hoveringOver = overlaps[o].index;
				break;
			}
		} else if(typeof cursor.ini.hoveringOver != "undefined") {
			cursor.frame.data[0][0].attr = WHITE;
			delete cursor.ini.hoveringOver;
		}

		for(var s = 0; s < Sprite.profiles.length; s++) {

			if(Sprite.profiles[s].ini.type == "shooter") {
				Sprite.profiles[s].putWeapon();
				continue;
			}

			if(	Sprite.profiles[s].ini.type == "projectile"
				&&
				Sprite.profiles[s].open
			) {
				var overlaps = Sprite.checkOverlap(Sprite.profiles[s]);
				for(var o = 0; o < overlaps.length; o++) {
					if(overlaps[o].ini.type != "lemon")
						continue;
					overlaps[o].remove();
					lost++;
					frames.counters.lostSaved.clear();
					frames.counters.lostSaved.putmsg(lost + "/" + saved);
				}
			}

			if(Sprite.profiles[s].ini.type != "lemon" || !Sprite.profiles[s].open)
				continue;

			// Remove any lemons that have gone off the screen
			if(	!Sprite.profiles[s].open
				||
				Sprite.profiles[s].y + Sprite.profiles[s].ini.height > frames.field.y + frames.field.height
				||
				Sprite.profiles[s].x + Sprite.profiles[s].ini.width <= frames.field.x
				||
				Sprite.profiles[s].x >= frames.field.x + frames.field.width
			) {
				Sprite.profiles[s].remove();
				lost++;
				frames.counters.lostSaved.clear();
				frames.counters.lostSaved.putmsg(lost + "/" + saved);
			}

			// Don't stand on top of a door, start a death march if on a hazard
			var below = Sprite.checkBelow(Sprite.profiles[s]);
			if(below) {
				for(var b = 0; b < below.length; b++) {
					if(	below[b].ini.type == "exit"
						||
						below[b].ini.type == "entrance"
					) {
						Sprite.profiles[s].moveTo(
							Sprite.profiles[s].x,
							Sprite.profiles[s].y + 1
						);
						break;
					} else if(
						below[b].ini.type == "hazard"
						&&
						Sprite.profiles[s].ini.skill != "dying"
					) {
						Sprite.profiles[s].ini.constantmotion = 0;
						lost++;
						frames.counters.lostSaved.clear();
						frames.counters.lostSaved.putmsg(lost + "/" + saved);
						Sprite.profiles[s].ini.skill = "dying";
						colorize(Sprite.profiles[s], COLOUR_DYING);
						timer.addEvent(
							1000,
							false,
							remove,
							[Sprite.profiles[s]]
						);
						break;
					}
				}
			} else if(
				Sprite.profiles[s].ini.skill != "digger"
				&&
				typeof Sprite.profiles[s].ini.forceDrop == "undefined"
				&&
				Sprite.profiles[s].ini.skill != "basher"
			) {
				/*	Sprite() moves things horizontally first, then vertically.
					In certain circumstances (a hole of only the same width as
					the sprite has just appeared beneath it) we need to force
					the sprite to drop one cell to initiate a fall.	*/
				Sprite.profiles[s].moveTo(
					Sprite.profiles[s].x,
					Sprite.profiles[s].y + 1
				);
				// But we only want to do it once, not every .cycle()
				Sprite.profiles[s].ini.forceDrop = true;
				/*	This will be cleared in fallIfNoFloor once the lemon
					hits bottom. */
			}

			// Animate the lemon's walk, if applicable
			if(	((	!Sprite.profiles[s].inFall
					&&
					system.timer - Sprite.profiles[s].lastMove > Sprite.profiles[s].ini.speed
				)
				||
				(	Sprite.profiles[s].inFall
					&&
					system.timer - Sprite.profiles[s].lastYMove > Sprite.profiles[s].ini.speed	
				))
				&&
				Sprite.profiles[s].ini.gravity == 1
				&&
				Sprite.profiles[s].ini.skill != "blocker"
				&&
				Sprite.profiles[s].position != "nuked"
				&&
				Sprite.profiles[s].ini.skill != "builder"
				&&
				Sprite.profiles[s].ini.skill != "digger"
				&&
				typeof Sprite.profiles[s].ini.lastDig == "undefined"
			) {
				Sprite.profiles[s].changePosition(
					(Sprite.profiles[s].position == "normal") ? "normal2" : "normal"
				);
			}

			// Make the different types of lemons behave as they should
			switch(Sprite.profiles[s].ini.skill) {

				case "lemon":
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case "basher":
					bashersGonnaBash(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case "blocker":
					fallIfNoFloor(Sprite.profiles[s]);
					break;

				case "bomber":
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case "builder":
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					buildersGonnaBuild(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case "climber":
					climbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case "digger":
					diggersGonnaDig(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					break;

				default:
					break;

			}

		}

		// Cycle the other cyclable things
		timer.cycle();
		Sprite.cycle();

		// Tell the parent script how to proceed
		if(countDown <= 0) {
			if(saved < (total / 2))
				return LEVEL_TIME;
			this.score = (saved * 100);
			return LEVEL_NEXT;
		} else if(lost + saved == total && saved >= (total / 2)) {
			this.score = (saved * 100) + (countDown * 10);
			return LEVEL_NEXT;
		} else if(lost + saved == total && saved < (total / 2)) {
			return LEVEL_DEAD;
		} else {
			return LEVEL_CONTINUE;
		}

	}

	// Do what the user asked us to do, if it's valid
	this.getcmd = function(userInput) {

		var ret = true;

		switch(userInput.toUpperCase()) {

			case "":
				break;

			// Quit
			case "Q":
				ret = false;
				break;

			// Pause
			case "P":
				state = STATE_PAUSE;
				break;

			// Help
			case "H":
				state = STATE_HELP;
				break;

			// Cursor movement
			case "8":
			case KEY_UP:
				if(cursor.y > frames.field.y)
					cursor.moveTo(cursor.x, cursor.y - 1);
				break;

			case "2":
			case KEY_DOWN:
				if(cursor.y < frames.field.y + frames.field.height - 1)
					cursor.moveTo(cursor.x, cursor.y + 1);
				break;

			case "4":
			case KEY_LEFT:
				if(cursor.x > frames.field.x)
					cursor.moveTo(cursor.x - 1, cursor.y);
				break;

			case "6":
			case KEY_RIGHT:
				if(cursor.x < frames.field.x + frames.field.width - 1)
					cursor.moveTo(cursor.x + 1, cursor. y);
				break;

			case "7":
				if(cursor.y > frames.field.y && cursor.x > frames.field.x)
					cursor.moveTo(cursor.x - 1, cursor.y - 1);
				break;

			case "9":
				if(cursor.y > frames.field.y && cursor.x < frames.field.x + frames.field.width - 1)
					cursor.moveTo(cursor.x + 1, cursor.y - 1);
				break;

			case "1":
				if(cursor.y < frames.field.y + frames.field.height - 1 && cursor.x > frames.field.x)
					cursor.moveTo(cursor.x - 1, cursor.y + 1);
				break;

			case "3":
				if(cursor.y < frames.field.y + frames.field.height - 1 && cursor.x < frames.field.x + frames.field.width - 1)
					cursor.moveTo(cursor.x + 1, cursor.y + 1);
				break;

			// Nuke the h'wales
			case "N":
				for(var s = 0; s < Sprite.profiles.length; s++) {
					if(Sprite.profiles[s].ini.type != "lemon" || !Sprite.profiles[s].open)
						continue;
					nuke(Sprite.profiles[s]);
				}
				nuked = true;
				break;

			// Basher
			case KEY_BASH:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.basher < 1)
					break;
				quotas.basher--;
				frames.counters.basher.clear();
				frames.counters.basher.putmsg(quotas.basher);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "basher";
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR_BASHER);
				break;

			// Blocker
			case KEY_BLOCK:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.blocker < 1)
					break;
				quotas.blocker--;
				frames.counters.blocker.clear();
				frames.counters.blocker.putmsg(quotas.blocker);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "blocker";
				Sprite.profiles[cursor.ini.hoveringOver].ini.constantmotion = 0;
				Sprite.profiles[cursor.ini.hoveringOver].changePosition("fall");
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR_BLOCKER);
				timer.addEvent(1000, true, tapFoot, [Sprite.profiles[cursor.ini.hoveringOver]]);
				break;

			// Bomber
			case KEY_BOMB:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.bomber < 1)
					break;
				quotas.bomber--;
				frames.counters.bomber.clear();
				frames.counters.bomber.putmsg(quotas.bomber);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "bomber";
				nuke(Sprite.profiles[cursor.ini.hoveringOver]);
				break;

			// Builder
			case KEY_BUILD:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.builder < 1)
					break;
				quotas.builder--;
				frames.counters.builder.clear();
				frames.counters.builder.putmsg(quotas.builder);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "builder";
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR_BUILDER);
				break;

			// Climber
			case KEY_CLIMB:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.climber < 1)
					break;
				quotas.climber--;
				frames.counters.climber.clear();
				frames.counters.climber.putmsg(quotas.climber);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "climber";
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR_CLIMBER);
				break;

			// Digger
			case KEY_DIG:
				if(typeof cursor.ini.hoveringOver == "undefined")
					break;
				if(quotas.digger < 1)
					break;
				quotas.digger--;
				frames.counters.digger.clear();
				frames.counters.digger.putmsg(quotas.digger);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = "digger";
				Sprite.profiles[cursor.ini.hoveringOver].ini.constantmotion = 0;
				Sprite.profiles[cursor.ini.hoveringOver].changePosition("fall");
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR_DIGGER);
				break;

			default:
				break;
		}

		return ret;

	}

	/*	Halt or resume all timed events.
		This doesn't actually block - that should be done in the main loop.
		If no argument is specified, events are removed from the timer and an
		array of these events is returned.
		If an argument is specified, it is assumed to be an array of events,
		and they are added back into the timer. */
	this.pause = function(events) {
		if(typeof events == "undefined") {
			var events = timer.events;
			timer.events = [];
			return events;
		} else {
			for(var e = 0; e < events.length; e++) {
				timer.addEvent(
					events[e].interval,
					events[e].repeat,
					events[e].action,
					events[e].arguments,
					events[e].context
				);
			}
		}
	}

	// Reset the Sprite globals, clean up the display
	this.close = function() {
		
		cursor.remove();
		Sprite.platforms = [];

		for(var s = 0; s < Sprite.profiles.length; s++) {
			Sprite.profiles[s].remove();
		}
		Sprite.profiles = [];

		frames.game.delete();

	}

	loadLevel(l, frame);

}