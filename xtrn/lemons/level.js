// This is the actual game.  It's a disaster, but it gets the job done.
var Level = function (l, n) {

	// Scope some variables for use by functions and methods in this object
	var timer = new Timer(),
		frames = {
			labels: {},
			counters: {}
		},
		cursor,		// The 1x1 cursor frame
		countDown,	// Level timeout
		lost = 0,	// Lemons lost
		saved = 0,	// Lemons saved
		total = 0,	// Total lemon count for this level
		nuked = false,
		currentSkill = null,
		quotas = {};
	quotas[KEY_BASH] = 0;
	quotas[KEY_BLOCK] = 0;
	quotas[KEY_BOMB] = 0;
	quotas[KEY_BUILD] = 0;
	quotas[KEY_CLIMB] = 0;
	quotas[KEY_DIG] = 0;

	// The parent Game object will want to read this
	this.score = 0;

	/*	Change the colour of a lemon's peel without altering the colour of its
		shoes or 'nuked' position. */
	function colorize(sprite, colour) {
		var fgmask = (1<<0)|(1<<1)|(1<<2)|(1<<3);
		for (var y = 0; y < sprite.frame.data.length; y++) {
			for (var x = 0; x < 9; x++) {
				sprite.frame.data[y][x].attr&=~fgmask;
				if ((y == 2 || y == 5) && (x == 0 || x == 2 || x == 3 || x == 5 || x == 6 || x == 8)) {
					sprite.frame.data[y][x].attr=BG_BLACK|BROWN;
				} else {
					sprite.frame.data[y][x].attr|=colour;
				}
			}
		}
		sprite.frame.invalidate();
	}

	//	Turn skilled lemon back into an ordinary zombie-lemon.
	function lemonize(sprite) {
		colorize(sprite, COLOUR_LEMON);
		sprite.ini.constantmotion = 1;
		sprite.ini.gravity = 1;
		sprite.ini.speed = .25;
		sprite.ini.skill = "lemon";
		if (typeof sprite.ini.buildCount != "undefined") delete sprite.ini.buildCount;
		if (typeof sprite.ini.lastBuild != "undefined") delete sprite.ini.lastBuild;
		if (typeof sprite.ini.climbStart != "undefined") delete sprite.ini.climbStart;
		if (typeof sprite.ini.lastDig != "undefined") delete sprite.ini.lastDig;
	}

	// Remove the lemon from the screen
	function remove(sprite) {
		if (!sprite.open) return;
		sprite.remove();
	}

	/*	If a lemon encounters blocks stacked >= 2 characters high it
		will turn and start walking the other way.
		If a lemon encounters a block 1 character high, it will climb
		on top of it (stairs, etc.)
		Normal, bomber, and builder lemons should be passed to this function.
	*/
	function turnOrClimbIfObstacle(sprite) {

		var beside = (sprite.bearing == "w") ? Sprite.checkLeft(sprite) : Sprite.checkRight(sprite);
		if (!beside) beside = Sprite.checkOverlap(sprite);
		if (!beside) return;

		for (var b = 0; b < beside.length; b++) {

			if (beside[b].ini.type != "block" && beside[b].ini.type != "lemon" && beside[b].ini.type != "hazard") continue;

			if (beside[b].ini.type == "block" && beside[b].y == sprite.y + sprite.ini.height - 1) {
				sprite.moveTo((sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1), sprite.y - 1);
				if (Sprite.checkOverlap(sprite)) {
					sprite.moveTo((sprite.bearing == "w") ? (sprite.x + 1) : (sprite.x - 1), sprite.y + 1);
					sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
					if (sprite.ini.skill == "builder") lemonize(sprite);
				}
				sprite.lastMove = system.timer;
				break;

			} else {
				sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
				sprite.lastMove = system.timer;
				var overlaps = Sprite.checkOverlap(sprite);
				if (overlaps) {
					for (var o = 0; o < overlaps.length; o++) {
						if (overlaps[o].ini.type != "block" && overlaps[o].ini.type != "lemon" && overlaps[o].ini.type != "hazard") {
							continue;
						}
						if (sprite.bearing == "w") {
							while (sprite.x < overlaps[o].x + overlaps[o].frame.width) {
								sprite.move("reverse");
							}
						} else if (sprite.bearing == "e") {
							while (sprite.x + sprite.ini.width > overlaps[o].x) {
								sprite.move("reverse");
							}
						}
					}
				}
				if (sprite.ini.skill == "builder") lemonize(sprite);
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
	function climbIfObstacle(sprite) {

		if (system.timer - sprite.lastYMove < sprite.ini.speed) return;

		if (Sprite.checkAbove(sprite)) {
			lemonize(sprite);
			sprite.lastMove = system.timer;
			return;
		}

		var beside = (sprite.bearing == "w") ? Sprite.checkLeft(sprite) : Sprite.checkRight(sprite);
		if (!beside) beside = Sprite.checkOverlap(sprite);
		if (!beside) return;

		if (typeof sprite.ini.climbStart == "undefined") sprite.ini.climbStart = sprite.y;
		sprite.ini.gravity = 0;
		sprite.ini.constantmotion = 0;
		for (var b = 0; b < beside.length; b++) {
			if (beside[b].ini.type == "entrance" || beside[b].ini.type == "exit" || beside[b].ini.type == "projectile") {
				lemonize(sprite);
				return;
			} else if (beside[b].ini.type == "lemon") {
				sprite.turnTo((sprite.bearing == "w") ? "e" : "w");
				sprite.ini.gravity = 1;
				sprite.ini.constantmotion = 1;
				return;
			}
		}

		sprite.moveTo((sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1), sprite.y - 1);
		sprite.lastYMove = system.timer;
		var overlaps = Sprite.checkOverlap(sprite);
		if (overlaps) {
			for (var o = 0; o < overlaps.length; o++) {
				if (overlaps[o].y != sprite.y + sprite.ini.height - 1) continue;
				if (sprite.bearing == "w") {
					while (sprite.x < overlaps[o].x + overlaps[o].frame.width) {
						sprite.move("reverse");
					}
				} else if (sprite.bearing == "e") {
					while (sprite.x + sprite.ini.width > overlaps[o].x) {
						sprite.move("reverse");
					}
				}
				break;
			}
		} else if (sprite.ini.climbStart > sprite.y && sprite.ini.climbStart - sprite.y > 1) {
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
	function fallIfNoFloor(sprite) {

		if(sprite.inFall && sprite.ini.constantmotion == 1) {
			// Stop lemons from moving horizontally when falling
			sprite.ini.constantmotion = 0;
		} else if(
			!sprite.inFall
			&& sprite.ini.constantmotion == 0
			&& sprite.ini.gravity == 1
			&& sprite.ini.skill != KEY_BLOCK
			&& sprite.ini.skill != KEY_DIG
			&& sprite.ini.skill != KEY_BASH
			&& sprite.ini.skill != KEY_BUILD
			&& sprite.ini.skill != "dying"
		) {

			if (typeof sprite.ini.forceDrop == "boolean") delete sprite.ini.forceDrop;

			if(typeof sprite.ini.ticker == "undefined" && sprite.ini.skill != "climber") {
				lemonize(sprite);
			} else {
				sprite.ini.constantmotion = 1;
			}
		}
	
	}

	/*	If a lemon has reached the exit, remove it and update counters
		accordingly.
		All but blocker lemons should be passed to this function. */
	function removeIfAtExit(sprite) {
		var overlaps = Sprite.checkOverlap(sprite);
		if (!overlaps) return;
		for (var o = 0; o < overlaps.length; o++) {
			if (overlaps[o].ini.type != "exit") continue;
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
	function bashersGonnaBash(sprite) {

		if (typeof sprite.ini.lastDig != "undefined" && system.timer - sprite.ini.lastDig < .5) return;

		var beside = (sprite.bearing == "e") ? Sprite.checkRight(sprite) : Sprite.checkLeft(sprite);
		if (!beside) beside = Sprite.checkOverlap(sprite);
		if (!beside) {
			sprite.ini.constantmotion = 1;
			return;
		}

		sprite.ini.constantmotion = 0;

		var blockFound = false;
		var columnClear = false;
		for (var b = 0; b < beside.length; b++) {

			if (beside[b].ini.type == "lemon") {
				sprite.turnTo((sprite.bearing == "e") ? "w" : "e");
				return;
			}

			if (beside[b].ini.type != "block" || !beside[b].open) continue;

			if (beside[b].y == sprite.y + sprite.ini.height - 1) {
				sprite.moveTo((sprite.bearing == "w") ? (sprite.x - 1) : (sprite.x + 1), sprite.y - 1);
				if (!Sprite.checkOverlap(sprite)) {
					sprite.ini.constantmotion = 1;
					return;
				}
				sprite.moveTo((sprite.bearing == "w") ? (sprite.x + 1) : (sprite.x - 1), sprite.y + 1);
			}

			blockFound = true;

			if (beside[b].ini.material == "metal") {
				lemonize(sprite);
				sprite.turnTo((sprite.bearing == "e") ? "w" : "e");
				return;
			}

			if (sprite.bearing == "e") {
				var x = (beside[b].x - sprite.x == 3) ? 0 : ((beside[b].x - sprite.x == 2) ? 1 : 2);
			} else {
				var x = (sprite.x - beside[b].x == 3) ? 2 : ((sprite.x - beside[b].x == 2) ? 1 : 0);
			}

			beside[b].frame.invalidate();
			sprite.ini.lastDig = system.timer;

			if (beside[b].frame.data[0][x].ch == ascii(220)) {
				beside[b].frame.data[0][x].ch = " ";
				beside[b].frame.data[0][x].attr = BG_BLACK;
				if(	(sprite.bearing == "e" && x == 2) || (sprite.bearing == "w" && x == 0)) beside[b].remove();
				columnClear = (b == beside.length - 1);
				break;
			}

			if (beside[b].frame.data[0][x].ch == ascii(223)) {
				beside[b].frame.data[0][x].ch = ascii(220);
				beside[b].frame.data[0][x].attr = BG_BLACK|BROWN;
				break;
			}

			if (beside[b].frame.data[0][x].ch == ascii(219)) {
				beside[b].frame.data[0][x].ch = ascii(220);
				beside[b].frame.data[0][x].attr = BG_BLACK|RED;
				break;
			}

		}

		if (columnClear) {
			sprite.move("forward");
			if (((sprite.bearing == "e" && !Sprite.checkRight(sprite)) || (sprite.bearing == "w" && !Sprite.checkLeft(sprite))) && !Sprite.checkOverlap(sprite)) {
				lemonize(sprite);
				return;
			}
		} else if (!blockFound) {
			lemonize(sprite);
		}

	}

	//	Like bashersGonnaBash, but for digging downward.
	function diggersGonnaDig(sprite) {

		if (typeof sprite.ini.lastDig != "undefined" && system.timer - sprite.ini.lastDig < .5) return;

		var below = Sprite.checkBelow(sprite);
		if (!below) below = Sprite.checkOverlap(sprite);
		if (!below) return;

		sprite.ini.constantmotion = 0;

		var clear = below.length;
		for (var b = 0; b < below.length; b++) {

			if (below[b].ini.type != "block" || !below[b].open) {
				clear--;
				continue;
			}

			if (below[b].ini.material == "metal") {
				lemonize(sprite);
				return;
			}

			var cleared = true;
			for (var c = 0; c < 3; c++) {
				if (below[b].frame.data[0][c].ch != " ") cleared = false;
			}				
			if (cleared) {
				below[b].remove();
				clear--;
				continue;
			}

			below[b].frame.invalidate();
			sprite.ini.lastDig = system.timer;

			for (var c = 0; c < 3; c++) {

				if (below[b].frame.data[0][c].ch == ascii(220)) {
					below[b].frame.data[0][c].ch = " ";
					below[b].frame.data[0][c].attr = 0;
					break;
				}

				if (below[b].frame.data[0][c].ch == ascii(223)) {
					below[b].frame.data[0][c].ch = ascii(220);
					below[b].frame.data[0][c].attr = BG_BLACK|BROWN;
					break;
				}

				if (below[b].frame.data[0][c].ch == ascii(219)) {
					below[b].frame.data[0][c].ch = ascii(220);
					below[b].frame.data[0][c].attr = BG_BLACK|RED;
					break;
				}

			}

		}

		if (clear == 0) {
			sprite.moveTo(sprite.x, sprite.y + 1);
			if (!Sprite.checkBelow(sprite)) {
				lemonize(sprite);
				sprite.lastMove = system.timer;
			}
		}

	}

	// Build a staircase four blocks high, or until an obstacle is encountered
	function buildersGonnaBuild(sprite) {

		if (typeof sprite.ini.lastBuild == "undefined") {
			sprite.ini.lastBuild = system.timer;
			sprite.ini.buildCount = 0;
			sprite.ini.constantmotion = 0;
		}
		if (system.timer - sprite.ini.lastBuild < .5) return;

		if (sprite.ini.buildCount == 4) {
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
		if (overlap) {
			block.remove();
			Sprite.profiles.splice(block.index, 1);
			lemonize(sprite);
			return;
		}
		block.frame.open();
		sprite.moveTo((sprite.bearing == "e") ? (sprite.x + 3) : (sprite.x - 3), sprite.y - 1);

	}

	// Draw the time left until explosion at the centre of each of a lemon's positions
	function ticker(sprite) {
		sprite.ini.ticker--;
		sprite.frame.data[1][1].ch = sprite.ini.ticker;
		sprite.frame.data[1][4].ch = sprite.ini.ticker;
		sprite.frame.data[1][7].ch = sprite.ini.ticker;
		sprite.frame.data[4][1].ch = sprite.ini.ticker;
		sprite.frame.data[4][4].ch = sprite.ini.ticker;
		sprite.frame.data[4][7].ch = sprite.ini.ticker;
	}

	// Make the lemon look explodey, cause some damage
	function explode(sprite) {
		if (!sprite.open) return;
		sprite.changePosition("nuked");
		sprite.ini.speed = 2000;
		var overlap = Sprite.checkOverlap(sprite, 1);
		for (var o = 0; overlap && o < overlap.length; o++) {
			if (overlap[o].ini.type == "lemon") continue;
			overlap[o].remove();
		}
		lost++;
		frames.counters.lostSaved.clear();
		frames.counters.lostSaved.putmsg(lost + "/" + saved);
	}

	// Redden a lemon, and prepare it for obliteration
	function nuke(sprite) {
		sprite.ini.ticker = 5;
		timer.addEvent(5000, false, explode, [sprite]);
		timer.addEvent(6000, false, remove, [sprite]);
		timer.addEvent(1000, 5, ticker, [sprite]);
		colorize(sprite, COLOUR_NUKED);
		ticker(sprite);
	}

	// Make a lemon tap its foot
	function tapFoot(sprite) {
		sprite.changePosition((sprite.position == "normal") ? "normal2" : "normal");
	}

	/*	Populate the terminal with a status bar, the level's static sprites,
		and set up timed events for releasing lemons and level timeout.	*/
	function loadLevel(level, frame) {

		// The parent frame for all sprites & frames created by Level
		frames.game = new Frame(frame.x, frame.y, frame.width, frame.height, BG_BLACK|LIGHTGRAY, frame);

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

		frames.labels[KEY_BASH] = new Frame(
			frames.statusBar.x,
			frames.statusBar.y,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_BASHER,
			frames.statusBar
		);

		frames.counters[KEY_BASH] = new Frame(
			frames.statusBar.x + 9,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.labels[KEY_BOMB] = new Frame(
			frames.statusBar.x + 13,
			frames.statusBar.y,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_BOMBER,
			frames.statusBar
		);
		
		frames.counters[KEY_BOMB] = new Frame(
			frames.statusBar.x + 21,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.labels[KEY_CLIMB] = new Frame(
			frames.statusBar.x + 25,
			frames.statusBar.y,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_CLIMBER,
			frames.statusBar
		);

		frames.counters[KEY_CLIMB] = new Frame(
			frames.statusBar.x + 34,
			frames.statusBar.y,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.labels[KEY_BLOCK] = new Frame(
			frames.statusBar.x,
			frames.statusBar.y + 1,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_BLOCKER,
			frames.statusBar
		);
		
		frames.counters[KEY_BLOCK] = new Frame(
			frames.statusBar.x + 9,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.labels[KEY_BUILD] = new Frame(
			frames.statusBar.x + 25,
			frames.statusBar.y + 1,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_BUILDER,
			frames.statusBar
		);
		
		frames.counters[KEY_BUILD] = new Frame(
			frames.statusBar.x + 34,
			frames.statusBar.y + 1,
			3,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_STATUSBAR_FG,
			frames.statusBar
		);

		frames.labels[KEY_DIG] = new Frame(
			frames.statusBar.x + 13,
			frames.statusBar.y + 1,
			9,
			1,
			COLOUR_STATUSBAR_BG|COLOUR_DIGGER,
			frames.statusBar
		);
		
		frames.counters[KEY_DIG] = new Frame(
			frames.statusBar.x + 21,
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
		
		frames.labels[KEY_BASH].putmsg(KEY_BASH + ') Bash');
		frames.labels[KEY_BOMB].putmsg(KEY_BOMB + ') Bomb');
		frames.labels[KEY_BLOCK].putmsg(KEY_BLOCK + ') Block');
		frames.labels[KEY_BUILD].putmsg(KEY_BUILD + ') Build');
		frames.labels[KEY_CLIMB].putmsg(KEY_CLIMB + ') Climb');
		frames.labels[KEY_DIG].putmsg(KEY_DIG + ') Dig');

		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 5 - frames.statusBar.x, 1);
		frames.statusBar.putmsg("N)uke", COLOUR_NUKED|COLOUR_STATUSBAR_BG);
		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 5 - frames.statusBar.x, 2);
		frames.statusBar.putmsg("P)ause", WHITE|COLOUR_STATUSBAR_BG);

		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 13 - frames.statusBar.x, 1);
		frames.statusBar.putmsg("H)elp", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 13 - frames.statusBar.x, 2);
		frames.statusBar.putmsg("Q)uit", WHITE|COLOUR_STATUSBAR_BG);

		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 24 - frames.statusBar.x, 1);
		frames.statusBar.putmsg("  Released:       Time:\r\n", WHITE|COLOUR_STATUSBAR_BG);
		frames.statusBar.gotoxy(frames.counters[KEY_CLIMB].x + 24 - frames.statusBar.x, 2);
		frames.statusBar.putmsg("Lost/Saved: ", WHITE|COLOUR_STATUSBAR_BG);

		// Display the initial lost/saved value
		frames.counters.lostSaved.putmsg(lost + "/" + saved);
		// Set the quota values
		if (typeof level.quotas == "undefined") level.quotas = {};
		for (var q in level.quotas) {
			switch (q) {
				case 'basher':
					quotas[KEY_BASH] = level.quotas[q];
					frames.counters[KEY_BASH].putmsg(quotas[KEY_BASH]);
					break;
				case 'bomber':
					quotas[KEY_BOMB] = level.quotas[q];
					frames.counters[KEY_BOMB].putmsg(quotas[KEY_BOMB]);
					break;
				case 'blocker':
					quotas[KEY_BLOCK] = level.quotas[q];
					frames.counters[KEY_BLOCK].putmsg(quotas[KEY_BLOCK]);
					break;
				case 'builder':
					quotas[KEY_BUILD] = level.quotas[q];
					frames.counters[KEY_BUILD].putmsg(quotas[KEY_BUILD]);
					break;
				case 'climber':
					quotas[KEY_CLIMB] = level.quotas[q];
					frames.counters[KEY_CLIMB].putmsg(quotas[KEY_CLIMB]);
					break;
				case 'digger':
					quotas[KEY_DIG] = level.quotas[q];
					frames.counters[KEY_DIG].putmsg(quotas[KEY_DIG]);
					break;
				default:
					break;
			}
		}

		// Add bricks, etc. to the screen
		for (var b = 0; b < level.blocks.length; b++) {
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
		for (var h = 0; h < level.hazards.length; h++) {
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
		for (var s = 0; s < level.shooters.length; s++) {
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
		function releaseLemon(x, y) {
			new Sprite.Profile("lemon", frames.field, x, y, "e", "normal");
			Sprite.profiles[Sprite.profiles.length - 1].frame.open();
			Sprite.profiles[Sprite.profiles.length - 1].ini.skill = "lemon";
			remaining--;
			frames.counters.remaining.clear();
			frames.counters.remaining.putmsg((level.lemons - remaining) + "/" + level.lemons);
			if (nuked) nuke(Sprite.profiles[Sprite.profiles.length - 1]);
		}
		frames.counters.remaining.putmsg(remaining);

		timer.addEvent(
			3000,
			level.lemons,
			releaseLemon,
			[frames.field.x + level.entrance.x - 1, frames.field.y + level.entrance.y - 1]
		);
		total = level.lemons; // When lost + saved == total, the level is done

		// Set up a timed event to update the clock
		countDown = level.time + 3; // Don't penalize users for delayed release
		function tickTock() {
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
	this.cycle = function () {
		/*	Record the index of the first lemon that overlaps with the cursor,
			if any, for use when assigning skills during this.getcmd.
			Make the cursor red if there's an overlap, white if not. */
		cursor.frame.top();
		var overlaps = Sprite.checkOverlap(cursor);
		if (overlaps) {
			for (var o = 0; o < overlaps.length; o++) {
				if (overlaps[o].ini.type != "lemon") continue;
				cursor.frame.data[0][0].attr = LIGHTRED;
				cursor.ini.hoveringOver = overlaps[o].index;
				break;
			}
		} else if (typeof cursor.ini.hoveringOver != "undefined") {
			cursor.frame.data[0][0].attr = WHITE;
			delete cursor.ini.hoveringOver;
		}

		for (var s = 0; s < Sprite.profiles.length; s++) {

			if (Sprite.profiles[s].ini.type == "shooter") {
				Sprite.profiles[s].putWeapon();
				continue;
			}

			if (Sprite.profiles[s].ini.type == "projectile" && Sprite.profiles[s].open) {
				var overlaps = Sprite.checkOverlap(Sprite.profiles[s]);
				for (var o = 0; o < overlaps.length; o++) {
					if (overlaps[o].ini.type != "lemon") continue;
					overlaps[o].remove();
					lost++;
					frames.counters.lostSaved.clear();
					frames.counters.lostSaved.putmsg(lost + "/" + saved);
				}
			}

			if (Sprite.profiles[s].ini.type != "lemon" || !Sprite.profiles[s].open) continue;

			// Remove any lemons that have gone off the screen
			if (!Sprite.profiles[s].open
				|| Sprite.profiles[s].y + Sprite.profiles[s].ini.height > frames.field.y + frames.field.height
				|| Sprite.profiles[s].x + Sprite.profiles[s].ini.width <= frames.field.x
				|| Sprite.profiles[s].x >= frames.field.x + frames.field.width
			) {
				Sprite.profiles[s].remove();
				lost++;
				frames.counters.lostSaved.clear();
				frames.counters.lostSaved.putmsg(lost + "/" + saved);
			}

			// Don't stand on top of a door, start a death march if on a hazard
			var below = Sprite.checkBelow(Sprite.profiles[s]);
			if (below) {
				for (var b = 0; b < below.length; b++) {
					if (below[b].ini.type == "exit" || below[b].ini.type == "entrance") {
						Sprite.profiles[s].moveTo(Sprite.profiles[s].x, Sprite.profiles[s].y + 1);
						break;
					} else if (below[b].ini.type == "hazard" && Sprite.profiles[s].ini.skill != "dying") {
						Sprite.profiles[s].ini.constantmotion = 0;
						lost++;
						frames.counters.lostSaved.clear();
						frames.counters.lostSaved.putmsg(lost + "/" + saved);
						Sprite.profiles[s].ini.skill = "dying";
						colorize(Sprite.profiles[s], COLOUR_DYING);
						timer.addEvent(1000, false, remove, [Sprite.profiles[s]]);
						break;
					}
				}
			} else if (
				Sprite.profiles[s].ini.skill != KEY_DIG
				&& typeof Sprite.profiles[s].ini.forceDrop == "undefined"
				&& Sprite.profiles[s].ini.skill != KEY_BASH
			) {
				/*	Sprite() moves things horizontally first, then vertically.
					In certain circumstances (a hole of only the same width as
					the sprite has just appeared beneath it) we need to force
					the sprite to drop one cell to initiate a fall.	*/
				Sprite.profiles[s].moveTo(Sprite.profiles[s].x, Sprite.profiles[s].y + 1);
				// But we only want to do it once, not every .cycle()
				Sprite.profiles[s].ini.forceDrop = true;
				/*	This will be cleared in fallIfNoFloor once the lemon
					hits bottom. */
			}

			// Animate the lemon's walk, if applicable
			if (((!Sprite.profiles[s].inFall && system.timer - Sprite.profiles[s].lastMove > Sprite.profiles[s].ini.speed)
				|| (Sprite.profiles[s].inFall && system.timer - Sprite.profiles[s].lastYMove > Sprite.profiles[s].ini.speed	))
				&& Sprite.profiles[s].ini.gravity == 1
				&& Sprite.profiles[s].ini.skill != KEY_BLOCK
				&& Sprite.profiles[s].position != "nuked"
				&& Sprite.profiles[s].ini.skill != KEY_BUILD
				&& Sprite.profiles[s].ini.skill != KEY_DIG
				&& typeof Sprite.profiles[s].ini.lastDig == "undefined"
			) {
				Sprite.profiles[s].changePosition((Sprite.profiles[s].position == "normal") ? "normal2" : "normal");
			}

			// Make the different types of lemons behave as they should
			switch (Sprite.profiles[s].ini.skill) {

				case "lemon":
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case KEY_BASH:
					bashersGonnaBash(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case KEY_BLOCK:
					fallIfNoFloor(Sprite.profiles[s]);
					break;

				case KEY_BOMB:
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case KEY_BUILD:
					turnOrClimbIfObstacle(Sprite.profiles[s]);
					buildersGonnaBuild(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case KEY_CLIMB:
					climbIfObstacle(Sprite.profiles[s]);
					fallIfNoFloor(Sprite.profiles[s]);
					removeIfAtExit(Sprite.profiles[s]);
					break;

				case KEY_DIG:
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
		if (countDown <= 0) {
			if (saved < (total / 2)) return LEVEL_TIME;
			this.score = (saved * 100);
			return LEVEL_NEXT;
		} else if (lost + saved == total && saved >= (total / 2)) {
			this.score = (saved * 100) + (countDown * 10);
			return LEVEL_NEXT;
		} else if (lost + saved == total && saved < (total / 2)) {
			return LEVEL_DEAD;
		} else {
			return LEVEL_CONTINUE;
		}

	}

	// Do what the user asked us to do, if it's valid
	this.getcmd = function (userInput) {

		var ret = true;

		if (userInput.mouse !== null) {
			// If it wasn't a left-click
			if (userInput.mouse.button != 0 && !userInput.mouse.press) return ret;
			// If they clicked inside the statusbar region ...
			if (userInput.mouse.y >= frames.statusBar.y && userInput.mouse.y <= frames.statusBar.y + 1) {
				if (userInput.mouse.x < frames.labels[KEY_BOMB].x) {
					if (userInput.mouse.y == frames.statusBar.y) {
						return this.getcmd({ key: KEY_BASH, mouse: null });
					} else {
						return this.getcmd({ key: KEY_BLOCK, mouse: null });
					}
				} else if (userInput.mouse.x < frames.labels[KEY_CLIMB].x) {
					if (userInput.mouse.y == frames.statusBar.y) {
						return this.getcmd({ key: KEY_BOMB, mouse: null });
					} else {
						return this.getcmd({ key: KEY_DIG, mouse: null });
					}
				} else if (userInput.mouse.x < frames.counters[KEY_CLIMB].x + 5) {
					if (userInput.mouse.y == frames.statusBar.y) {
						return this.getcmd({ key: KEY_CLIMB, mouse: null });
					} else {
						return this.getcmd({ key: KEY_BUILD, mouse: null });
					}
				} else if (userInput.mouse.x < frames.counters[KEY_CLIMB].x + 13) {
					if (userInput.mouse.y == frames.statusBar.y) {
						for (var s = 0; s < Sprite.profiles.length; s++) {
							if (Sprite.profiles[s].ini.type != "lemon" || !Sprite.profiles[s].open) continue;
							nuke(Sprite.profiles[s]);
						}
						nuked = true;
					} else {
						state = STATE_PAUSE;
					}
				} else if (userInput.mouse.x < frames.counters[KEY_CLIMB].x + 24) {
					if (userInput.mouse.y == frames.statusBar.y) {
						state = STATE_HELP;
					} else {
						return false;
					}
				}
			} else if (
				userInput.mouse.y >= frames.field.y
				&& userInput.mouse.y < frames.field.y + frames.field.height
				&& userInput.mouse.x >= frames.field.x
				&& userInput.mouse.x < frames.field.x + frames.field.width
			) {
				cursor.moveTo(userInput.mouse.x, userInput.mouse.y);
				this.cycle();
				if (currentSkill != null) return this.getcmd({ key: '\r', mouse: null });
			}
			return ret;
		}

		switch (userInput.key.toUpperCase()) {

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
				if (cursor.y > frames.field.y) cursor.moveTo(cursor.x, cursor.y - 1);
				break;

			case "2":
			case KEY_DOWN:
				if (cursor.y < frames.field.y + frames.field.height - 1) cursor.moveTo(cursor.x, cursor.y + 1);
				break;

			case "4":
			case KEY_LEFT:
				if (cursor.x > frames.field.x) cursor.moveTo(cursor.x - 1, cursor.y);
				break;

			case "6":
			case KEY_RIGHT:
				if (cursor.x < frames.field.x + frames.field.width - 1) cursor.moveTo(cursor.x + 1, cursor. y);
				break;

			case "7":
				if (cursor.y > frames.field.y && cursor.x > frames.field.x) cursor.moveTo(cursor.x - 1, cursor.y - 1);
				break;

			case "9":
				if (cursor.y > frames.field.y && cursor.x < frames.field.x + frames.field.width - 1) cursor.moveTo(cursor.x + 1, cursor.y - 1);
				break;

			case "1":
				if (cursor.y < frames.field.y + frames.field.height - 1 && cursor.x > frames.field.x) cursor.moveTo(cursor.x - 1, cursor.y + 1);
				break;

			case "3":
				if (cursor.y < frames.field.y + frames.field.height - 1 && cursor.x < frames.field.x + frames.field.width - 1) cursor.moveTo(cursor.x + 1, cursor.y + 1);
				break;

			case "N":
				for (var s = 0; s < Sprite.profiles.length; s++) {
					if (Sprite.profiles[s].ini.type != "lemon" || !Sprite.profiles[s].open) continue;
					nuke(Sprite.profiles[s]);
				}
				nuked = true;
				break;

			case KEY_BASH:
			case KEY_BLOCK:
			case KEY_BOMB:
			case KEY_BUILD:
			case KEY_CLIMB:
			case KEY_DIG:
				if (currentSkill !== null) {
					frames.labels[currentSkill].data[0] = frames.labels[currentSkill].data[0].map(function (e) {
						if (typeof e != 'object') return e;
						e.attr &=~ BG_LIGHTGRAY;
						e.attr |= COLOUR_STATUSBAR_BG;
						return e;
					});
					frames.labels[currentSkill].invalidate();		
				}
				currentSkill = userInput.key.toUpperCase();
				frames.labels[currentSkill].data[0] = frames.labels[currentSkill].data[0].map(function (e) {
					if (typeof e != 'object') return e;
					e.attr &=~ COLOUR_STATUSBAR_BG;
					e.attr |= BG_LIGHTGRAY;
					return e;
				});
				frames.labels[currentSkill].invalidate();
			case "\r":
			case " ":
				if (typeof cursor.ini.hoveringOver == "undefined") break;
				if (quotas[currentSkill] < 1) break;
				quotas[currentSkill]--;
				frames.counters[currentSkill].clear();
				frames.counters[currentSkill].putmsg(quotas[currentSkill]);
				Sprite.profiles[cursor.ini.hoveringOver].ini.skill = currentSkill;
				colorize(Sprite.profiles[cursor.ini.hoveringOver], COLOUR[currentSkill]);
				if (currentSkill == KEY_BLOCK || currentSkill == KEY_DIG) {
					Sprite.profiles[cursor.ini.hoveringOver].ini.constantmotion = 0;
					Sprite.profiles[cursor.ini.hoveringOver].changePosition("fall");
					if (currentSkill == KEY_BLOCK) timer.addEvent(1000, true, tapFoot, [Sprite.profiles[cursor.ini.hoveringOver]]);
				} else if (currentSkill == KEY_BOMB) {
					nuke(Sprite.profiles[cursor.ini.hoveringOver]);
				}
				break;

			default:
				break;
		}

		return ret;

	}

	this.pause = function (events) {
		if (typeof events == "undefined") {
			var events = timer.events;
			timer.events = [];
			return events;
		} else {
			for (var e = 0; e < events.length; e++) {
				timer.addEvent(events[e].interval, events[e].repeat, events[e].action, events[e].arguments, events[e].context);
			}
		}
	}

	// Reset the Sprite globals, clean up the display
	this.close = function () {
		cursor.remove();
		Sprite.platforms = [];
		for (var s = 0; s < Sprite.profiles.length; s++) {
			Sprite.profiles[s].remove();
		}
		Sprite.profiles = [];
		frames.game.delete();
	}

	loadLevel(l, frame);

}
