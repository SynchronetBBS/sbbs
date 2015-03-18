// The Help object loads the help screen into the terminal.
var Help = function() {

	var helpFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		BG_BLACK|WHITE,
		frame
	);

	var helpSubFrame = new Frame(
		helpFrame.x + 1,
		helpFrame.y + 1,
		helpFrame.width - 2,
		helpFrame.height - 2,
		BG_BLACK|WHITE,
		helpFrame
	);

	helpFrame.open();
	helpFrame.drawBorder([CYAN, LIGHTCYAN, WHITE]);
	helpFrame.home();
	helpFrame.center(ascii(180) + "Lemons - Help" + ascii(195));
	helpFrame.end();
	helpFrame.center(ascii(180) + "Press any key to continue" + ascii(195));
	helpSubFrame.load(js.exec_dir + "help.bin", 76, 22);

	this.close = function() {
		helpFrame.delete();
	}

}