if(js.global.getColor == undefined)
	js.global.load(js.global,"funclib.js");
if(js.global.Frame == undefined)
	js.global.load(js.global,"frame.js");

function InputLine(frame) {
	/* private properties */
	var properties = {
		frame:undefined,
		attr:undefined,
		cursor:undefined,
		buffer:[]
	};
	var settings = {
		show_border:true,
		show_title:true,
		show_cursor:false,
		mask_input:false,
		auto_clear:true,
		cursor_attr:BG_LIGHTGRAY|BLACK,
		cursor_char:"_",
		mask_char:"*",
		timeout:10,
		max_buffer:200
	};
	
	/* protected properties */
	this.__defineGetter__("frame",function() {
		return properties.frame;
	});
	this.__defineGetter__("max_buffer",function() {
		return settings.max_buffer;
	});
	this.__defineSetter__("max_buffer",function(num) {
		if(num > 0 && num < 10000)
			settings.max_buffer = Number(num);
	});
	this.__defineGetter__("auto_clear",function() {
		return settings.auto_clear;
	});
	this.__defineSetter__("auto_clear",function(bool) {
		if(typeof bool == "boolean")
			settings.auto_clear = bool;
	});
	this.__defineGetter__("show_cursor",function() {
		return settings.show_cursor;
	});
	this.__defineSetter__("show_cursor",function(bool) {
		if(settings.show_cursor == bool)
			return;
		if(typeof bool == "boolean")
			settings.show_cursor = bool;
		if(settings.show_cursor) {
			if(!properties.cursor) {
				properties.cursor = new Frame(properties.frame.x,properties.frame.y,1,1,
					BG_LIGHTGRAY|BLACK,properties.frame);
				properties.cursor.putmsg("\r" + settings.cursor_char);
			}
			if(properties.frame.is_open) {
				properties.cursor.open();
			}
		}
		else if(properties.cursor.is_open) {
			properties.cursor.close();
		}
	});
	this.__defineGetter__("mask_input",function() {
		return settings.mask_input;
	});
	this.__defineSetter__("mask_input",function(bool) {
		if(settings.mask_input == bool)
			return;
		if(typeof bool == "boolean")
			settings.mask_input = bool;
		printBuffer();
	});	
	this.__defineGetter__("attr",function() {
		return properties.attr;
	});
	this.__defineSetter__("attr",function(attr) {
		if(!isNaN(attr))
			properties.attr = Number(attr);
	});
	this.__defineGetter__("cursor_attr",function() {
		return settings.cursor_attr;
	});
	this.__defineSetter__("cursor_attr",function(attr) {
		if(attr >= 0 && attr < 512) {
			settings.cursor_attr = Number(attr);
			if(properties.cursor)
				properties.cursor.attr = settings.cursor_attr;
		}
	});
	this.__defineGetter__("cursor_char",function() {
		return settings.cursor_char;
	});
	this.__defineSetter__("cursor_char",function(ch) {
		if(ch && ch.length == 1) {
			settings.cursor_char = ch;
			if(properties.cursor)
				properties.cursor.putmsg("\r" + settings.cursor_char);
		}
	});
	this.__defineGetter__("mask_char",function() {
		return settings.mask_char;
	});
	this.__defineSetter__("mask_char",function(ch) {
		if(ch == settings.mask_char)
			return;
		if(ch && ch.length == 1) {
			settings.mask_char = ch;
			if(settings.mask_input) 
				printBuffer();
		}
	});
	this.__defineGetter__("timeout",function() {
		return properties.timeout;
	});
	this.__defineSetter__("timeout",function(num) {
		if(num > 0 && num < 10000)
			settings.timeout = Number(num);
	});
	this.__defineGetter__("buffer",function() {
		return properties.buffer;
	});
	this.__defineGetter__("overrun",function() {
		return properties.buffer.length-properties.frame.width;
	});
	
	/* public methods */
	this.open = function() {
		properties.frame.open();
		if(properties.cursor)
			properties.cursor.top();
	}
	this.cycle = function() {
		properties.frame.cycle();
	}
	this.close = function() {
		properties.frame.close();
	}
	this.clear = function() {
		reset();
	}
	this.getkey = function(use_hotkeys) {
		var key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,settings.timeout);
		if(key == "") 
			return undefined;
		if(use_hotkeys) 
			return key;
		switch(key) {
		case '\x0c':	/* CTRL-L */
		case '\x00':	/* CTRL-@ (NULL) */
		case '\x03':	/* CTRL-C */
		case '\x04':	/* CTRL-D */
		case '\x0b':	/* CTRL-K */
		case '\x0e':	/* CTRL-N */
		case '\x0f':	/* CTRL-O */
		case '\x09': 	// TAB
		case '\x10':	/* CTRL-P */
		case '\x11':	/* CTRL-Q */
		case '\x12':	/* CTRL-R */
		case '\x13':	/* CTRL-S */
		case '\x14':	/* CTRL-T */
		case '\x15':	/* CTRL-U */
		case '\x16':	/* CTRL-V */
		case '\x17':	/* CTRL-W */
		case '\x18':	/* CTRL-X */
		case '\x19':	/* CTRL-Y */
		case '\x1a':	/* CTRL-Z */
		case '\x1c':	/* CTRL-\ */
		case '\x1f':	/* CTRL-_ */
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_HOME:
		case KEY_END:
			return key;
		case '\x7f':	/* DELETE */
		case '\b':
			return backspace();
		case '\r':
		case '\n':
			return submit();
		case '\x1b':
			return false;
		default:
			return bufferKey(key);
		}
	}
	this.popxy = function() {
		var position=getxy();
		gotoxy(position);
	}
	this.init = function(x,y,w,h,frame,attr) {
		properties.frame = new Frame(x,y,w,h,attr,frame);
		properties.frame.v_scroll = false;
		properties.frame.h_scroll = true;
		properties.frame.open();
	}
	
	/* private functions */
	function bufferKey(key) {
		if(properties.buffer.length >= settings.max_buffer) 
			return undefined;
		properties.buffer+=key;
	
		if(properties.buffer.length>properties.frame.width) 
			printBuffer();
		else if(settings.mask_input) 
			properties.frame.write(settings.mask_char,properties.attr);
		else
			properties.frame.write(key,properties.attr);
			
		if(settings.show_cursor)
			printCursor();
		return undefined;
	}
	function backspace() {
		if(properties.buffer.length == 0) 
			return undefined;
		properties.buffer=properties.buffer.substr(0,properties.buffer.length-1);
		if(properties.buffer.length+1>=properties.frame.width) {
			printBuffer();
		}
		else {
			properties.frame.left();
			properties.frame.write(" ");
			properties.frame.left();
		}
		if(settings.show_cursor)
			printCursor();
		return undefined;
	}
	function printBuffer() {
		if(settings.mask_input) {
			maskBuffer();
			return;
		}
		properties.frame.clear();
		if(this.overrun > 0) {
			var truncated=properties.buffer.substr(overrun);
			properties.frame.write(truncated);
		}
		else {
			properties.frame.write(properties.buffer);
		}
	}
	function maskBuffer() {
		properties.frame.clear();
		if(this.overrun > 0) {
			properties.frame.write(properties.buffer.substr(overrun).replace(/./g,settings.mask_char));
		}
		else {
			properties.frame.write(properties.buffer.replace(/./g,settings.mask_char));
		}
	}
	function gotoxy(position) {
		console.gotoxy(position);
		return position;
	}
	function getxy() {
		var overrun=properties.buffer.length-properties.frame.width;
		var position = {};
		if(overrun > 0) {
			position.x=properties.frame.x+properties.frame.width
			position.y=properties.frame.y;
		}
		else {
			position.x=properties.frame.x+properties.buffer.length;
			position.y=properties.frame.y;
		}
		return position;
	}
	function printCursor() {
		var position = getxy();
		properties.cursor.moveTo(position.x,position.y);
		//gotoxy(position);
	}
	function reset() {
		properties.buffer = [];
		properties.frame.clear();
		if(settings.show_cursor)
			printCursor();
	}
	function submit() {
		// if(strlen(properties.buffer)<1) 
			// return null;
		if(properties.buffer[0]==";") {
			if(properties.buffer.length>1 && (user.compare_ars("SYSOP") || bbs.sys_status&SS_TMPSYSOP)) {
				bbs.command_str='';
				js.global.load("str_cmds.js",properties.buffer.substr(1));
				properties.frame.invalidate();
				if(settings.auto_clear)
					reset();
				return null;
			}
		}
		var cmd=properties.buffer;
		if(settings.auto_clear)
			reset();
		return cmd;
	}
	function init(frame) {
		if(frame instanceof Frame)
			properties.frame=frame;
	}
	init.apply(this,arguments);
}

