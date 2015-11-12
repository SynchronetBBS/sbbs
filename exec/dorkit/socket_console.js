/*
 * Clears the current screen to black and moves to location 1,1
 */

dk.console.remote_io = {
	sock:new Socket(true, dk.connection.socket),
	clear:function() {
		this.print("\x0c");
	},

	/*
	 * Clears to end of line.
	 * Not available witout ANSI (???)
	 */
	cleareol:function() {
		if(dk.console.ansi)
			this.print('\x1b[K');
	},

	/*
	 * Moves the cursor to the specified position.
	 * returns false on error.
	 * Not available without ANSI
	 */
	gotoxy:function(x,y) {
		if(dk.console.ansi)
			this.print(format("\x1b[%u;%uH", y, x));
	},

	/*
	 * Writes a string unmodified.
	 */
	print:function(string) {
		this.sock.write(string.replace(/\xff/g, "\xff\xff"));
	},
};
if (!dk.console.remote_io.sock.is_connected)
	throw("Unable to create socket object");

var input_queue = load(true, "socket_input.js", argv[0], argv[1]);
js.on_exit("input_queue.write(''); input_queue.poll(0x7fffffff);");
