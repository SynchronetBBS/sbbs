/* $id: $ */

/*
	by Matt Johnson (MCMLXXIX) 2010
	
	the Layout() object is intended as an array of Layout_Window() objects.
	the Layout_Window() object contains tabs, which contain whatever content 
	they are supplied with. each tab object should have the following methods, 
	though it may exist without them:
	
		handle_command()
		cycle()
		draw()
		
	this setup allows a modular tabbed window interface for things such as:
	
		IRC chat rooms
		user lists
		lightbar menus
		ANSI graphics (static or animated)
		etc...
	
*/

load("funclib.js");
load("sbbsdefs.js");

var layout_settings;

/* main layout object */
function Layout(theme) 
{
	/* private properties */
	var windows=[];
	var windows_map=[];
	var index={ value:0 };
	
	/* public window index */
	this.index getter=function() {
		return index.value;
	}
	
	this.index setter=function(value) {
		if(isNaN(value) || !windows[value]) return false;
		index.value=value;
		return true;
	}
	
	/* initialize layout */
	this.init=function(theme) {
		/* load theme, if specified */
		if(theme) {
			//ToDo: create layout_themes.ini
		}
		
		/* otherwise, load default color scheme */
		else {
			layout_settings=new Layout_Settings(
				LIGHTGRAY, /* window foreground */
				BG_BLACK,	/* window background */
				YELLOW,	/* window highlight foreground */
				BG_BLUE,	/* window highlight background */
				LIGHTGRAY,	/* border foreground color */
				BG_BLACK,	/* border background color */
				BLACK,		/* tab foreground (normal) */
				BG_LIGHTGRAY, /* tab background */
				LIGHTCYAN,	/* tab highlight foreground */	
				BG_CYAN,		/* tab highlight background */
				RED,		/* tab alert foregruond */
				MAGENTA,	/* tab update foreground */
				BLUE		/* tab user msg foreground */
			);
		}
	}

	/* add a new window to layout */
	this.add_window=function(name,x,y,w,h) {
		if(!name) {
			log(LOG_WARNING, "ERROR: no window name specified");
			return false;
		}
		if(windows_map[name.toUpperCase()] >= 0) {
			log(LOG_WARNING, "ERROR: a window with this name already exists: " + name);
			return false;
		} 
		
		
		/* if no parameters are supplied, initialize fullscreen defaults */
		if(!x) x=1;
		if(!y) y=1;
		if(!w) w=console.screen_columns-1;
		if(!h) h=console.screen_rows;
		
		/* verify window parameters */
		if(x <= 0 || y <= 0 
			|| isNaN(x) || isNaN(y) 
			|| (x-1+w) > console.screen_columns
			|| (y-1+h) > console.screen_rows) {
			log(LOG_WARNING, "ERROR: invalid window parameters");
			return false;
		} 

		var window=new Layout_Window(name,x,y,w,h);
		window.window_position=windows.length;
		window.layout_position=index;
		windows_map[name.toUpperCase()]=windows.length;
		windows.push(window);
		return true;
	}
	
	/* layout update loop */
	this.cycle=function() {
		for(var w=0;w<windows.length;w++) 
			windows[w].cycle.apply(this,arguments);
	}
	
	/* get a window by name */
	this.window=function(name) {
		if(windows_map[name.toUpperCase()] >= 0) {
			return windows[windows_map[name.toUpperCase()]];
		} 
		return false;
	}
	
	/* return current window object */
	this.current_window getter=function() {
		return windows[this.index];
	}

	/* set current window object by name */
	this.current_window setter=function(name) {
		if(windows_map[name.toUpperCase()] >= 0) {
			this.index=windows_map[name.toUpperCase()];
			return true;
		} 
		return false;
	}
	
	/* draw all layout windows */
	this.draw=function() {
		for(var w=0;w<windows.length;w++)
			windows[w].drawWindow();
	}
	
	/* handle layout commands/window commands */
	this.handle_command=function(cmd) {
		switch(cmd) {
		case '\x09':	/* CTRL-I TAB */
			var old_index=this.index;
			for(count=0;count<windows.length;count++) {
				if(this.index+1>=windows.length)
					this.index=0;
				else 
					this.index++;
				if(windows[this.index].interactive) break;
			}
			windows[old_index].drawTitle();
			windows[this.index].drawTitle();
			return true;
		default:
			return windows[this.index].handle_command(cmd);
		}
	}

	this.init(theme);
}

/* layout settings object */
function Layout_Settings(wfg,wbg,wtfg,wtbg,bfg,bbg,tfg,tbg,hfg,hbg,afg,nfg,pfg)
{
	/* main window colors */
	this.wfg=wfg;
	this.wbg=wbg;
	
	/* title hightlight colors */
	this.wtfg=wtfg;
	this.wtbg=wtbg;

	/* window border color */
	this.bfg=bfg;
	this.bbg=bbg;
	
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

/* tabbed window object */
function Layout_Window(name,x,y,w,h)
{
	this.name=name;

	/* private array containing window tabs 
		tabs can be anything that will fit in the window
		such as: chat windows, user list, lightbar menus */
	var tabs=[];
	var tabs_map=[];
	var index={ value:0 };
	
	/* private window properties */
	var show_title=true;
	var show_tabs=true;
	var show_border=true;
	var interactive=true;

	/* main window position (upper left corner) */
	this.x=x;
	this.y=y;
	
	/* window dimensions */
	this.width=w;
	this.height=h;

	
	/* public tab index */
	this.index getter=function() {
		return index.value;
	}
	
	this.index setter=function(value) {
		if(isNaN(value) || !tabs[value]) return false;
		index.value=value;
		return true;
	}
	
	/* update loop */
	this.cycle=function() {
		for(var t=0;t<tabs.length;t++) 
			if(typeof tabs[t].cycle == "function") 
				tabs[t].cycle.apply(this,arguments);
	}
	
	/* get a tab by name */
	this.tab=function(name) {
		if(tabs_map[name.toUpperCase()] >= 0) {
			return tabs[tabs_map[name.toUpperCase()]];
		} 
		return false;
	}
	
	/* toggle display options (can only be done prior to adding any tabs) */
	this.show_title setter=function(bool) {
		if(tabs.length > 0) return false;
		show_title=bool;
		return true;
	}

	this.show_tabs setter=function(bool) {
		if(tabs.length > 0) return false;
		show_tabs=bool;
		return true;
	}

	this.show_border setter=function(bool) {
		if(tabs.length > 0) return false;
		show_border=bool;
		return true;
	}

	this.interactive setter=function(bool)	 {
		if(tabs.length > 0) return false;
		interactive=bool;
		return true;
	}
		
	/* get display options */
	this.show_border getter=function() {
		return show_border;
	}

	this.show_title getter=function() {
		return show_title;
	}
	
	this.show_tabs getter=function() {
		return show_tabs;
	}

	this.interactive getter=function() {
		return interactive;
	}
	
	/* return current tab object */
	this.current_tab getter=function() {
		return tabs[this.index];
	}
	
	/* set current tab object */
	this.current_tab setter=function(name) {
		if(tabs_map[name.toUpperCase()] >= 0) {
			this.index=tabs_map[name.toUpperCase()];
			return true;
		} 
		return false;
	}
	
	/* add a named tab to this window */
	this.add_tab=function(type,name) {
		if(!(type && name)) {
			log(LOG_WARNING, "ERROR: invalid tab parameters");
			return false;
		}
		if(tabs_map[name.toUpperCase()] >= 0) {
			log(LOG_WARNING, "ERROR: a tab with this name already exists: " + name);
			return false;
		} 
		
		var tab=false;
		var x=this.x;
		var y=this.y;
		var w=this.width;
		var h=this.height;
		
		if(show_title) {
			h-=1;
			y+=1;
		}
		if(show_tabs) {
			h-=1;
			y+=1;
		}
		if(show_border) {
			w-=2;
			h-=2;
			x+=1;
			y+=1;
		}
		
		switch(type.toUpperCase()) {
		case "GRAPHIC":
			tab=new Tab_Graphic(name,x,y,w,h);
			break;
		case "LIGHTBAR":
			tab=new Tab_Lightbar(name,x,y,w,h);
			break;
		case "CHAT":
			tab=new Tab_Chat(name,x,y,w,h);
			break;
		case "CUSTOM":
			tab=new Window_Tab();
			tab.setProperties(name,x,y,w,h);
			tab.type="custom";
		} 
		
		if(!tab) {
			log(LOG_WARNING, "ERROR: invalid tab type specified");
			return false;
		}
	
		tabs_map[name.toUpperCase()]=tabs.length;
		tabs.push(tab);
		log(LOG_DEBUG, "new tab created: " + name);
		return true;
	}
	
	/* handle window commands */
	this.handle_command=function(cmd) {
		switch(cmd) {
		case KEY_LEFT:
			this.index--;
			if(this.index < 0) this.index=tabs.length-1;
			this.drawTabs();
			this.draw();
			return true;
		case KEY_RIGHT:
			this.index++;
			if(this.index >= tabs.length) this.index=0;
			this.drawTabs();
			this.draw();
			return true;
		default:
			if(tabs[this.index] &&
				typeof tabs[this.index].handle_command == "function")
				return tabs[this.index].handle_command(cmd);
			return false;
		}
	}
	
	/* draw the window and contents of the currently active tab */
	this.drawWindow=function() {
		if(this.show_border) this.drawBorder();
		if(this.show_title) this.drawTitle();
		if(this.show_tabs) this.drawTabs();
		this.draw();
	}
	
	/* draw the tab list for the current window */
	this.drawTabs=function() {
		var tab_str="\1n" + getColor(layout_settings.tbg) + getColor(layout_settings.tfg) + "<";
		for(var t=0;t<tabs.length;t++) {
			tab_str+="\1n" + getColor(layout_settings.tbg) + " ";
			if(t == this.index) {
				tab_str+=getColor(layout_settings.hbg) + getColor(layout_settings.hfg);
			} else {
				switch(tabs[t].status) {
				case -1:
					tab_str+=getColor(layout_settings.tfg);
					break;
				case 0:
					tab_str+=getColor(layout_settings.nfg);
					break;
				case 1:
					tab_str+=getColor(layout_settings.afg);
					break;
				case 2:
					tab_str+=getColor(layout_settings.pfg);
					break;
				}
			}
			tab_str+=tabs[t].name;
		}
		tab_str+="\1n" + getColor(layout_settings.tbg) + getColor(layout_settings.tfg);
		
		var x=this.x;
		var y=this.y;
		var w=this.width;
		
		if(this.show_title) y+=1;
		if(this.show_border) {
			x+=1;
			y+=1;
			w-=2;
		}
		console.gotoxy(x,y);
		console.putmsg(printPadded(tab_str,w-1) + ">",P_SAVEATR);
	}
	
	/* draw the title bar of the current window */
	this.drawTitle=function() {
		var color_str="";
		if(this.window_position == this.layout_position.value) {
			color_str+=getColor(layout_settings.wtbg) 
				+ getColor(layout_settings.wtfg); 
		} else {
			color_str+=getColor(layout_settings.ibg) 
				+ getColor(layout_settings.ifg);
		}
		var x=this.x;
		var y=this.y;
		var w=this.width;
		
		if(this.show_border) {
			x+=1;
			y+=1;
			w-=2;
		}
		
		console.gotoxy(x,y);
		console.putmsg(color_str + centerString(this.name,w),P_SAVEATR);
	}
	
	/* draw the border of the current window */
	this.drawBorder=function() {
		var border_color=getColor(layout_settings.bbg) + getColor(layout_settings.bfg);
		setPosition(this.x,this.y);
		pushMessage(splitPadded(border_color + "\xDA","\xBF",this.width,"\xC4"));
		for(var y=0;y<this.height-2;y++) {
			pushMessage(splitPadded(border_color + "\xB3","\xB3",this.width));
		}
		pushMessage(splitPadded(border_color + "\xC0","\xD9",this.width,"\xC4"));
	}
	
	/* draw the contents of the current window */
	this.draw=function() {
		if(tabs[this.index] &&
			typeof tabs[this.index].draw == "function") {
			tabs[this.index].clear();
			tabs[this.index].draw.apply(this,arguments);
		}
	}

	log(LOG_DEBUG, format("window initialized (x: %i y: %i w: %i h: %i)",this.x,this.y,this.width,this.height));
}
 
/* window tab prototype object */
function Window_Tab()
{
	this.status=-1;

	/* dynamic content and command processor placeholders */
	this.handle_command;
	this.cycle;
	this.draw;
	
	this.clear=function()
	{
		clearBlock(this.x,this.y,this.width,this.height,layout_settings.bg);
	}
	
	this.setProperties=function(name,x,y,w,h)
	{
		this.name=name;
		this.x=x;
		this.y=y;
		this.width=w;
		this.height=h;
	}
}

/* chat tab object extends Window_Tab */
function Tab_Chat(name,x,y,w,h)
{
	if(!chat) {
		load("chateng.js");
		js.global.chat=new ChatEngine();
	}
	this.setProperties(name,x,y,w,h);
	this.type="chat";
	
	this.init=function(chatroom)
	{
		chatroom.init(this.x,this.y,this.width,this.height);
		this.chatroom=chatroom;
	}
}
Tab_Chat.prototype=new Window_Tab;

/* graphic tab object extends Window_Tab */
function Tab_Graphic(name,x,y,w,h)
{
	if(!Graphic) {
		load("graphic.js");
	}
	this.setProperties(name,x,y,w,h);
	this.type="graphic";
	this.graphic=new Graphic(w,h,layout_settings.wbg);
	
	this.putmsg=this.graphic.putmsg;
	this.load=this.graphic.load;
	this.draw=function() {
		this.graphic.draw(this.x,this.y);
	}
}
Tab_Graphic.prototype=new Window_Tab;

/* lightbar tab object extends Window_Tab */
function Tab_Lightbar(name,x,y,w,h)
{
	if(!Lightbar) {
		load("lightbar.js");
	}
	this.setProperties(name,x,y,w,h);
	this.type="lightbar";
	this.load=function(lightbar) {
		//ToDo: add lightbar tab native support
	}
}
Tab_Lightbar.prototype=new Window_Tab;
