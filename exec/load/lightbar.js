/*
 * Generic lightbar interface.
 * $Id$
 */

/* ToDo: Support multiple columns */
/* ToDo: Support "seperators" (Perhaps just make items with no retval not
 * ToDo: be selectable?) */
//if(SYS_CLOSED==undefined)
	load("sbbsdefs.js");

/*
 * Lightbar object
 * Properties:
 *  xpos: Horizontal position of lightbar menu (1-based)
 *  xpos: Vertical position of lightbar menu (1-based)
 *  items: an array of objects each having the following properties:
 *         text - The displayed text.  A | prefixes a hotkey
 *         retval - The value to return if this is selected
 *       OPTIONAL Properties:
 *         width - The width of this item.  If not specified, is the width of
 *                 the text.  Otherwise, the text is truncated or padded with
 *				   spaces to fit the width.
 *  direction: 0 for vertical, 1 for horizontal.
 *			   Horizontal menus always have one space of padding added between
 *             items.
 *  fg: Foreground colour of a non-current item
 *  bg: Background colour of a non-current item
 *  hfg: Foreground colour of a current item
 *  hbg: Background colour of a current item
 *  kfg: Hotkey forground colour for non-current item
 *  khfg: Hotkey foreground colour for current item
 *  current: Index of currently highlighted item (ToDo: This should be passed by reference (how?)!)
 *  align: If width is greater than the text length, a zero indicates the text
 *         should be left-aligned, a 1 indicates it should be right-aligned, and
 *         a 2 indicates it should be centered.
 */
function Lightbar(items)
{
	this.fg=7;
	this.bg=1;
	this.xpos=1;
	this.ypos=1;
	this.direction=0;
	this.hfg=1;
	this.hbg=7;
	this.kfg=15;
	this.khfg=15;
	this.current=0;
	this.align=0;
	this.getval=Lightbar_getval;
	this.clear=Lightbar_clearitems;
	this.add=Lightbar_additem;
	if(items==undefined)
		this.items=new Array();
	else
		this.items=items;
}

function Lightbar_additem(txt, retval, width)
{
	var item=new Object;

	if(txt==undefined) {
		alert("Text of item undefined!");
		return;
	}
	if(retval==undefined) {
		alert("Retval of item undefined!");
		return;
	}
	item.text=txt;
	item.retval=retval;
	if(width!=undefined)
		item.width=width;
	this.items.push(item);
}

function Lightbar_clearitems()
{
	this.items=new Array();
}

function Lightbar_getval(current)
{
	var retval;
	if(current!=undefined)
		this.current=current;
	retval=lightbar_func(this.xpos, this.ypos, this.items, this.direction, this.fg
			, this.bg, this.hfg, this.hbg, this.kfg, this.khfg, this.current
			, this.align);
	if(retval==null) {
		for(i=0; i<this.items.length; i++) {
			if(this.items[i] != undefined && this.items[i].text != undefined) {
				writeln((i+1)+": "+this.items[i].text);
			}
		}
		write("Choose an option: ");
		i=console.getnum(i);
		if(this.items[i]==undefined)
			return(null);
		if(this.items[i].retval==undefined)
			return(undefined);
		retval=this.items[i].retval;
	}

	for(i=0; i<this.items.length; i++) {
		if(retval==this.items[i].retval) {
			this.current=i;
			break;
		}
	}
	return(retval);
}

/*
 * Super-Overlord Lightbar function... draws and returns selected value.
 * Parameters:
 *  xpos: Horizontal position of lightbar menu (1-based)
 *  xpos: Vertical position of lightbar menu (1-based)
 *  items: an array of objects each having the following properties:
 *         text - The displayed text.  A | prefixes a hotkey
 *         retval - The value to return if this is selected
 *       OPTIONAL Properties:
 *         width - The width of this item.  If not specified, is the width of
 *                 the text.  Otherwise, the text is truncated or padded with
 *				   spaces to fit the width.
 *  direction: 0 for vertical, 1 for horizontal.
 *			   Horizontal menus always have one space of padding added between
 *             items.
 *  fg: Foreground colour of a non-current item
 *  bg: Background colour of a non-current item
 *  hfg: Foreground colour of a current item
 *  hbg: Background colour of a current item
 *  kfg: Hotkey forground colour for non-current item
 *  khfg: Hotkey foreground colour for current item
 *  current: Index of currently highlighted item (ToDo: This should be passed by reference (how?)!)
 *  align: If width is greater than the text length, a zero indicates the text
 *         should be left-aligned, a 1 indicates it should be right-aligned, and
 *         a 2 indicates it should be centered.
 * Return Values:
 *  null for error, retval from the selected item for non-errors
 */
function lightbar_func(xpos, ypos, items, direction, fg, bg, hfg, hbg, kfg, khfg, current, align)
{
	var attr=bg<<4|fg;
	var cattr=hbg<<4|hfg;
	var kattr=bg<<4|kfg;
	var kcattr=hbg<<4|khfg;
	var ret=undefined;

	if(!(user.settings & USER_ANSI)) {
		alert("ANSI not supported for lightbar!");
		return(null);
	}
	if(!(user.settings & USER_COLOR)) {
		alert("Colour not supported for lightbar!");
		return(null);
	}

	if(direction < 0 || direction > 1) {
		alert("Unknown lightbar direction!");
		return(null);
	}

	/* Check that a vertical lightbar fits on the screen */
	if(direction==0 && (ypos+items.length-1 > console.screen_rows)) {
		alert("Screen too short for lightbar!");
		return(null);
	}

	/* Ensure current is valid */
	if(current<0 || current >= items.length) {
		alert("current parameter is out of range!");
		return(null);
	}

	/* Check that a horizontal lightbar fits on the screen */
	if(direction==1) {
		var end=xpos;
		var i;
		for(i=0; i<items.length; i++) {
			if(items[i].width==undefined) {
				if(items[i]==undefined) {
					alert("Sparse items array!");
					return(null);
				}
				if(items[i].text==undefined) {
					alert("No text for item "+i+"!");
					return(null);
				}
				var cleaned=items[i].text;
				cleaned=cleaned.replace(/\|/g,'');
				end+=cleaned.length+1;
			}
			else {
				end+=items[i].width
			}
		}
	}

	/* Main loop */
	while(1) {
		var i;
		var j;

		/* Draw items */
		var curx=xpos;
		var cury=ypos;
		var cursx=xpos;
		var cursy=ypos;
		for(i=0; i<items.length; i++) {
			var width;
			var cleaned=items[i].text;

			cleaned=cleaned.replace(/\|/g,'');
			width=cleaned.length;
			if(items[i]==undefined) {
				alert("Sparse items array!");
				return(null);
			}
			if(items[i].text==undefined) {
				alert("No text for item "+i+"!");
				return(null);
			}
			if(items[i].width!=undefined)
				width=items[i].width;
			console.gotoxy(curx, cury);
			if(i==current) {
				cursx=curx;
				cursy=cury;
			}
			j=0;
			if(cleaned.length < width) {
				if(align==1) {
					if(current==i)
						console.attributes=cattr;
					else
						console.attributes=attr;
					for(;j<width-cleaned.length;j++)
						console.write(' ');
				}
				if(align==2) {
					if(current==i)
						console.attributes=cattr;
					else
						console.attributes=attr;
					for(;j<(width-cleaned.length)/2;j++)
						console.write(' ');
				}
			}
			for(; j<items[i].text.length; j++) {
				if(width > -1 && j > width)
					break;
				if(items[i].text.substr(j,1)=='|') {
					if(current==i)
						console.attributes=kcattr;
					else
						console.attributes=kattr;
					j++;
				}
				else {
					if(current==i)
						console.attributes=cattr;
					else
						console.attributes=attr;
				}
				console.write(items[i].text.substr(j,1));
			}
			if(current==i)
				console.attributes=cattr;
			else
				console.attributes=attr;
			while(j<width) {
				console.write(" ");
				j++;
			}
			if(direction==0)
				cury++;
			else {
				console.attributes=attr;
				console.write(" ");
				curx+=width+1;
			}
		}
		console.gotoxy(cursx,cursy);
		if(ret!=undefined)
			return(ret);

		/* Get input */
		var key=console.getkey(K_UPPER);
		switch(key) {
			case KEY_UP:
				if(direction==0) {
					if(current==0)
						current=items.length;
					current--;
				}
				break;
			case KEY_DOWN:
				if(direction==0) {
					current++;
					if(current==items.length)
						current=0;
				}
				break;
			case KEY_LEFT:
				if(direction==1) {
					if(current==0)
						current=items.length;
					current--;
				}
				break;
			case KEY_RIGHT:
				if(direction==1) {
					current++;
					if(current==items.length)
						current=0;
				}
				break;
			case KEY_HOME:
				current=0;
				break;
			case KEY_END:
				current=items.length-1;
				break;
			case '\r':
			case '\n':
				if(items[current].retval==undefined)
					return(undefined);
				return(items[current].retval);
				break;
			default:
				for(i=0; i<items.length; i++) {
					if(items[i].text.indexOf('|'+key)!=-1) {
						current=i;
						if(items[current].retval==undefined)
							return(undefined);
						/* Let it go through once more to highlight */
						ret=items[current].retval;
						break;
					}
				}
				break;
		}
	}
}
