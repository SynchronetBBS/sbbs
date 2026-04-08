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
		height

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
	this.__flags__ = {
		CLOSED:(1<<0),
		HIDDEN:(1<<1),
		DISABLED:(1<<2)
	};
	this.__properties__ = {
		frame:undefined,
		parent:undefined,
		status:this.__flags__.CLOSED,
		index:0,
		offset:0,
		text:"",
		line:1,
		attr:undefined,
		list:[],
		items:[],
        first_letter_search:false,
		character_search:true
	};
	this.__values__ = {
		NOT_HANDLED:false,
		HANDLED:true
	}
	this.__commands__ = {
		DOWN:KEY_DOWN,
		UP:KEY_UP,
		HOME:KEY_HOME,
		END:KEY_END,
		PGDN:KEY_PAGEDN,
		PGUP:KEY_PAGEUP,
		SELECT:"\r",
		DELETE:KEY_DEL
	}

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

	/* private functions */
	this.__matchHotkey__ = function(cmd) {
		if(!cmd.match(/\w/))
			return this.__values__.NOT_HANDLED;
		for(var i=0;i<this.__properties__.items.length;i++) {
			if(!this.__properties__.items[i].hotkey) {
				continue;
			}
			if(this.__properties__.items[i].hotkey.toUpperCase() == cmd.toUpperCase()) {
				this.__properties__.index = i;
				return this.__properties__.items[i].action();
			}
		}
		var pattern = new RegExp(cmd, "i");
		if(this.__properties__.character_search) {
			if (this.__properties__.first_letter_search) {
				pattern = new RegExp('^' + cmd, 'i');
			}
        } else {
			return this.__values__.NOT_HANDLED;
        }
		var stop=this.__properties__.items.length-1;
		if(this.depth == 0) {
			stop=this.__properties__.index;
		}
		for(var i=this.__properties__.index+1;;i++) {
			if(i >= this.__properties__.items.length)
				i=0;
			if(this.__properties__.items[i].status&this.__flags__.DISABLED)
				continue;
			if(this.__properties__.items[i].text.search(pattern) >= 0) {
				this.__properties__.index=i;
				if(this.__properties__.items[i] instanceof Tree)
					this.__properties__.items[i].index = -1;
				return this.__values__.HANDLED;
			}
			if(i == stop) {
				break;
			}
		}
		return this.__values__.NOT_HANDLED;
	}

	function init(frame,text,tree) {
		if(tree instanceof Tree) {
			this.__properties__.parent = tree;
			this.__properties__.status |= this.__flags__.CLOSED;
			this.__properties__.index = -1;
			this.colors=this.__properties__.parent.colors;
		}
		if(frame instanceof Frame) {
			this.__properties__.frame=frame;
			this.__properties__.frame.attr = this.colors.bg + this.colors.fg;
		}
		if(text) {
			this.__properties__.text = text;
		}
	}
	init.apply(this,arguments);
}

/* protected tree properties */
Tree.prototype.__defineGetter__("frame",function() {
	if(this.__properties__.parent)
		return this.__properties__.parent.frame;
	return this.__properties__.frame;
});
Tree.prototype.__defineSetter__("frame",function(frame) {
	if(frame instanceof Frame) {
		this.__properties__.frame = frame;
		this.__properties__.frame.attr = this.colors.bg + this.colors.fg;
		return true;
	}
	else {
		throw new Error("frame parameter must be a Frame() object");
	}
});
Tree.prototype.__defineGetter__("items",function() {
	return this.__properties__.items;
});
Tree.prototype.__defineSetter__("items",function(items) {
	if(items instanceof Array) {
		this.__properties__.items = items;
		return true;
	}
	else {
		throw new Error("items parameter must be an array");
	}
});
Tree.prototype.__defineGetter__("parent",function() {
	return this.__properties__.parent;
});
Tree.prototype.__defineSetter__("parent",function(tree) {
	if(tree instanceof Tree) {
		this.__properties__.parent = tree;
		return true;
	}
	return false;
});
Tree.prototype.__defineGetter__("text",function() {
	return this.__properties__.text;
});
Tree.prototype.__defineSetter__("text",function(text) {
	if(typeof text == "string") {
		this.__properties__.text = text;
		return true;
	}
	return false;
});
Tree.prototype.__defineSetter__("attr",function(attr) {
	this.__properties__.attr = attr;
});
Tree.prototype.__defineGetter__("attr",function() {
	return this.__properties__.attr;
});
Tree.prototype.__defineGetter__("status",function() {
	return this.__properties__.status;
});
Tree.prototype.__defineGetter__("hash",function() {
	if(this.__properties__.parent)
		return this.__properties__.parent.hash + "\t" + this.__properties__.text;
	return this.__properties__.text;
});
Tree.prototype.__defineGetter__("depth", function() {
	if(!this.__properties__.parent)
		return(0);
	else
		return(this.__properties__.parent.depth+1);
});
Tree.prototype.__defineGetter__("index", function() {
	return this.__properties__.index;
});
Tree.prototype.__defineSetter__("index", function(index) {
	if(index !== -1 && !this.__properties__.items[index])
		return false;
	this.__properties__.index = index;
	return true;
});
Tree.prototype.__defineGetter__("line", function() {
	if(this.__properties__.parent)
		return this.__properties__.parent.line;
	else
		return this.__properties__.line;
});
Tree.prototype.__defineSetter__("line", function(line) {
	if(this.__properties__.parent)
		this.__properties__.parent.line=line;
	else
		this.__properties__.line=line;
	return true;
});
Tree.prototype.__defineGetter__("current", function() {
	return this.__properties__.items[this.__properties__.index];
});
Tree.prototype.__defineGetter__("currentItem", function() {
	var current = this;
	while(current.current instanceof Tree)
		current = current.current;
	if(current.current instanceof TreeItem)
		return current.current;
	else
		return current;
});
Tree.prototype.__defineGetter__("currentTree", function() {
	var current = this;
	while(current.current instanceof Tree)
		current = current.current;
	return current;
});
Tree.prototype.__defineGetter__("height", function() {
	var c = 0;
	for(var i = 0; i < this.__properties__.items.length; i++) {
		if(this.__properties__.items[i] instanceof TreeItem && this.__properties__.items[i].status != this.__flags__.HIDDEN) {
			c++;
		} else if(this.__properties__.items[i] instanceof Tree) {
			if(this.__properties__.items[i].status == 0)
				c = c + this.__properties__.items[i].height;
			else if(this.__properties__.items[i].status != this.__flags__.HIDDEN)
				c++;
		}
	}
	return c;
});
Tree.prototype.__defineGetter__(
    "first_letter_search", function () {
        return this.__properties__.first_letter_search;
    }
);
Tree.prototype.__defineSetter__(
    "first_letter_search", function (value) {
        if (typeof value != 'boolean') {
            throw 'Invalid first_letter_search parameter';
        }
        this.__properties__.first_letter_search = value;
    }
);
Tree.prototype.__defineGetter__(
	"offset", function () {
		return this.__properties__.offset;
	}
);

/* tree methods */
Tree.prototype.cycle = function() {
	return this.__properties__.frame.cycle();
}
Tree.prototype.getcmd = function(cmd) {
	/* initialize return value */
	var retval=this.__values__.NOT_HANDLED;

    if (typeof cmd == 'string') cmd = { key: cmd, mouse: null };

	if(!(this.__properties__.status&this.__flags__.CLOSED)) {
		/* if the current tree item is a subtree, pass control to the next subtree */
		if(this.current instanceof Tree)
			retval=this.current.getcmd(cmd.key);

		/* if the submenu did not handle it, let this menu handle the command */
		if(retval === this.__values__.NOT_HANDLED) {
            if (cmd.mouse !== null && cmd.mouse.release && cmd.mouse.x >= this.frame.x && cmd.mouse.x < this.frame.x + this.frame.width && cmd.mouse.y >= this.frame.y && cmd.mouse.y < this.frame.y + this.frame.height) {
                var my = cmd.mouse.y - this.frame.y;
                switch (cmd.mouse.button) {
                    case 0: // left click
                        while (this.offset + my > this.index && this.down() == this.__values__.HANDLED) {
                        }
                        while (this.offset + my < this.index && this.up() == this.__values__.HANDLED) {
                        }
                        retval = this.getcmd(this.__commands__.SELECT);
                        break;
                    case 64: // scroll up
                        retval = this.up();
                        break;
                    case 65: // scroll down
                        retval = this.down();
                        break;
                    default:
                        break;
                }
            } else {
                switch(cmd.key) {
                case this.__commands__.DOWN:
                    retval = this.down();
                    break;
                case this.__commands__.UP:
                    retval = this.up();
                    break;
                case this.__commands__.HOME:
                    retval = this.home();
                    break;
                case this.__commands__.END:
                    retval = this.end();
                    break;
                case this.__commands__.PGUP:
                    retval = this.pageUp(0,this.frame.height);
                    break;
                case this.__commands__.PGDN:
                    retval = this.pageDown(0,this.frame.height);
                    break;
                case this.__commands__.DELETE:
                    retval = this.deleteItem();
                    break;
                case this.__commands__.SELECT:
                    if(this.current !== undefined && this.__properties__.index >= 0)
                        retval = this.current.action();
                    else
                        retval = this.close();
                    break;
                default:
                    retval=this.__matchHotkey__(cmd.key);
                    break;
                }
            }

			/* update the tree on an item being handled */
			if(retval !== this.__values__.NOT_HANDLED) {
				this.refresh();
			}
		}

		/* handle any residual movement from pageup or pagedown
		on a subtree */
		else {
			switch(cmd.key) {
			case this.__commands__.PGUP:
				retval = this.pageUp(retval,this.frame.height-1);
				break;
			case this.__commands__.PGDN:
				retval = this.pageDown(retval,this.frame.height-1);
				break;
			case this.__commands__.HOME:
				retval = this.home();
				break;
			case this.__commands__.END:
				retval = this.end();
				break;
			}

			/* update the tree on an item being handled */
			if(retval !== this.__values__.NOT_HANDLED) {
				this.refresh();
			}
		}
	}
	else {
		switch(cmd.key) {
		case "\r":
			if(this.__properties__.status&this.__flags__.CLOSED)
				retval = this.open();
			break;
		}
	}
	/* return whatever retval contains */
	return retval;
}
Tree.prototype.addTree = function(text) {
	var tree;
	if(text instanceof Tree) {
		tree = text;
		this.items.push(tree);
		tree.frame = this.frame;
	}
	else {
		tree=new Tree(this.frame,text,this);
		this.items.push(tree);
	}
	if(!(this.status&this.__flags__.CLOSED))
		this.refresh();
	return tree;
}
Tree.prototype.addItem = function(text,func,args) {
	var item;
	if(text instanceof TreeItem) {
		item = text;
		this.items.push(item);
		item.parent = this;
	}
	else {
		var args = Array.prototype.slice.apply(arguments);
		item=new TreeItem(args.shift(),this,args.shift(),args);
		this.items.push(item);
	}
	if(!(this.status&this.__flags__.CLOSED))
		this.refresh();
	return item;
}
Tree.prototype.deleteItem = function(index) {
	if(!index)
		index = this.__properties__.index;
	if(!this.__properties__.items[index])
		return this.__values__.NOT_HANDLED;
	var item = this.__properties__.items.splice(index,1);
	if(!this.__properties__.items[this.__properties__.index])
		this.__properties__.index--;
	this.refresh();
	return this.__values__.HANDLED;
}
Tree.prototype.open = function() {
	if(this.__properties__.status&this.__flags__.CLOSED) {
		this.__properties__.status&=~this.__flags__.CLOSED;
		if(typeof this.onOpen == "function")
			this.onOpen();
		this.refresh();
	}
	return true;
}
Tree.prototype.close = function() {
	if(this.__properties__.status&this.__flags__.CLOSED)
		return false;
	this.__properties__.status|=this.__flags__.CLOSED;
	if(typeof this.onClose == "function")
		this.onClose();
	this.refresh();
	return true;
}
Tree.prototype.show = function() {
	if(this.__properties__.status&this.__flags__.HIDDEN) {
		this.__properties__.status&=~this.__flags__.HIDDEN;
		this.refresh();
		return true;
	}
	return false;
}
Tree.prototype.hide = function() {
	if(this.__properties__.status&this.__flags__.CLOSED)
		return false;
	this.__properties__.status|=this.__flags__.CLOSED;
	this.refresh();
	return true;
}
Tree.prototype.enable = function() {
	if(this.__properties__.status&this.__flags__.DISABLED) {
		this.__properties__.status&=~this.__flags__.DISABLED;
		this.refresh();
		return true;
	}
	return false;
}
Tree.prototype.disable = function() {
	if(this.__properties__.status&this.__flags__.DISABLED)
		return false;
	this.__properties__.status|=this.__flags__.DISABLED;
	this.refresh();
	return true;
}
Tree.prototype.trace = function(hash) {
	hash=hash.split("\t");
	var text=hash.shift();
	for each(var i in this.__properties__.items) {
		if(i.text == text) {
			if(i instanceof Tree && hash.length > 0)
				return i.trace(hash.join("\t"));
			return i;
		}
	}
	return false;
}
Tree.prototype.list = function(list) {
	if(this.__properties__.parent)
		this.__properties__.parent.list(list);
	else
		this.__properties__.list.push(list);
}
Tree.prototype.refresh=function() {
	if(this.__properties__.parent) {
		this.__properties__.parent.refresh();
	}
	else {
		if(!this.frame)
			return;
		this.generate();
		//var offset = 0;
		if(this.line - this.__properties__.offset > this.frame.height) {
			this.__properties__.offset = this.line-this.frame.height;
		}
		else if(this.line <= this.__properties__.offset) {
			this.__properties__.offset = this.line - 1;
		}
		var output = this.__properties__.list.splice(this.__properties__.offset,this.frame.height);

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
		this.__properties__.list=[];
	}
}

/* tree navigation */
Tree.prototype.up=function() {
	var start = this.__properties__.index;
	while(this.current) {
		this.__properties__.index--;
		if(this.__properties__.index < 0) {
			if(this.depth == 0) {
				this.__properties__.index = 0;
				return this.__values__.HANDLED;
			}
			else {
				return this.__values__.HANDLED;
			}
		}
		if(this.__properties__.index == start)
			break;
		if(!(this.current.status&this.__flags__.DISABLED)) {
			if(this.current instanceof Tree)
				this.current.updateIndex(this.__commands__.UP);
			return this.__values__.HANDLED;
		}
	}
	return this.__values__.NOT_HANDLED;
}
Tree.prototype.down=function() {
	var start = this.__properties__.index;
	while(this.__properties__.index == -1 || this.current) {
		this.__properties__.index++;
		if(this.__properties__.index >= this.__properties__.items.length) {
			this.__properties__.index = this.__properties__.items.length-1;
			return this.__values__.NOT_HANDLED;
		}
		if(this.__properties__.index == start)
			break;
		if(!(this.current.status&this.__flags__.DISABLED)) {
			if(this.current instanceof Tree)
				this.current.updateIndex(this.__commands__.DOWN);
			return this.__values__.HANDLED;
		}
	}
	return this.__values__.NOT_HANDLED;
}
Tree.prototype.home=function() {
	/*ToDo: track starting position and compare to ending position
	(prevent infinite loop and avoid empty menus) */
	if(this.depth == 0) {
		this.__properties__.index = 0;
		while(this.current.status&this.__flags__.disabled)
			this.down();
		if(this.current instanceof Tree)
			this.current.index = -1;
		return this.__values__.HANDLED;
	}
	return this.__values__.NOT_HANDLED;
}
Tree.prototype.end=function() {
	/*ToDo: track starting position and compare to ending position
	(prevent infinite loop and avoid empty menus) */
	this.__properties__.index = this.__properties__.items.length-1;
	while(this.current.status&this.__flags__.disabled)
		this.up();
	if(this.current instanceof Tree &&
		!(this.current.status&this.__flags__.CLOSED) &&
		!(this.current.status&this.__flags__.DISABLED) &&
		!(this.current.status&this.__flags__.HIDDEN))
		this.current.end();
	if(this.depth == 0)
		return this.__values__.HANDLED;
	return this.__values__.NOT_HANDLED;
}
Tree.prototype.pageUp=function(count,distance) {
	/* move up in this menu until the given distance
	or the top of the menu have been reached */
	for(var i=count;i<distance;i++) {
		var r;
		if(this.current instanceof Tree &&
			!(this.current.status&this.__flags__.CLOSED) &&
			!(this.current.status&this.__flags__.DISABLED) &&
			!(this.current.status&this.__flags__.HIDDEN)) {
			var r=this.current.pageUp(i,distance);
			if(r === this.__values__.HANDLED)
				break;
			else if(r > i && r < distance)
				i = r;
		}
		if(i < distance && this.up() == this.__values__.NOT_HANDLED) {
			return i;
		}
	}
	return this.__values__.HANDLED;

}
Tree.prototype.pageDown=function(count,distance) {
	/* move up in this menu until the given distance
	or the top of the menu have been reached */
	for(var i=count;i<distance;i++) {
		var r;
		if(this.current instanceof Tree &&
			!(this.current.status&this.__flags__.CLOSED) &&
			!(this.current.status&this.__flags__.DISABLED) &&
			!(this.current.status&this.__flags__.HIDDEN)) {
			var r=this.current.pageDown(i,distance);
			if(r === this.__values__.HANDLED)
				break;
			else if(r > i && r < distance)
				i = r;
		}
		if(i < distance && this.down() == this.__values__.NOT_HANDLED) {
			break;
		}
	}
	return this.__values__.HANDLED;
}

/** DO NOT USE, SHIT WILL EXPLODE, INTERNAL USE ONLY **/
Tree.prototype.generate=function(last,current,line) {
	if(this.__properties__.status&this.__flags__.HIDDEN)
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
			if(this.__properties__.index == -1) {
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
		if(this.__properties__.status&this.__flags__.CLOSED)
			list.push({ch:"+",attr:bg+fg});
		else
			list.push({ch:"-",attr:bg+fg});

		/* if this tree is disabled, use disabled fg */
		if(this.__properties__.status&this.__flags__.DISABLED)
			fg=this.colors.dfg;

		else if(current) {
			/* if current, lightbar fg */
			if(this.__properties__.index == -1)
				fg=this.colors.lfg;
			else
				fg=this.colors.cfg;
		}
		/* normal item fg */
		else if(this.__properties__.attr == undefined)
			fg=this.colors.hfg;
		/* special item fg */
		else
			fg=this.__properties__.attr;

		/* push text string into list */
		for(var i=0;i<this.__properties__.text.length;i++) {
			if(this.__properties__.text[i] == "|")
				list.push({ch:this.__properties__.text[++i],attr:bg+this.colors.kfg});
			else
				list.push({ch:this.__properties__.text[i],attr:bg+fg});
		}

		line++;
		this.list(list);
	}

	/* if this tree is "open", list its items */
	if(!(this.__properties__.status&this.__flags__.CLOSED)) {
		for(var i in this.__properties__.items) {
			var l = (i == this.__properties__.items.length-1);
			var c = (current && this.__properties__.index == i);
			line = this.__properties__.items[i].generate(l,c,line);
		}
	}

	return line;
}
Tree.prototype.updateIndex=function(cmd) {
	if(!(this.status&this.__flags__.CLOSED)) {
		if(cmd == this.__commands__.UP)
			this.__properties__.index = this.__properties__.items.length-1;
		else if(cmd == this.__commands__.DOWN)
			this.__properties__.index = -1;
		if(this.__properties__.items[this.__properties__.index] instanceof Tree)
			this.__properties__.items[this.__properties__.index].updateIndex(cmd);
		return true;
	}
	return false;
}

/* tree item */
function TreeItem(text,parent,func,args) {

	/* private properties */
	this.__flags__ = {
		CLOSED:(1<<0),
		HIDDEN:(1<<1),
		DISABLED:(1<<2)
	};
	this.__properties__ = {
		parent:undefined,
		status:undefined,
		text:undefined,
		attr:undefined
	};

	/* public properties */
	this.func = func;
	this.args = args;

	/* private functions */
	function init(text,parent,func,args) {
		this.__properties__.text = text;
		this.__properties__.parent = parent;
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

/* protected properties */
TreeItem.prototype.__defineGetter__("parent",function() {
	return this.__properties__.parent;
});
TreeItem.prototype.__defineSetter__("parent",function(tree) {
	if(tree instanceof Tree) {
		this.__properties__.parent = tree;
		return true;
	}
	return false;
});
TreeItem.prototype.__defineGetter__("text",function() {
	return this.__properties__.text;
});
TreeItem.prototype.__defineSetter__("text",function(text) {
	if(typeof text == "string") {
		this.__properties__.text = text;
		return true;
	}
	return false;
});
TreeItem.prototype.__defineGetter__("colors",function() {
	return this.__properties__.parent.colors;
});
TreeItem.prototype.__defineSetter__("attr",function(attr) {
	this.__properties__.attr = attr;
});
TreeItem.prototype.__defineGetter__("attr",function() {
	return this.__properties__.attr;
});
TreeItem.prototype.__defineGetter__("status",function() {
	return this.__properties__.status;
});
TreeItem.prototype.__defineGetter__("hash",function() {
	return this.__properties__.parent.hash+"\t"+this.__properties__.text;
});
TreeItem.prototype.__defineGetter__("frame",function() {
	return this.__properties__.parent.frame;
});
TreeItem.prototype.__defineGetter__("depth",function() {
	return this.__properties__.parent.depth+1;
});
TreeItem.prototype.__defineGetter__("line", function() {
	return this.__properties__.parent.line;
});
TreeItem.prototype.__defineSetter__("line", function(line) {
	this.__properties__.parent.line=line;
});
TreeItem.prototype.__defineGetter__("hotkey",function() {
	var kIndex = this.__properties__.text.indexOf("|")+1;
	if(kIndex > 0) {
		return this.__properties__.text[kIndex];
	}
	else {
		return undefined;
	}
});

/* public methods */
TreeItem.prototype.action=function() {
	return this.func;
}
TreeItem.prototype.show = function() {
	if(this.__properties__.status&this.__flags__.HIDDEN) {
		this.__properties__.status&=~this.__flags__.HIDDEN;
		this.refresh();
		return true;
	}
	return false;
}
TreeItem.prototype.hide = function() {
	if(this.__properties__.status&this.__flags__.HIDDEN)
		return false;
	this.__properties__.status|=this.__flags__.HIDDEN;
	this.refresh();
	return true;
}
TreeItem.prototype.enable = function() {
	if(this.__properties__.status&this.__flags__.DISABLED) {
		this.__properties__.status&=~this.__flags__.DISABLED;
		this.refresh();
		return true;
	}
	return false;
}
TreeItem.prototype.disable = function() {
	if(this.__properties__.status&this.__flags__.DISABLED)
		return false;
	this.__properties__.status|=this.__flags__.DISABLED;
	this.refresh();
	return true;
}
TreeItem.prototype.list=function(str) {
	this.__properties__.parent.list(str);
}
TreeItem.prototype.refresh=function() {
	this.__properties__.parent.refresh();
}

/** DO NOT USE, SHIT WILL EXPLODE, INTERNAL USE ONLY **/
TreeItem.prototype.generate=function(last,current,line) {
	if(this.__properties__.status&this.__flags__.HIDDEN)
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
	if(this.__properties__.status&this.__flags__.DISABLED)
		fg=this.colors.dfg;
	/* otherwise, if current? lightbar fg */
	else if(current)
		fg=this.colors.lfg;
	/* normal item fg */
	else if(this.__properties__.attr == undefined)
		fg=this.colors.fg;
	/* special item fg */
	else
		fg=this.__properties__.attr;

	/* push text string into list */
	for(var i=0;i<this.__properties__.text.length;i++) {
		if(this.__properties__.text[i] == "|")
			list.push({ch:this.__properties__.text[++i],attr:bg+this.colors.kfg});
		else
			list.push({ch:this.__properties__.text[i],attr:bg+fg});
	}

	this.list(list);
	return ++line;
}
