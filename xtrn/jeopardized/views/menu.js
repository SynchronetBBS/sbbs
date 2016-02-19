var Menu = function (frame) {

	var self = this;
	var state = STATE_MENU_TREE;

	var frames = {
		top : null,
		menu : null,
		tree : null
	};

	var tree,
		messages,
		title;

	function initDisplay() {
		frames.top = new Frame(1, 1, frame.width, frame.height, WHITE, frame);
		frames.top.checkbounds = false;
		frames.top.centralize(frame);
		frames.menu = new Frame(
			frames.top.x,
			frames.top.y + 9,
			12,
			15,
			WHITE,
			frames.top
		);
		frames.tree = new Frame(1, 1, 1, 1, WHITE, frames.menu);
		frames.tree.nest(1, 2);
		title = {
			x : frames.menu.width - 6,
			y : 1,
			attr : WHITE,
			text : 'Menu'
		};
		frames.menu.drawBorder([LIGHTBLUE,CYAN,LIGHTCYAN,WHITE], title);
		frames.top.open();
		frames.top.load(js.exec_dir + 'views/jeopardized.bin', 80, 9);
	}

	function initTree() {
		tree = new Tree(frames.tree);
		tree.colors.fg = WHITE;
		tree.colors.bg = BG_BLACK;
		tree.colors.lfg = WHITE;
		tree.colors.lbg = BG_BLUE;
		tree.colors.kfg = LIGHTCYAN;
		tree.addItem('|Play', STATE_GAME);
		tree.addItem('|Scores', STATE_SCORE);
		tree.addItem('|News', STATE_NEWS);
		tree.addItem('|Help', STATE_HELP);
		tree.addItem('|Credits', STATE_CREDIT);
		tree.addItem('|Quit', STATE_QUIT);
		tree.open();
	}

	function init() {
		initDisplay();
		initTree();
		messages = new Messages(
			frames.top,
			frames.top.x + 13,
			frames.top.y + 9,
			frames.top.width - 13,
			15,
			true
		);
	}

	this.cycle = function () {
		tree.cycle();
		messages.cycle();
	}

	this.getcmd = function (cmd) {
		var ret = false;
		switch (state) {
			case STATE_MENU_TREE:
				if (cmd === '\x09' || cmd === KEY_RIGHT) {
					state = STATE_MENU_MESSAGES;
					title.attr = LIGHTGRAY;
					frames.menu.drawBorder(LIGHTGRAY, title);
					messages.focus = true;
				} else {
					ret = tree.getcmd(cmd);
				}
				break;
			case STATE_MENU_MESSAGES:
				if (cmd === '\x09' || cmd === KEY_LEFT) {
					state = STATE_MENU_TREE;
					title.attr = WHITE;
					frames.menu.drawBorder(
						[LIGHTBLUE,CYAN,LIGHTCYAN,WHITE],
						title
					);
					messages.focus = false;
				} else {
					messages.getcmd(cmd);
				}
				break;
			default:
				break;
		}
		return ret;
	}

	this.close = function () {
		tree.close();
		messages.close();
		frames.top.close();
	}

	init();

}