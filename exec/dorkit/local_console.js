/*
 * Implements the local console using conio
 */

require('graphic.js', 'Graphic');
if (js.global.conio !== undefined) {
	conio.init();
	conio.setcursortype(2);
	dk.console.input_queue_callback.push(function() {
		if (conio.kbhit)
			return ascii(conio.getch());
	});
}

dk.console.local_io = {
	screen:new Screen(dk.cols, dk.height, 7, ' ', true),

	update:function() {
		if (this.screen.touched) {
			var b = [];

			conio.puttext(1, 1, this.screen.graphic.width, this.screen.graphic.height, this.screen.graphic.puttext);
			conio.gotoxy(this.screen.pos.x+1, this.screen.pos.y+1);
			conio.textattr = this.screen.attr.value;
			this.screen.touched = false;
		}
	},
	clear:function() {
		if (js.global.conio !== undefined) {
			this.print("\x1b[2J\x1b[1;1H");
			this.update();
		}
	},
	cleareol:function() {
		if (js.global.conio !== undefined) {
			this.print('\x1b[K');
			this.update();
		}
	},
	gotoxy:function(x,y) {
		if (js.global.conio !== undefined) {
			this.print(format("\x1b[%u;%uH", y+1, x+1));
			this.update();
		}
	},
	movex:function(pos) {
		if (js.global.conio !== undefined) {
			if (pos == 1)
				return this.print("\x1b[C");
			if (pos > 1)
				return this.print("\x1b["+pos+"C");
			if (pos == -1)
				return this.print("\x1b[D");
			if (pos < -1)
				return this.print("\x1b["+(0-pos)+"D");
			this.update();
		}
	},
	movey:function(pos) {
		if (js.global.conio !== undefined) {
			if (pos == 1)
				return this.print("\x1b[B");
			if (pos > 1)
				return this.print("\x1b["+pos+"B");
			if (pos == -1)
				return this.print("\x1b[A");
			if (pos < -1)
				return this.print("\x1b["+(0-pos)+"A");
			this.update();
		}
	},
	print:function(string) {
		if (js.global.conio !== undefined) {
			this.screen.print(string);
			this.update();
		}
	},
};


// TODO
if (false && js.global.conio !== undefined) {
	// Get stuff that would come from the dropfile if there was one.
	// From the bbs object.
	dk.connection.node = 0;
	dk.connection.time = strftime("%H:%M", time());
	dk.user.seconds_remaining_from = time();
	dk.user.seconds_remaining = 43200;	// 12 hours should be enough for anyone!
	dk.user.minutes_remaining = 720;

	// From the client object...
	dk.connection.type = 'LOCAL';

	// From the console object
	dk.user.ansi_supported = true;
	dk.console.rows = conio.screenheight;
	dk.console.cols = conio.screenwidth;
	dk.console.local_screen = undefined;

	// From the user object...
	dk.user.alias = 'Local User';
	dk.user.full_name = 'Local User';
	dk.user.location = 'Local';
	dk.user.number = 0;

	dk.user.alias = 'Sysop';
}
