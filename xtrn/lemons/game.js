// The Game object tracks a user's progress and score across multiple levels
var Game = function (host, port) {

	var gameState = GAME_STATE_CHOOSELEVEL;
	var stats = {
		level: 0, // Dummy value
		score: 0, // Index into the 'levels' array (see below)
		turns: 5, // How many turns to start the player off with
	};
	var popUp = false;
	var events = false;
	var levelChooser = false;
	var levelState = LEVEL_CONTINUE;
	var dbHelper = new DBHelper(host, port);
	var levels = dbHelper.getLevels();
	var l = dbHelper.getHighestLevel();
	if (l >= levels.length) l = levels.length - 1;

	this.paused = false;

	var LevelChooser = function () {

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

		this.getcmd = function (userInput) {
			if (userInput.key == "" || (userInput.key != "\x08" && userInput.key != "\r" && userInput.key != "\n" && isNaN(parseInt(userInput.key)))) {
				return;
			}
			if (userInput.key == "\x08" && input.length > 0) {
				input = input.substr(0, input.length - 1);
			} else if (userInput.key == "\r" || userInput.key == "\n") {
				var ret = parseInt(input);
				input = "";
				inputFrame.clear();
				inputFrame.putmsg(input);
				if (input.length > 3 || isNaN(ret) || ret > (l + 1) || ret < 1) return;
				return (ret - 1);
			} else {
				input += userInput.key;
			}
			inputFrame.clear();
			inputFrame.putmsg(input);
			return;
		}

		this.close = function () {
			lcFrame.delete();
		}

		lcFrame.open();

	}

	var level;

	// Handle user input passed to us by the parent script	
	this.getcmd = function (userInput) {
		if (gameState == GAME_STATE_CHOOSELEVEL) {
			if (l == 0) {	
				level = new Level(levels[stats.level], stats.level);
				gameState = GAME_STATE_NORMAL;
			} else if (!levelChooser) {
				levelChooser = new LevelChooser();
			} else {
				var ll = levelChooser.getcmd(userInput);
				if (typeof ll != "undefined") {
					levelChooser.close();
					levelChooser = false;
					stats.level = ll;
					level = new Level(levels[stats.level], stats.level);
					gameState = GAME_STATE_NORMAL;
					return true;
				}
			}
		} else if (gameState == GAME_STATE_NORMAL && !level.getcmd(userInput)) {
			level.close();
			return false;
		} else if (gameState == GAME_STATE_POPUP && (userInput.key != "" || userInput.mouse !== null)) {
			gameState = GAME_STATE_NORMAL;
			popUp.close();
		}
		return true;
	}

	this.cycle = function () {
		if (gameState == GAME_STATE_CHOOSELEVEL) {
			return true;
		} else if (gameState == GAME_STATE_POPUP) {
			return true;
		} else if (levelState == LEVEL_DEAD || levelState == LEVEL_TIME) {
			level.close();
			if (stats.turns < 1) {
				stats.score = stats.score + level.score;
				return false;
			} else {
				level = new Level(levels[stats.level], stats.level);
			}
		} else if (levelState == LEVEL_NEXT) {
			level.close();
			if (stats.level < levels.length - 1) {
				stats.level++;
				level = new Level(levels[stats.level], stats.level);
			} else {
				return false;
			}
		}

		levelState = level.cycle();
		switch (levelState) {
			case LEVEL_DEAD:
				stats.turns--;
				popUp = new PopUp([
					"What a sour outcome - less than 50% of your lemons survived!",
					"Score: " + stats.score,
					stats.turns + " turns remaining",
					"[ Press any key to continue ]"
				]);
				gameState = GAME_STATE_POPUP;
				break;
			case LEVEL_TIME:
				stats.turns--;
				popUp = new PopUp([
					"You ran out of time!  Now the lemons are going to miss their party.",
					"Score: " + stats.score,
					stats.turns + " turns remaining",
					"[ Press any key to continue ]"
				]);
				gameState = GAME_STATE_POPUP;
				break;
			case LEVEL_NEXT:
				stats.score += level.score;
				popUp = new PopUp([
					"You saved some lemons.  Savour this pointless victory!",
					"Score: " + stats.score,
					"[ Press any key to continue ]"
				]);
				gameState = GAME_STATE_POPUP;
				break;
			case LEVEL_CONTINUE:
				break;
			default:
				break;
		}

		return true;

	}

	this.pause = function () {
		if (!events) {
			events = level.pause();
			this.paused = true;
		} else {
			level.pause(events);
			events = false;
			this.paused = false;
		}
	}

	this.close = function () {
		dbHelper.addScore(stats.score, stats.level);
		dbHelper.close();
	}

}