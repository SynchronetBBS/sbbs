/*
 * Handles ANSI, but does not provide the print method.
 * Shared by jsut about everything except sbbs and local.
 */

dk.console.remote_io = {
	clear:function() {
		this.print("\x1b[2J\x1b[1;1H");
	},

	/*
	 * Clears to end of line.
	 * Not available witout ANSI (???)
	 */
	cleareol:function() {
		if(dk.console.ansi)
			this.print('\x1b[K');
	},

	movex:function(pos) {
		if (pos == 1)
			return this.print("\x1b[C");
		if (pos > 1)
			return this.print("\x1b["+pos+"C");
		if (pos == -1)
			return this.print("\x1b[D");
		if (pos < -1)
			return this.print("\x1b["+(0-pos)+"D");
	},

	movey:function(pos) {
		if (pos == 1)
			return this.print("\x1b[B");
		if (pos > 1)
			return this.print("\x1b["+pos+"B");
		if (pos == -1)
			return this.print("\x1b[A");
		if (pos < -1)
			return this.print("\x1b["+(0-pos)+"A");
	},

	/*
	 * Moves the cursor to the specified position.
	 * returns false on error.
	 * Not available without ANSI
	 */
	gotoxy:function(x,y) {
		if(dk.console.ansi)
			this.print(format("\x1b[%u;%uH", y+1, x+1));
	},

};
