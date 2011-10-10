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
		
	usage example:
	
		var layout=new Layout();
		
	
*/

load("funclib.js");
load("sbbsdefs.js");

/* layout color settings */
var layout_settings={
	vfg:	LIGHTGRAY, 	/* view foreground */
	vbg:	BG_BLACK,	/* view background */
	lfg:	LIGHTCYAN,	/* menu highlight foreground */
	lbg:	BG_BLUE,	/* menu highlight background */
	vtfg:	YELLOW,		/* view highlight foreground */
	vtbg:	BG_BLUE,	/* view highlight background */
	bfg:	LIGHTGRAY,	/* border foreground color */
	bbg:	BG_BLACK,	/* border background color */
	tfg:	BLACK,		/* tab foreground (normal) */
	tbg:	BG_LIGHTGRAY, /* tab background */
	hfg:	LIGHTCYAN,	/* tab highlight foreground */	
	hbg:	BG_CYAN,	/* tab highlight background */
	afg:	RED,		/* tab alert foregruond */
	nfg:	MAGENTA,	/* tab update foreground */
	pfg:	BLUE,		/* tab user msg foreground */
	cfg:	LIGHTGRAY,	/* clock foreground */
	cbg:	BG_BLUE,	/* clock background */
	ifg:	DARKGRAY,	/* inactive view foreground */
	ibg:	BG_BLACK	/* inactive view background */
};

/* tab status settings */
var tab_settings={
	INACTIVE:-1,
	DEFAULT:0,
	NOTICE:1,
	ALERT:2,
	PRIVATE:3
};

/* main layout object */
function Layout() 
{
	/* private properties */
	var views=[];
	var views_map=[];
	var index={ value:0 };
	
	/* public view index */
	this.__defineGetter__("index", function() {
		return index.value;
	});
	
	this.__defineSetter__("index", function(value) {
		if(isNaN(value) || !views[value]) return false;
		index.value=value;
		return true;
	});
	
	/* initialize layout */
	this.init=function() {
		this.load();
	}

	/* load layout settings */
	this.load=function() {
		var settings_file=new File(
			system.data_dir + "user/" +
			printPadded(user.number,4,"0","right") + 
			".layout.ini"
		);
		if(file_exists(settings_file.name)) {
			settings_file.open('r',true);
			
			if(!settings_file.is_open) {
				log(LOG_WARNING,"error loading user layout settings");
			}
			else {
				var set=settings_file.iniGetObject("settings");
				settings_file.close();
				for(var s in set) 
					layout_settings[s]=set[s];
				log(LOG_DEBUG,"loaded user layout settings");
			}
		}
	}
	
	/* save layout settings */
	this.save=function() {
		for(var s in layout_settings) 
			this.save_settings(s,layout_settings[s]);
	}
	
	/* save individual layout setting */
	this.save_setting=function(setting,value) {
		var settings_file=new File(
			system.data_dir + "user/" +
			printPadded(user.number,4,"0","right") + 
			".layout.ini"
		);
		settings_file.open((file_exists(settings_file.name)?'r+':'w+'),true);
		if(!settings_file.is_open) {
			log("error opening user layout settings file",LOG_WARNING);
			return;
		}
		settings_file.iniSetValue("settings",setting,value);
		settings_file.close();
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
			log(LOG_WARNING, format("x:%s y:%s w:%s h:%s",x,y,w,h));
			return false;
		} 

		var view=new Layout_View(name,x,y,w,h);
		view.view_position=views.length;
		view.layout_position=index;
		views_map[name.toUpperCase()]=views.length;
		views.push(view);
		return true;
	}
	
	/* delete a named view from this layout */
	this.del_view=function(name) {
		var view_index=views_map[name.toUpperCase()];
		views.splice(view_index,1);
		delete views_map[name.toUpperCase()];
		for(var v=0;v<views.length;v++) {
			views_map[views[v].name.toUpperCase()]=v;
			views[v].view_position=v;
		}
		if(!views[this.index])
			this.index=0;
		log(LOG_DEBUG, "view deleted: " + name);
		this.draw();
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
	this.__defineGetter__("current_view", function() {
		return views[this.index];
	});

	/* set current view object by name */
	this.__defineSetter__("current_view", function(name) {
		if(views_map[name.toUpperCase()] >= 0) {
			this.index=views_map[name.toUpperCase()];
			return true;
		} 
		return false;
	});
	
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
		case '\x09':	/* CTRL-I TAB */
			var old_index=this.index;
			for(count=0;count<views.length;count++) {
				if(this.index+1>=views.length)
					this.index=0;
				else 
					this.index++;
				if(this.current_view.interactive) break;
			}
			if(old_index != this.index) {
				if(views[old_index].expire) 
					this.del_view(views[old_index].name);
				else {
					if(views[old_index].show_title)
						views[old_index].drawTitle();
				}
				if(views[this.index].show_title)
					this.current_view.drawTitle();
			}
			return cmd[0];
		}
		return this.current_view.handle_command(str);
	}
	this.init();
}

/* tabbed view object */
function Layout_View(name,x,y,w,h)
{
	this.name=name;
	this.update_tabs=true;
	this.update_title=true;
	this.expire=false;

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
	
	this.view_position=undefined;
	this.layout_position=undefined;
		
	/* public tab index */
	this.__defineGetter__("index", function() {
		return index.value;
	});
	
	this.__defineSetter__("index", function(value) {
		if(isNaN(value) || !tabs[value]) return false;
		index.value=value;
		return true;
	});
	
	/* return a list of tab names */
	this.__defineGetter__("tabs", function() {
		var list=[];
		for each(var t in tabs) {
			list.push(t.name);
		}
		return list;
	});
	
	/* update loop */
	this.cycle=function() {
		for(var t in tabs) {
			if(typeof tabs[t].cycle == "function") 
				tabs[t].cycle();
			/* verify tab existance in case tab	self-deleted */
			if(!tabs[t])
				continue;
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
	this.__defineSetter__("show_title", function(bool) {
		if(tabs.length > 0) return false;
		show_title=bool;
		return true;
	});

	this.__defineSetter__("show_tabs", function(bool) {
		if(tabs.length > 0) return false;
		show_tabs=bool;
		return true;
	});

	this.__defineSetter__("show_border", function(bool) {
		if(tabs.length > 0) return false;
		show_border=bool;
		return true;
	});

	this.__defineSetter__("interactive", function(bool) {
		if(tabs.length > 0) return false;
		interactive=bool;
		return true;
	});
		
	/* get display options */
	this.__defineGetter__("show_border", function() {
		return show_border;
	});

	this.__defineGetter__("show_title", function() {
		return show_title;
	});
	
	this.__defineGetter__("show_tabs", function() {
		return show_tabs;
	});

	this.__defineGetter__("interactive", function() {
		return interactive;
	});
	
	this.clear=function() {
		clearBlock(this.x,this.y,this.width,this.height);
	}
	
	/* return current tab object */
	this.__defineGetter__("current_tab", function() {
		return tabs[this.index];
	});
	
	/* set current tab object */
	this.__defineSetter__("current_tab", function(name) {
		if(tabs_map[name.toUpperCase()] >= 0) {
			this.index=tabs_map[name.toUpperCase()];
			return true;
		} 
		return false;
	});
	
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
		this.update_tabs=true;
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
				tabs[this.index].status=tab_settings.DEFAULT;
				this.drawTabs();
				this.draw();
				return KEY_LEFT;
			case KEY_RIGHT:
				if(this.index+1>=tabs.length)
					this.index=0;
				else 
					this.index++;
				tabs[this.index].status=tab_settings.DEFAULT;
				this.drawTabs();
				this.draw();
				return KEY_RIGHT;
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
		var tab_str="";
		if(tabs.length > 0) {
			tab_str+=
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
		}
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
		if(this.layout_position == undefined || (this.view_position == this.layout_position.value)) {
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
		var border_color="\1n" + getColor(layout_settings.bbg) + getColor(layout_settings.bfg);
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

/* view tab prototype object */
function View_Tab()
{
	this.status=0;
	this.fixed=false;
	this.hotkeys=false;
	this.update=true;

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
		//this.parent_view=parent_view;
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
		this.window=new Window();
		this.window.init(this.x,this.y,this.width,this.height);
	}
	this.__defineGetter__("update", function() {
		return this.window.update;
	});
	this.__defineSetter__("update", function(update) {
		this.window.update=update;
	});
	this.cycle=function() {
		/* verify local channel cache */
		if(!this.chat.channels[this.name.toUpperCase()]) {
			this.parent_view.del_tab(this.name);
			return;
		}	

		this.channel=this.chat.channels[this.name.toUpperCase()];
		/* move any waiting messages to window */
		while(this.channel.messages.length > 0) {
			new_messages = true;
			var msg = this.channel.messages.shift();
			var str = 
				getColor(this.chat.settings.NICK_COLOR) + msg.nick.name + 
				getColor(this.chat.settings.TEXT_COLOR) + ": " + msg.str;
			this.window.post(str);
		}
	}
	this.handle_command=function(cmd) {
		switch(cmd[0]) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_HOME:
		case KEY_END:
			/* pass all window-navigation commands to the window object command handler */
			return this.window.handle_command(cmd);
		/* process a user command */
		case '/':
			return this.chat.handle_command(this.name,cmd);
		/* process all other input */
		default:
			/* send a message to the current channel */
			return this.chat.submit(this.name,cmd);
		}
	}
	this.draw=function() {
		this.window.draw();
	}
}

Tab_Templates["GRAPHIC"]=function() 
{
	this.type="graphic";
	this.hotkeys=true;
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
		/* if this is the initial lightbar, set top level */
		if(!this.lightbar) 
			lightbar.top=true;
		/* if we are stacking menus and the previous lightbar is not top level */
		else if(!lightbar.previous && !lightbar.top)
			lightbar.previous=this.lightbar;
		/* if we are supplied a lightbar, make it fit */
		if(lightbar)  
			this.lightbar=lightbar;
		/* otherwise create a blank lightbar */
		else
			this.lightbar=new Lightbar();
		
		this.lightbar.lpadding="";
		this.lightbar.rpadding="";
		this.lightbar.force_width=this.width;
		this.lightbar.height=this.height;
		this.lightbar.xpos=this.x;
		this.lightbar.ypos=this.y;
		
		this.update=true;
	}
	this.handle_command=function(key) {
		key=this.lightbar.getval(undefined,key);
		if( (key == "\b" || key == "\x7f") && this.lightbar.previous)
			this.init(this.lightbar.previous);
		return key;
	}
	this.draw=function() {
		this.lightbar.fg=layout_settings.vfg;
		this.lightbar.bg=layout_settings.vbg>>4;
		this.lightbar.lfg=layout_settings.lfg;
		this.lightbar.lbg=layout_settings.lbg>>4;
		this.lightbar.draw();
		if(this.lightbar.node_action)
			bbs.node_action=this.lightbar.node_action;
	}
	this.add=function(txt, retval, width, lpadding, rpadding, disabled, nodraw)	{
		this.lightbar.add(txt, retval, width, lpadding, rpadding, disabled, nodraw);
	}
}

Tab_Templates["TREE"]=function() 
{
	this.type="tree";
	this.hotkeys=true;
	
	if(!js.global.Tree) {
		load(js.global,"tree.js");
	}
	
	this.init=function(tree) {
		/* if we are supplied a tree, make it fit */
		if(tree) { 
			this.tree=tree;
			this.tree.x=this.x;
			this.tree.y=this.y;
			this.tree.width=this.width;
			this.tree.height=this.height;
			this.tree.open=true;
			this.tree.index=0;
		}
		/* otherwise create a blank lightbar */
		else
			this.tree=new Tree(this.x,this.y,this.width,this.height);
		this.update=true;
	}
	this.addItem = function() {
		return this.tree.addItem.apply(this.tree,arguments);
	}
	this.addTree = function() {
		return this.tree.addTree.apply(this.tree,arguments);
	}
	this.handle_command=function(key) {
		return this.tree.handle_command(key);
	}
	this.draw=function() {
		tree_settings.fg=layout_settings.vfg;
		tree_settings.bg=layout_settings.vbg;
		tree_settings.lfg=layout_settings.lfg;
		tree_settings.lbg=layout_settings.lbg;
		this.tree.draw();
		if(this.tree.node_action)
			bbs.node_action=this.tree.node_action;
	}
}

Tab_Templates["CLOCK"]=function() 
{
	this.type="clock";
	this.hotkeys=true;
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
		this.clock.fg=layout_settings.cfg;
		this.clock.bg=layout_settings.cbg;
		this.clock.draw(this.x,this.y);
	}
}

Tab_Templates["CALENDAR"]=function() 
{
	this.type="calendar";
	this.hotkeys=true;
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

/* use this function to add/remove tabs
from a view automatically  */
function updateChatView(chat,view) 
{
	/* synchronize channel list with chat object */
	for(var c in chat.channels) {
		var chan = chat.channels[c];
		/* if we do not have a tab matching a joined channel, 
		add a new tab for that channel */
		if(!view.tab(chan.name)) 
			view.add_tab("chat",chan.name,chat);
	}
}
