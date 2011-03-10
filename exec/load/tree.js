/*	$id: $ 

	Lightbar Menu Tree 
	by Matt Johnson (MCMLXXIX) 2011
	
	usage example:
	
	// create a new tree in the top left corner; 40 columns 40 rows
	var tree=new Tree(1,1,40,10);
	
	// add an item with a hotkey of "i" and a return value of 1
	tree.addItem("|item one",1);

	// add a hidden item with no hotkey and a return value of 2
	tree.addItem("item two",2,item_status.hidden);

	// add a disabled item with a hotkey of "t" and a return value of "meh"
	tree.addItem("i|tem three","meh",item_status.disabled);

	// create a subtree with a hotkey of "m"
	var subtree=tree.addTree("|menu title");
	
	// add items to the subtree the same as before
	subtree.addItem("|logoff",bbs.hangup);

	// add an item with a hotkey of "e" and 
	// a command value of the result of function "getval(4,5,6)"
	tree.addItem("it|em four",getval,4,5,6);
	
	// draw the tree 
	tree.draw();
		
*/
load("sbbsdefs.js");
load("funclib.js");

/* states for setting tree item visibility */
var item_status={
	enabled:1,
	disabled:0,
	hidden:-1
}

/* tree item color settings */
var tree_settings={
	// non-current item foreground
	fg:LIGHTGRAY,
	// non-current item/empty space background 
	bg:BG_BLACK,
	// current item foreground
	lfg:LIGHTCYAN,
	// current item background
	lbg:BG_BLUE,
	// disabled item foreground
	dfg:DARKGRAY,
	// hotkey foreground
	kfg:YELLOW,
	// tree branch foreground
	tfg:BROWN,
	// tree heading foreground
	hfg:WHITE,
	// tree heading background
	hbg:BG_BLACK,
	// tree expansion foreground
	xfg:YELLOW
}

/* main tree object */
function Tree(xpos,ypos,w,h) {
	var x = xpos;
	var y = ypos;
	var width=w;
	var height=h;
	
	/*	only the top-level
		tree will have a set of coordinates,
		so we can assume it is the top
		if any are supplied here */
	if(x || y || width || height) {
		this.open=true;
		this.index=0;
	}
	else {
		this.open=false;
		this.index=-1;
	}
		
	/* 	these properties will only exist in a sub-tree */
	//	this.parent_index;
	//	this.parent;
	//	this.hotkey;
	
	this.status=item_status.enabled;
	this.text="";
	this.items=[];

	/* add a menu item */
	this.addItem=function(text,status,command,args) {
		var args = Array.prototype.slice.apply(arguments);
		var item=new Tree_Item(args.shift(),args.shift(),args.shift(),args);
		item.parent=this;
		item.parent_index=this.items.length;
		this.items.push(item);
		return item;
	}
	
	/* add a sub-tree */
	this.addTree=function(text,status) {
		var tree=new Tree();
		tree.text=text;
		if(status != undefined)
			tree.status=status;
		tree.parent=this;
		tree.parent_index=this.items.length;
		this.items.push(tree);
		return tree;
	}
	
	/* toggle tree expansion */
	this.toggle=function() {
		this.open = (this.open ? false : true ); /* It's motherfucked. */
		this.index=-1;
	}
	
	/* display tree */
	this.draw=function(offset) {
		/* initialize list at top level tree only */
		if(this.depth == 0) {
			offset=" ";
			this.list=[];
			this.list.index=0;
		}
		/* if this is not the top tree, draw tree text */
		if(this.depth > 0) 
			this.post_tree(offset);
		/* if this tree is open, draw its items */
		if(this.open) 
			this.post_items(offset);
		/* draw list at top level tree only */
		if(this.depth == 0)
			this.draw_list();
	}
	
	/* display truncated item list */
	this.draw_list=function() {
		console.gotoxy(x,y);
		console.pushxy();
		
		var ypos=y;
		var end=parseInt(this.list.index+(this.height/2),10);
		if(end > this.list.length-1)
			end=this.list.length-1;
		var index=end-this.height+1;
		if(index < 0) 
			index = 0;
		var l=0;
		for(;l<this.height && index < this.list.length;l++) {
			if(ypos >= console.screen_rows)
				break;
			var str=this.list[index];
			console.putmsg(format("%-*s",this.width+(str.length-console.strlen(str)),str));
			console.popxy();
			console.down();
			console.pushxy();
			ypos++;
			index++;
		}
		for(;l < this.height;l++) {
			if(ypos >= console.screen_rows)
				break;
			console.putmsg(format("%*s",this.width,""));
			console.popxy();
			console.down();
			console.pushxy();
			ypos++;
		}
	}
	
	/* draw tree items */
	this.post_items=function(offset) {
		if(this.index >= 0 && this.items[this.index].status != item_status.enabled) {
			this.next_index(1);
		}
		var spacer="";
		for(var i=0;i<this.items.length;i++) {
			if(this.parent && this.parent.depth > 0) {
				if(this.parent_index == this.parent.items.length - 1)
					spacer="  ";
				else
					spacer="\xB3 ";
			}
			this.items[i].draw(offset+spacer);
		}
	}
	
	/* draw tree title (as item in parent tree) */
	this.post_tree=function(offset) {
		var str="";
		var curr=false;
		var bg="\1n" + getColor(tree_settings.bg);
		var fg=getColor(tree_settings.fg);
		/* if this is the current menu and it is either not open,
			or we arent highlighting one of its items */
		if(this.current && (this.index < 0 || !this.open)) {
			bg=getColor(tree_settings.lbg);
			fg=getColor(tree_settings.lfg);
			curr=true;
		}
		else if(this.status == item_status.disabled) {
			fg=getColor(tree_settings.dfg);
		}
		str+=bg;
		/* draw tree branches */
		if(this.depth > 1) {
			str+=getColor(tree_settings.tfg) + offset;
			if(this.status == item_status.enabled) {
				if(this.parent_index == this.parent.items.length-1)
					str+="\xC0";
				else 
					str+="\xC3";
			}
			else {
				if(this.parent_index == this.parent.items.length-1)
					str+=" ";
				else 
					str+="\xB3";
			}
		}
		/* draw tree expansion character */
		if(this.status == item_status.enabled) {
			str+=getColor(tree_settings.xfg);
			if(this.open) 
				str+="-";
			else
				str+="+";
		}
		else {
			str+=getColor(tree_settings.dfg);
			str+=" ";
		}
		/* restore item colors */
		str+=bg + fg;
		/* draw text */
		var c=0;	
		for(;c<this.text.length && (c+this.depth+2)<this.width;c++) {
			if(this.text[c]=="|") {
				if(this.status == item_status.disabled) 
					str+=this.text[++c];
				else {
					str+=getColor(tree_settings.kfg) + this.text[++c] + bg + fg;
					this.hotkey=this.text[c].toUpperCase();
				}
			}
			else {
				str+=this.text[c];
			}
		}
		this.putmsg(str,curr);
	}
	
	/* post a string to the window */
	this.putmsg=function(str,current) {
		if(this.list) {
			if(current)
				this.list.index=this.list.length;
			this.list.push(str);
		}
		else
			this.parent.putmsg(str,current)
	}
	
	/* handle a menu command */
	this.handle_command=function(cmd) {
		/* if this menu tree is empty */
		if(this.depth == 0 && !this.items.length)
			return false;
		/* initialize return value */
		var retval=false;
		if(retval == false) {
			/* check to see if the current tree item is a subtree */
			if(this.open && this.items[this.index] && 
				typeof this.items[this.index].handle_command == "function") 
				/* if so, pass control to the next subtree */
				retval=this.items[this.index].handle_command(cmd);
		}
		/* otherwise let this menu handle the command */
		if(retval == false) {
			/* process the command */
			retval=this.process_command(cmd);
			/* update current item's index if it is a tree */
			if(this.items[this.index] && typeof this.items[this.index].update_index == "function") 
				this.items[this.index].update_index(cmd);
		}
		if(retval == false) {
			/* attempt a character match on tree items */
			if(this.open)
				retval=this.match_hotkey(cmd);
		}
		if(this.depth == 0) {
			this.draw();
			if(typeof retval == "boolean")
				return false;
		}
		/* return whatever retval contains */
		return retval;
	}
	
	/* update this tree's index based on the command used to enter it */
	this.update_index=function(cmd) {
		if(!this.open) 
			this.index=-1;
		else {
			switch(cmd.toUpperCase()) {
			case KEY_UP:
				this.index=this.items.length-1;
				break;
			case KEY_DOWN:
				this.index=-1;
				break;
			}
		}
		/* update current item's index if it is a tree */
		if(this.items[this.index] && typeof this.items[this.index].update_index == "function") 
			this.items[this.index].update_index(cmd);
	}
	
	/* process a command key (called internally from handle_command only) */
	this.process_command=function(cmd) {
		/* if we are at the tree heading, 
			for status toggle */
		switch(cmd.toUpperCase()) {
		case '+':
			if(this.open)
				return true;
		case '\r':
			if(this.index < 0) {
				this.toggle();
				return true;
			}
			/* if the command has been sent here for processing
				and the current item could not handle it,
				then the current item's handle_command property
				is the return value */
			if(this.open) {
				if(this.items[this.index].action != undefined) {
					this.items[this.index].action();
					return true;
				}
				else
					return(this.items[this.index].command);
			}
			break;
		case KEY_DOWN:
			if(this.depth > 0 && (!this.open || this.index == this.items.length -1)) 
				return false;
			this.next_index(1);
			return true;
		case KEY_END:
			if(this.depth > 0 && !this.open) 
				return false;
			while(this.index < this.items.length -1)
				this.next_index(1);
			if(this.depth == 0)
				return true;
			else
				return false;
		case KEY_UP:
			if(this.depth > 0 && (!this.open || this.index == -1)) 
				return false;
			this.next_index(-1);
			return true;
		case KEY_HOME:
			if(this.depth > 0 && !this.open) 
				return false;
			while(this.index > 0)
				this.next_index(-1);
			if(this.depth == 0)
				return true;
			else
				return false;
		case '-':
		case '\b':
			if(this.depth > 0) {
				if(this.open) {
					this.toggle();
					return true;
				}
				return false;
			}
			break;
		}
		return false;
	}
	
	/* scan tree for possible hotkey matches */
	this.match_hotkey=function(cmd) {
		var pattern=new RegExp(cmd,"i");
		var stop=this.index>0?this.index-1:this.items.length-1;
		for(var i=this.index+1;;i++) {
			if(i == -1)
				continue;
			if(i >= this.items.length) 
				i=0;
			if(this.items[i].status==item_status.enabled && this.items[i].text.search(pattern) >= 0) {
				this.current=i;
				return true;
			}
			if(i == stop) 
				break;
		}	
		return false;
	}
	
	/* return the next valid menu index in the direction supplied */
	this.next_index=function(i) {
		var start=this.index;
		while(1) {
			this.index += i;
			if(this.index < -1)
				this.index = this.items.length-1;
			else if(this.index >= this.items.length) 
				this.index = -1;
			if(this.depth == 0 && this.index == -1) 
				continue;
			if(this.index == start)
				break;
			if(this.index == -1 || this.items[this.index].status == item_status.enabled)
				break;
		}
		return true;
	}

	/* calculate tree depth */
	this.depth getter=function() {
		if(!this.parent)
			return(0);
		else
			return(this.parent.depth+1);
	}
	
	/* trace back to main tree to see if this tree is the current item */
	this.current getter=function() {
		if(!this.parent)
			return true;
		else if(this.parent_index == this.parent.index)
			if(this.parent.current)
				return true;
		else
			return false;
	}
	
	/* trace back to main tree and set this menu current */
	this.current setter=function(i) {
		this.index=i;
		if(this.parent)
			this.parent.current=this.parent_index;
	}

	/* return x coordinate */
	this.x getter=function() {
		if(this.parent)
			return this.parent.x;
		else
			return x;
	}

	/* set x coordinate */
	this.x setter=function(xpos) {
		if(this.parent)
			this.parent.x=xpos;
		else
			x=xpos;
	}
	
	/* return y coordinate */
	this.y getter=function() {
		if(this.parent)
			return this.parent.y;
		else
			return y;
	}

	/* set y coordinate */
	this.y setter=function(ypos) {
		if(this.parent)
			this.parent.y=ypos;
		else
			y=ypos;
	}
	
	/* return tree width */
	this.width getter=function() {
		if(this.parent)
			return this.parent.width;
		else
			return width;
	}
	
	/* set tree width */
	this.width setter=function(w) {
		if(this.parent)
			this.parent.width=w;
		else 
			width=w;
	}
	
	/* return tree height */
	this.height getter=function() {
		if(this.parent)
			return this.parent.height;
		else
			return height;
	}
	
	/* set tree height */
	this.height setter=function(h) {
		if(this.parent)
			this.parent.height=h;
		else
			height=h;
	}
	
}

/* tree item object */
function Tree_Item(text,status,command,args) {
	/* 
	this.parent_index;
	this.parent;
	this.hotkey;
	*/
	if(text != undefined)
		this.text=text;
	else
		this.text="";
	if(status != undefined)
		this.status=status;
	else
		this.status=item_status.enabled;
	
	/* item return value or command handler */
	this.command=command;
	/* if a command handler is passed, 
	use it to handle our arguments (if any) */
	if(typeof command == "function") {
		this.action=function() {
			this.command.apply(this,this.args);
		}
	}
	/* command arguments */
	this.args=args;
	
	/* draw the current item text into the master list */
	this.draw=function(offset) {
		var str="";
		var curr=false;
		/* if this item is hidden, do not draw */
		if(this.status == item_status.hidden) 
			return false;
		/* set color attributes */
		var bg="\1n" + getColor(tree_settings.bg);
		var fg=getColor(tree_settings.fg);
		/* if this is the current tree and the current tree index */
		if(this.parent.current && this.parent_index == this.parent.index) {
			bg=getColor(tree_settings.lbg);
			fg=getColor(tree_settings.lfg);
			curr=true;
		}
		/* if this item is disabled */
		else if(this.status == item_status.disabled) {
			fg=getColor(tree_settings.dfg);
		}
		/* draw line offset */
		str+=bg + getColor(tree_settings.tfg)+offset;
		/* draw tree branches */
		if(this.parent.depth > 0) {
			if(this.status == item_status.enabled) {
				if(this.parent_index == this.parent.items.length-1)
					str+="\xC0\xC4";
				else 
					str+="\xC3\xC4";
			}
			else {
				if(this.parent_index == this.parent.items.length-1)
					str+="  ";
				else 
					str+="\xB3 ";
			}
		}
		/* restore item colors */
		str+=bg+fg;
		/* draw text */
		var c=0;
		var count=0;
		for(;c<this.text.length && (count+offset.length+2)<this.parent.width;c++) {
			if(this.text[c]=="|") {
				if(this.status == item_status.disabled) 
					str+=this.text[++c];
				else {
					str+=getColor(tree_settings.kfg) + this.text[++c] + bg + fg;
					this.hotkey=this.text[c].toUpperCase();
				}
			}
			else {
				if(this.text[c] == ctrl('A'))
					count--;
				else
					count++;
				str+=this.text[c];
			}
		}
		/* push the item string into the master display list */
		this.parent.putmsg(str,curr);
	}
}








