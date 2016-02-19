var Game = function (frame) {

	var self = this;

	var popUp,
		messages;

	this.state = {
		usr : null,
		game : null,
		round : null,
		wager : null,
		state : STATE_GAME_PLAY
	};

	var frames = {
		border : null,
		top : null,
		board : null,
		stats : null
	};

	function initData() {

		self.state.usr = database.getUser(user);
		if (typeof self.state.usr === 'undefined') {
			throw 'DB: error reading users.' + database.getUserID(user);
		}

		self.state.game = database.getUserGameState(user);
		if (typeof self.state.game === 'undefined') {
			throw 'DB: error reading game.users.' + database.getUserID(user);
		}

	}

	function initDisplay() {

		frames.top = new Frame(
			frame.x,
			frame.y,
			frame.width,
			frame.height,
			WHITE,
			frame
		);
		frames.top.checkbounds = false;

		frames.board = new Frame(
			frames.top.x,
			frames.top.y,
			frames.top.width,
			16,
			WHITE,
			frames.top
		);

		frames.stats = new Frame(
			frames.top.x,
			frames.board.y + frames.board.height,
			frames.top.width,
			1,
			BG_BLUE|WHITE,
			frames.top
		);

		frames.top.open();

	}

	function startRound() {
		if (self.state.game.round > 0 && self.state.game.round < 4 &&
			(	(	self.state.game.round < 3 &&
					countAnswers(self.state.game, self.state.game.round) < 25
				) ||
				(	self.state.game.round === 3 &&
					countAnswers(self.state.game, self.state.game.round) < 1
				)
			)
		) {
			self.state.round = new Round(
				frames.board,
				self.state.game.round,
				self
			);
			popUp = new PopUp(
				frame,
				'Starting round ' + self.state.game.round,
				'[Press any key to continue]',
				PROMPT_ANY,
				STATE_GAME_PLAY
			);
		} else {
			popUp = new PopUp(
				frame,
				"You've gone as far as you can for today.\r\n" +
				"Please come back tomorrow to play again.",
				"[Press any key to continue]",
				PROMPT_ANY
			);
		}
		refreshStats();
		self.state.state = STATE_GAME_POPUP;
	}

	function refreshStats() {
		frames.stats.clear();
		frames.stats.center(
			format(
				'Round: \1h\1c%s\1h\1w  ' +
				'Today: \1h\1%s$%s\1h\1w  ' +
				'Total: \1h\1%s$%s\1h\1w  ' +
				'Answers: \1h\1g%s\1h\1w:\1h\1r%s ',
				self.state.game.round,
				self.state.game.winnings > 0 ? 'g' : 'r',
				self.state.game.winnings,
				self.state.usr.winnings > 0 ? 'g' : 'r',
				self.state.usr.winnings,
				self.state.usr.answers.correct,
				self.state.usr.answers.incorrect
			)
		);
		frames.stats.gotoxy(frames.stats.width - 10, 1);
		frames.stats.putmsg('\1h\1wQ\1cuit');
	}

	function init() {
		initData();
		initDisplay();
		messages = new Messages(
			frames.top,
			frames.top.x,
			frames.stats.y + 1,
			frames.top.width,
			7,
			true
		);
		startRound();
	}

	this.getcmd = function (cmd) {
		var ret = true;
		switch (this.state.state) {
			case STATE_GAME_PLAY:
				if (cmd === '\x09') {
					messages.focus = true;
					this.state.state = STATE_GAME_MESSAGES;
				} else {
					ret = this.state.round.getcmd(cmd);
					if (ret === RET_ROUND_QUIT) {
						this.state.round.close();
						ret = false;
					} else if (ret === RET_ROUND_NEXT) {
						this.state.round.close();
						database.nextRound(user);
						this.state.game.round++;
						startRound();
						ret = true;
					} else if (ret === RET_ROUND_STOP) {
						this.state.round.close();
						this.state.game.round = -1;
						startRound();
						ret = true;
					} else if (ret === RET_ROUND_LAST) {
						this.state.round.close();
						database.nextRound(user);
						this.state.game.round = 4;
						startRound();
						ret = true;
					} else {
						ret = true;
					}
				}
				break;
			case STATE_GAME_MESSAGES:
				if (cmd === '\x09') {
					messages.focus = false;
					this.state.state = STATE_GAME_PLAY;
				} else {
					messages.getcmd(cmd);
				}
				break;
			case STATE_GAME_POPUP:
				if (typeof popUp.getcmd(cmd) !== 'undefined') {
					var s = popUp.close();
					if (typeof s !== 'undefined') {
						this.state.state = s;
					} else {
						ret = false;
					}
				}
				break;
			default:
				break;
		}
		return ret;
	}

	this.cycle = function () {
		if (self.state.round !== null) self.state.round.cycle();
		messages.cycle();
	}

	this.close = function () {
		messages.close();
		if (self.state.round !== null) self.state.round.close();
		frames.top.close();
	}

	this.__defineGetter__(
		'winnings',
		function () {
			return self.state.game.winnings;
		}
	);

	this.__defineSetter__(
		'winnings',
		function (val) {
			self.state.game.winnings = self.state.game.winnings + val;
			self.state.usr.winnings = self.state.usr.winnings + val;
			if (val > 0) {
				self.state.usr.answers.correct++;
			} else {
				self.state.usr.answers.incorrect++;
			}
			refreshStats();
		}
	);

	init();

}