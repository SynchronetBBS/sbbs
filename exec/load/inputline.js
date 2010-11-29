load("str_cmds.js");
full_redraw=false;

function InputLine(x,y,w,max,bg,fg,title)
{
	this.x=1;
	this.y=24;
	this.width=79;
	this.max_buffer=200;
	
	this.bg=BG_LIGHTGRAY;
	this.fg=BLACK;
	this.buffer="";
	
	this.title_bg=BG_LIGHTGRAY;
	this.title_fg=BLACK;
	this.title="";
	
	this.clear=function() 
	{
		console.gotoxy(this.x + console.strlen(this.title),this.y);
		console.attributes=this.bg;
		console.write(format("%*s",this.width-console.strlen(this.title),""));
	}
	this.init=function(x,y,w,max,bg,fg,title) 
	{
		if(x) this.x=x;
		if(y) this.y=y;
		if(w) this.width=w;
		if(max) this.max_buffer=max;
		if(bg) this.bg=bg;
		if(fg) this.fg=fg;
		if(title) this.title=title;
	
		if(this.x + this.w > console.screen_columns) {
			log("input line wider than screen. adjusting to fit.");
			this.w=console.screen_columns-this.x-1;
		}
		if(this.x >= console.screen_columns || this.y > console.screen_rows) {
			log("input line off the screen. using defaults.");
			this.x=1;
			this.y=1;
		}
	}
	this.submit=function()
	{
		if(strlen(this.buffer)<1) return null;
		
		if(this.buffer[0]==";") {
			if(this.buffer.length>1 && (user.compare_ars("SYSOP") || bbs.sys_status&SS_TMPSYSOP)) {
				console.clear();
				str_cmds(this.buffer.substr(1));
				this.reset();
				this.clear();
				full_redraw=true;
				return null;
			}
		}
		
		var command=this.buffer + "\r\n";
		this.reset();
		this.clear();
		return command;
	}
	this.inkey=function(hotkeys)
	{
		var key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO);
		if(!key) 
			return false;
		if(hotkeys) 
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
		case '\x7f':	/* DELETE */
		case '\x1b':	/* ESC */
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
			return key;
		case '\b':
			this.backspace();
			return false;
		case '\r':
		case '\n':
			return this.submit();
		default:
			this.bufferKey(key);
			return false;
		}
	}
	this.backspace=function()
	{
		if(this.buffer.length>0) {
			if(this.buffer.length<=this.getWidth()) {
				this.getxy();
				console.left();
				console.attributes=this.bg;
				console.write(" ");
				this.buffer=this.buffer.substr(0,this.buffer.length-1);
			} else {
				this.buffer=this.buffer.substr(0,this.buffer.length-1);
				console.gotoxy(this);
				console.right(console.strlen(this.title));
				this.printBuffer();
			}
			return true;
		} else {
			return false;
		}
	}
	this.reset=function()
	{
		this.buffer="";
	}
	this.getxy=function()
	{
		console.gotoxy(this.x + console.strlen(this.title) + this.buffer.length,this.y);
	}
	this.bufferKey=function(key)
	{
		if(this.buffer.length >= this.max_buffer) {
			return false;
		} else if(this.buffer.length>=this.getWidth()) {
			this.buffer+=key;
			console.gotoxy(this);
			console.right(console.strlen(this.title));
			this.printBuffer();
		} else {
			this.getxy();
			this.buffer+=key;
			console.attributes=this.fg + this.bg;
			console.write(key);
		}
		return true;
	}
	this.getWidth=function()
	{
		return this.width-console.strlen(this.title);
	}
	this.draw=function()
	{
		console.gotoxy(this);
		this.printTitle();
		if(this.x + this.width == console.screen_columns && this.y == console.screen_rows) 
			console.cleartoeol(this.bg);
		this.printBuffer();
	}
	this.printTitle=function()
	{
		if(this.title.length > 0) {
			console.attributes=this.title_fg + this.title_bg;
			console.write(this.title);
		}
	}
	this.printBuffer=function()
	{
		console.attributes=this.fg+this.bg;
		if(this.buffer.length>this.getWidth()) {
			var overrun=this.buffer.length-this.getWidth();
			var truncated=this.buffer.substr(overrun);
			console.write(truncated);
		} else {
			console.write(printPadded(this.buffer,this.getWidth()));
		}
	}

	this.init(x,y,w,max,bg,fg,title);
}
