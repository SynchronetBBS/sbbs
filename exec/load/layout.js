/* $id: $ */

/*
	by Matt Johnson (MCMLXXIX) 2010
	
	the Layout() object is intended as an array of Window() objects.
	the Window() object contains tabs, which contain whatever content 
	they are supplied with. each tab object must have a draw() method
	and a handle_command() method. 

	this setup allows a modular tabbed window interface for things such as:
	
		IRC chat rooms
		user lists
		lightbar menus
		ANSI graphics
		etc...
	
	usage example:
	
		var x=1;
		var y=1;
		var width=79;
		var height=24;
		var use_scrollbar=true;
		var use_theme=false;
		
		var layout=new Layout();
		layout.add("chat");
		layout.current_window.init(x,y,width,height,use_scrollbar,use_theme);
		
		var chat=new ChatEngine();
		chat.init(x,y,width,height);
		chat.joinChan("#synchronet");

		layout.current_window.add("#synchronet");
		layout.current_window.current_tab.init(chat.chatrooms["#synchronet"]);
		
		layout.current_window.current_tab.cycle();
		layout.window("chat").tab("#synchronet").cycle();
		
		layout.window("chat").tab("#synchronet").draw(argument1,argument2,etc...);
		layout.current_window.draw();
		
		layout.current_window.cycle();
		layout.cycle();
*/

load("funclib.js");

function Layout() 
{
	this.index=0;
	this.windows=[];
	this.windows_map=[];
	
	/* add a new window to layout */
	this.add=function(name) {
		/* if window does not exist, create new window */
		if(!this.windows_map[name.toUpperCase()] >= 0) {
			var window=new Layout_Window(name);
			this.index=this.windows.length;
			this.windows_map[name.toUpperCase()]=this.windows.length;
			this.windows.push(window);
			log(LOG_DEBUG, "new window created: " + name);
			return true;
		}
		
		/* otherwise fail */
		else {
			log(LOG_WARNING, "ERROR: a window with this name already exists: " + name);
			return false;
		} 
	}
	
	/* layout update loop */
	this.cycle=function() {
		for(var w=0;w<this.windows.length;w++) 
			this.windows[w].cycle.apply(this,arguments);
	}
	
	/* return current window object */
	this.current_window getter=function() {
		return this.windows[this.index];
	}

	/* get a window by name */
	this.window=function(name) {
		if(this.windows_map[name.toUpperCase()] >= 0) {
			return this.windows[this.windows_map[name.toUpperCase()]];
		} 
		return false;
	}
	
	/* set current window object by name */
	this.current_window setter=function(name) {
		if(this.windows_map[name.toUpperCase()] >= 0) {
			this.index=this.windows_map[name.toUpperCase()];
			return true;
		} 
		return false;
	}
	
	/* handle layout commands/window commands */
	this.handle_command=function(key) {
		switch(key.toUpperCase()) {
		case '\x09':	/* CTRL-I TAB */
			this.index++;
			if(this.index >= this.length) this.index=0;
			return true;
		default:
			if(this[this.index].handle_command)
				return this[this.index].handle_command(key);
			return false;
		}
	}
}

function Layout_Window(name)
{
	this.name=name;

	/* main window position (upper left corner) */
	this.x=1;
	this.y=1;
	
	/* window dimensions */
	this.width=79;
	this.height=24;

	/* 	array containing window tabs 
		tabs can be anything that will fit in the window
		such as: chat windows, user list, lightbar menus */
	this.tabs=[];
	this.tabs_map=[];
	this.index=0;
	
	this.scrollbar;
	this.scrollback=-1;
	
	this.settings;
	
	/* initialize window settings */
	this.init=function(x,y,w,h,sb,t) {
	
		/* if no parameters are supplied, initialize fullscreen */
		if(!(x || y || w || h)) {
			this.x=1;
			this.y=1;
			this.width=79;
			this.height=24;
			log(LOG_DEBUG, format("window initialized (x: %i y: %i w: %i h: %i)",this.x,this.y,this.width,this.height));
		}
		
		/* verify window parameters */
		else if(x <= 0 || y <= 0 
			|| isNaN(x) || isNaN(y) 
			|| (x+w) >= console.screen_columns
			|| (y+h) >= console.screen_rows) {
			log(LOG_WARNING, "ERROR: invalid or missing window parameters");
			return false;
		} 
		
		/* initialize window with supplied parameters */
		else {
			if(x) this.x=x;
			if(y) this.y=y;
			if(w) this.width=w;
			if(h) this.height=h;
			
			log(LOG_DEBUG, format("window initialized (x: %i y: %i w: %i h: %i)",this.x,this.y,this.width,this.height));
		}
		
		/* initialize scrollback */
		if(sb > 0) {
			this.scrollbar=new Scrollbar(this.x+this.width,this.y,this.height,"vertical","\1k\1h"); 
			this.scrollback=sb;
			
			log(LOG_DEBUG, format("scrollback buffer set (%i)",sb));
			log(LOG_DEBUG, format("scrollbar initialized (x: %i y: %i h: %i)",this.x+this.width,this.y,this.height));
		}
		
		/* load theme, if specified */
		if(t) {
			//ToDo: create window_themes.ini
		}
		
		/* otherwise, load default color scheme */
		else {
			this.settings=new Window_Settings(
				LIGHTGRAY, /* window foreground */
				BG_BLACK,	/* window background */
				LIGHTBLUE,	/* window highlight foreground */
				BG_BLUE,	/* window highlight background */
				LIGHTGRAY,	/* border foreground */
				BLACK,		/* tab foreground (normal) */
				BG_LIGHTGRAY, /* tab background */
				LIGHTRED,	/* tab highlight foreground */	
				BG_RED,		/* tab highlight background */
				RED,		/* tab alert foregruond */
				MAGENTA,	/* tab update foreground */
				BLUE		/* tab user msg foreground */
			);
		}
	}
	
	/* update loop */
	this.cycle=function() {
		for(var t=0;t<this.tabs.length;t++) 
			if(typeof this.tabs[t].cycle == "function") 
				this.tabs[t].cycle.apply(this,arguments);
	}
	
	/* get a tab by name */
	this.tab=function(name) {
		if(this.tabs_map[name.toUpperCase()] >= 0) {
			return this.tabs[this.tabs_map[name.toUpperCase()]];
		} 
		return false;
	}
	
	/* return current tab object */
	this.current_tab getter=function() {
		return this.tabs[this.index];
	}
	
	/* set current tab object */
	this.current_tab setter=function(name) {
		if(this.tabs_map[name.toUpperCase()] >= 0) {
			this.index=this.tabs_map[name.toUpperCase()];
			return true;
		} 
		return false;
	}
	
	/* add a named tab to this window */
	this.add=function(name) {
	
		/* if tab does not exist, create new tab */
		if(!this.tabs_map[name.toUpperCase()] >= 0) {
			this.index=this.tabs.length;
			this.tabs_map[name.toUpperCase()]=this.tabs.length;
			this.tabs.push(new Window_Tab(name,this.x+1,this.y+1,this.width-2,this.height-1));
			log(LOG_DEBUG, "new tab created: " + name);
			return true;
		}
		
		/* otherwise fail */
		else {
			log(LOG_WARNING, "ERROR: a tab with this name already exists: " + name);
			return false;
		} 
	}
	
	/* handle window commands */
	this.handle_command=function(key) {
		switch(key.toUpperCase()) {
		case KEY_LEFT:
		case KEY_RIGHT:
			break;
		}
	}
	
	/* draw the window and contents of the currently active tab */
	this.drawWindow=function() {
		this.drawTitle();
		this.drawBorder();
		this.drawWindow();
	}
	
	/* draw the title bar of the current window */
	this.drawTitle=function() {	
		var title_str=getColor(this.settings.tbg) + getColor(BLACK);
		title_str+=" \xDD";
		if(this.active == true) 
			title_str+=getColor(this.settings.whbg) + getColor(this.settings.whbg);
		else 
			title_str+=getColor(BG_BLACK) + getColor(DARKGRAY);
		title_str+=this.name;
		var title_str=getColor(this.settings.tbg) + getColor(BLACK);
		title_str+="\xDe";
		for(var t=0;t<this.tabs.length;t++) {
			if(this.tabs[t].active == true) {
				title_str+=getColor(this.settings.hbg) + getColor(this.settings.hfg);
			} else {
				switch(this.tabs[t].status) {
				case -1:
					title_str+=getColor(this.settings.tbg) + getColor(this.settings.tfg);
					break;
				case 0:
					title_str+=getColor(this.settings.tbg) + getColor(this.settings.nfg);
					break;
				case 1:
					title_str+=getColor(this.settings.tbg) + getColor(this.settings.afg);
					break;
				case 2:
					title_str+=getColor(this.settings.tbg) + getColor(this.settings.pfg);
					break;
				}
			}
			title_str+=this.tabs[t].name + " ";
		}
		title_str+=getColor(this.settings.tbg) + getColor(this.settings.tfg);

		console.gotoxy(this.x,this.y);
		console.putmsg(printPadded(title_str,this.width-1) + "x");
	}
	
	/* draw the border of the current window */
	this.drawBorder=function() {
		setPosition(this.x,this.y+1);
		console.attributes=BG_BLACK + this.settings.bfg;
		for(var y=0;y<this.height-1;y++) {
			pushMessage(splitPadded("\xDD","\xDE",this.width));
		}
	}
	
	/* draw the contents of the current window */
	this.draw=function() {
		clearBlock(this.x,this.y,this.w,this.h,this.settings.bg);
		if(typeof this.tabs[this.index].draw == "function") 
			this.tabs[this.index].draw.apply(this,this.x+1,this.y+1,this.width-2,this.height-1);
	}
}

function Window_Settings(wfg,wbg,whfg,whbg,bfg,tfg,tbg,hfg,hbg,afg,nfg,pfg)
{
	/* main window colors */
	this.wfg=wfg;
	this.wbg=wbg;
	
	/* window hightlight colors */
	this.whfg=whfg;
	this.whbg=whbg;

	/* window border color */
	this.bfg=bfg;
	
	/* tab colors */
	this.tfg=tfg;
	this.tbg=tbg;
	
	/* tab highlight colors */
	this.hfg=hfg;
	this.hbg=hbg;
	
	/* tab alert color */
	this.afg=afg;
	
	/* tab notice color */
	this.nfg=nfg;
	
	/* tab user message color */
	this.pfg=pfg;
	
	/* inactive window colors */
	this.ibg=BG_BLACK;
	this.ifg=DARKGRAY;
}

function Window_Tab(name,x,y,w,h)
{
	this.name=name;
	this.status=-1;
	this.active=false;

	/* dynamic content and command processor placeholders */
	this.handle_command;
	this.cycle;
	this.draw;
	
	this.init=function(draw,handle_command,cycle)
	{
		this.draw=draw;
		this.handle_command=handle_command;
		this.cycle=cycle;
	}
}

