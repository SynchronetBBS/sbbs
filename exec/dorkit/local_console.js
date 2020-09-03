/*
 * Implements the local console using conio
 */

require('graphic.js', 'Graphic');
if (js.global.conio !== undefined && dk.console.local) {
	conio.init();
	conio.clrscr();
	conio.setcursortype(2);
	dk.console.input_queue_callback.push(function() {
		'use strict';

		var ch;
		if (conio.kbhit) {
			ch = conio.getch();
			if (ch === 0) {
				if (conio.kbhit) {
					ch = conio.getch();
					switch(ch) {
						case 0x47:
							return dk.console.key.KEY_HOME;
						case 72:
							return dk.console.key.KEY_UP;
						case 0x4f:
							return dk.console.key.KEY_END;
						case 80:
							return dk.console.key.KEY_DOWN;
						case 0x52:
							return dk.console.key.KEY_INS;
						case 0x53:
							return dk.console.key.KEY_DEL;
						case 0x4b:
							return dk.console.key.KEY_LEFT;
						case 0x4d:
							return dk.console.key.KEY_RIGHT;
						case 0x49:
							return dk.console.key.KEY_PGUP;
						case 0x51:
							return dk.console.key.KEY_PGDOWN;
						default:
							if (ch >= 0x3a && ch <= 0x44)
								return dk.console.key['KEY_F'+(ch - 0x39)];
							if (ch >= 0x7a && ch <= 0x7b)
								return dk.console.key['KEY_F'+(ch - 0x6f)];
					}
				}
				return undefined;
			}

			return ascii(ch);
		}
		return undefined;
	});
}
var dk_local_console_loaded = true;

dk.console.local_io = {
	screen:new Screen(dk.console.cols, dk.console.rows, 7, ' ', true),

	update:function() {
		'use strict';
		if (js.global.conio !== undefined && dk.console.local) {
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
		}
		else {
			this.screen.full = false;
			this.screen.touched = [];
		}
	},
	clear:function() {
		'use strict';
		this.print("\x1b[2J\x1b[1;1H");
		this.update();
	},
	cleareol:function() {
		'use strict';
		this.print('\x1b[K');
		this.update();
	},
	gotoxy:function(x,y) {
		'use strict';
		this.print(format("\x1b[%u;%uH", y+1, x+1));
		this.update();
	},
	movex:function(pos) {
		'use strict';
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
	},
	movey:function(pos) {
		'use strict';
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
	},
	print:function(string) {
		'use strict';
		this.screen.print(string);
		this.update();
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
