js.load_path_list.unshift(js.exec_dir+"/dorkit/");
if (js.global.system !== undefined)
	js.load_path_list.unshift(system.exec_dir+"/dorkit/");
log("Load path: "+js.load_path_list.join(", "));
load("attribute.js");
load("graphic.js");

var dk = {
	console:{
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
			POSITION_REPORT:'POSITION_REPORT'
		},

		x:1,					// Current column (1-based)
		y:1,					// Current row (1-based)
		attr:new Attribute(7),	// Current attribute
		ansi:true,				// ANSI support is enabled
		charset:'cp437',		// Supported character set
		local:true,				// True if writes should go to the local screen
		remote:true,			// True if writes should go to the remote terminal
		rows:undefined,			// Rows in users terminal
		cols:undefined,			// Columns in users terminal
		keybuf:'',

		/*
		 * Returns a string with ^A codes converted to ANSI or stripped
		 * as appropriate.
		 */
		parse_ctrla:function(txt, orig_attr) {
			var ret='';
			var i;
			var curr_attr;
			var next_attr;

			if (orig_attr !== undefined)
				curr_attr = new Attribute(orig_attr);
			next_attr = new Attribute(curr_attr);

			function attr_str() {
				var ansi_str;

				if (curr_attr === undefined || curr_attr.value != next_attr.value) {
					ansi_str = next_attr.ansi(curr_attr);
					curr_attr.value = next_attr.value;
					return ansi_str;
				}
				return '';
			}

			for (i=0; i<txt.length; i++) {
				if (txt.charCodeAt(i)==1) {
					i++;
					switch(txt.substr(i, 1)) {
						case '\1':
							ret += attr_str()+'\1';
							break;
						case 'K':
							next_attr.fg = Attribute.BLACK;
							break;
						case 'R':
							next_attr.fg = Attribute.RED;
							break;
						case 'G':
							next_attr.fg = Attribute.GREEN;
							break;
						case 'Y':
							next_attr.fg = Attribute.YELLOW;
							break;
						case 'B':
							next_attr.fg = Attribute.BLUE;
							break;
						case 'M':
							next_attr.fg = Attribute.MAGENTA;
							break;
						case 'C':
							next_attr.fg = Attribute.CYAN;
							break;
						case 'W':
							next_attr.fg = Attribute.WHITE;
							break;
						case '0':
							next_attr.bg = Attribute.BLACK;
							break;
						case '1':
							next_attr.bg = Attribute.RED;
							break;
						case '2':
							next_attr.bg = Attribute.GREEN;
							break;
						case '3':
							next_attr.bg = Attribute.YELLOW;
							break;
						case '4':
							next_attr.bg = Attribute.BLUE;
							break;
						case '5':
							next_attr.bg = Attribute.MAGENTA;
							break;
						case '6':
							next_attr.bg = Attribute.CYAN;
							break;
						case '7':
							next_attr.bg = Attribute.WHITE;
							break;
						case 'H':
							next_attr.bright = true;
							break;
						case 'I':
							next_attr.blink = true;
							break;
						case 'N':
							next_attr.value = 7;
							break;
						case '-':
							if (next_attr.blink || next_attr.bright || next_attr.bg !== Attribute.BLACK)
								next_attr.value = 7;
							break;
						case '_':
							if (next_attr.blink || next_attr.bg !== Attribute.BLACK)
								next_attr.value = 7;
							break;
						default:
							break;
					}
				}
				else {
					ret += attr_str()+txt.substr(i, 1);
				}
			}
			return ret;

			return txt.replace(/\1([\x00-\xff])/g, function(unused, code) {
				switch(code) {
					case '\1':
						return '\1';
					case 'K':
						curr_attr.fg = Attribute.BLACK;
						break;
				}
			});
			/* ToDo: Expand \1D, \1T, \1<, \1Z */
			/* ToDo: "Expand" (ie: remove from string when appropriate) per-level/per-flag stuff */
			/* ToDo: Strip ANSI (I betcha @-codes can slap it in there) */
			while(p<txt.length) {
				ch=txt[p++];
				switch(ch) {
					case '\1':		/* CTRL-A code */
						ch=txt[p++].toUpperCase();
						switch(ch) {
							case '\1':	/* A "real" ^A code */
								this.data[x][y].ch=ch;
								this.data[x][y].attr=curattr;
								x++;
								if(x>=this.width) {
									x=0;
									y++;
									log("next char: [" + txt[p] + "]");
									if(txt[p] == '\r') p++;
									if(txt[p] == '\n') p++;
								}
								break;
							case 'K':	/* Black */
								curattr=(curattr)&0xf8;
								break;
							case 'R':	/* Red */
								curattr=((curattr)&0xf8)|this.defs.RED;
								break;
							case 'G':	/* Green */
								curattr=((curattr)&0xf8)|this.defs.GREEN;
								break;
							case 'Y':	/* Yellow */
								curattr=((curattr)&0xf8)|this.defs.BROWN;
								break;
							case 'B':	/* Blue */
								curattr=((curattr)&0xf8)|this.defs.BLUE;
								break;
							case 'M':	/* Magenta */
								curattr=((curattr)&0xf8)|this.defs.MAGENTA;
								break;
							case 'C':	/* Cyan */
								curattr=((curattr)&0xf8)|this.defs.CYAN;
								break;
							case 'W':	/* White */
								curattr=((curattr)&0xf8)|this.defs.LIGHTGRAY;
								break;
							case '0':	/* Black */
								curattr=(curattr)&0x8f;
								break;
							case '1':	/* Red */
								curattr=((curattr)&0x8f)|(this.defs.RED<<4);
								break;
							case '2':	/* Green */
								curattr=((curattr)&0x8f)|(this.defs.GREEN<<4);
								break;
							case '3':	/* Yellow */
								curattr=((curattr)&0x8f)|(this.defs.BROWN<<4);
								break;
							case '4':	/* Blue */
								curattr=((curattr)&0x8f)|(this.defs.BLUE<<4);
								break;
							case '5':	/* Magenta */
								curattr=((curattr)&0x8f)|(this.defs.MAGENTA<<4);
								break;
							case '6':	/* Cyan */
								curattr=((curattr)&0x8f)|(this.defs.CYAN<<4);
								break;
							case '7':	/* White */
								curattr=((curattr)&0x8f)|(this.defs.LIGHTGRAY<<4);
								break;
							case 'H':	/* High Intensity */
								curattr|=this.defs.HIGH;
								break;
							case 'I':	/* Blink */
								curattr|=this.defs.BLINK;
								break;
							case 'N':	/* Normal (ToDo: Does this do ESC[0?) */
								curattr=7;
								break;
							case '-':	/* Normal if High, Blink, or BG */
								if(curattr & 0xf8)
									curattr=7;
								break;
							case '_':	/* Normal if blink/background */
								if(curattr & 0xf0)
									curattr=7;
								break;
							case '[':	/* CR */
								x=0;
								break;
							case ']':	/* LF */
								y++;
								break;
							default:	/* Other stuff... specifically, check for right movement */
								if(ch.charCodeAt(0)>127) {
									x+=ch.charCodeAt(0)-127;
									if(x>=this.width)
										x=this.width-1;
								}
						}
						break;
					case '\7':		/* Beep */
						break;
					case '\r':
						x=0;
						break;
					case '\n':
						y++;
						break;
					default:
						this.data[x][y]=new this.Cell(ch,curattr);
						x++;
						if(x>=this.width) {
							x=0;
							y++;
							if(txt[p] == '\r') p++;
							if(txt[p] == '\n') p++;
						}
				}
			}
		},

		/*
		 * Clears the current screen to black and moves to location 1,1
		 * sets the current attribute to 7
		 */
		clear:function() {
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

		/*
		 * Returns a Graphic object representing the specified block
		 * or undefined on error (ie: invalid block specified).
		 */
		getblock:function(sx,sy,ex,ey) {
		},

		/*
		 * Writes a string unmodified.
		 */
		print:function(string) {
			if (this.local)
				this.local_io.print(string);
			if (this.remote)
				this.remote_io.print(string);
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
			this.println(this.parse_ctrla(line));
		},

		/*
		 * Writes a string after parsing ^A codes and appends a "\r\n".
		 */
		aprintln:function(line) {
			this.println(this.parse_ctrla(line));
		},

		/*
		 * Waits up to timeout millisections and returns true if a key
		 * is pressed before the timeout.  For ANSI sequences, returns
		 * true when the entire ANSI sequence is available.
		 */
		waitkey:function(timeout) {
			var q = new Queue("dorkit_input");
			// TODO: Parse ANSI here!
			if (q.poll(timeout) === false)
				return false;
			return true;
		},

		/*
		 * Returns a single *KEY*, ANSI is parsed to a single key.
		 * Returns undefined if there is no key pressed.
		 */
		getkey:function() {
			var ret;
			var m;

			if (this.keybuf.length > 0) {
				var ret = this.keybuf.substr(0,1);
				this.keybuf = this.keybuf.substr(1);
				return ret;
			}
			if (!this.waitkey(0))
				return undefined;
			var q = new Queue("dorkit_input");
			ret = q.read();
			if (ret.length > 1) {
				if (ret.substr(0, 9) === 'POSITION_') {
					m = ret.match(/^POSITION_([0-9]+)_([0-9]+)/);
					if (m == NULL)
						return undefined;
					this.last_pos.x = parseInt(m[2], 10);
					this.last_pos.y = parseInt(m[1], 10);
					ret = 'POSITION_REPORT';
				}
				ret=ret.replace(/\x00.*$/,'');
			}
			return ret;
		},

		/*
		 * Returns a single byte... ANSI is not parsed.
		 */
		getbyte:function() {
			if (this.keybuf.length > 0) {
				var ret = this.keybuf.substr(0,1);
				this.keybuf = this.keybuf.substr(1);
				return ret;
			}
			if (!this.waitkey(0))
				return undefined;
			var q = new Queue("dorkit_input");
			ret = q.read();
			if (ret.length > 1) {
				ret=this.key[ret.replace(/^.*\x00/,'')];
				this.keybuf = ret.substr(1);
				ret = ret.substr(0,1);
			}
			return ret;
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
		default_attr:undefined,
		mode:(js.global.bbs !== undefined
				&& js.global.server !== undefined
				&& js.global.client !== undefined
				&& js.global.user !== undefined
				&& js.global.console !== undefined) ? 'sbbs'
				: (js.global.jsexec_revision !== undefined ? 'jsexec'
					: (js.global.jsdoor_revision !== undefined ? 'jsdoor' : undefined))
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
			if(waitkey(500))
				if (getkey() == key.console.POSITION_REPORT)
					return true;
		}
		return false;
	},

	parse_dropfile:function(path) {
		var f = new File(path);
		var df;

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
		this.rows = parseInt(df[20], 10);
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
		this.system.default_attr = new Attribute(parseInt(df[40], 10));
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
					break;
				case '-s':
				case '-socket':
					if (i+1 < argc)
						this.connection.socket = argv[++i];
					break;
				case '-l':
				case '-local':
					this.console.local = true;
					this.console.remote = false;
					break;
			}
		}
		if (this.connection.telnet === undefined)
			this.connection.telnet = false;
	}
};

load("local_console.js");
dk.parse_cmdline(argc, argv);
if (dk.connection.socket !== undefined)
	dk.system.mode = 'socket';

switch(dk.system.mode) {
	case 'sbbs':
		load("sbbs_console.js");
		break;
	case 'jsexec':
		load("jsexec_console.js");
		break;
	case 'jsdoor':
		load("jsdoor_console.js");
		break;
	case 'socket':
		load("socket_console.js", dk.connection.socket, dk.connection.telnet);
		break;
}
