/*
 * Implements the local console using conio
 */

require('graphic.js', 'Graphic');
if (js.global.conio !== undefined) {
	conio.init();
	conio.clrscr();
	conio.setcursortype(2);
	dk.console.input_queue_callback.push(function() {
		'use strict';
		if (conio.kbhit) {
			return ascii(conio.getch());
		}
	});
}
var dk_local_console_loaded = true;

dk.console.local_io = {
	screen:new Screen(dk.cols, dk.height, 7, ' ', true),

	update:function() {
		'use strict';
		if (this.screen.touched.length > 0 || this.screen.full) {
			var i;

			if (this.screen.full) {
				conio.puttext(1, 1, this.screen.graphic.width, this.screen.graphic.height, this.screen.graphic.puttext);
			}
			this.screen.full = false;
			for (i = 0; i < this.screen.touched.length; i += 1) {
				conio.puttext(this.screen.touched[i].sx+1, this.screen.touched[i].sy+1, this.screen.touched[i].ex+1, this.screen.touched[i].ey+1, this.screen.touched[i].b);
			}
			this.screen.touched = [];
		}
		conio.gotoxy(this.screen.pos.x+1, this.screen.pos.y+1);
		conio.textattr = this.screen.attr.value;
	},
	clear:function() {
		'use strict';
		if (js.global.conio !== undefined) {
			this.print("\x1b[2J\x1b[1;1H");
			this.update();
		}
	},
	cleareol:function() {
		'use strict';
		if (js.global.conio !== undefined) {
			this.print('\x1b[K');
			this.update();
		}
	},
	gotoxy:function(x,y) {
		'use strict';
		if (js.global.conio !== undefined) {
			this.print(format("\x1b[%u;%uH", y+1, x+1));
			this.update();
		}
	},
	movex:function(pos) {
		'use strict';
		if (js.global.conio !== undefined) {
			if (pos === 1) {
				return this.print("\x1b[C");
			}
			if (pos > 1) {
				return this.print("\x1b["+pos+"C");
			}
			if (pos === -1) {
				return this.print("\x1b[D");
			}
			if (pos < -1) {
				return this.print("\x1b["+(0-pos)+"D");
			}
			this.update();
		}
	},
	movey:function(pos) {
		'use strict';
		if (js.global.conio !== undefined) {
			if (pos === 1) {
				return this.print("\x1b[B");
			}
			if (pos > 1) {
				return this.print("\x1b["+pos+"B");
			}
			if (pos === -1) {
				return this.print("\x1b[A");
			}
			if (pos < -1) {
				return this.print("\x1b["+(0-pos)+"A");
			}
			this.update();
		}
	},
	print:function(string) {
		'use strict';
		if (js.global.conio !== undefined) {
			this.screen.print(string);
			this.update();
		}
	}
};
dk.console.local_io.screen.touched = [];

// Get stuff that would come from the dropfile if there was one.
// From the bbs object.
if (dk.connection.node === undefined)
	dk.connection.node = 0;
if (dk.connection.time === undefined)
	dk.connection.time = strftime("%H:%M", time());
if (dk.user.seconds_remaining_from === undefined)
	dk.user.seconds_remaining_from = time();
if (dk.user.seconds_remaining === undefined)
	dk.user.seconds_remaining = 43200;	// 12 hours should be enough for anyone!
if (dk.user.minutes_remaining === undefined)
	dk.user.minutes_remaining = 720;

	// From the client object...
if (dk.connection.type === undefined)
	dk.connection.type = 'LOCAL';

	// From the console object
if (dk.user.ansi_supported === undefined)
	dk.user.ansi_supported = true;
if (dk.console.rows === undefined)
	dk.console.rows = conio.screenheight;
if (dk.console.cols === undefined)
	dk.console.cols = conio.screenwidth;
delete dk.console.local_screen;

	// From the user object...
if (dk.user.alias === undefined)
	dk.user.alias = 'Local User';
if (dk.user.full_name === undefined)
	dk.user.full_name = 'Local User';
if (dk.user.location === undefined)
	dk.user.location = 'Local';
if (dk.user.number === undefined)
	dk.user.number = 0;
if (dk.user.alias === undefined)
	dk.user.alias = 'Sysop';
