/* $Id$ */
/* Window-style layout library for Synchronet 3.15+ 
 * 
 * NOTE: frame.js is required to use this library
 * if a Frame() object is not supplied to a Layout()
 * on creation, one will be created internally, however
 * it is recommended that you create an external frame object
 * as a basis for any other libraries that use frames.
 * 
 * NOTE: There is no real need to manually draw the contents of a layout
 * as long as either Frame.cycle() or Layout.cycle() are part of your
 * loop. If you use "blocking" operations with a layout or frame, 
 * then use Layout.draw() or Frame.draw() to update your views.
 * 
 * Layout methods:
 * 
 * 	addView(title,x,y,width,height,frame) 
 * 	getViewByTitle(title)
 * 	draw()
 * 	cycle()
 * 	getcmd(cmd)
 * 
 * Layout properties:
 * 
 * 	x
 * 	y
 * 	width
 * 	height
 * 	frame
 * 	colors = {
 * 		border_bg,
 * 		border_fg,
 * 		title_bg,
 *		title_fg, 
 *		tab_bg,
 *		tab_fg,
 *		view_bg,
 *		view_fg
 *  }
 * 
 * LayoutView methods:
 * 
 * 	addTab(title,frame)
 * 	getTabByTitle(title)
 * 	draw()
 * 	cycle()
 * 	getcmd(cmd)
 * 
 * LayoutView properties:
 * 
 * 	x
 * 	y
 * 	width
 * 	height
 * 	title
 * 	frame
 * 	colors (a reference to the parent layout's "colors" property)
 *  
 * LayoutView toggles: 
 * 
 * NOTE: changing these values will resize and reinitialize 
 * the view's components
 * 
 * 	show_border = (true|false)
 * 	show_title = (true|false)
 * 	show_tabs = (true|false)
 * 
 * 
 * 
 */

/* main layout object, intended to contain child layout view objects */ 
function Layout(frame) {
	
	/* private properties */
	var properties={
		views:[],
		index:0
	};
	var colors={
		title_bg:BG_BLUE,
		title_fg:YELLOW,
		tab_bg:BG_LIGHTGRAY,
		tab_fg:BLACK,
		view_bg:BG_BLACK,
		view_fg:GREEN,
		border_bg:BG_BLACK,
		border_fg:LIGHTGRAY
	};
	var frames={
		main:undefined
	}
	
	/* read-only properties */
	this.__defineGetter__("x",function() {
		return frames.main.x;
	});
	this.__defineGetter__("y",function() {
		return frames.main.y;
	});
	this.__defineGetter__("width",function() {
		return frames.main.width;
	});
	this.__defineGetter__("height",function() {
		return frames.main.height;
	});
	this.__defineGetter__("colors",function() {
		return colors;
	});
	this.__defineGetter__("frame",function() {
		return frames.main;
	});
	
	/* settings */
	this.__defineGetter__("current_view",function() {
		return properties.views[properties.index];
	});
	this.__defineSetter__("current_view",function(view) {
		//Todo
	});

	/* public methods */
	this.addView=function(title,x,y,w,h) {
		var f = new Frame(x,y,w,h,undefined,frames.main);
		var v = new LayoutView(title,f,this);
		properties.views.push(v);
		return v;
	}
	this.draw=function() {
		for each(var view in properties.views)
			view.draw();
	}
	this.cycle=function() {
		return frames.main.cycle();
	}
	this.getViewByTitle=function(title) {
		for each(var v in properties.views) {
			if(v.title.toUpperCase() == title.toUpperCase())
				return v;
		}
	}
	
	/* default command handler for views */
	this.getcmd=function(cmd) {
		if(!cmd) 
			return false;
		switch(cmd.toUpperCase()) {
		case "\t":
			properties.index++;
			if(properties.index >= properties.views.length)
				properties.index = 0;
			return true;
		default:
			if(properties.views.length > 0)
				return properties.views[properties.index].handle_command(cmd);
			break;
		}
		return false;
	}
	
	/* constructor */
	function init(frame) {
		if(frame instanceof Frame)
			frames.main = frame;
		else
			frames.main = new Frame();
	}
	init.call(this,frame);
}

/* layout view object, holds view tab objects
 * NOTE: this can be instantiated independently from Layout, 
 * but must be supplied a frame with the desired dimensions,
 * normally instantiated via Layout.addView() */
function LayoutView(title,frame,parent) {
	/* private properties */
	var properties={
		title:undefined,
		index:0,
		tabs:[]
	}
	var relations={
		parent:undefined
	}
	var settings={
		show_title:true,
		show_tabs:true,
		show_border:true
	}
	var frames={
		main:undefined,
		border:undefined,
		title:undefined,
		tabs:undefined,
		content:undefined
	};
	
	/* read-only properties */
	this.__defineGetter__("x",function() {
		return frames.main.x;
	});
	this.__defineGetter__("y",function() {
		return frames.main.y;
	});
	this.__defineGetter__("width",function() {
		return frames.main.width;
	});
	this.__defineGetter__("height",function() {
		return frames.main.height;
	});
	this.__defineGetter__("colors",function() {
		return relations.parent.colors;
	});
	this.__defineGetter__("title",function() {
		return properties.title;
	});
	this.__defineGetter__("frame",function() {
		return frames.main;
	});
	
	/* settings */
	this.__defineGetter__("show_title",function() {
		return settings.show_title;
	});
	this.__defineSetter__("show_title",function(bool) {
		if(typeof bool !== "boolean")
			return false;
		settings.show_title = bool;
		updateViewFrames();
		return true;
	});
	this.__defineGetter__("show_tabs",function() {
		return settings.show_tabs;
	});
	this.__defineSetter__("show_tabs",function(bool) {
		if(typeof bool !== "boolean")
			return false;
		settings.show_tabs = bool;
		updateViewFrames();
		return true;
	});
	this.__defineGetter__("show_border",function() {
		return settings.show_border;
	});
	this.__defineSetter__("show_border",function(bool) {
		if(typeof bool !== "boolean")
			return false;
		settings.show_border = bool;
		updateViewFrames();
		return true;
	});
	this.__defineGetter__("current_tab",function() {
		return properties.tabs[properties.index];
	});
	this.__defineSetter__("current_tab",function(tab) {
		//Todo
	});
	
	/* public methods */
	this.draw=function() {
		frames.main.draw();
	}
	this.cycle=function() {
		for each(var t in properties.tabs)
			if(typeof t.cycle == "function")
				t.cycle();
		return frames.main.cycle();
	}
	this.addTab=function(title) {
		/* use this view's location and dimensions as 
		starting point for new tabs */
		var x = frames.content.x;
		var y = frames.content.y;
		var w = frames.content.width;
		var h = frames.content.height;
		var attr = this.colors.view_bg + this.colors.view_fg;
		var f = new Frame(x,y,w,h,attr,frames.content);
		var t = new ViewTab(title,f,this);
		properties.tabs.push(t);
		setTabs();
		return t;
	}
	this.getTabByTitle=function(title) {
		for each(var t in properties.tabs) {
			if(t.title.toUpperCase() == title.toUpperCase())
				return t;
		}
	}
	this.getcmd=function(cmd) {
		if(!cmd) 
			return false;
		var old_index = properties.index;
		var handled = false;
		switch(cmd.toUpperCase()) {
		case KEY_LEFT:
			properties.index--;
			handled = true;
			break;
		case KEY_RIGHT:
			properties.index++;
			handled = true;
			break;
		default:
			if(properties.tabs.length > 0)
				return properties.tabs[properties.index].handle_command(cmd);
			break;
		}
		if(properties.index >= properties.tabs.length)
			properties.index = 0;
		else if(properties.index < 0)
			properties.index = properties.tabs.length-1;
		if(properties.index != old_index)
			setTabs();
	}
	
	/* private methods */
	function updateViewFrames() {
		var x = frames.main.x;
		var y = frames.main.y;
		var w = frames.main.width;
		var h = frames.main.height;
		var colors = relations.parent.colors;
		
		/* delete all existing content frames */
		if(frames.border)
			frames.border.delete();
		if(frames.title) 
			frames.title.delete();
		if(frames.tabs) 
			frames.tabs.delete();
		if(frames.content) 
			frames.content.delete();
		/* recreate any frames set to be displayed */
		if(settings.show_border) {
			var attr = colors.border_bg + colors.border_fg;
			frames.border = new Frame(x,y,w,h,attr,frames.main);
			setBorder();
			x++;
			y++;
			w-=2;
			h-=2;
		}
		if(settings.show_title) {
			var attr = colors.title_bg + colors.title_fg;
			frames.title = new Frame(x,y,w,1,attr,frames.main);
			setTitle();
			y++;
			h--;
		}
		if(settings.show_tabs) {
			var attr = colors.tab_bg + colors.tab_fg;
			frames.tabs = new Frame(x,y,w,1,attr,frames.main);
			setTabs();
			y++;
			h--;
		}

		frames.content = new Frame(x,y,w,h,undefined,frames.main); 
		frames.main.open();
	}
	function setBorder() {
		var f = frames.border;
		var h = "\xC4";
		var v = "\xB3";
		var tl = "\xDA";
		var tr = "\xBF";
		var bl = "\xC0";
		var br = "\xD9";
		var hline = "";
		for(var x=0;x<frames.main.width-2;x++)
			hline+=h;
		var vline = format(v + "%*s" + v,frames.main.width-2,"");
		f.home();
		f.putmsg(tl + hline + tr + "\r\n");
		for(var y=0;y<frames.main.height-2;y++)
			f.putmsg(vline + "\r\n");
		f.putmsg(bl + hline + br);
	}
	function setTabs() {
		var f = frames.tabs;
		var w = f.width;
		var max_width=w-2;
		var la="<";
		var ra=">";

		/* populate string with current tab highlight */
		var tab_str="";
		if(properties.tabs.length > 0) {
			tab_str+=properties.tabs[properties.index].title;
			var t=properties.index-1;
			while(t >= 0 && console.strlen(tab_str) < max_width) {
				if(console.strlen(tab_str)+properties.tabs[t].title.length+1 > max_width) 
					break;
				tab_str=properties.tabs[t].title + " " + tab_str;
				t--;
			}
			if(t >= 0) 
				la = "\1r\1h<";
			t=properties.index+1;
			while(t < properties.tabs.length && console.strlen(tab_str) < max_width) {
				if(console.strlen(tab_str)+properties.tabs[t].title.length+1 > max_width) 
					break;
				tab_str=
					tab_str + " " +	properties.tabs[t].title;
				t++;
			}
			if(t < properties.tabs.length) 
				ra = "\1r\1h>";
		}
		f.home();
		f.putmsg(la + format("%-*s",max_width,tab_str) + ra);
	}
	function setTitle() {
		var f = frames.title;
		f.home();
		f.center(properties.title);
	}
	function setContent() {
		//var f = frames.content;
		//f.home();
		//ToDo: resize content?
	}
	
	/* constructor */
	function init(title,frame,parent) {
		properties.title = title;
		if(frame instanceof Frame)
			frames.main = frame;
		else
			frames.main = new Frame();
		relations.parent = parent;
		updateViewFrames();
	}
	init.call(this,title,frame,parent);
}

/* view tab object, meant to inhabit a layout view.
 * will generally occupy the same space as other view tabs
 * within a given view, cannot be effectively instantiated
 * on its own, but rather through ViewFrame.addTab() */
function ViewTab(title,frame,parent) {
	/* private properties */
	var properties={
		title:undefined
	}
	var frames={
		main:undefined
	}
	var relations={
		parent:undefined
	}

	/* read-only properties */
	this.__defineGetter__("x",function() {
		return frames.main.x;
	});
	this.__defineGetter__("y",function() {
		return frames.main.y;
	});
	this.__defineGetter__("width",function() {
		return frames.main.width;
	});
	this.__defineGetter__("height",function() {
		return frames.main.height;
	});
	this.__defineGetter__("colors",function() {
		return relations.parent.colors;
	});
	this.__defineGetter__("title",function() {
		return properties.title;
	});
	this.__defineGetter__("frame",function() {
		return frames.main;
	});
	
	/* default command handler (can be overridden for specialized tabs) */
	this.getcmd = function(cmd) {
		if(!cmd)
			return false;
		frames.main.putmsg(cmd);
		return true;
	}
	
	/* constructor */
	function init(title,frame,parent) {
		properties.title = title;
		frames.main = frame;
		frames.main.open();
		relations.parent = parent;
	}
	init.call(this,title,frame,parent);
}