var BinScroller = function (frame, bin, w, h) {

	var frames = {
		border : null,
		bin : null
	};

	var scrollbar;

	function init() {
		frames.border = new Frame(1, 1, 1, 1, WHITE, frame);
		frames.border.nest();
		frames.border.drawBorder([LIGHTBLUE,CYAN,LIGHTCYAN,WHITE]);
		frames.bin = new Frame(1, 1, 1, 1, WHITE, frames.border);
		frames.bin.nest(1, 1);
		frames.bin.load(bin, w, h);
		scrollbar = new ScrollBar(frames.bin);
		frames.border.open();
	}

	this.cycle = function () {
		scrollbar.cycle();
	}

	this.getcmd = function (cmd) {
		var ret = true;
		switch (cmd) {
			case KEY_UP:
				frames.bin.scroll(0, -1);
				break;
			case KEY_DOWN:
				if (frames.bin.data_height > frames.bin.height &&
					frames.bin.offset.y + frames.bin.height <
						frames.bin.data_height
				) {
					frames.bin.scroll(0, 1);
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