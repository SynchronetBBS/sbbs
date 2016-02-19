var Wager = function (frame, game, round, category, clue) {

	this.category = category;
	this.clue = clue;

	if (round.number < 3) {
		var msg = "You've been DOUBLY JEOPARDIZED!";
		var max = Math.max(
			round[category].clues[round[category].clues.length - 1].value,
			game.winnings
		);
	} else {
		var msg = "FINAL ROUND";
		var max = Math.max(1000, game.winnings);
	}

	msg += '\r\n' + 
		'Category: ' + round[category].name + '\r\n' +
		'Please enter your bet: $5 - $' + max;

	var popUp = new PopUp(frame, msg, '$', PROMPT_NUMBER, STATE_ROUND_WAGER);

	this.cycle = function () {
		popUp.cycle();
	}

	this.getcmd = function (cmd) {
		var ret = popUp.getcmd(cmd);
		if (typeof ret !== 'number') return;
		if (ret < 5 || ret > max) return;
		return ret;
	}

	this.close = function () {
		popUp.close();
	}

}