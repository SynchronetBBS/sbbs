var Answer = function (frame, round, category, clue, answer, game) {

	var frames = {
		border : null,
		top : null
	};

	game.state.game.rounds[round.number][category].push(clue);

	function initDisplay() {
		frames.border = new Frame(
			frame.x + 5,
			frame.y + 5,
			70,
			9,
			WHITE,
			frame
		);
		frames.top = new Frame(1, 1, 1, 1, BG_BLUE|WHITE, frames.border);
		frames.top.nest(1, 1);
		frames.border.drawBorder([LIGHTBLUE,CYAN,LIGHTCYAN,WHITE]);
		frames.border.open();
	}

	function getResult() {
		value = (
			game.state.wager === null
			? round[category].clues[clue].value
			: game.state.wager
		);
		if (compareAnswer(round[category].clues[clue].id, answer)) {
			frames.top.attr = GREEN;
			frames.top.center('Correct!');
			game.winnings = value;
		} else {
			frames.top.attr = RED;
			frames.top.center('Incorrect!');
			frames.top.putmsg('\r\n\r\n');
			frames.top.attr = LIGHTGRAY;
			frames.top.center('The correct answer is:');
			frames.top.crlf();
			frames.top.attr = WHITE;
			frames.top.center(round[category].clues[clue].answer);
			game.winnings = (0 - value);
		}
		frames.top.putmsg('\r\n\r\n');
		frames.top.attr = LIGHTBLUE;
		frames.top.center('[Press any key to continue]\r\n');
		frames.top.center('[Press \1h\1rR\1h\1b to report a bad clue]');
	}

	function init() {
		initDisplay();
		getResult();
	}

	this.getcmd = function (cmd) {
		if (cmd.toUpperCase() === 'R') {
			frames.top.gotoxy(1, frames.top.height);
			frames.top.clearline();
			frames.top.gotoxy(1, frames.top.height);
			frames.top.center('\1h\1rThis clue has been flagged.');
			database.reportClue(user, round[category].clues[clue], answer);
			return false;
		} else {
			return (cmd !== '');
		}
	}

	this.cycle = function () { }

	this.close = function () {
		frames.border.close();
	}

	init();

}