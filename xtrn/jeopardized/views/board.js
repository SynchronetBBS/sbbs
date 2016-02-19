var Board = function(frame, round, ugs) {

	var COLUMN_WIDTH = 14;
	var HEADER_HEIGHT = 5;

	var state = {
		column : 0,
		row : 0
	};

	var frames = {
		top : null,
		category : {
			'0' : {
				header : null,
				clues : {
					'0' : null,
					'1' : null,
					'2' : null,
					'3' : null,
					'4' : null
				}
			},
			'1' : {
				header : null,
				clues : {
					'0' : null,
					'1' : null,
					'2' : null,
					'3' : null,
					'4' : null
				}
			},
			'2' : {
				header : null,
				clues : {
					'0' : null,
					'1' : null,
					'2' : null,
					'3' : null,
					'4' : null
				}
			},
			'3' : {
				header : null,
				clues : {
					'0' : null,
					'1' : null,
					'2' : null,
					'3' : null,
					'4' : null
				}
			},
			'4' : {
				header : null,
				clues : {
					'0' : null,
					'1' : null,
					'2' : null,
					'3' : null,
					'4' : null
				}
			}
		}
	};

	function highlight(frame, bool) {
		bgAttr = (
			bool
			? BG_CYAN
			: (frame._jclueval === '' ? BG_LIGHTGRAY : BG_BLUE)
		);
		frame.attr = bgAttr|WHITE;
		frame.clear();
		frame.center(frame._jclueval);
	}

	function navigate(direction) {
		switch (direction) {
			case KEY_UP:
				if (state.row > 0) {
					highlight(
						frames.category[state.column].clues[state.row],
						false
					);
					state.row--;
					highlight(
						frames.category[state.column].clues[state.row],
						true
					);
				}
				break;
			case KEY_DOWN:
				if (state.row < 4) {
					highlight(
						frames.category[state.column].clues[state.row],
						false
					);
					state.row++;
					highlight(
						frames.category[state.column].clues[state.row],
						true
					);
				}
				break;
			case KEY_LEFT:
				if (state.column > 0) {
					highlight(
						frames.category[state.column].clues[state.row],
						false
					);
					state.column--;
					highlight(
						frames.category[state.column].clues[state.row],
						true
					);
				}
				break;
			case KEY_RIGHT:
				if (state.column < 4) {
					highlight(
						frames.category[state.column].clues[state.row],
						false
					);
					state.column++;
					highlight(
						frames.category[state.column].clues[state.row],
						true
					);
				}
				break;
			default:
				break;
		}
	}

	function drawCategory(category, index) {

		frames.category[index].header = new Frame(
			frames.top.x + (index * (COLUMN_WIDTH + 2)),
			frames.top.y,
			COLUMN_WIDTH,
			HEADER_HEIGHT,
			BG_BLUE|WHITE,
			frames.top
		);
		frames.category[index].header.word_wrap = true;
		frames.category[index].header.centerWrap(category.name);
		if (frames.category[index].header.data_height >
				frames.category[index].header.height
		) {
			frames.category[index].header.scroll(0, -1);			
		}

		category.clues.forEach(
			function (clue, cIndex) {
				frames.category[index].clues[cIndex] = new Frame(
					frames.category[index].header.x,
					(	frames.category[index].header.y + 
						frames.category[index].header.height + 1 +
						(cIndex * 2)
					),
					COLUMN_WIDTH,
					1,
					BG_BLUE|WHITE,
					frames.top
				);
				if (ugs.rounds[round.number][index].indexOf(cIndex) >= 0) {
					frames.category[index].clues[cIndex]._jclueval = '';
					highlight(frames.category[index].clues[cIndex], false);
				} else {
					var value = clue.value;
					if (value > 999) {
						value += '';
						value = value.split('');
						value.splice(1, 0, ',');
						value = value.join('');
					}
					value = '$' + value;
					frames.category[index].clues[cIndex].center(value);
					frames.category[index].clues[cIndex]._jclueval = value;
				}
			}
		);

	}

	function initDisplay() {
		frames.top = new Frame(
			frame.x + 1,
			frame.y,
			(COLUMN_WIDTH * round.length) + round.length + 2,
			HEADER_HEIGHT + (round[0].clues.length * 2),
			WHITE,
			frame
		);
		round.forEach(drawCategory);
		frames.top.open();
		highlight(frames.category['0'].clues['0'], true);
	}

	this.getcmd = function (cmd) {
		var ret;
		switch (cmd) {
			case KEY_UP:
			case KEY_DOWN:
			case KEY_LEFT:
			case KEY_RIGHT:
				navigate(cmd);
				break;
			case '\r':
				var c = frames.category[state.column].clues[state.row];
				if (c._jclueval !== '') {
					c.clear();
					c._jclueval = '';
					ret = state;
				}
				break;
			default:
				break;
		}
		return ret;
	}

	this.close = function () {
		frames.top.close();
	}

	this.cycle = function () { }

	initDisplay();

}