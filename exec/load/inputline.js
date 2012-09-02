bbs.command_str='';
load("str_cmds.js");
load("funclib.js");
load("frame.js");

function InputLine(frame,text) {
	/* private properties */
	var properties = {
		frame:undefined,
		text:undefined,
		buffer:[]
	};
	var settings = {
		show_border:true,
		show_title:true,
		timeout:10,
		max_buffer:200
	};
	
	/* protected properties */
	this.__defineGetter__("frame",function() {
		return properties.frame;
	});
	this.__defineGetter__("max_buffer",function() {
		return properties.max_buffer;
	});
	this.__defineSetter__("max_buffer",function(num) {
		if(num > 0 && num < 10000)
			settings.max_buffer = Number(num);
	});
	this.__defineGetter__("timeout",function() {
		return properties.timeout;
	});
	this.__defineSetter__("timeout",function() {
		return properties.timeout;
		if(num > 0 && num < 10000)
			settings.timeout = Number(num);
	});
	
	/* public properties */
	this.colors = {
		bg:BG_BLUE,
		fg:WHITE
	};
	
	/* public methods */
	this.getkey = function(use_hotkeys) {
		var key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,settings.timeout);
		if(key == "") 
			return undefined;
		if(use_hotkeys) 
			return key;
		switch(key) {
		case '\x00':	/* CTRL-@ (NULL) */
		case '\x03':	/* CTRL-C */
		case '\x04':	/* CTRL-D */
		case '\x0b':	/* CTRL-K */
		case '\x0c':	/* CTRL-L */
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
	this.init = function(x,y,w,h,frame) {
		var attr = this.colors.bg + this.colors.fg;
		properties.frame = new Frame(x,y,w,h,attr,frame);
		properties.frame.v_scroll = false;
		properties.frame.h_scroll = true;
		properties.frame.open();
	}
	this.popxy = function() {
		var overrun=properties.buffer.length-properties.frame.width;
		if(overrun > 0) 
			console.gotoxy(properties.frame.x+properties.frame.width,properties.frame.y);
		else
			console.gotoxy(properties.frame.x+properties.buffer.length,properties.frame.y);
	}
	
	/* private functions */
	function bufferKey(key) {
		if(properties.buffer.length >= settings.max_buffer) 
			return undefined;
		properties.buffer+=key;
		if(properties.buffer.length>properties.frame.width) 
			printBuffer();
		 else 
			properties.frame.putmsg(key);
		return undefined;
	}
	function backspace() {
		if(properties.buffer.length == 0) 
			return undefined;
		properties.buffer=properties.buffer.substr(0,properties.buffer.length-1);
		if(properties.buffer.length+1>=properties.frame.width) 
			printBuffer();
		else {
			properties.frame.left();
			properties.frame.putmsg(" ");
			properties.frame.left();
		}
		return undefined;
	}
	function printBuffer() {
		properties.frame.clear();
		var overrun=properties.buffer.length-properties.frame.width;
		if(overrun > 0) {
			var truncated=properties.buffer.substr(overrun);
			properties.frame.putmsg(truncated);
		}
		else {
			properties.frame.putmsg(properties.buffer);
		}
	}
	function reset() {
		properties.buffer = [];
		properties.frame.clear();
	}
	function submit() {
		// if(strlen(properties.buffer)<1) 
			// return null;
		if(properties.buffer[0]==";") {
			if(properties.buffer.length>1 && (user.compare_ars("SYSOP") || bbs.sys_status&SS_TMPSYSOP)) {
				str_cmds(properties.buffer.substr(1));
				reset();
				return null;
			}
		}
		var cmd=properties.buffer;
		reset();
		return cmd;
	}
	function init(frame,text) {
		if(frame instanceof Frame)
			properties.frame=frame;
		if(text) 
			properties.text = text;
	}
	init.apply(this,arguments);
}
