// TODO: Auto-pause stuff...

js.load_path_list.unshift(js.exec_dir+"dorkit/");
js.on_exit("js.load_path_list.shift()");
if (typeof(system) !== 'undefined') {
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
	js.on_exit("js.load_path_list.shift()");
}
require("screen.js", 'Screen');

// polyfill the String object with repeat method.
// Swiped from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/repeat#Browser_compatibility
if (!String.prototype.repeat) {
  String.prototype.repeat = function(count) {
    'use strict';
    if (this == null) {
      throw new TypeError('can\'t convert ' + this + ' to object');
    }
    var str = '' + this;
    count = +count;
    if (count != count) {
      count = 0;
    }
    if (count < 0) {
      throw new RangeError('repeat count must be non-negative');
    }
    if (count == Infinity) {
      throw new RangeError('repeat count must be less than infinity');
    }
    count = Math.floor(count);
    if (str.length == 0 || count == 0) {
      return '';
    }
    // Ensuring count is a 31-bit integer allows us to heavily optimize the
    // main part. But anyway, most current (August 2014) browsers can't handle
    // strings 1 << 28 chars or longer, so:
    if (str.length * count >= 1 << 28) {
      throw new RangeError('repeat count must not overflow maximum string size');
    }
    var rpt = '';
    for (;;) {
      if ((count & 1) == 1) {
        rpt += str;
      }
      count >>>= 1;
      if (count == 0) {
        break;
      }
      str += str;
    }
    return rpt;
  };
}

var dk = {
	console:{
		auto_pause:true,
		_auto_pause:{lines_since:0},
		last_pos:{x:1, y:1},
		key:{
			CTRL_A:'\x01',
			CTRL_B:'\x02',
			CTRL_C:'\x03',
			CTRL_D:'\x04',
			CTRL_E:'\x05',
			CTRL_F:'\x06',
			CTRL_G:'\x07',
			BEEP:'\x07',
			CTRL_H:'\x08',
			BACKSPACE:'\x08',
			CTRL_I:'\x09',
			TAB:'\x09',
			CTRL_J:'\x0a',
			LF:'\x0a',
			CTRL_K:'\x0b',
			CTRL_L:'\x0c',
			CLEAR:'\x0c',
			CTRL_M:'\x0d',
			RETURN:'\x0d',
			CTRL_N:'\x0e',
			CTRL_O:'\x0f',
			CTRL_P:'\x10',
			CTRL_Q:'\x11',
			CTRL_R:'\x12',
			CTRL_S:'\x13',
			PAUSE:'\x13',
			CTRL_T:'\x14',
			CTRL_U:'\x15',
			CTRL_V:'\x16',
			CTRL_W:'\x17',
			CTRL_X:'\x18',
			CTRL_Y:'\x19',
			CTRL_Z:'\x1a',
			ESCAPE:'\x1b',
			DELETE:'\x7f',
			/* End of ASCII */

			/* Start of extended characters */
			KEY_UP:'KEY_UP',
			KEY_DOWN:'KEY_DOWN',
			KEY_RIGHT:'KEY_RIGHT',
			KEY_LEFT:'KEY_LEFT',
			KEY_HOME:'KEY_HOME',
			KEY_END:'KEY_END',
			KEY_F1:'KEY_F1',
			KEY_F2:'KEY_F2',
			KEY_F3:'KEY_F3',
			KEY_F4:'KEY_F4',
			KEY_F5:'KEY_F5',
			KEY_F6:'KEY_F6',
			KEY_F7:'KEY_F7',
			KEY_F8:'KEY_F8',
			KEY_F9:'KEY_F9',
			KEY_F10:'KEY_F10',
			KEY_F11:'KEY_F11',
			KEY_F12:'KEY_F12',
			KEY_PGUP:'KEY_PGUP',
			KEY_PGDOWN:'KEY_PGDOWN',
			KEY_INS:'KEY_INS',
			KEY_DEL:'KEY_DEL',
			POSITION_REPORT:'POSITION_REPORT',
			UNKNOWN_ANSI:'UNKNOWN_ANSI'
		},

		x:1,					// Current column (1-based)
		y:1,					// Current row (1-based)
		_attr:{
			__proto__:Attribute.prototype,
			_value:7,
			_new_attr:new Attribute(7),
			get value() {
				return this._value;
			},
			set value(val) {
				if (val != this._new_attr.value) {
					this._new_attr.value = val;
					dk.console.print(this._new_attr.ansi(this));
					this._value = val;
				}
			}
		},
		ctrla_attr:function(code, attr) {
			switch(code.toUpperCase()) {
				case 'K':
					attr.fg = Attribute.BLACK;
					break;
				case 'R':
					attr.fg = Attribute.RED;
					break;
				case 'G':
					attr.fg = Attribute.GREEN;
					break;
				case 'Y':
					attr.fg = Attribute.YELLOW;
					break;
				case 'B':
					attr.fg = Attribute.BLUE;
					break;
				case 'M':
					attr.fg = Attribute.MAGENTA;
					break;
				case 'C':
					attr.fg = Attribute.CYAN;
					break;
				case 'W':
					attr.fg = Attribute.WHITE;
					break;
				case '0':
					attr.bg = Attribute.BLACK;
					break;
				case '1':
					attr.bg = Attribute.RED;
					break;
				case '2':
					attr.bg = Attribute.GREEN;
					break;
				case '3':
					attr.bg = Attribute.YELLOW;
					break;
				case '4':
					attr.bg = Attribute.BLUE;
					break;
				case '5':
					attr.bg = Attribute.MAGENTA;
					break;
				case '6':
					attr.bg = Attribute.CYAN;
					break;
				case '7':
					attr.bg = Attribute.WHITE;
					break;
				case 'H':
					attr.bright = true;
					break;
				case 'I':
					attr.blink = true;
					break;
				case 'N':
					attr.value = 7;
					break;
				case '-':
					if (attr.blink || attr.bright || attr.bg !== Attribute.BLACK)
						attr.value = 7;
					break;
				case '_':
					if (attr.blink || attr.bg !== Attribute.BLACK)
						attr.value = 7;
					break;
			}
		},
		_orig_attr:new Attribute(7),
		_next_attr:new Attribute(7),

		get attr() {
			return this._attr;
		},
		set attr(val) {
			function handle_string(str, obj) {
				var i;

				obj._next_attr.value = 7;
				for (i=0; i<val.length; i++)
					obj.ctrla_attr(str[i], obj._next_attr);
				obj.attr.value = obj._next_attr.value;
			}

			if (typeof(val)=='object') {
				if (val.constructor == String)
					handle_string(val, this);
				else
					this._attr.value = val.value;
			}
			else if(typeof(val)=='string')
				handle_string(val, this);
			else
				this._attr.value = val;
		},
		ansi:true,				// ANSI support is enabled
		charset:'cp437',		// Supported character set
		local:true,				// True if writes should go to the local screen
		remote:true,			// True if writes should go to the remote terminal
		rows:24,				// Rows in users terminal
		cols:80,				// Columns in users terminal

		keybuf:'',
		local_screen:new Screen(80, 24, 7, ' '),
		remote_screen:new Screen(80, 24, 7, ' '),
		input_queue:new Queue("dorkit_input"),

		/*
		 * Returns a string with ^A codes converted to ANSI or stripped
		 * as appropriate.
		 */
		parse_ctrla:function(txt, orig_attr) {
			var ret='';
			var i;
			var curr_attr;
			var next_attr = this._next_attr;

			if (orig_attr !== undefined) {
				curr_attr = this._orig_attr;
				curr_attr.value = orig_attr.value;
				next_attr.value = curr_attr.value;
			}
			else
				next_attr.value = 7;

			function attr_str() {
				var ansi_str;

				if (curr_attr === undefined || curr_attr.value != next_attr.value) {
					ansi_str = next_attr.ansi(curr_attr);
					if (curr_attr === undefined) {
						curr_attr = this._orig_attr;
						curr_attr.value = next_attr.value;
					}
					else
						curr_attr.value = next_attr.value;
					return ansi_str;
				}
				return '';
			}

			for (i=0; i<txt.length; i++) {
				if (txt.charCodeAt(i)==1) {
					i++;
					switch(txt[i]) {
						case '\1':
							ret += attr_str()+'\1';
							break;
						default:
							this.ctrla_attr(txt[i], next_attr);
							break;
					}
				}
				else {
					ret += attr_str() + txt.substr(i, 1);
				}
			}
			return ret + attr_str();
		},

		/*
		 * Clears the current screen to black and moves to location 1,1
		 * sets the current attribute to 7
		 */
		clear:function() {
			if (this.remote_screen !== undefined && this.auto_pause && this.remote_screen.touched) {
				this.auto_pause = false;
				this.pause();
				this.auto_pause = true;
			}
			this.attr=7;
			if (this.local)
				this.local_io.clear();
			if (this.remote)
				this.remote_io.clear();
		},

		/*
		 * Clears to end of line.
		 * Not available without ANSI (???)
		 */
		cleareol:function() {
			if (this.local)
				this.local_io.cleareol();
			if (this.remote)
				this.remote_io.cleareol();
		},

		/*
		 * Moves the cursor to the specified position.
		 * returns false on error.
		 * Not available without ANSI
		 */
		gotoxy:function(x,y) {
			if (this.local)
				this.local_io.gotoxy(x,y);
			if (this.remote)
				this.remote_io.gotoxy(x,y);
		},

		movex:function(pos) {
			if (this.local)
				this.local_io.movex(pos);
			if (this.remote)
				this.remote_io.movex(pos);
		},

		movey:function(pos) {
			if (this.local)
				this.local_io.movey(pos);
			if (this.remote)
				this.remote_io.movey(pos);
		},

		/*
		 * Returns a Graphic object representing the specified block
		 * or undefined on error (ie: invalid block specified).
		 */
		getblock:function(sx,sy,ex,ey) {
			return this.remote_screen.graphic.get(sx,sy,ex,ey);
		},

		/*
		 * Writes a string unmodified.
		 */
		print:function(string) {
			var m;

			if (this.local) {
				if (this.local_screen !== undefined) {
					this.local_screen.print(string);
					this._attr.value = this.local_screen.attr.value;
				}
				this.local_io.print(string);
			}
			if (this.remote) {
				if (this.remote_screen !== undefined) {
					if (this.remote_screen.new_lines >= this.rows && this.auto_pause) {
						this.auto_pause = false;
						this.pause();
						this.auto_pause = true;
					}
					this.remote_screen.print(string);
					this.attr.value = this.remote_screen.attr.value;
				}
				this.remote_io.print(string);
			}
		},
		centre:function(str) {
			var pos = (this.cols-str.length)/2;

			this.movex(pos-this.remote_screen.pos.x);
			this.print(str);
		},
		beep:function() {
			this.print("\x07");
		},

		/*
		 * Writes a string with a "\r\n" appended.
		 */
		println:function(line) {
			this.print(line+'\r\n');
		},

		/*
		 * Writes a string after parsing ^A codes.
		 */
		aprint:function(string) {
			this.print(this.parse_ctrla(string, this.attr));
		},

		/*
		 * Writes a string after parsing ^A codes and appends a "\r\n".
		 */
		aprintln:function(line) {
			this.println(this.parse_ctrla(line, this.attr));
		},

		/*
		 * An array of callbacks to fill the input queue with.
		 */
		input_queue_callback:[],

		/*
		 * Waits up to timeout millisections and returns true if a key
		 * is pressed before the timeout.  For ANSI sequences, returns
		 * true when the entire ANSI sequence is available.
		 */
		waitkey:function(timeout) {
			var i;
			var d;
			var end = (new Date()).valueOf() + timeout;

			if (this.keybuf.length > 0)
				return true;
			do {
				if (this.input_queue_callback.length > 0) {
					timeout = 10;
					for (i = 0; i < this.input_queue_callback.length; i++) {
						if ((d = this.input_queue_callback[i]()) !== undefined)
							this.keybuf += d;
					}
					if (this.keybuf.length)
						return true;
				}

				if (this.input_queue.poll(timeout) !== false)
					return true;
			} while ((new Date()).valueOf() < end);
			return false;
		},

		/*
		 * Returns a single *KEY*, ANSI is parsed to a single key.
		 * Returns undefined if there is no key pressed.
		 */
		getkey:function() {
			var ret;
			var m;

			if (this.keybuf.length > 0) {
				var ret = this.keybuf[0];
				this.keybuf = this.keybuf.substr(1);
				return ret;
			}
			if (!this.waitkey(0))
				return undefined;
			ret = this.input_queue.read();
			if (ret.length > 1) {
				if (ret.substr(0, 9) === 'POSITION_') {
					m = ret.match(/^POSITION_([0-9]+)_([0-9]+)/);
					if (m == null)
						return undefined;
					this.last_pos.x = parseInt(m[2], 10);
					this.last_pos.y = parseInt(m[1], 10);
					ret = 'POSITION_REPORT';
				}
				ret=ret.replace(/\x00.*$/,'');
			}
			return ret;
		},
		pause:function() {
			var attr = this.attr.value;

			this.attr='HR';
			this.aprint("[Hit a key]");
			this.attr.value = attr;
			while(!this.waitkey(10000));
			this.getkey();
			this.print("\b".repeat(11)+" ".repeat(11)+"\b".repeat(11));
			if (this.remote_screen !== undefined) {
				this.remote_screen.new_lines = 0;
				this.remote_screen.touched = [];
			}
		},

		/*
		 * Returns a single byte... ANSI is not parsed.
		 */
		getbyte:function() {
			if (this.keybuf.length > 0) {
				var ret = this.keybuf[0];
				this.keybuf = this.keybuf.substr(1);
				return ret;
			}
			if (!this.waitkey(0))
				return undefined;
			ret = this.input_queue.read();
			if (ret.length > 1) {
				ret=ret.replace(/^.*\x00/,'');
				this.keybuf = ret.substr(1);
				ret = ret[0];
			}
			return ret;
		},
		getstr_defaults:{
			timeout:undefined,	// Timeout, undefined for "wait forever"
			password:false,		// Password field (echo password_char instead of string)
			password_char:'*',	// Character to echo when entering passwords.
			upper_case:false,	// Convert to upper-case during editing
			integer:false,		// Input integer values only
			decimal:false,		// Input decimal values only
			edit:'',			// Edit this value rather than a new input
			crlf:true,			// Print CRLF after input
			exascii:false,		// Allow extended ASCII (Higher than 127)
			input_box:false,	// Draw an input "box" in a different background attr
			select:true,		// Select all when editing... first character typed if not movement will erase
			attr:undefined,		// Foreground attribute... used to draw the input box and for output... undefined means "use current"
			sel_attr:undefined,	// Selected text attribute... used for edit value when select is true.  Undefined uses inverse (swaps fg and bg, leaving bright and blink unswapped)
			len:80,			// Max length and length of input box
			max:undefined,		// Max value for decimal and integer
			min:undefined,		// Min value for decimal and integer
			hotkeys:undefined	// Hotkeys... if a char in this string is typed as the first char, returns that char immediately.
		},
		getstr:function(in_opts) {
			var i;
			var opt={};
			var str;
			var newstr;
			var key;
			var pos=0;
			var insmode=true;
			var orig_attr = new Attribute(this.attr);
			var dispstr;
			var decimal_re;
			var val;
			if (in_opts===undefined)
				in_opts={};

			function do_select_keep(obj) {
				if (opt.select) {
					opt.select = false;
					obj.movex(-pos);
					obj.print(dispstr);
					pos = str.length;
				}
			}

			function do_select_erase(obj) {
				if (opt.select) {
					opt.select = false;
					obj.movex(-pos);
					obj.print(' '.repeat(str.length));
					obj.movex(-str.length);
					dispstr = str = '';
					pos = 0;
				}
			}

			// Set up option defaults
			for (i in this.getstr_defaults) {
				if (in_opts[i] !== undefined)
					opt[i] = in_opts[i];
				else
					opt[i] = this.getstr_defaults[i];
			}
			if (opt.decimal)
				decimal_re = /^([0-9]+(\.[0-9]*)?)?$/;
			if (opt.attr === undefined)
				opt.attr=new Attribute(this.attr);
			if (opt.sel_attr === undefined) {
				opt.sel_attr=new Attribute(this.attr);
				i = opt.sel_attr.fg;
				opt.sel_attr.fg = opt.sel_attr.bg;
				opt.sel_attr.bg = i;
			}
			str = opt.edit;
			if (opt.password)
				dispstr = opt.password_char.repeat(str.length);
			else
				dispstr = str;
			this.attr.value = opt.attr.value;
			// Draw the input box...
			if (opt.input_box) {
				// TODO: Verify that it fits and do the "right" thing.
				for (i=0; i<opt.len; i++)
					this.print(' ');
				this.movex(-(opt.len));
			}
			if (opt.select)
				this.attr.value = opt.sel_attr.value;
			this.print(dispstr);
			if (opt.select)
				this.attr.value = opt.attr.value;
			pos = str.length;

			if (this.auto_pause && this.remote_screen !== undefined)
				this.remote_screen.new_lines = 0;

			while(1) {
				if (!this.waitkey(opt.timeout === undefined ? 1000 : opt.timeout)) {
					if (opt.timeout !== undefined)
						return str;
				}
				key = this.getkey();
				if (key !== undefined && opt.upper_case)
					key = key.toUpperCase();
				if (opt.hotkeys !== undefined && str.length === 0 && opt.hotkeys.indexOf(key) !== -1) {
					if (ascii(key) >= 32) {
						if (opt.password)
							this.print(opt.password_char);
						else
							this.print(key);
					}
					if (opt.crlf)
						this.println('');
					return key;
				}
				switch(key) {
					case 'KEY_HOME':
						do_select_keep(this);
						this.movex(-pos);
						pos=0;
						break;
					case 'KEY_END':
						do_select_keep(this);
						this.movex(str.length - pos);
						pos = str.length;
						break;
					case 'KEY_LEFT':
						do_select_keep(this);
						if (pos == 0)				// Already at start... ignoe TODO: Beep?
							break;
						pos--;
						this.movex(-1);
						break;
					case 'KEY_RIGHT':
						do_select_keep(this);
						if (pos >= str.length)		// Already at end... ignore TODO: Beep?
							break;
						pos++;
						this.movex(1);
						break;
					case '\x7f':
					case '\b':
						do_select_erase(this);
						if (pos == 0)				// Already at start... ignoe TODO: Beep?
							break;
						str = str.substr(0, pos - 1) + str.substr(pos);
						if (opt.password)
							dispstr = opt.password_char.repeat(str.length);
						else
							dispstr = str;
						pos--;
						this.movex(-1);
						this.print(dispstr.substr(pos)+' ');
						this.movex(-1-(str.length - pos));
						break;
					case 'KEY_DEL':
						do_select_erase(this);
						if (pos >= str.length)		// Already at end... ignore TODO: Beep?
							break;
						str = str.substr(0, pos) + str.substr(pos+1);
						if (opt.password)
							dispstr = opt.password_char.repeat(str.length);
						else
							dispstr = str;
						this.movex(-1);
						this.print(dispstr.substr(pos)+' ');
						this.movex(-1-(str.length - pos));
						break;
					case 'KEY_INS':
						do_select_keep(this);	// This is a bit weird...
						insmode = !insmode;
						break;
					case '\r':
						if (opt.integer || opt.decimal) {
							if (opt.integer)
								val = parseInt(str, 10);
							else
								val = parseFloat(str, 10);
							if (opt.min !== undefined && opt.min > val)
								break;
							if (opt.max !== undefined && opt.max < val)
								break;
						}
						if (opt.crlf)
							this.println('');
						return str;
					case undefined:
						break;
					default:
						// TODO: Better handling of numbers... leading zeros, negative values, etc.
						if (key.length > 1)			// Unhandled extended key... ignore TODO: Beep?
							break;
						if (opt.integer && (key < '0' || key > '9'))	// Invalid integer... ignore TODO: Beep?
							break;
						if ((!opt.exascii) && key.charCodeAt(0) > 127)	// Invalid EXASCII... ignore TODO: Beep?
							break;
						if (key.charCodeAt(0) < 32)	// Control char... ignore TODO: Beep?
							break;
						do_select_erase(this);
						if (str.length >= opt.len)	// String already too long... ignore TODO: Beep?
							break;
						newstr = str.substr(0, pos) + key + str.substr(insmode ? pos : pos+1);
						if (opt.decimal && newstr.search(decimal_re) === -1)
							break;
						str = newstr;
						if (opt.password)
							dispstr = opt.password_char.repeat(str.length);
						else
							dispstr = str;
						this.print(dispstr.substr(pos));
						pos++;
						this.movex(-(str.length-pos));
				}
			}
		},
	},
	connection:{
		type:undefined,
		baud:undefined,
		parity:undefined,
		node:undefined,
		dte:undefined,
		error_correcting:true,
		time:undefined,
		socket:undefined,
		telnet:false
	},
	user:{
		full_name:undefined,
		location:undefined,
		home_phone:undefined,
		work_phone:undefined,
		// Just a copy of work_phone when using door.sys
		data_phone:undefined,
		pass:undefined,
		level:undefined,
		times_on:undefined,
		last_called:undefined,
		// These need getter/setters
		seconds_remaining:undefined,
		minutes_remaining:undefined,
		conference:[],
		curr_conference:undefined,
		expires:undefined,
		number:undefined,
		default_protocol:undefined,	// Default transfer protocol... X, Y, Z, etc.
		uploads:undefined,
		upload_kb:undefined,
		downloads:undefined,
		download_kb:undefined,
		kb_downloaded_today:undefined,
		max_download_kb_per_day:undefined,
		birthdate:undefined,
		alias:undefined,
		ansi_supported:undefined,
		time_credits:undefined,
		last_new_file_scan_date:undefined,
		last_call_time:undefined,
		max_daily_files:undefined,
		downloaded_today:undefined,
		comment:undefined,
		doors_opened:undefined,
		messages_left:undefined,
		expert_mode:true
	},
	system:{
		main_dir:undefined,
		gen_dir:undefined,
		sysop_name:undefined,
		default_attr:new Attribute(7),
		mode:(typeof(bbs) !== 'undefined'
				&& typeof(server) !== 'undefined'
				&& typeof(client) !== 'undefined'
				&& typeof(user) !== 'undefined'
				&& typeof(console) !== 'undefined') ? 'sbbs'
				: (typeof(jsexec_revision) !== 'undefined' ? 'jsexec'
					: (typeof(jsdoor_revision) !== 'undefined' ? 'jsdoor' : undefined))
	},
	misc:{
		event_time:undefined,
		record_locking:undefined
	},

	detect_ansi:function() {
		var start = Date.now();
		this.console.remote_io.print("\x1b[s" +	// Save cursor position.
						"\x1b[255B" +	// Locate as far down as possible
						"\x1b[255C" +	// Locate as far right as possible
						"\b "+			// Print something (some terminals apparently need this)
						"\x1b[6n" +		// Get cursor position
						"\x1b[u"		// Restore cursor position
		);
		while(Date.now() - start < 500) {
			if(this.console.waitkey(500)) {
				if (this.console.getkey() == this.console.key.POSITION_REPORT) {
					// TODO: Should we trust the drop file on number of rows?
					if (this.console.cols != this.console.last_pos.x || this.console.rows != this.console.last_pos.y) {
						this.console.remote_screen = new Screen(this.console.last_pos.x, this.console.last_pos.y, 7, ' ');
						this.console.local_screen = new Screen(this.console.last_pos.x, this.console.last_pos.y, 7, ' ');
					}
					this.console.cols = this.console.last_pos.x;
					this.console.rows = this.console.last_pos.y;
					this.console.ansi = true;
					return true;
				}
			}
		}
		return false;
	},

	parse_dropfile:function(path) {
		var f = new File(path);
		var df;
		var rows;

		if (!f.open("r"))
			return false;

		df = f.readAll();
		f.close();
		if (df.length != 52)
			return false;

		this.connection.type = df[0];
		this.connection.baud = parseInt(df[1], 10);
		this.connection.parity = parseInt(df[2], 10);
		this.connection.node = parseInt(df[3], 10);
		this.connection.dte = parseInt(df[4], 10);
		if (df[5].toUpperCase() == 'N')
			this.local = false;
		// TODO: Some bools ignored here...
		this.user.full_name = df[9];
		this.user.location = df[10];
		this.user.home_phone = df[11];
		this.user.work_phone = df[12];
		this.user.data_phone = df[12];
		this.user.pass = df[13];
		this.user.level = parseInt(df[14], 10);
		this.user.times_on = parseInt(df[15], 10);
		// TODO: Parse a date out of this.
		this.user.last_called = df[16];
		this.user.seconds_remaining_from = file_date(path);
		this.user.seconds_remaining = parseInt(df[17], 10);
		this.user.minutes_remaining = parseInt(df[18], 10);
		switch(df[19].toUpperCase()) {
			case 'GR':
				this.ansi = true;
				this.codepage = 'cp437';
				break;
			case 'NG':
				this.ansi = false;
				this.codepage = 'cp437';
				break;
			default:	// ie: '7E'
				this.ansi = false;
				this.codepage = '7-bit';
				break;
		}
		rows = parseInt(df[20], 10);
		if (rows != this.console.rows) {
			this.console.remote_screen = new Screen(this.console.cols, rows, 7, ' ');
			this.console.local_screen = new Screen(this.console.cols, rows, 7, ' ');
		}
		this.rows = rows;
		this.user.expert_mode = (df[21].toUpperCase === 'Y') ? true : false;
		this.user.conference = df[22].split(/\s*,\s*/);
		this.user.curr_conference = parseInt(df[23]);
		// TODO: Parse a date out of this.
		this.user.expires = df[24];
		this.user.number = parseInt(df[25], 10);
		this.user.default_protocol = df[26];
		this.user.uploads = parseInt(df[27], 10);
		this.user.downloads = parseInt(df[28], 10);
		this.user.kb_downloaded_today = parseInt(df[29], 10);
		this.user.max_download_kb_per_day = parseInt(df[30], 10);
		// TODO: Parse a date out of this.
		this.user.birthdate = df[31];
		this.system.main_dir = df[32];
		this.system.gen_dir = df[33];
		this.system.sysop_name = df[34];
		this.user.alias = df[35];
		// TODO: Parse a timestamp thingie
		this.misc.event_time = df[36];
		this.connection.error_correcting = (df[37].toUpperCase === 'N') ? false : true;
		if (this.ansi == true)
			this.user.ansi_supported = true;
		else
			this.user.ansi_supported = (df[38].toUpperCase === 'Y') ? true : false;
		this.misc.record_locking = (df[39].toUpperCase === 'N') ? false : true;
		this.system.default_attr.value = parseInt(df[40], 10);
		this.user.time_credits = parseInt(df[41], 10);
		// TODO: Parse a date out of this.
		this.user.last_new_file_scan_date = df[42];
		// TODO: Parse a timestamp thingie
		this.connection.time = df[43];
		// TODO: Parse a timestamp thingie
		this.user.last_call_time = df[44];
		this.user.max_daily_files = parseInt(df[45], 10);
		this.user.downloaded_today = parseInt(df[46], 10);
		this.user.upload_kb = parseInt(df[47], 10);
		this.user.download_kb = parseInt(df[48], 10);
		this.user.comment = df[49];
		this.user.doors_opened = parseInt(df[50], 10);
		this.user.messages_left = parseInt(df[50], 10);
	},
	parse_cmdline:function(argc, argv) {
		var i;

		for (i=0; i<argc; i++) {
			switch(argv[i]) {
				case '-t':
				case '-telnet':
					this.connection.telnet = true;
					argv.splice(i, 1);
					argc--;
					i--;
					break;
				case '-s':
				case '-socket':
					if (i+1 < argc) {
						this.connection.socket = argv[i+1];
						argv.splice(i, 2);
						argc-=2;
						i--;
					}
					break;
				case '-l':
				case '-local':
					this.console.local = true;
					this.console.remote = false;
					argv.splice(i, 1);
					argc--;
					i--;
					break;
				case '-d':
				case '-dropfile':
					if (i+1 < argc) {
						this.parse_dropfile(argv[i+1]);
						argv.splice(i, 2);
						argc-=2;
						i--;
					}
					break;
			}
		}
		if (this.connection.telnet === undefined)
			this.connection.telnet = false;
	}
};
dk.console.center = dk.console.centre;

load("local_console.js");
dk.parse_cmdline(argc, argv);
if (dk.connection.socket !== undefined)
	dk.system.mode = 'socket';

log("Mode: "+dk.system.mode);
switch(dk.system.mode) {
	case 'sbbs':
		load("sbbs_console.js");
		break;
	case 'jsexec':
		load("jsexec_console.js");
		break;
	case 'jsdoor':
		load("jsexec_console.js");
		break;
	case 'socket':
		load("socket_console.js", dk.connection.socket, dk.connection.telnet);
		break;
}

// Fun stuff from sbbsdef.js...
log.EMERG       =0;			/* system is unusable                       */   
log.ALERT       =1;			/* action must be taken immediately         */   
log.CRIT        =2;			/* critical conditions                      */   
log.CRITICAL    =log.CRIT;	/* critical conditions                      */   
log.ERR         =3;			/* error conditions                         */   
log.ERROR       =log.ERR;	/* error conditions                         */   
log.WARNING     =4;			/* warning conditions                       */   
log.WARN        =log.WARNING;/* warning conditions                       */   
log.NOTICE      =5;			/* normal but significant condition         */   
log.INFO        =6;			/* informational                            */   
log.DEBUG       =7;			/* debug-level messages                     */   

directory.MARK		=(1<<1);	/* Append a slash to each name.  */
directory.NOSORT	=(1<<2);	/* Don't sort the names.  */
directory.APPEND	=(1<<5);	/* Append to results of a previous call.  */
directory.NOESCAPE  =(1<<6);	/* Backslashes don't quote metacharacters.  */
directory.PERIOD    =(1<<7); 	/* Leading `.' can be matched by metachars.  */
directory.ONLYDIR   =(1<<13);	/* Match only directories.  */


