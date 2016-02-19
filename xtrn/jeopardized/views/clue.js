var Clue = function(frame, round, category, c) {

	var self = this;

	var state = STATE_CLUE_INPUT;

	var TIME_LIMIT = 30;
	var ticks = TIME_LIMIT;

	var typeahead,
		timer;

	var clue = round[category].clues[c];

	this.expired = false;
	this.category = category;
	this.clue = c;

	var frames = {
		border: null,
		top : null,
		clue : null,
		hint : null
	};

	function initDisplay() {

		frames.border = new Frame(
			frame.x + 1,
			frame.y + 5,
			frame.width - 2,
			14,
			WHITE,
			frame
		);
		frames.border.checkbounds = false;

		frames.top = new Frame(1, 1, 1, 1, WHITE, frames.border);
		frames.top.nest(1, 1);
		frames.top.checkbounds = false;
		frames.top.center(round[category].name + ' for ' + clue.value);

		frames.clue = new Frame(
			frames.top.x + 1,
			frames.top.y + 1,
			frames.top.width - 2,
			frames.top.height - 5,
			BG_BLUE|WHITE,
			frames.top
		);
		frames.clue.word_wrap = true;
		frames.clue.putmsg(clue.clue);

		frames.time = new Frame(
			frames.top.x + 1,
			frames.clue.y + frames.clue.height + 2,
			frames.clue.width,
			1,
			GREEN,
			frames.top
		);
		frames.time.putmsg('\1h\1wTime left: \1n\1g' + TIME_LIMIT);

		frames.hint = new Frame(
			frames.time.x,
			frames.time.y + 1,
			frames.clue.width,
			1,
			WHITE,
			frames.top
		);
		var hint = clue.answer.replace(/\s/g, '  ');
		hint = hint.replace(/\w/g, '_ ');
		frames.hint.putmsg('Hint: ' + hint);

		frames.border.drawBorder([LIGHTBLUE, CYAN, LIGHTCYAN, WHITE]);
		frames.border.open();

		typeahead = new Typeahead(
			{	x : frames.top.x + 1,
				y : frames.clue.y + frames.clue.height + 1,
				frame : frames.top,
				len : frames.top.width - 2,
				fg : WHITE
			}
		);

	}

	function initTimer() {

		timer = new Timer();

		timer.addEvent(
			TIME_LIMIT * 1000,
			false,
			function () {
				self.expired = true;
			}
		);

		timer.addEvent(
			1000,
			TIME_LIMIT,
			function () {
				ticks--;
				if (ticks < 10) {
					frames.time.attr = RED;
				} else if (ticks < 20) {
					frames.time.attr = BROWN;
				}
				frames.time.gotoxy(12, 1);
				frames.time.putmsg(ticks + ' ');
			}
		);

	}

	function init() {
		initDisplay();
		initTimer();
	}

	this.cycle = function () {
		typeahead.cycle();
		timer.cycle();
	}

	this.close = function () {
		typeahead.close();
		frames.border.close();
	}

	this.getcmd = function (cmd) {
		var ret = typeahead.inkey(cmd);
		switch (state) {
			case STATE_CLUE_INPUT:
				break;
			default:
				break;
		}
		return ret;
	}

	init();

}