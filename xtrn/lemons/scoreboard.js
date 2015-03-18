// An interface to the Lemons JSON DB.  Slightly more than a scoreboard.
var ScoreBoard = function(host, port) {

	var frames = {};
	var jsonClient = new JSONClient(host, port);

	/*	Add a score to the database.  Impress nobody by faking an awesome
		score in Lemons! */
	this.addScore = function(score, level) {

		if(!jsonClient.connected)
			jsonClient.connect(host, port);

		var entry = {
			'player' : user.alias,
			'system' : system.name,
			'level' : level,
			'score' : score
		};

		jsonClient.write(DBNAME, "SCORES.LATEST", entry, 2);

	}

	// Returns the high scores array (20 most recent scores.)
	this.getScores = function() {

		if(!jsonClient.connected)
			jsonClient.connect(host, port);

		var scores = jsonClient.read(DBNAME, "SCORES.HIGH", 1);

		if(!scores)
			scores = [];

		return scores;

	}

	/*	Returns the number of the highest level the current user has reached.
		(Numbering starts at zero; returns zero if this is a new player or a
		an empty database.)	*/
	this.getHighestLevel = function() {

		if(!jsonClient.connected)
			jsonClient.connect(host, port);

		var player = jsonClient.read(
			DBNAME,
			"PLAYERS." + base64_encode(user.alias + "@" + system.name),
			1
		);

		if(!player)
			return 0;

		return player.LEVEL;

	}

	// Display the scoreboard.
	this.open = function() {

		if(!jsonClient.connected)
			jsonClient.connect(host, port);

		frames.scoreFrame = new Frame(
			frame.x,
			frame.y,
			frame.width,
			frame.height,
			BG_BLACK|WHITE,
			frame
		);

		frames.scoreSubFrame = new Frame(
			frames.scoreFrame.x + 1,
			frames.scoreFrame.y + 1,
			frames.scoreFrame.width - 2,
			frames.scoreFrame.height -2,
			BG_BLACK|WHITE,
			frames.scoreFrame
		);

		var putLine = function(player, system, level, score) {
			frames.scoreSubFrame.putmsg(
				format(
					"%-25s %-30s %5s %14s\r\n",
					player, system, level, score
				)
			);
		}

		frames.scoreFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
		frames.scoreFrame.home();
		frames.scoreFrame.center(ascii(180) + "Lemons - High Scores" + ascii(195));

		frames.scoreSubFrame.gotoxy(1, 2);
		frames.scoreSubFrame.attr = BG_BLACK|LIGHTCYAN;
		putLine("Player", "System", "Level", "Score");
		frames.scoreSubFrame.attr = BG_BLACK|WHITE;

		var scores = this.getScores();
		for(var s = 0; s < scores.length; s++) {
			putLine(
				scores[s].player,
				scores[s].system,
				(scores[s].level + 1),
				scores[s].score
			);
		}

		frames.scoreFrame.end();
		frames.scoreFrame.center(ascii(180) + "Press any key to continue" + ascii(195));

		frames.scoreFrame.open();

	}

	// Close the scoreboard if it's open, disconnect the JSON-DB client.
	this.close = function() {
		if(typeof frames.scoreFrame != "undefined")
			frames.scoreFrame.delete();
		jsonClient.disconnect();
	}

}