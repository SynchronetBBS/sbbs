/*	$id: $ 

	Lightbar Menu Tree 
	by Matt Johnson (MCMLXXIX) 2012
	
	properties:
	
		frame
		items
		parent
		text
		status
		colors
		hash
		depth
		index
		current
		currentItem
		currentTree
	
	methods:
	
		cycle()										//cycle frame object 
		getcmd(cmd)									//handle user input
		addTree(text)								//add a subtree to an existing tree
		addItem(text,[func|retval],[arg1,arg2..])	//add an item to the tree
		deleteItem(index)							//remove an item/subtree
		open()										//expand tree
		close()										//collapse tree
		show()										//show hidden tree
		hide()										//hide tree
		enable()									//enable a disabled item
		disable()									//disable an item
		trace()										//locate a tree item based on hash record 
		refresh()									//reload tree data into frame
	
	usage example:
	
	// create a new frame in the top left corner; 40 columns 40 rows
	var frame = new Frame(1,1,40,40);
	
	// use the frame as a place to load a menu tree
	var tree=new Tree(frame,"My Menu");
	
	// add an item with a hotkey of "i" and a return value of 1
	tree.addItem("|item one",1);
	
	// add an item with no hotkey that takes 4 arguments in its function
	tree.addItem("item two",myFunc,"arg1","arg2","arg3","arg4");

	// create a subtree with a hotkey of "m"
	var subtree=tree.addTree("|menu title");
	
	// add items to the subtree the same as before
	subtree.addItem("|logoff",bbs.hangup);
	
	// open frame canvas and load tree data into it
	frame.open();
	tree.open();
	
	// display the frame contents
	frame.draw();
	
*/

load("funclib.js");
function Tree(frame,text,tree) {

	/* private properties */
	var flags = {
		CLOSED:(1<<0),
		HIDDEN:(1<<1),
		DISABLED:(1<<2)
	};
	var properties = {
		frame:undefined,
		parent:undefined,
		status:flags.CLOSED,
		index:0,
		text:"",
		line:1,
		attr:undefined,
		list:[],
		items:[]
	};
	var values = {
		NOT_HANDLED:false,
		HANDLED:true
	}
	var commands = {
		DOWN:KEY_DOWN,
		UP:KEY_UP,
		HOME:KEY_HOME,
		END:KEY_END,
		PGDN:"]",
		PGUP:"[",
		SELECT:"\r",
		DELETE:"\x7f"
	}
		
	/* protected properties */
	this.__defineGetter__("frame",function() {
		if(properties.parent)
			return properties.parent.frame;
		return properties.frame;
	});
	this.__defineSetter__("frame",function(frame) {
		if(frame instanceof Frame) {
			properties.frame = frame;
			properties.frame.attr = this.colors.bg + this.colors.fg;
			return true;
		}
		else {
			throw("frame parameter must be a Frame() object");
		}
	});
	this.__defineGetter__("items",function() {
		return properties.items;
	});
	this.__defineGetter__("parent",function() {
		return properties.parent;
	});
	this.__defineSetter__("parent",function(tree) {
		if(tree instanceof Tree) {
			properties.parent = tree;
			return true;
		}
		return false;
	});
	this.__defineGetter__("text",function() {
		return properties.text;
	});
	this.__defineSetter__("text",function(text) {
		if(typeof text == "string") {
			properties.text = text;
			return true;
		}
		return false;
	});
	this.__defineSetter__("attr",function(attr) {
		properties.attr = attr;
	});
	this.__defineGetter__("attr",function() {
		return properties.attr;
	});
	this.__defineGetter__("status",function() {
		return properties.status;
	});
	this.__defineGetter__("hash",function() {
		if(properties.parent)
			return properties.parent.hash + "\t" + properties.text;
		return properties.text;
	});
	this.__defineGetter__("depth", function() {
		if(!properties.parent)
			return(0);
		else
			return(properties.parent.depth+1);
	});
	this.__defineGetter__("index", function() {
		return properties.index;
	});
	this.__defineSetter__("index", function(index) {
		if(index !== -1 && !properties.items[index])
			return false;
		properties.index = index;
		return true;
	});
	this.__defineGetter__("line", function() {
		if(properties.parent)
			return properties.parent.line;
		else
			return properties.line;
	});
	this.__defineSetter__("line", function(line) {
		if(properties.parent)
			properties.parent.line=line;
		else 
			properties.line=line;
		return true;
	});
	this.__defineGetter__("current", function() {
		return properties.items[properties.index];
	});
	this.__defineGetter__("currentItem", function() {
		var current = this;
		while(current.current instanceof Tree) 
			current = current.current;
		if(current.current instanceof TreeItem)
			return current.current;
		else
			return current;
	});
	this.__defineGetter__("currentTree", function() {
		var current = this;
		while(current.current instanceof Tree) 
			current = current.current;
		return current;
	});
	
	/* public properties */
	this.colors = {
		// non-current item foreground
		fg:LIGHTGRAY,
		// non-current item/empty space background 
		bg:BG_BLACK,
		// current item foreground
		lfg:LIGHTRED,
		// current item background
		lbg:BG_RED,
		// current tree heading foreground
		cfg:WHITE,
		// current tree heading background
		cbg:BG_BLACK,
		// disabled item foreground
		dfg:DARKGRAY,
		// hotkey foreground
		kfg:YELLOW,
		// tree branch foreground
		tfg:BROWN,
		// tree heading foreground
		hfg:LIGHTGRAY,
		// tree heading background
		hbg:BG_BLACK,
		// tree expansion foreground
		xfg:YELLOW
	};
	
	/* tree methods */
	this.cycle = function() {
		return properties.frame.cycle();
	}
	this.getcmd = function(cmd) {
		/* initialize return value */
		var retval=values.NOT_HANDLED;
		
		if(!(properties.status&flags.CLOSED)) {
			/* if the current tree item is a subtree, pass control to the next subtree */
			if(this.current instanceof Tree) 
				retval=this.current.getcmd(cmd);
			
			/* if the submenu did not handle it, let this menu handle the command */
			if(retval === values.NOT_HANDLED) {
				switch(cmd) {
				case commands.DOWN:
					retval = this.down();
					break;
				case commands.UP:
					retval = this.up();
					break;
				case commands.HOME:
					retval = this.home();
					break;
				case commands.END:
					retval = this.end();
					break;
				case commands.PGUP:
					retval = this.pageUp(0,this.frame.height);
					break;
				case commands.PGDN:
					retval = this.pageDown(0,this.frame.height);
					break;
				case commands.DELETE:
					retval = this.deleteItem();
					break;
				case commands.SELECT:
					if(properties.index >= 0) 
						retval = this.current.action();						
					else 
						retval = this.close();
					break;
				default:
					retval=matchHotkey(cmd);
					break;
				}
				
				/* update the tree on an item being handled */
				if(retval !== values.NOT_HANDLED) {
					this.refresh();
				}
			}
			
			/* handle any residual movement from pageup or pagedown
			on a subtree */
			else {
				switch(cmd) {
				case commands.PGUP:
					retval = this.pageUp(retval,this.frame.height-1);
					break;
				case commands.PGDN:
					retval = this.pageDown(retval,this.frame.height-1);
					break;
				case commands.HOME:
					retval = this.home();
					break;
				case commands.END:
					retval = this.end();
					break;
				}
				
				/* update the tree on an item being handled */
				if(retval !== values.NOT_HANDLED) {
					this.refresh();
				}
			}
		}
		else {
			switch(cmd) {
			case "\r":
				if(properties.status&flags.CLOSED)
					retval = this.open();
				break;
			}
		}
		/* return whatever retval contains */
		return retval;
	}
	this.addTree = function(text) {
		if(text instanceof Tree) {
			this.items.push(tree);
			tree.frame = this.frame;
		}
		else {
			var tree=new Tree(this.frame,text,this);
			this.items.push(tree);
		}
		if(!(this.status&flags.CLOSED))
			this.refresh();
		return tree;
	}
	this.addItem = function(text,func,args) {
		if(text instanceof TreeItem) {
			this.items.push(text);
			text.parent = this;
		}
		else {
			var args = Array.prototype.slice.apply(arguments);
			var item=new TreeItem(args.shift(),this,args.shift(),args);
			this.items.push(item);
		}
		if(!(this.status&flags.CLOSED))
			this.refresh();
		return item;
	}		
	this.deleteItem = function(index) {
		if(!index)
			index = properties.index;
		if(!properties.items[index])
			return values.NOT_HANDLED;
		var item = properties.items.splice(index,1);
		if(!properties.items[properties.index])
			properties.index--;
		this.refresh();
		return values.HANDLED;
	}
	this.open = function() {
		if(properties.status&flags.CLOSED) {
			properties.status&=~flags.CLOSED;
			if(typeof this.onOpen == "function") 
				this.onOpen();
			this.refresh();
		}
		return true;
	}
	this.close = function() {
		if(properties.status&flags.CLOSED)
			return false;
		properties.status|=flags.CLOSED;
		if(typeof this.onClose == "function") 
			this.onClose();
		this.refresh();
		return true;
	}
	this.show = function() {
		if(properties.status&flags.HIDDEN) {
			properties.status&=~flags.HIDDEN;
			this.refresh();
			return true;
		}
		return false;
	}
	this.hide = function() {
		if(properties.status&flags.CLOSED)
			return false;
		properties.status|=flags.CLOSED;
		this.refresh();
		return true;
	}
	this.enable = function() {
		if(properties.status&flags.DISABLED) {
			properties.status&=~flags.DISABLED;
			this.refresh();
			return true;
		}
		return false;
	}
	this.disable = function() {
		if(properties.status&flags.DISABLED)
			return false;
		properties.status|=flags.DISABLED;
		this.refresh();
		return true;
	}
	this.trace = function(hash) {
		hash=hash.split("\t");
		var text=hash.shift();
		for each(var i in properties.items) {
			if(i.text == text) {
				if(i instanceof Tree && hash.length > 0)
					return i.trace(hash.join("\t"));
				return i;
			}
		}
		return false;
	}
	this.list = function(list) {
		if(properties.parent) 
			properties.parent.list(list);
		else 
			properties.list.push(list);
	}
	this.refresh=function() {
		if(properties.parent) {
			properties.parent.refresh();
		}
		else {
			if(!this.frame)
				return;
			this.generate();
			var offset = 0;
			if(this.line > this.frame.height)
				offset = this.line-this.frame.height;
			var output = properties.list.splice(offset,this.frame.height);
			
			var y;
			var x;
			
			/* push tree items into frame */
			for(y=0;y<output.length;y++) {
				var ch;
				var attr;
				var line = output[y].splice(0,this.frame.width);
				for(x=0;x<line.length;x++) {
					ch = line[x].ch;
					attr = line[x].attr;
					this.frame.setData(x,y,ch,attr,false);
				}
				/* clear remaining spaces at end of line */
				for(;x<this.frame.width;x++) {
					this.frame.setData(x,y," ",attr,false);
				}
			}
			/* clear remaining lines */
			for(y++;y<=this.frame.height;y++) {
				this.frame.gotoxy(0,y);
				this.frame.clearline(this.colors.bg);
			}
			properties.list=[];
		}
	}

	/* tree navigation */
	this.up=function() {
		var start = properties.index;
		while(this.current) {
			properties.index--;
			if(properties.index < 0) {
				if(this.depth == 0) {
					properties.index = 0;
					return values.HANDLED;
				}
				else {
					return values.HANDLED;
				}
			}
			if(properties.index == start)
				break;
			if(!(this.current.status&flags.DISABLED)) {
				if(this.current instanceof Tree) 
					this.current.updateIndex(commands.UP);
				return values.HANDLED;
			}
		}
		return values.NOT_HANDLED;
	}
	this.down=function() {
		var start = properties.index;
		while(properties.index == -1 || this.current) {
			properties.index++;
			if(properties.index >= properties.items.length) {
				properties.index = properties.items.length-1;
				return values.NOT_HANDLED;
			}
			if(properties.index == start)
				break;
			if(!(this.current.status&flags.DISABLED)) {
				if(this.current instanceof Tree) 
					this.current.updateIndex(commands.DOWN);
				return values.HANDLED;
			}
		}
		return values.NOT_HANDLED;
	}
	this.home=function() {
		/*ToDo: track starting position and compare to ending position 
		(prevent infinite loop and avoid empty menus) */
		if(this.depth == 0) {
			properties.index = 0;
			while(this.current.status&flags.disabled)
				this.down();
			if(this.current instanceof Tree) 
				this.current.index = -1;
			return values.HANDLED;
		}
		return values.NOT_HANDLED;
	}
	this.end=function() {
		/*ToDo: track starting position and compare to ending position 
		(prevent infinite loop and avoid empty menus) */
		properties.index = properties.items.length-1;
		while(this.current.status&flags.disabled)
			this.up();
		if(this.current instanceof Tree && 
			!(this.current.status&flags.CLOSED) &&
			!(this.current.status&flags.DISABLED) && 
			!(this.current.status&flags.HIDDEN)) 
			this.current.end();
		if(this.depth == 0)
			return values.HANDLED;
		return values.NOT_HANDLED;
	}
	this.pageUp=function(count,distance) {
		/* move up in this menu until the given distance
		or the top of the menu have been reached */
		for(var i=count;i<distance;i++) {
			var r;
			if(this.current instanceof Tree && 
				!(this.current.status&flags.CLOSED) &&
				!(this.current.status&flags.DISABLED) && 
				!(this.current.status&flags.HIDDEN)) {
				var r=this.current.pageUp(i,distance);
				if(r === values.HANDLED) 
					break;
				else if(r > i && r < distance)
					i = r;
			}
			if(i < distance && this.up() == values.NOT_HANDLED) {
				return i;
			}
		}
		return values.HANDLED;

	}
	this.pageDown=function(count,distance) {
		/* move up in this menu until the given distance
		or the top of the menu have been reached */
		for(var i=count;i<distance;i++) {
			var r;
			if(this.current instanceof Tree && 
				!(this.current.status&flags.CLOSED) &&
				!(this.current.status&flags.DISABLED) && 
				!(this.current.status&flags.HIDDEN)) {
				var r=this.current.pageDown(i,distance);
				if(r === values.HANDLED) 
					break;
				else if(r > i && r < distance)
					i = r;
			}
			if(i < distance && this.down() == values.NOT_HANDLED) {
				return i;
			}
		}
		return values.HANDLED;
	}
	
	/** DO NOT USE, SHIT WILL EXPLODE, INTERNAL USE ONLY **/
	this.generate=function(last,current,line) {
		if(properties.status&flags.HIDDEN)
			return line;

		var list=[];
		var bg=this.colors.hbg;
		var fg=this.colors.hfg;
		
		/* if this tree is the top level, initialize recursive shit */
		if(this.depth == 0) {
			current = true;
			line = 1;
		}
		
		/* if this is a subtree, do stuff? */
		else if(this.depth > 0) {
		
			/* if this is the current item, set lightbar bg color */
			if(current) {
				if(properties.index == -1) {
					bg=this.colors.lbg;
					this.line = line;
				}
				else {
					bg=this.colors.cbg;
				}
			}
			
			/* add indentation on subtrees */
			for(var i=0;i<this.depth-2;i++) 
				list.push({ch:" ",attr:bg+fg});
				
			/* do not draw tree branches on top level tree */
			if(this.depth > 1) {
				/* if this is the bottom of a subtree */
				if(last) {
					fg=this.colors.tfg;
					list.push({ch:"\xC0",attr:bg+fg});
				}
				/* otherwise draw a right-hand tee for a tree item */
				else {
					fg=this.colors.tfg;
					list.push({ch:"\xC3",attr:bg+fg});
				}
			}
			
			fg = this.colors.xfg;
			if(properties.status&flags.CLOSED) 
				list.push({ch:"+",attr:bg+fg});
			else 
				list.push({ch:"-",attr:bg+fg});
			
			/* if this tree is disabled, use disabled fg */
			if(properties.status&flags.DISABLED)
				fg=this.colors.dfg;
				
			else if(current) {
				/* if current, lightbar fg */
				if(properties.index == -1)
					fg=this.colors.lfg;
				else
					fg=this.colors.cfg;
			}
			/* normal item fg */
			else if(properties.attr == undefined)
				fg=this.colors.hfg;
			/* special item fg */
			else
				fg=properties.attr;
			
			/* push text string into list */
			for(var i=0;i<properties.text.length;i++) {
				if(properties.text[i] == "|")
					list.push({ch:properties.text[++i],attr:bg+this.colors.kfg});
				else
					list.push({ch:properties.text[i],attr:bg+fg});
			}

			line++;
			this.list(list);
		}
		
		/* if this tree is "open", list its items */
		if(!(properties.status&flags.CLOSED)) {
			for(var i in properties.items) {
				var l = (i == properties.items.length-1);
				var c = (current && properties.index == i);
				line = properties.items[i].generate(l,c,line);
			}
		}
		
		return line;
	}
	this.updateIndex=function(cmd) {
		if(!(this.status&flags.CLOSED)) {
			if(cmd == commands.UP)
				properties.index = properties.items.length-1;
			else if(cmd == commands.DOWN)
				properties.index = -1;
			if(properties.items[properties.index] instanceof Tree)
				properties.items[properties.index].updateIndex(cmd);
			return true;
		}
		return false;
	}
	
	/* private functions */
	function matchHotkey(cmd) {
		if(!cmd.match(/\w/))
			return values.NOT_HANDLED;
		var pattern=new RegExp(cmd,"i");
		var stop=properties.items.length-1;
		if(this.depth == 0)
			stop=properties.index;
		for(var i=properties.index+1;;i++) {
			if(i >= properties.items.length) 
				i=0;
			if(properties.items[i].status&flags.DISABLED)
				continue;
			if(properties.items[i].text.search(pattern) >= 0) {
				properties.index=i;
				if(properties.items[i] instanceof Tree)
					properties.items[i].index = -1;
				return values.HANDLED;
			}
			if(i == stop)
				break;
		}	
		return values.NOT_HANDLED;
	}
	function init(frame,text,tree) {
		if(tree instanceof Tree) {
			properties.parent = tree;
			properties.status |= flags.CLOSED;
			properties.index = -1;
			this.colors=properties.parent.colors;
		}
		if(frame instanceof Frame) {
			properties.frame=frame;
			properties.frame.attr = this.colors.bg + this.colors.fg;
		}
		if(text) {
			properties.text = text;
		}
	}
	init.apply(this,arguments);
}

function TreeItem(text,parent,func,args) {
	
	/* private properties */
	var flags = {
		CLOSED:(1<<0),
		HIDDEN:(1<<1),
		DISABLED:(1<<2)
	};
	var properties = {
		parent:undefined,
		status:undefined,
		text:undefined,
		attr:undefined
	};
	
	/* protected properties */
	this.__defineGetter__("parent",function() {
		return properties.parent;
	});
	this.__defineSetter__("parent",function(tree) {
		if(tree instanceof Tree) {
			properties.parent = tree;
			return true;
		}
		return false;
	});
	this.__defineGetter__("text",function() {
		return properties.text;
	});
	this.__defineSetter__("text",function(text) {
		if(typeof text == "string") {
			properties.text = text;
			return true;
		}
		return false;
	});
	this.__defineGetter__("colors",function() {
		return properties.parent.colors;
	});
	this.__defineSetter__("attr",function(attr) {
		properties.attr = attr;
	});
	this.__defineGetter__("attr",function() {
		return properties.attr;
	});
	this.__defineGetter__("status",function() {
		return properties.status;
	});
	this.__defineGetter__("hash",function() {
		return properties.parent.hash+"\t"+properties.text;
	});
	this.__defineGetter__("frame",function() {
		return properties.parent.frame;
	});
	this.__defineGetter__("depth",function() {
		return properties.parent.depth+1;
	});
	this.__defineGetter__("line", function() {
		return properties.parent.line;
	});
	this.__defineSetter__("line", function(line) {
		properties.parent.line=line;
	});
	this.__defineGetter__("hotkey",function() {
		return properties.text.indexOf("|")+1;
	});
	
	/* public properties */
	this.func = func;
	this.args = args;
	
	/* public methods */
	this.action=function() {
		return this.func;
	}
	this.show = function() {
		if(properties.status&flags.HIDDEN) {
			properties.status&=~flags.HIDDEN;
			this.refresh();
			return true;
		}
		return false;
	}
	this.hide = function() {
		if(properties.status&flags.HIDDEN)
			return false;
		properties.status|=flags.HIDDEN;
		this.refresh();
		return true;
	}
	this.enable = function() {
		if(properties.status&flags.DISABLED) {
			properties.status&=~flags.DISABLED;
			this.refresh();
			return true;
		}
		return false;
	}
	this.disable = function() {
		if(properties.status&flags.DISABLED)
			return false;
		properties.status|=flags.DISABLED;
		this.refresh();
		return true;
	}
	this.list=function(str) {
		properties.parent.list(str);
	}
	this.refresh=function() {
		properties.parent.refresh();
	}

	/** DO NOT USE, SHIT WILL EXPLODE, INTERNAL USE ONLY **/
	this.generate=function(last,current,line) {
		if(properties.status&flags.HIDDEN)
			return line;
			
		var list=[];
		var bg=this.colors.bg;
		var fg=this.colors.fg;
		
		/* if this is the current item, set lightbar bg color */
		if(current) {
			bg=this.colors.lbg;
			this.line = line;
		}
		
		/* add indentation on subtrees */
		for(var i=0;i<this.depth-2;i++) 
			list.push({ch:" ",attr:bg+fg});
		/* do not draw tree branches on top level tree */
		if(this.depth == 1) 
			list.push({ch:" ",attr:bg+fg});
		/* if this is the bottom of a subtree */
		else if(last) {
			fg=this.colors.tfg;
			list.push({ch:"\xC0",attr:bg+fg});
		}
		/* otherwise draw a right-hand tee for a tree item */
		else {
			fg=this.colors.tfg;
			list.push({ch:"\xC3",attr:bg+fg});
		}
		
		/* if this tree is disabled, use disabled fg */
		if(properties.status&flags.DISABLED)
			fg=this.colors.dfg;
		/* otherwise, if current? lightbar fg */
		else if(current)
			fg=this.colors.lfg;
		/* normal item fg */
		else if(properties.attr == undefined)
			fg=this.colors.fg;
		/* special item fg */
		else
			fg=properties.attr;
		
		/* push text string into list */
		for(var i=0;i<properties.text.length;i++) {
			if(properties.text[i] == "|")
				list.push({ch:properties.text[++i],attr:bg+this.colors.kfg});
			else
				list.push({ch:properties.text[i],attr:bg+fg});
		}

		this.list(list);
		return ++line;
	}
	
	/* private functions */
	function init(text,parent,func,args) {
		properties.text = text;
		properties.parent = parent;
		this.func=func;
		this.args=args;
		if(typeof func == "function") {
			this.action=function() {
				return this.func.apply(this,this.args);
			}
		}
	}
	init.apply(this,arguments);
}

