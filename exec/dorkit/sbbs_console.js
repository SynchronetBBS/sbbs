load("sbbsdefs.js");
js.global.dk_old_ctrlkey_passthru = console.ctrlkey_passthru;
js.global.dk_old_pauseoff = bbs.sys_status & SS_PAUSEOFF;
js.on_exit("console.ctrlkey_passthru=js.global.dk_old_ctrlkey_passthru;bbs.sys_status=(bbs.sys_status &~ SS_PAUSEOFF)|js.global.dk_old_pauseoff");
console.ctrlkey_passthru=0x7fffffff;	// Disable all parsing.

/*
 * Clears the current screen to black and moves to location 1,1
 */

dk.console.remote_io = {
	clear:function() {
		console.clear();
	},

	/*
	 * Clears to end of line.
	 * Not available witout ANSI (???)
	 */
	cleareol:function() {
		console.cleartoeol();
	},

	/*
	 * Moves the cursor to the specified position.
	 * returns false on error.
	 * Not available without ANSI
	 */
	gotoxy:function(x,y) {
		console.gotoxy(x+1,y+1);
	},

	movex(pos) {
		if (pos > 0)
			console.right(pos);
		if (pos < 0)
			console.left(0-pos);
	},

	movey(pos) {
		if (pos > 0)
			console.down(pos);
		if (pos < 0)
			console.up(0-pos);
	},

	/*
	 * Writes a string unmodified.
	 */
	print:function(string) {
		console.write(string);
	},
};

var input_queue = load(true, "sbbs_input.js");
js.on_exit("input_queue.write(''); input_queue.poll(0x7fffffff);");
