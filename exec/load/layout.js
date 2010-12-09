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
var tab_settings=new Tab_Settings();

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
	this.handle_command=function(str) {
		cmd=str.split(" ");
		switch(cmd[0].toUpperCase()) {
		case "/CLOSE":
			//ToDo: add window deletion
			break;
		case '\x09':	/* CTRL-I TAB */
			var old_index=this.index;
			for(count=0;count<views.length;count++) {
				if(this.index+1>=views.length)
					this.index=0;
				else 
					this.index++;
				if(this.current_view.interactive) break;
			}
			views[old_index].drawTitle();
			this.current_view.drawTitle();
			break;
		}
		return this.current_view.handle_command(str);
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
	this.update_tabs=true;
	this.update_title=true;

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
		for(var t=0;t<tabs.length;t++) {
			if(typeof tabs[t].cycle == "function") 
				tabs[t].cycle();
			/* if this tab is flagged for an update */
			if(tabs[t].update) {
				/* clear tab update flag */
				tabs[t].update=false;
				/* if this is the current visible tab, draw */
				if(t == this.index) 
					tabs[t].draw();
				/* flag this view for a tab listing update */
				else 
					this.update_tabs=true;
			}
		}
		if(this.show_title && this.update_title) {
			this.drawTitle();
			this.update_title=false;
		}
		if(this.show_tabs && this.update_tabs) {
			this.drawTabs();
			this.update_tabs=false;
		}
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
			tab=new Tab_Templates[type.toUpperCase()](this,name,tabs.length,x,y,w,h);
		}
		/* otherwise create a custom tab */
		else {
			tab=new View_Tab();
			tab.type=type;
		}
		
		/* assign tab properties */
		tab.setProperties(this,name,tabs.length,x,y,w,h);
		
		/* if tab has an initialization method */
		if(typeof tab.init == "function") {
			tab.init(data);
		}
	
		tabs_map[name.toUpperCase()]=tabs.length;
		tabs.push(tab);
		log(LOG_DEBUG, "new tab created: " + name);
		return true;
	}
	
	/* delete a named tab from this window */
	this.del_tab=function(name) {
		var tab_index=tabs_map[name.toUpperCase()];
		tabs.splice(tab_index,1);
		delete tabs_map[name.toUpperCase()];
		for(var t=0;t<tabs.length;t++) {
			tabs_map[tabs[t].name.toUpperCase()]=t;
			tabs[t].view_index=t;
		}
		this.update_tabs=true;
		if(!tabs[this.index])
			this.index=0;
		this.current_tab.update=true;
		log(LOG_DEBUG, "tab deleted: " + name);
		return true;
	}
	
	/* handle view commands */
	this.handle_command=function(str) {
		cmd=str.split(" ");
		if(tabs.length > 1) {
			switch(cmd[0].toUpperCase()) {
			case "/CLOSE":
				var name=this.current_tab.name.toUpperCase();
				if(cmd[1]) {
					name=cmd[1].toUpperCase();
					if(!(tabs_map[name] >= 0)) {
						log(LOG_WARNING, "ERROR: no tab with this name exists: " + cmd[1]);
						return false;
					} 
				}
				if(tabs[tabs_map[name]].fixed) {
					log(LOG_WARNING, "ERROR: cannot delete this tab");
					return false;
				}
				if(typeof tabs[this.index].handle_command == "function")
					tabs[this.index].handle_command(str);
				this.del_tab(name);
				break;
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
			}
		} 
		if(tabs[this.index] &&
			typeof tabs[this.index].handle_command == "function")
			return tabs[this.index].handle_command(str);
		return false;
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
		var x=this.x;
		var y=this.y;
		var w=this.width;
		
		if(this.show_title) y+=1;
		if(this.show_border) {
			x+=1;
			y+=1;
			w-=2;
		}
		
		var max_width=w-2;
		var norm_bg="\1n" + getColor(layout_settings.tbg);
		var left_arrow=getColor(layout_settings.tfg) + "<";
		var right_arrow=getColor(layout_settings.tfg) + ">";
		
		/* populate string with current tab highlight */
		var tab_str=
			getColor(layout_settings.hbg) + 
			getColor(layout_settings.hfg) +
			tabs[this.index].name +
			norm_bg;
		/* 	if the string is shorter than the current window's width,
			continue adding tabs until it is not, starting to the left */
		var t=this.index-1;
		while(t >= 0 && console.strlen(tab_str) < max_width) {
			if(console.strlen(tab_str)+tabs[t].name.length+1 > max_width) 
				break;
			var color=norm_bg;
			switch(tabs[t].status) {
			case tab_settings.INACTIVE:
				color+=getColor(layout_settings.tfg);
				break;
			case tab_settings.DEFAULT:
				color+=getColor(layout_settings.tfg);
				break;
			case tab_settings.NOTICE:
				color+=getColor(layout_settings.nfg);
				break;
			case tab_settings.ALERT:
				color+=getColor(layout_settings.afg);
				break;
			case tab_settings.PRIVATE:
				color+=getColor(layout_settings.pfg);
				break;
			}
			tab_str=
				color + 
				tabs[t].name + 
				norm_bg + 
				" " +
				tab_str;
			t--;
		}
		if(t >= 0) left_arrow="\1r\1h<";
		/* 	if the string is still shorter than the width, 
			begin adding tabs to the right */
		t=this.index+1;
		while(t < tabs.length && console.strlen(tab_str) < max_width) {
			if(console.strlen(tab_str)+tabs[t].name.length+1 > max_width) 
				break;
			var color="";
			switch(tabs[t].status) {
			case tab_settings.INACTIVE:
				color+=getColor(layout_settings.tfg);
				break;
			case tab_settings.DEFAULT:
				color+=getColor(layout_settings.tfg);
				break;
			case tab_settings.NOTICE:
				color+=getColor(layout_settings.nfg);
				break;
			case tab_settings.ALERT:
				color+=getColor(layout_settings.afg);
				break;
			case tab_settings.PRIVATE:
				color+=getColor(layout_settings.pfg);
				break;
			}
			tab_str=
				tab_str + 
				norm_bg +
				" " +
				color + 
				tabs[t].name;
			t++;
		}
		if(t < tabs.length) right_arrow="\1r\1h>";
		
		console.gotoxy(x,y);
		console.putmsg(
			norm_bg + 
			left_arrow + 
			printPadded(tab_str,max_width) + 
			right_arrow
			,P_SAVEATR);
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

	log(LOG_DEBUG, 
		format("view initialized (x: %i y: %i w: %i h: %i)"
		,this.x,this.y,this.width,this.height));
}

/* tab status values */
function Tab_Settings()
{
	this.INACTIVE=-1;
	this.DEFAULT=0;
	this.NOTICE=1;
	this.ALERT=2;
	this.PRIVATE=3;
}
 
/* view tab prototype object */
function View_Tab()
{
	this.status=0;
	this.fixed=false;
	this.hotkeys=false;
	this.update=false;

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
	this.setProperties=function(parent_view,name,view_index,x,y,w,h)
	{
		this.parent_view=parent_view;
		this.name=name;
		this.view_index=view_index;
		this.x=x;
		this.y=y;
		this.width=w;
		this.height=h;
	}
}

/* pre-defined view tabs */
var Tab_Templates=[];

Tab_Templates["CHAT"]=function() 
{
	this.type="chat";
	if(!js.global.Window) {
		load(js.global,"msgwndw.js");
	}
	
	this.init=function(chat) {
		this.chat=chat;
		this.channel=this.chat.channels[this.name.toUpperCase()];
		this.window=new Window();
		this.window.init(this.x,this.y,this.width,this.height);
		this.channel.window=this.window;
		/* override chat channel post method to put messages in a graphic window */
		this.channel.post=function(msg) {
			this.window.post(msg);
		}
		/* move any waiting messages to window */
		while(this.channel.message_list.length > 0) 
			this.window.post(this.channel.message_list.shift());
	}
	this.update getter=function() {
		return this.window.update;
	}
	this.update setter=function(update) {
		this.window.update=update;
	}
	this.cycle=function() {
		/* 	if this tab is not the current view tab,
			mark this tab for a user list update */
		if(!this.channel.update_users && this.parent_view.index != this.view_index) {
			this.channel.update_users=true;
		}
	}
	this.handle_command=function(cmd) {
		switch(cmd) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_HOME:
		case KEY_END:
			/* pass all window-navigation commands to the window object command handler */
			return this.window.handle_command(cmd);
			break;
		default:
			/* pass any other commands to the chat command handler */
			return this.chat.handle_command(this.name,cmd);
			break;
		}
	}
	this.draw=function() {
		this.window.draw();
	}
}

Tab_Templates["USERLIST"]=function() 
{
	this.type="userlist";
	
	this.init=function(chat_view) {
		this.chat_view=chat_view;
		this.window=new Window();
		this.window.init(this.x,this.y,this.width,this.height);
	}
	this.cycle=function() {
		if(!this.chat_view.current_tab) {
			this.clear();
			this.update=false;
			return;
		}
		
		var chat=this.chat_view.current_tab.chat;
		var channel=chat.channels[this.chat_view.current_tab.name.toUpperCase()];
		
		if(channel && channel.update_users) {
			channel.update_users=false;
			
			var num_users=countMembers(channel.users);
			this.window.clear();
			if(num_users > 0) {
				this.window.init(this.x,this.y,this.width,this.height);
				for each(var u in channel.users) {
					var str="";
					for(var m=0;m<u.modes.length;m++) 
						str+=u.modes[m];
					str+=u.nick;
					this.window.post(str);
				}
			}
		}
		this.update=this.window.update;
	}
	this.draw=function() {
		this.window.draw();
	}
}

Tab_Templates["GRAPHIC"]=function() 
{
	this.type="graphic";
	if(!js.global.Graphic) {
		load(js.global,"graphic.js");
	}
	
	this.init=function(graphic) {
		if(graphic)
			this.graphic=graphic;
		else 
			this.graphic=new Graphic(this.width,this.height,layout_settings.vbg);
	}
	this.putmsg=function(xpos, ypos, txt, attr, scroll) {
		this.graphic.putmsg(xpos, ypos, txt, attr, scroll);
	}
	this.load=function(file) {
		this.graphic.load(file);
	}
	this.draw=function(xoff,yoff,delay) {
		this.graphic.draw(this.x,this.y,this.width,this.height,xoff,yoff,delay);
	}
}

Tab_Templates["LIGHTBAR"]=function() 
{
	this.type="lightbar";
	this.hotkeys=true;
	if(!js.global.Lightbar) {
		load(js.global,"lightbar.js");
	}
	
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
		this.lightbar.force_width=this.width;
		this.lightbar.height=this.height;
		this.lightbar.xpos=this.x;
		this.lightbar.ypos=this.y;
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

Tab_Templates["CLOCK"]=function() 
{
	this.type="clock";
	if(!js.global.DigitalClock) {
		load(js.global,"clock.js");
	}
	
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
		this.update=this.clock.update();
	}
	this.draw=function() {
		this.clock.draw(this.x,this.y);
	}
}

Tab_Templates["CALENDAR"]=function() 
{
	this.type="calendar";
	if(!js.global.Calendar) {
		load(js.global,"calendar.js");
	}
	
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
		this.calendar.draw();
	}
	this.handle_command=function(key) {
		return this.calendar.handle_command(key);
	}
}

for(var tt in Tab_Templates) 
	Tab_Templates[tt].prototype=new View_Tab;
