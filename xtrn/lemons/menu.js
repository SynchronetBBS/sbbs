// The Menu object displays the Lemons splash screen and a small Tree menu.
var Menu = function() {

	var frames = {};

	frames.splash = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);

	frames.treeFrame = new Frame(
		frames.splash.x + 50,
		frames.splash.y + 10,
		20,
		6,
		BG_BLACK|WHITE,
		frames.splash
	);

	frames.treeSubFrame = new Frame(
		frames.treeFrame.x + 1,
		frames.treeFrame.y + 1,
		frames.treeFrame.width - 2,
		frames.treeFrame.height - 2,
		BG_BLACK|WHITE,
		frames.treeFrame
	);

	frames.splash.open();
	frames.splash.load(js.exec_dir + "lemons.bin", 80, 24)
	frames.splash.top();
	frames.treeFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	frames.treeFrame.open();

	var changeState = function(s) {
		state = s;
	}

	var tree = new Tree(frames.treeSubFrame);
	tree.colors.fg = LIGHTGRAY;
	tree.colors.bg = BG_BLACK;
	tree.colors.lfg = WHITE;
	tree.colors.lbg = BG_BLUE;
	tree.colors.kfg = LIGHTCYAN;
	tree.addItem("|Play", changeState, STATE_PLAY);
	tree.addItem("|Help", changeState, STATE_HELP);
	tree.addItem("|Scores", changeState, STATE_SCORES);
	tree.addItem("|Quit", changeState, STATE_EXIT);
	tree.open();

	this.getcmd = function(userInput) {
		tree.getcmd(userInput);
	}

	this.close = function() {
		tree.close();
		frames.treeFrame.delete();
		frames.splash.delete();
	}

}