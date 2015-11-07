function Attribute(value) {
	if (typeof(value) == 'object' && value.constructor === Attribute)
		this.value = value.value;
	else
		this.value = value;
}

Attribute.prototype = {
	// TODO: High intensity background, not blink
	get blink() {
		return (this.value & 0x80) ? true : false;
	},
	set blink(val) {
		if (val)
			this.value |= 0x80;
		else
			this.value &= ~0x80;
	},

	get bg() {
		return (this.value >> 4) & 0x07;
	},
	set bg(val) {
		this.value &= 0x8f;
		this.value |= ((val << 4) & 0x70);
	},

	get fg() {
		return this.value & 0x0f;
	},
	set fg(val) {
		this.value &= 0xf0;
		this.value |= val & 0x0f;
	}
}

var dk = {
	key:{
		CTRL_A:1,
		CTRL_B:2,
		CTRL_C:3,
		CTRL_D:4,
		CTRL_E:5,
		CTRL_F:6,
		CTRL_G:7,
		BEEP:7,
		CTRL_H:8,
		BACKSPACE:8,
		CTRL_I:9,
		TAB:9,
		CTRL_J:10,
		LF:10,
		CTRL_K:11,
		CTRL_L:12,
		CLEAR:12,
		CTRL_M:13,
		RETURN:13,
		CTRL_N:14,
		CTRL_O:15,
		CTRL_P:16,
		CTRL_Q:17,
		CTRL_R:18,
		CTRL_S:19,
		PAUSE:19,
		CTRL_T:20,
		CTRL_U:21,
		CTRL_V:22,
		CTRL_W:23,
		CTRL_X:24,
		CTRL_Y:25,
		CTRL_Z:26,
		ESCAPE:27,
		SPACE:32,
		EXCLAIM:33,
		QUOTEDBL:34,
		HASH:35,
		DOLLAR:36,
		PERCENT:37,
		AMPERSAND:38,
		QUOTE:39,
		LEFTPAREN:40,
		RIGHTPAREN:41,
		ASTERISK:42,
		PLUS:43,
		COMMA:44,
		MINUS:45,
		PERIOD:46,
		SLASH:47,
		0:48,
		1:49,
		2:50,
		3:51,
		4:52,
		5:53,
		6:54,
		7:55,
		8:56,
		9:57,
		COLON:58,
		SEMICOLON:59,
		LESS:60,
		EQUALS:61,
		GREATER:62,
		QUESTION:63,
		AT:64,
		A:65,
		B:66,
		C:67,
		D:68,
		E:69,
		F:70,
		G:71,
		H:72,
		I:73,
		J:74,
		K:75,
		L:76,
		M:77,
		N:78,
		O:79,
		P:80,
		Q:81,
		R:82,
		S:83,
		T:84,
		U:85,
		V:86,
		W:87,
		X:88,
		Y:89,
		Z:90,
		LEFTBRACKET:91,
		BACKSLASH:92,
		RIGHTBRACKET:93,
		CARET:94,
		UNDERSCORE:95,
		BACKQUOTE:96,
		a:97,
		b:98,
		c:99,
		d:100,
		e:101,
		f:102,
		g:103,
		h:104,
		i:105,
		j:106,
		k:107,
		l:108,
		m:109,
		n:110,
		o:111,
		p:112,
		q:113,
		r:114,
		s:115,
		t:116,
		u:117,
		v:118,
		w:119,
		x:120,
		y:121,
		z:122,
		LEFTBRACE:123,
		PIPE:124,
		RIGHTBRACE:125,
		TILDE:126,
		DELETE:127,
		/* End of ASCII */
	},


	console:{
		x:1,					// Current column (1-based)
		y:1,					// Current row (1-based)
		attr:new Attribute(7),	// Current attribute
		ansi:true,				// ANSI support is enabled
		charset:'cp437',		// Supported character set
		local:true,				// True if writes should go to the local screen
		remote:true,			// True if writes should go to the remote terminal
		rows:24,				// Rows in users terminal

		/*
		 * Clears the current screen to black and moves to location 1,1
		 * sets the current attribute to 7
		 */
		clear:function() {
		},

		/*
		 * Clears to end of line.
		 * Not available witout ANSI (???)
		 */
		cleareol:function() {
		},

		/*
		 * Moves the cursor to the specified position.
		 * returns false on error.
		 * Not available without ANSI
		 */
		gotoxy:function(x,y) {
		},

		/*
		 * Returns a Graphic object representing the specified block
		 * or undefined on error (ie: invalid block specified).
		 */
		getblock:function(sx,sy,ex,ey) {
		},

		/*
		 * Writes a string with a "\r\n" appended.
		 */
		println:function(line) {
		},

		/*
		 * Writes a string unmodified.
		 */
		print:function(string) {
		},

		/*
		 * Writes a string after parsing ^A codes and appends a "\r\n".
		 */
		aprintln:function(line) {
		},

		/*
		 * Writes a string after parsing ^A codes.
		 */
		aprint:function(string) {
		},

		/*
		 * Waits up to timeout millisections and returns true if a key
		 * is pressed before the timeout.  For ANSI sequences, returns
		 * true when the entire ANSI sequence is available.
		 */
		waitkey:function(timeout) {
		},

		/*
		 * Returns a single *KEY*, ANSI is parsed to a single key.
		 * Returns undefined if there is no key pressed.
		 */
		getkey:function() {
		},

		/*
		 * Returns a single byte... ANSI is not parsed.
		 */
		getbyte:function() {
		},
	},
	connection:{
		type:undefined,
		baud:undefined,
		parity:undefined,
		node:undefined,
		dte:undefined,
		error_correcting:true,
		time:undefined
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
	}
};

js.load_path_list.unshift(js.exec_dir+"/jsdoor/");

switch(dk.system.mode) {
	case 'sbbs':
		load("jsdoor_sbbs_console.js");
		break;
	case 'jsexec':
		load("jsdoor_jsexec_console.js");
		break;
	case 'jsdoor':
		load("jsdoor_console.js");
		break;
}
