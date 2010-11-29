/* $id: $ */

/*
	by Matt Johnson (MCMLXXIX) 2010
	
	the Layout() object is intended as an array of Layout_View() objects.
	the Layout_View() object contains tabs, which contain whatever content 
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
	var views=[];
	var views_map=[];
	var index={ value:0 };
	
	/* public view index */
	this.index getter=function() {
		return index.value;
	}
	
	this.index setter=function(value) {
		if(isNaN(value) || !views[value]) return false;
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
				LIGHTGRAY, /* view foreground */
				BG_BLACK,	/* view background */
				LIGHTRED,	/* menu highlight foreground */
				RED,	/* menu highlight background (use foreground value) */
				YELLOW,	/* view highlight foreground */
				BG_BLUE,	/* view highlight background */
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

	/* add a new view to layout */
	this.add_view=function(name,x,y,w,h) {
		if(!name) {
			log(LOG_WARNING, "ERROR: no view name specified");
			return false;
		}
		if(views_map[name.toUpperCase()] >= 0) {
			log(LOG_WARNING, "ERROR: a view with this name already exists: " + name);
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
			log(LOG_WARNING, "ERROR: invalid view parameters");
			return false;
		} 

		var view=new Layout_View(name,x,y,w,h);
		view.view_position=views.length;
		view.layout_position=index;
		views_map[name.toUpperCase()]=views.length;
		views.push(view);
		return true;
	}
	
	/* layout update loop */
	this.cycle=function() {
		for(var w=0;w<views.length;w++) 
			views[w].cycle();
	}
	
	/* get a view by name */
	this.view=function(name) {
		if(views_map[name.toUpperCase()] >= 0) {
			return views[views_map[name.toUpperCase()]];
		} 
		return false;
	}
	
	/* return current view object */
	this.current_view getter=function() {
		return views[this.index];
	}

	/* set current view object by name */
	this.current_view setter=function(name) {
		if(views_map[name.toUpperCase()] >= 0) {
			this.index=views_map[name.toUpperCase()];
			return true;
		} 
		return false;
	}
	
	/* draw all layout views */
	this.draw=function() {
		for(var w=0;w<views.length;w++)
			views[w].drawView();
	}
	this.redraw=this.draw;
	
	/* handle layout commands/view commands */
	this.handle_command=function(cmd) {
		switch(cmd) {
		case '\x09':	/* CTRL-I TAB */
			var old_index=this.index;
			for(count=0;count<views.length;count++) {
				if(this.index+1>=views.length)
					this.index=0;
				else 
					this.index++;
				if(views[this.index].interactive) break;
			}
			views[old_index].drawTitle();
			views[this.index].drawTitle();
			return true;
		default:
			return views[this.index].handle_command(cmd);
		}
	}

	this.init(theme);
}

/* layout settings object */
function Layout_Settings(vfg,vbg,mhfg,mhbg,vtfg,vtbg,bfg,bbg,tfg,tbg,hfg,hbg,afg,nfg,pfg)
{
	/* main view colors */
	this.vfg=vfg;
	this.vbg=vbg;
	
	/* menu highlights */
	this.mhfg=mhfg;
	this.mhbg=mhbg;
	
	/* title hightlight colors */
	this.vtfg=vtfg;
	this.vtbg=vtbg;

	/* view border color */
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
	
	/* inactive view colors */
	this.ibg=BG_BLACK;
	this.ifg=DARKGRAY;
}

/* tabbed view object */
function Layout_View(name,x,y,w,h)
{
	this.name=name;

	/* private array containing view tabs 
		tabs can be anything that will fit in the view
		such as: chat views, user list, lightbar menus */
	var tabs=[];
	var tabs_map=[];
	var index={ value:0 };
	
	/* private view properties */
	var show_title=true;
	var show_tabs=true;
	var show_border=true;
	var interactive=true;

	/* main view position (upper left corner) */
	this.x=x;
	this.y=y;
	
	/* view dimensions */
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
				tabs[t].cycle();
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
	this.add_tab=function(type,name,data) {
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
		
		/* if a template exists for the specified tab type */
		if(Tab_Templates[type.toUpperCase()]) {
			tab=new Tab_Templates[type.toUpperCase()](name,x,y,w,h);
		}
		/* otherwise create a custom tab */
		else {
			tab=new View_Tab();
			tab.setProperties(name,x,y,w,h);
			tab.type=type;
		}
		
		/* if tab has an initialization method */
		if(typeof tab.init == "function") {
			tab.init(data);
		}
	
		tabs_map[name.toUpperCase()]=tabs.length;
		tabs.push(tab);
		log(LOG_DEBUG, "new tab created: " + name);
		return true;
	}
	
	/* handle view commands */
	this.handle_command=function(cmd) {
		if(tabs.length > 1) {
			switch(cmd) {
			case KEY_LEFT:
				if(this.index-1<0)
					this.index=tabs.length-1;
				else 
					this.index--;
				this.drawTabs();
				this.draw();
				return true;
			case KEY_RIGHT:
				if(this.index+1>=tabs.length)
					this.index=0;
				else 
					this.index++;
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
		else {
			if(tabs[this.index] &&
				typeof tabs[this.index].handle_command == "function")
				return tabs[this.index].handle_command(cmd);
			return false;
		}
	}
	
	/* draw the view and contents of the currently active tab */
	this.drawView=function() {
		if(this.show_border) this.drawBorder();
		if(this.show_title) this.drawTitle();
		if(this.show_tabs) this.drawTabs();
		this.draw();
	}
	
	/* draw the tab list for the current view */
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
	
	/* draw the title bar of the current view */
	this.drawTitle=function() {
		var color_str="";
		if(this.view_position == this.layout_position.value) {
			color_str+=getColor(layout_settings.vtbg) 
				+ getColor(layout_settings.vtfg); 
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
	
	/* draw the border of the current view */
	this.drawBorder=function() {
		var border_color=getColor(layout_settings.bbg) + getColor(layout_settings.bfg);
		setPosition(this.x,this.y);
		pushMessage(splitPadded(border_color + "\xDA","\xBF",this.width,"\xC4"));
		for(var y=0;y<this.height-2;y++) {
			pushMessage(splitPadded(border_color + "\xB3","\xB3",this.width));
		}
		pushMessage(splitPadded(border_color + "\xC0","\xD9",this.width,"\xC4"));
	}
	
	/* draw the contents of the current view */
	this.draw=function() {
		if(tabs[this.index] &&
			typeof tabs[this.index].draw == "function") {
			tabs[this.index].clear();
			tabs[this.index].draw();
		}
	}
	this.redraw=this.draw;

	log(LOG_DEBUG, format("view initialized (x: %i y: %i w: %i h: %i)",this.x,this.y,this.width,this.height));
}
 
/* view tab prototype object */
function View_Tab()
{
	this.status=-1;
	this.hotkeys=false;

	/* dynamic content and command processor placeholders */
	// this.handle_command;
	// this.cycle;
	// this.draw;
	
	this.clear=function()
	{
		clearBlock(this.x,this.y,this.width,this.height,layout_settings.bg);
	}
	this.redraw=function()
	{
		if(typeof this.draw == "function") this.draw();
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

/* pre-defined view tabs */
var Tab_Templates=[];

Tab_Templates["CHAT"]=function(name,x,y,w,h) {
	this.type="chat";
	if(!js.global.Window) {
		load(js.global,"msgwndw.js");
		js.global.chat=new ChatEngine("IRC");
	}
	this.setProperties(name,x,y,w,h);
	this.init=function(window)
	{
		if(window)
			this.window=window;
		else 
			this.window=new Window();
		this.window.init(this.x,this.y,this.width,this.height);
	}
	this.handle_command=function(cmd)
	{
		if(!this.window) return false;
		if(this.window.handle_command(cmd)) this.draw();
	}
	this.draw=function()
	{
		if(!this.window) return false;
		this.window.draw();
	}
}

Tab_Templates["GRAPHIC"]=function(name,x,y,w,h) {
	this.type="graphic";
	this.setProperties(name,x,y,w,h);
	if(!js.global.Graphic) {
		load(js.global,"graphic.js");
	}
	this.graphic=new Graphic(w,h,layout_settings.vbg);
	this.putmsg=this.graphic.putmsg;
	this.load=this.graphic.load;
	this.draw=function() {
		this.graphic.draw(this.x,this.y);
	}
}

Tab_Templates["LIGHTBAR"]=function(name,x,y,w,h) {
	this.type="lightbar";
	this.hotkeys=true;
	if(!js.global.Lightbar) {
		load(js.global,"lightbar.js");
	}
	this.setProperties(name,x,y,w,h);
	this.init=function(lightbar) {
		if(lightbar) { 
			this.lightbar=lightbar;
		}
		else {
			this.lightbar=new Lightbar();
		}
		
		this.lightbar.lpadding="";
		this.lightbar.rpadding="";
		this.lightbar.fg=layout_settings.vfg;
		this.lightbar.bg=layout_settings.vbg;
		this.lightbar.hfg=layout_settings.mhfg;
		this.lightbar.hbg=layout_settings.mhbg;
		this.lightbar.force_width=w;
		this.lightbar.height=h;
		this.lightbar.xpos=x;
		this.lightbar.ypos=y;
	}
	this.handle_command=function(key) {
		return this.lightbar.getval(undefined,key);
	}
	this.draw=function() {
		this.lightbar.draw();
	}
	this.add=function(txt, retval, width, lpadding, rpadding, disabled, nodraw)	{
		this.lightbar.add(txt, retval, width, lpadding, rpadding, disabled, nodraw);
	}
}

Tab_Templates["CLOCK"]=function(name,x,y,w,h) {
	this.type="clock";
	if(!js.global.DigitalClock) {
		load(js.global,"clock.js");
	}
	this.setProperties(name,x,y,w,h);
	this.init=function(clock) {
		if(clock) { 
			this.clock=clock;
		}
		else {
			this.clock=new DigitalClock();
		}
		this.clock.init(this.x,this.y,this.width);
	}
	this.cycle=function() {
		if(!this.clock) return false;
		if(this.clock.update())
			this.clock.draw(this.x,this.y);
	}
	this.draw=function() {
		if(!this.clock) return false;
		this.clock.draw(this.x,this.y);
	}
}

Tab_Templates["CALENDAR"]=function(name,x,y,w,h) {
	this.type="calendar";
	if(!js.global.Calendar) {
		load(js.global,"calendar.js");
	}
	this.setProperties(name,x,y,w,h);
	this.init=function(calendar) {
		if(calendar) { 
			this.calendar=calendar;
			this.calendar.x=this.x;
			this.calendar.y=this.y;
		}
		else {
			this.calendar=new Calendar(this.x,this.y);
		}
		this.name=this.calendar.month.name;
	}
	this.draw=function() {
		if(!this.calendar) return false;
		this.calendar.draw();
	}
	this.handle_command=function(key) {
		if(this.calendar.handle_command(key)) return true;
		return false;
	}
}

for(var tt in Tab_Templates) 
	Tab_Templates[tt].prototype=new View_Tab;
