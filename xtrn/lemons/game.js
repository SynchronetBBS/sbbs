// The Game object tracks a user's progress and score across multiple levels
var Game = function(host, port) {

	// We want the user to start by choosing an initial level, if applicable.
	var gameState = GAME_STATE_CHOOSELEVEL;

	// Some values to be tracked throughout this gaming session
	var stats = {
		'level' : 0, // Dummy value
		'score' : 0, // Index into the 'levels' array (see below)
		'turns' : 5  // How many turns to start the player off with
	};

	// Just an initial value
	var levelState = LEVEL_CONTINUE;

	// This will always either be false or an instance of PopUp (lemons.js)
	var popUp = false;

	/*	This will always either be false (if not paused) or an array of stored
		Timer events (if paused). */
	var events = false;

	// Create a DBHelper object, which we'll use for a few things later
	var dbHelper = new DBHelper(host, port);

	// Load the levels from the DB (or local file)
	var levels = dbHelper.getLevels();

	// Get this user's highest-achieved level
	var l = dbHelper.getHighestLevel();

	// This is unnecessary except in a very edge case
	if(l >= levels.length)
		l = levels.length - 1;

	// This will be a LevelChooser if we're picking a level
	var levelChooser = false;

	// Make the paused/unpaused status of this Game publically accessible
	this.paused = false;

	// A custom prompt that works with this game's input & state model
	var LevelChooser = function() {

		var lcFrame = new Frame(
			frame.x + Math.ceil((frame.width - 32) / 2),
			frame.y + Math.ceil((frame.height - 3) / 2),
			32,
			3,
			BG_BLACK|WHITE,
			frame
		);

		var lcSubFrame = new Frame(
			lcFrame.x + 1,
			lcFrame.y + 1,
			lcFrame.width - 2,
			1,
			BG_BLACK|WHITE,
			lcFrame
		);

		var inputFrame = new Frame(
			lcSubFrame.x + lcSubFrame.width - 5,
			lcSubFrame.y,
			3,
			1,
			BG_BLUE|WHITE,
			lcSubFrame
		);

		lcFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
		lcSubFrame.putmsg("Start at level: (1 - " + (l + 1) + ") ");

		var input = (l + 1) + "";
		inputFrame.putmsg(input);

		this.getcmd = function(userInput) {
			
			// We're only looking for numbers, backspace, or enter
			if(	userInput == ""
				||
				(	userInput != "\x08"
					&&
					userInput != "\r"
					&&
					userInput != "\n"
					&&
					isNaN(parseInt(userInput))
				)
			) {
				return;
			}

			// Handle backspace
			if(userInput == "\x08" && input.length > 0) {
			
				input = input.substr(0, input.length - 1);
			
			// Handle enter
			} else if(userInput == "\r" || userInput == "\n") {
			
				var ret = parseInt(input);
				
				input = "";
				inputFrame.clear();
				inputFrame.putmsg(input);

				// Don't return an invalid number
				if(input.length > 3 || isNaN(ret) || ret > (l + 1) || ret < 1)
					return;
				else
					return (ret - 1);
			
			// Anything else that's gotten this far can be appended
			} else {
			
				input += userInput;
			
			}

			// If we've gotten this far, the input box needs a refresh
			inputFrame.clear();
			inputFrame.putmsg(input);

			return;

		}

		this.close = function() {
			lcFrame.delete();
		}

		lcFrame.open();

	}

	var level; // We'll need this variable within some of the following methods

	// Handle user input passed to us by the parent script	
	this.getcmd = function(userInput) {

		// If the user should be choosing a level ...
		if(gameState == GAME_STATE_CHOOSELEVEL) {
		
			// If this is a new user or they haven't beat level 0, start there
			if(l == 0) {
	
				level = new Level(levels[stats.level], stats.level);
				gameState = GAME_STATE_NORMAL;
	
			// Otherwise let them start at up to their highest level
			} else if(!levelChooser) {

				levelChooser = new LevelChooser();

			// And if a chooser is already open, use it
			} else {

				var ll = levelChooser.getcmd(userInput);
				// The chooser should return a valid level number, or nothing
				if(typeof ll != "undefined") {

					levelChooser.close();
					levelChooser = false;
					stats.level = ll;
					level = new Level(levels[stats.level], stats.level);
					gameState = GAME_STATE_NORMAL;
					return true;

				}

			}
		
		/*	If gameplay is in progress, pass user input to the Level object.
			If Level.getcmd returns false, the user hit 'Q' to quit. */
		} else if(gameState == GAME_STATE_NORMAL && !level.getcmd(userInput)) {
		
			level.close();
			return false;
		
		//	If a pop-up is on the screen, remove it if the user hits a key
		} else if(gameState == GAME_STATE_POPUP && userInput != "") {
		
			gameState = GAME_STATE_NORMAL;
			popUp.close();
		
		}
		
		// Game.getcmd will return true unless the user hit 'Q' during gameplay
		return true;
	
	}

	/*	This is called during the main loop when in gameplay state, and returns
		true unless the user runs out of turns or beats the game. */
	this.cycle = function() {

		// If we're waiting for the player to choose a level
		if(gameState == GAME_STATE_CHOOSELEVEL) {

			return true;

		// If we're waiting for them to dismiss a pop-up message
		} else if(gameState == GAME_STATE_POPUP) {

			return true;

		// If they failed to rescue enough lemons, or ran out of time
		} else if(levelState == LEVEL_DEAD || levelState == LEVEL_TIME) {

			// Close the level, perchance to reopen it and start again
			level.close();
			// If the user has run out of turns ...
			if(stats.turns < 1) {
				stats.score = stats.score + level.score;
				return false;
			// If the user has turns left, let them try again
			} else {
				level = new Level(levels[stats.level], stats.level);
			}

		// If they beat the level, move on to the next one if it exists
		} else if(levelState == LEVEL_NEXT) {

			level.close();
			if(stats.level < levels.length - 1) {
				stats.level++;
				level = new Level(levels[stats.level], stats.level);
			} else {
				return false;
			}

		}

		// Call the Level.cycle housekeeping method, respond as necessary
		levelState = level.cycle();
		switch(levelState) {

			// The user failed to rescue enough lemons.  Raise a pop-up.
			case LEVEL_DEAD:
				stats.turns--;
				popUp = new PopUp(
					[	"What a sour outcome - less than 50% of your lemons survived!",
						"Score: " + stats.score,
						stats.turns + " turns remaining",
						"[ Press any key to continue ]"
					]
				);
				gameState = GAME_STATE_POPUP;
				break;

			// The user ran out of time.  Raise a pop-up.
			case LEVEL_TIME:
				stats.turns--;
				popUp = new PopUp(
					[	"You ran out of time!  Now the lemons are going to miss their party.",
						"Score: " + stats.score,
						stats.turns + " turns remaining",
						"[ Press any key to continue ]"
					]
				);
				gameState = GAME_STATE_POPUP;
				break;

			// The user beat the level.  Raise a pop-up.
			case LEVEL_NEXT:
				stats.score += level.score;
				popUp = new PopUp(
					[	"You saved some lemons.  Savour this pointless victory!",
						"Score: " + stats.score,
						"[ Press any key to continue ]"
					]
				);
				gameState = GAME_STATE_POPUP;
				break;

			// The level can continue cycling.  Don't do shit.
			case LEVEL_CONTINUE:
				break;

			default:
				break;

		}

		return true; // Everything's satisfactory in the neighbourhood

	}

	// Pause or resume the level
	this.pause = function() {

		/*	If the level isn't paused, pause it and store the timed events
			that it returns. */
		if(!events) {

			events = level.pause();
			this.paused = true;

		/*	If the level is paused, unpause it by passing in the array of
			stored events. */
		} else {

			level.pause(events);
			events = false;
			this.paused = false;

		}

	}

	// If the user is done with this gaming session, save their score
	this.close = function() {
		dbHelper.addScore(stats.score, stats.level);
		dbHelper.close();
	}

}