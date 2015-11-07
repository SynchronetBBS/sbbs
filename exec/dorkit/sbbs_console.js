js.global.dk_old_ctrlkey_passthru = console.ctrlkey_passthru;
js.on_exit("console.ctrlkey_passthru=js.global.dk_old_ctrlkey_passthru");
console.ctrlkey_passthru=0xffffffff;	// Disable all parsing.

/*
 * Clears the current screen to black and moves to location 1,1
 * sets the current attribute to 7
 */
dk.remote.clear=function() {
	console.clear(7);
}

/*
 * Clears to end of line.
 * Not available witout ANSI (???)
 */
dk.remote.cleareol=function() {
	console.cleartoeol();
}

/*
 * Moves the cursor to the specified position.
 * returns false on error.
 * Not available without ANSI
 */
dk.remote.gotoxy=function(x,y) {
	console.gotoxy(x,y);
}

/*
 * Writes a string with a "\r\n" appended.
 */
dk.remote.println=function(line) {
	console.writeln(line);
}

/*
 * Writes a string unmodified.
 */
dk.remote.print=function(string) {
	console.write(string);
}

/*
 * Writes a string after parsing ^A codes and appends a "\r\n".
 */
dk.remote.aprintln=function(line) {
	console.putmsg(line+"\r\n");
}

/*
 * Writes a string after parsing ^A codes.
 */
dk.remote.aprint=function(string) {
	// Can't use putmsg() due to pipe-code etc support.
	console.putmsg(line, P_NOATCODES|P_NOPAUSE);
}

/*
 * Waits up to timeout millisections and returns true if a key
 * is pressed before the timeout.  For ANSI sequences, returns
 * true when the entire ANSI sequence is available.
 */
dk.remote.waitkey=function(timeout) {
}

/*
 * Returns a single *KEY*, ANSI is parsed to a single key.
 * Returns undefined if there is no key pressed.
 */
dk.remote.getkey=function() {
}

/*
 * Returns a single byte... ANSI is not parsed.
 */
dk.remote.getbyte=function() {
}
