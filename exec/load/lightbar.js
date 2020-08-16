/*
 * Generic lightbar interface.
 * $Id: lightbar.js,v 1.48 2020/04/09 04:44:11 deuce Exp $
 */

/* ToDo: Support multiple columns */
require("sbbsdefs.js", 'SYS_CLOSED');
require("mouse_getkey.js", 'mouse_getkey');

/*
 * Lightbar object
 * Properties:
 *  xpos: Horizontal position of lightbar menu (1-based)
 *  ypos: Vertical position of lightbar menu (1-based)
 *  items: an array of objects each having the following properties:
 *         text - The displayed text.  A | prefixes a hotkey
 *         retval - The value to return if this is selected
 *       OPTIONAL Properties:
 *         width - The width of this item.  If not specified, is the width of
 *                 the text.  Otherwise, the text is truncated or padded with
 *				   spaces to fit the width.
 *         lpadding - the string that is displated before THIS item and is not
 *				      highlighted
 *         rpadding - the string that is displated after THIS item and is not
 *				      highlighted
 *         disabled - Indicates that this item is disabled and cannot be
 *				      selected
 *  direction: 0 for vertical, 1 for horizontal.
 *			   Horizontal menus always have one space of padding added between
 *             items.
 *  fg: Foreground colour of a non-current item
 *  bg: Background colour of a non-current item
 *  hfg: Foreground colour of a current item
 *  hbg: Background colour of a current item
 *  dfg: Foreground colour of a disabled item
 *  dbg: Background colour of a disabled item
 *  kfg: Hotkey forground colour for non-current item
 *  khfg: Hotkey foreground colour for current item
 *  current: Index of currently highlighted item
 *  align: If width is greater than the text length, a zero indicates the text
 *         should be left-aligned, a 1 indicates it should be right-aligned, and
 *         a 2 indicates it should be centered.
 *  force_width: forces the width of all items to this value.
 *  lpadding: this string is displayed before each item, and is not highlighted
 *  rpadding: this string is displayed AFTER each item, and is not highlighted
 *      NOTE: Padding is not included in width
 *  hblanks: The number of horizontal blanks between items for horizontal menus.
 *  hotkeys: A string of keys which will immediately return the key value rather
 *         than a retval
 *  nodraw: Indicates that the lightbar does not need to be redrawn on next entry.
 *			This is reset to false on every return
 */
function Lightbar(items)
{
	if(items==undefined)
		this.items=new Array();
	else
		this.items=items;
}
Lightbar.prototype.fg=7;
Lightbar.prototype.bg=1;
Lightbar.prototype.xpos=1;
Lightbar.prototype.ypos=1;
Lightbar.prototype.direction=0;
Lightbar.prototype.hfg=1;
Lightbar.prototype.hbg=7;
Lightbar.prototype.dfg=8;
Lightbar.prototype.dbg=1;
Lightbar.prototype.kfg=15;
Lightbar.prototype.khfg=15;
Lightbar.prototype.current=0;
Lightbar.prototype.align=0;
Lightbar.prototype.force_width=-1;
Lightbar.prototype.lpadding=null;
Lightbar.prototype.rpadding=null;
Lightbar.prototype.hblanks=2;
Lightbar.prototype.hotkeys='';
Lightbar.prototype.callback=undefined;
Lightbar.prototype.timeout=0;
Lightbar.prototype.mouse_miss_key=undefined;
Lightbar.prototype.mouse_miss_str=undefined;
Lightbar.prototype.mouse_enabled=false;

Lightbar.prototype.add = function(txt, retval, width, lpadding, rpadding, disabled, nodraw)
{
	var item=new Object;

	if(txt==undefined) {
		alert("Text of item undefined!");
		return;
	}
	item.text=txt;
	if(retval!=undefined)
		item.retval=retval;
	if(width!=undefined)
		item.width=width;
	if(lpadding!=undefined)
		item.lpadding=lpadding;
	if(rpadding!=undefined)
		item.rpadding=rpadding;
	if(disabled!=undefined)
		item.disabled=disabled;
	if(nodraw!=undefined)
		item.nodraw=nodraw;
	this.items.push(item);
};

Lightbar.prototype.clear = function()
{
	this.items=new Array();
};

Lightbar.prototype.failsafe_getval = function()
{
	var retval;
	for(i=0; i<this.items.length; i++) {
		if(this.items[i] != undefined && this.items[i].text != undefined) {
			writeln((i+1)+": "+this.items[i].text);
		}
	}
	write("Choose an option: ");
	i=console.getnum(i);
	i--;
	if(this.items[i]==undefined)
		return(null);
	if(this.items[i].retval==undefined)
		return(undefined);
	this.current=i;
	retval=this.items[i].retval;
	this.nodraw=false;

	return(retval);
};

/*
 * Super-Overlord Lightbar method... draws and returns selected value.
 */
Lightbar.prototype.getval = function(current,key)
{
	var loop=true;
	if(key) loop=false;
	var ret=undefined;
	var last_cur;
	var mk;

	if(!this.nodraw)
		this.draw();
	delete this.mouse_miss_str;

	/* Main loop */
	while(bbs.online) {
		last_cur=this.current;
		/* Get input */
		if(key==undefined || key=='' || key==null) {
			mk = mouse_getkey(K_NOSPIN, this.timeout > 1 ? this.timeout : undefined, this.mouse_enabled);
			if (mk.mouse !== null) {
				// Mouse
				if (mk.mouse.press && mk.mouse.mods === 0 && mk.mouse.button == 0 && mk.mouse.motion == 0) {
					key = 'Mouse';
					this.mouse_miss_str = mk.mouse.ansi;
				}
				else {
					key = undefined;
				}
			}
			else {
				key = mk.key.toUpperCase();
			}
		}

		else {
			if(this.hotkeys.indexOf(key)!=-1) {
				this.nodraw=false;
				return(key);
			}
			
			switch(key) {
				case 'Mouse':
					if (mk.mouse.button === 0) {
						var hit = this.mouse_hit(mk.mouse.x, mk.mouse.y);
						if (hit === -1) {
							if (this.mouse_miss_key !== undefined) {
								return this.mouse_miss_key;
							}
						}
						else {
							delete this.mouse_miss_str;
							this.draw(hit);
							this.nodraw=false;
							if(this.items[this.current].retval==undefined)
								return(undefined);
							return(this.items[this.current].retval);
						}
					}
					else
						delete this.mouse_miss_str;
					break;
				case KEY_UP:
					if(this.direction==0) {
						do {
							if(this.current==0)
								this.current=this.items.length;
							this.current--;
						} while(this.items[this.current].disabled || this.items[this.current].retval==undefined);
					}
					break;
				case KEY_DOWN:
					if(this.direction==0) {
						do {
							this.current++;
							if(this.current==this.items.length)
								this.current=0;
						} while(this.items[this.current].disabled || this.items[this.current].retval==undefined);
					}
					break;
				case KEY_LEFT:
					if(this.direction==1) {
						do {
							if(this.current==0)
								this.current=this.items.length;
							this.current--;
						} while(this.items[this.current].disabled || this.items[this.current].retval==undefined);
					}
					break;
				case KEY_RIGHT:
					if(this.direction==1) {
						do {
							this.current++;
							if(this.current==this.items.length)
								this.current=0;
						} while(this.items[this.current].disabled || this.items[this.current].retval==undefined);
					}
					break;
				case KEY_HOME:
					this.current=0;
					while(this.items[this.current].disabled || this.items[this.current].retval==undefined) {
						this.current++;
						if(this.current==this.items.length)
							this.current=0;
					}
					break;
				case KEY_END:
					this.current=this.items.length-1;
					while(this.items[this.current].disabled || this.items[this.current].retval==undefined) {
						if(this.current==0)
							this.current=this.items.length;
						this.current--;
					}
					break;
				case '\r':
				case '\n':
					this.nodraw=false;
					if(this.items[this.current].retval==undefined)
						return(undefined);
					return(this.items[this.current].retval);
					break;
				default:
					for(i=0; i<this.items.length; i++) {
						if((!this.items[i].disabled) && (this.items[i].text.indexOf('|'+key)!=-1 
						|| this.items[i].text.indexOf('|'+key.toLowerCase())!=-1)) {
							if(this.items[i].retval==undefined)
								continue;
							this.current=i;
							/* Let it go through once more to highlight */
							ret=this.items[this.current].retval;
							log("default case: " + key);
							break;
						}
					}
					break;
			}
			this.draw();
			if(ret!=undefined) {
				return(ret);
			}

			if(!loop) return key;
			key=undefined;
		} 
	}
};

Lightbar.prototype.mouse_hit = function(x, y)
{
	if(this.direction < 0 || this.direction > 1) {
		alert("Unknown lightbar direction!");
		return -1;
	}

	var i;
	var curx=this.xpos;
	var cury=this.ypos;

	for(i=0; i<this.items.length; i++) {
		var width;

		// Some basic validation.
		if(this.items[i]==undefined) {
			alert("Sparse items array!");
			return -1;
		}
		if(this.items[i].text==undefined) {
			alert("No text for item "+i+"!");
			return -1;
		}

		// Set up a cleaned version for length calculations.
		var cleaned=this.items[i].text;
		cleaned=cleaned.replace(/\|/g,'');

		/*
		 * Calculate the width.  Forced width, item width, text width
		 * In that order.  First one wins.
		 */
		if(this.force_width>0)
			width=this.force_width;
		else {
			if(this.items[i].width!=undefined)
				width=this.items[i].width;
			else
				width=cleaned.length;
		}

		var lpadding=this.lpadding;
		if(this.items[i].lpadding!=undefined)
			lpadding=this.items[i].lpadding;

		if(lpadding != undefined && lpadding != null) {
			curx+=lpadding.length;
		}

		if (cury === y) {
			if (x >= curx && x <= curx + width)
				return i;
		}
		curx += width;

		var rpadding=this.rpadding;
		if(this.items[i].rpadding!=undefined)
			rpadding=this.items[i].rpadding;
		curx += rpadding.length;

		if(this.direction==0) {
			cury++;
			curx=this.xpos;
		}
		else {
			curx += this.hblanks;
		}
	}
	return -1;
};

Lightbar.prototype.draw = function(current)
{
	var attr=this.bg<<4|this.fg;
	var cattr=this.hbg<<4|this.hfg;
	var dattr=this.dbg<<4|this.dfg;
	var kattr=this.bg<<4|this.kfg;
	var kcattr=this.hbg<<4|this.khfg;

	if(current!=undefined)
		this.current=current;
	if(!(user.settings & USER_ANSI)) {
		return;
	}
	if(!(user.settings & USER_COLOR)) {
		return;
	}

	if(this.direction < 0 || this.direction > 1) {
		alert("Unknown lightbar direction!");
		return;
	}

	/* Check that a vertical lightbar fits on the screen */
	if(this.direction==0 && (this.ypos+this.items.length-1 > console.screen_rows)) {
		alert("Screen too short for lightbar!");
		return;
	}

	/* Ensure current is valid */
	if(this.current<0 || this.current >= this.items.length) {
		alert("current parameter is out of range!");
		this.current=0;
		if(this.items.length<=0)
			return;
	}

	var orig_cur=this.current;
	while(this.items[this.current].retval==undefined || this.items[this.current].disabled) {
		this.current++;
		if(this.current==this.items.length)
			this.current=0;
		if(this.current==orig_cur) {
			alert("No items with a return value!");
			return;
		}
	}

	/* Check that a horizontal lightbar fits on the screen */
	/* ToDo: This is pretty expensive... do we NEED this?
	if(this.direction==1) {
		var end=this.xpos;
		var i;
		for(i=0; i<this.items.length; i++) {
			if(this.force_width>0) {
				end+=this.force_width+1;
				continue;
			}
			if(this.items[i].width==undefined) {
				if(this.items[i]==undefined) {
					alert("Sparse items array!");
					return;
				}
				if(this.items[i].text==undefined) {
					alert("No text for item "+i+"!");
					return;
				}
				var cleaned=this.items[i].text;
				cleaned=cleaned.replace(/\|/g,'');
				end+=cleaned.length+1;
			}
			else {
				end+=this.items[i].width
			}
		}
	} */

	var i;
	var j;
	var k;
	var ch;

	/* Draw items */
	var curx=this.xpos;
	var cury=this.ypos;
	var cursx=this.xpos;
	var cursy=this.ypos;
	var item_count=0;
	for(i=0; i<this.items.length; i++) {
		var width;

		// Some basic validation.
		if(this.items[i]==undefined) {
			alert("Sparse items array!");
			return;
		}
		if(this.items[i].text==undefined) {
			alert("No text for item "+i+"!");
			return;
		}

		// Set up a cleaned version for length calculations.
		var cleaned=this.items[i].text;
		cleaned=cleaned.replace(/\|/g,'');

		/*
		 * Calculate the width.  Forced width, item width, text width
		 * In that order.  First one wins.
		 */
		if(this.force_width>0)
			width=this.force_width;
		else {
			if(this.items[i].width!=undefined)
				width=this.items[i].width;
			else
				width=cleaned.length;
		}

		// Set up padding variables
		var lpadding=this.lpadding;
		var rpadding=this.rpadding;
		if(this.items[i].lpadding!=undefined)
			lpadding=this.items[i].lpadding;
		if(this.items[i].rpadding!=undefined)
			rpadding=this.items[i].rpadding;

		// Move to the start of this line
		console.gotoxy(curx, cury);

		// Output the padding
		if(lpadding != undefined && lpadding != null) {
			console.attributes=attr;
			console.write(lpadding);
			curx+=lpadding.length;
		}

		// If this is the current selection, remember the location as we'll
		// move the cursor back here later.
		if(i==this.current) {
			cursx=curx;
			cursy=cury;
		}

		/* Pad at the left if right aligned or centered */
		k=0;
		if(cleaned.length < width) {
			if(this.align==1 || this.align==2) {
				if(this.current==i)
					console.attributes=cattr;
				else {
					if(this.items[i].disabled)
						console.attributes=dattr;
					else
						console.attributes=attr;
				}
				for(;k<(width-cleaned.length)/this.align;k++)
					console.write(' ');
			}
		}
		
		// Output each char.
		for(j=0; j<this.items[i].text.length; j++) {
			// Check if we've gone past width somehow
			if(width > -1 && k > width)
				break;

			// Get next char.
			ch=this.items[i].text.substr(j,1);
			// Is this a hotkey?
			if(ch=='|') {
				// Yep, change attributes and get next char
				if(this.items[i].disabled)
					console.attributes=dattr;
				else {
					if(this.current==i)
						console.attributes=kcattr;
					else
						console.attributes=kattr;
				}
				j++;
				ch=this.items[i].text.substr(j,1);
			}
			else {
				// Make sure the attribute is correct.
				if(this.current==i)
					console.attributes=cattr;
				else {
					if(this.items[i].disabled)
						console.attributes=dattr;
					else
						console.attributes=attr;
				}
			}
			console.write(ch);
			k++;
		}
		
		// Set correct attribute for right width padding
		if(this.current==i)
			console.attributes=cattr;
		else {
			if(this.items[i].disabled)
				console.attributes=dattr;
			else
				console.attributes=attr;
		}

		// Add any required spaces to bring it up to width
		while(k<width) {
			console.write(" ");
			k++;
		}

		// Output the right padding
		if(rpadding != undefined && rpadding != null) {
			console.attributes=attr;
			console.write(rpadding);
			curx+= rpadding.length;
		}

		/*
		 * If we're vertical, move to next line.  Otherwise, write the correct
		 * number of spaces.
		 */
		if(this.direction==0) {
			cury++;
			curx=this.xpos;
		}
		else {
			console.attributes=attr;
			curx+=width;
			for(j=0; j<this.hblanks; j++) {
				console.write(" ");
				curx++;
			}
		}

		// This item is valid... make note of that.
		if(this.items[i].retval!=undefined)
			item_count++;
	}
	if(item_count==0) {
		alert("No items with a return value!");
		return;
	}
	
	// Move cursor to the current selection
	console.gotoxy(cursx,cursy);
};

Lightbar;
