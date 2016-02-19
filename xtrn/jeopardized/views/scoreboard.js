var Scoreboard = function (frame) {

	var lFrame,
		layout,
		tabTodayMoney,
		tabTotalMoney;

	var header = format(
		"%-20s%-25s%19s   Answers\r\n",
		"Alias", "System", "Winnings"
	);

	function formatLine(record) {
		var us = base64_decode(record.id).split('@');
		return format(
			"\1h\1c%-20s\1n\1c%-25s\1h%s%19s\1g%6s\1w:\1r%-6s\r\n",
			(	us[0].length > 19
				? (us[0].substr(0, 16) + '\1k...')
				: us[0].substr(0, 19)
			),
			(	us[1].length > 24
				? (us[1].substr(0, 21) + '\1k...')
				: us[1].substr(0, 24)
			),
			(record.winnings >= 0 ? '\1g' : '\1r'),
			'$' + record.winnings,
			record.answers.correct,
			record.answers.incorrect
		);
	}

	function initDisplay() {
		lFrame = new Frame(1, 1, 1, 1, WHITE, frame);
		lFrame.nest();
		lFrame.height = lFrame.height - 1;

		var hFrame = new Frame(
			lFrame.x,
			lFrame.y + lFrame.height,
			lFrame.width,
			1,
			BG_BLUE|WHITE,
			lFrame
		);
		hFrame.center(
			'\1cLEFT\1w/\1cRIGHT \1wto switch tabs : ' +
			'\1cUP\1w/\1cDOWN \1wto scroll : ' +
			'\1cQ \1wto quit'
		);

		layout = new Layout(lFrame);
		layout.colors.view_fg = WHITE;
		layout.colors.border_fg = LIGHTCYAN;
		layout.colors.title_fg = WHITE;
		layout.colors.tab_fg = WHITE;
		layout.colors.tab_bg = BG_BLUE;
		layout.colors.inactive_tab_bg = BG_BLUE;
		layout.colors.inactive_tab_fg = LIGHTGRAY;

		var view = layout.addView(
			"Scoreboard",
			lFrame.x,
			lFrame.y,
			lFrame.width,
			lFrame.height
		);

		tabTodayMoney = view.addTab('Today\'s Game', 'FRAME');
		tabTotalMoney = view.addTab('Total', 'FRAME');
		tabTodayMoney._scrollbar = new ScrollBar(tabTodayMoney.frame);
		tabTotalMoney._scrollbar = new ScrollBar(tabTotalMoney.frame);
		tabTodayMoney.frame.putmsg(header);
		tabTotalMoney.frame.putmsg(header);

		lFrame.open();
		layout.open();

	}

	function initData() {
		var data = database.getRankings();
		data.today.money.forEach(
			function (d) {
				tabTodayMoney.frame.putmsg(formatLine(data.today.data[d]));
			}
		);
		data.total.money.forEach(
			function (d) {
				tabTotalMoney.frame.putmsg(formatLine(data.total.data[d]));
			}
		);
	}

	function init() {
		initDisplay();
		initData();
	}

	this.getcmd = function (cmd) {
		var ret = true;
		switch (cmd.toUpperCase()) {
			case '\x09':
			case KEY_LEFT:
			case KEY_RIGHT:
			case KEY_UP:
			case KEY_DOWN:
				layout.getcmd(cmd);
				break;
			case 'Q':
				ret = false;
				break;
			default:
				break;
		}
		return ret;
	}

	this.cycle = function () {
		layout.current.current._scrollbar.cycle();
		layout.cycle();
	}

	this.close = function () {
		layout.close();
	}

	init();

}