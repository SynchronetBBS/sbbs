var News = function (frame) {

	var frames = {
		border : null,
		news : null
	};

	var scrollbar, news;

	function init() {
		frames.border = new Frame(1, 1, 1, 1, WHITE, frame);
		frames.border.nest();
		frames.border.drawBorder([LIGHTBLUE,CYAN,LIGHTCYAN,WHITE]);
		frames.news = new Frame(1, 1, 1, 1, WHITE, frames.border);
		frames.news.nest(1, 1);
		scrollbar = new ScrollBar(frames.news);
		frames.news.putmsg(
			'\1h\1cJeopardized! \1wNews:                             ' +
			'\1c(\1wUP\1c/\1wDOWN \1nto scroll\1h\1c, ' +
			'\1wQ\1n to quit\1h\1c)\r\n\r\n'
		);
		news = database.getNews();
		news.reverse();
		var line = '\1h\1k';
		for (var n = 0; n < frames.news.width; n++) {
			line += ascii(196);
		}
		news.forEach(
			function (n) {
				frames.news.putmsg(n);
				frames.news.crlf();
				frames.news.putmsg(line);
				frames.news.crlf();
			}
		);
		frames.border.open();
		while (frames.news.up()) { mswait(1); }
	}

	this.cycle = function () {
		scrollbar.cycle();
	}

	this.getcmd = function (cmd) {
		var ret = true;
		switch (cmd) {
			case KEY_UP:
				frames.news.scroll(0, -1);
				break;
			case KEY_DOWN:
				if (frames.news.data_height > frames.news.height &&
					frames.news.offset.y + frames.news.height <
						frames.news.data_height
				) {
					frames.news.scroll(0, 1);
				}
				break;
			default:
				if (cmd !== '') ret = false;
				break;
		}
		return ret;
	}

	this.close = function () {
		frames.border.close();
	}

	init();

}