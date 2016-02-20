var Round = function(frame, r, game) {

	var board, wager, clue, answer, state;
	var round = database.getRound(r);

	this.number = r;

	if (r < 3 && countAnswers(game.state.game, r) < 25) {
		state = STATE_ROUND_BOARD;
		board = new Board(frame, round, game.state.game);
	} else if (r === 3 && countAnswers(game.state.game, r) < 1) {
		state = STATE_ROUND_WAGER;
		wager = new Wager(frame, game, round, 0, 0);
		database.markClueAsUsed(user, 3, 0, 0);
	} else {
		state = STATE_ROUND_COMPLETE;
	}

	this.getcmd = function (cmd) {

		var progress = RET_ROUND_CONTINUE;

		switch (state) {

			case STATE_ROUND_BOARD:
				if (cmd.toUpperCase() === 'Q') {
					progress = RET_ROUND_QUIT;
				} else {
					var ret = board.getcmd(cmd);
					if (typeof ret === 'object' &&
						typeof ret.column === 'number' &&
						typeof ret.row === 'number'
					) {
						database.markClueAsUsed(user, r, ret.column, ret.row);
						if (round[ret.column].clues[ret.row].dd) {
							state = STATE_ROUND_WAGER;
							wager = new Wager(
								frame, game, round, ret.column, ret.row
							);
						} else {
							state = STATE_ROUND_CLUE;
							clue = new Clue(frame, round, ret.column, ret.row);
						}
					}
				}
				break;

			case STATE_ROUND_WAGER:
				var ret = wager.getcmd(cmd);
				if (typeof ret === 'number') {
					game.state.wager = ret;
					wager.close();
					state = STATE_ROUND_CLUE;
					clue = new Clue(frame, round, wager.category, wager.clue);
				}
				break;

			case STATE_ROUND_CLUE:
				var ret = clue.getcmd(cmd);
				if (typeof ret === 'string' && ret !== '') {
					state = STATE_ROUND_ANSWER;
					clue.close();
					database.submitAnswer(
						user,
						r,
						clue.category,
						clue.clue,
						ret,
						game.state.wager
					);
					answer = new Answer(
						frame,
						round,
						clue.category,
						clue.clue,
						ret,
						game
					);
				}
				break;

			case STATE_ROUND_ANSWER:
				var ret = answer.getcmd(cmd);
				if (ret) {
					answer.close();
					game.state.wager = null;
					var ac = countAnswers(game.state.game, r);
					if (r < 3) {
						state = STATE_ROUND_BOARD;
						if (ac >= 10 && game.state.game.winnings >= 0) {
							progress = RET_ROUND_NEXT;
						} else if (ac >= 25) {
							progress = RET_ROUND_STOP;
						}
					} else if (r >= 3) {
						progress = RET_ROUND_LAST;
					}
				}
				break;

			case STATE_ROUND_COMPLETE:
				progress = RET_ROUND_STOP;
				break;

			default:
				break;

		}

		return progress;

	}

	this.cycle = function () {

		switch (state) {

			case STATE_ROUND_BOARD:
				board.cycle();
				break;

			case STATE_ROUND_WAGER:
				wager.cycle();
				break;
			
			case STATE_ROUND_CLUE:
				clue.cycle();
				if (clue.expired) {
					clue.close();
					database.submitAnswer(
						user,
						r,
						clue.category,
						clue.clue,
						'!wrong_answer!',
						game.state.wager
					);
					answer = new Answer(
						frame,
						round,
						clue.category,
						clue.clue,
						'!wrong_answer!',
						game
					);
					state = STATE_ROUND_ANSWER;
				}
				break;
			
			case STATE_ROUND_ANSWER:
				answer.cycle();
				break;
			
			default:
				break;
	
		}

	}

	this.close = function () {
		if (typeof clue !== 'undefined') clue.close();
		if (typeof board !== 'undefined') board.close();
	}

}