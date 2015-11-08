js.global.dk_old_ctrlkey_passthru = console.ctrlkey_passthru;
js.on_exit("console.ctrlkey_passthru=js.global.dk_old_ctrlkey_passthru");
console.ctrlkey_passthru=0x7fffffff;	// Disable all parsing.

/*
 * Clears the current screen to black and moves to location 1,1
 * sets the current attribute to 7
 */

dk.console.remote_io = {
	clear:function() {
		console.clear(7);
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
		console.gotoxy(x,y);
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
