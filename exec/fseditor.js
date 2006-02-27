/* ToDo: At what point should trailing whitespace be removed? */

var line=new Array();
var xpos=0;									/* Current xpos of insert point */
var last_xpos=-1;							/* The xpos you'd like to be at.  -1 when not valid
												Used to retain horiz. position when moving vertically */
var ypos=0;									/* Current index into line[] of insert point */
var insert=true;							/* Insert mode */
var topline=0;								/* Index into line[] of line at edit_top */
var edit_top=2;								/* First line of edit window */
var edit_bottom=console.screen_rows;		/* Last line of edit window */
var lines_on_screen=edit_bottom-edit_top+1;	/* Height of edit window */
var curattr=7;								/* Current attribute */
/* Create an initial line... this may not be necessary when replying */
line[0]=new Line();

function Line()
{
	var i;

	/* The actual text of this line */
	this.text='';
	/* The attributes of this line */
	this.attr='';
	/* If this line ends with a "hard" CR */
	this.hardcr=false;
	/* This line was kludge-wrapped (ie: not on a word boundary) */
	this.kludged=false;
	/* Start of actual text (after quote indicator) */
	this.firstchar=0;
}

/*
 * Draws a line on a screen.  l is the index into line[], x is where in
 * the line drawing needs to start.  clear is a boolean indicating if text
 * from the end of the line needs
 * to be cleared (ie: the line is now shorter)
 */
function draw_line(l,x,clear)
{
	var i;
	var yp;

	if(x==undefined || x < 0 || isNaN(x))
		x=0;
	if(x>79)
		x=79;
	if(clear==undefined)
		clear=true;
	if(l==undefined || isNaN(l))
		return;
	yp=l-topline+edit_top;
	/* Does this line even exist? */
	if(line[l]==undefined) {
		console.gotoxy(x+1,yp);
		console.cleartoeol();
	}
	else {
		/* Is line on the current screen? */
		if(yp<edit_top)
			return;
		if(yp>edit_bottom)
			return;

		/* ToDo we need to optimize cursor movement somehow... */
		console.gotoxy(x+1,yp);
		for(; x<line[l].text.length && x<80; x++) {
			console.attributes=ascii(line[l].attr.substr(x,1));
			console.write(line[l].text.substr(x,1));
		}
		if(clear && x<80) {
			console.attributes=7;
			console.cleartoeol();
		}
	}
}

/*
 * Scrolls edit window up count lines.  Allows negative scrolling
 */
function scroll(count)
{
	var i;

	topline+=count;
	if(topline+lines_on_screen>=line.length)
		topline=line.length-lines_on_screen;
	if(topline<0)
		topline=0;
	for(i=0; i<lines_on_screen; i++)
		draw_line(i+topline);
}

/*
 * Adds a new line below line l.
 */
function add_new_line_below(l)
{
	var i;

	line.splice(l+1,0,new Line());
	/* Scroll lines below down (if on display) */
	for(i=l+1; i<topline+lines_on_screen; i++) {
		/* No need to scroll 'em if they aren't there... we're inserting remember? */
		if(line[i]!=undefined)
			draw_line(i);
	}
}

/*
 * Advances to the next line creating a new line if necessary.
 */
function next_line()
{
	ypos++;
	if(line[ypos]==undefined)
		line.push(new Line());
	if(ypos-topline>=lines_on_screen)
		scroll(1);		/* Scroll up one line */
	if(last_xpos>=0)
		xpos=last_xpos;
	if(xpos>line[ypos].text.length)
		xpos=line[ypos].text.length;
}

/*
 * Moves to the previous line if it's possible to do so.
 * Will not create another line.
 */
function try_prev_line()
{
	/* Yes, it's possible */
	if(ypos>0) {
		ypos--;
		if(ypos<topline)
			scroll(-1);		/* Scroll down one line */
		if(last_xpos>=0)
			xpos=last_xpos;
		if(xpos>line[ypos].text.length)
			xpos=line[ypos].text.length;
		return(true);
	}
	console.beep();
	return(false);
}

/*
 * Advances to the next line if it's possible to do so.
 * Will not create another line.
 */
function try_next_line()
{
	/* Yes, it's possible */
	if(ypos<line.length-1) {
		ypos++;
		if(ypos>=topline+lines_on_screen)
			scroll(1);		/* Scroll up one line */
		if(last_xpos>=0)
			xpos=last_xpos;
		if(xpos>line[ypos].text.length)
			xpos=line[ypos].text.length;
		return(true);
	}
	console.beep();
	return(false);
}

/* ToDo: Optimize cursor movement */
function set_cursor()
{
	var x;
	var y;

	y=edit_top+(ypos-topline);
	x=xpos+1;
	if(xpos>80)
		xpos=80;
	console.gotoxy(x,y);
}

var lastinsert=false;
function status_line()
{
	if(insert != lastinsert) {
		lastinsert=insert;
		console.gotoxy(1,1);
		if(insert)
			console.write("Insert Mode");
		else
			console.write("Overwrite Mode");
		console.cleartoeol();
		set_cursor();
	}
}

/*
 * "Unwraps" lines starting at l.  Appends stuff from the next line(s) unless there's a
 * "hard" CR.  Returns the number of chars moved from line[l+1] to line[l]
 */
function unwrap_line(l)
{
	var done=false;
	var words;
	var space;
	var re;
	var lines=0;
	var ret=-1;
	var first=true;

	while(!done && line[l+1]!=undefined) {
		if(line[l]==undefined) {
			draw_line(l);
			l++;
			break;
		}
		/* There's a hardcr... all done now. */
		if(line[l].hardcr)
			break;
		space=79-line[l].text.length;
		if(space<1)
			break;
		/* Attempt to unkludge... */
		if(line[l].kludged) {
			re=new RegExp("^([^\\s]{1,"+space+"}\\s*)","");
			words=line[l+1].text.match(re);
			if(words != null) {
				line[l].text+=words[1];
				line[l].attr+=line[l+1].attr.substr(0,words[1].length);
				line[l+1].text=line[l+1].text.substr(words[1].length);
				line[l+1].attr=line[l+1].attr.substr(words[1].length);
				space-=words[1].length;
				if(first)
					ret=words[1].length;
				if(line[l+1].text.length==0) {
					line[l].kludged=false;
					line[l].hardcr=line[l+1].hardcr;
					line.splice(l+1,1);
				}
				if(words[1].search(/\s/)!=-1)
					line[l].kludged=false;
				lines++;
			}
			else
				line[l].kludged=false;
		}
		/* Get first word(s) of next line */
		re=new RegExp("^(.{1,"+space+"}(?![^\\s])\\s*)","");
		words=line[l+1].text.match(re);
		if(words != null) {
			line[l].text+=words[1];
			line[l].attr+=line[l+1].attr.substr(0,words[1].length);
			line[l+1].text=line[l+1].text.substr(words[1].length);
			line[l+1].attr=line[l+1].attr.substr(words[1].length);
			if(first) {
				if(ret==-1)
					ret=words[1].length;
				else
					ret+=words[1].length;
			}
			if(line[l+1].text.length==0) {
				line[l].hardcr=line[l+1].hardcr;
				line.splice(l+1,1);
			}
			lines++;
		}
		else {
			/* Nothing on the next line... can't be kludged */
			if(line[l].kludged)
				line[l].kludged=false;
			if(line[l+1].text.length==0) {
				line[l].hardcr=line[l+1].hardcr;
				line.splice(l+1,1);
				/* 
				 * We deleted a line... now we need to redraw that line
				 *  and everything after it.
				 */
				var i;
				for(i=l+1;i<topline+lines_on_screen;i++)
					draw_line(i);
			}
			done=true;
		}
		if(lines>0)
			draw_line(l);
		l++;
		first=false;
	}
	if(lines>0)
		draw_line(l);
	return(ret);
}

/*
 * Wraps lines starting at l.  Returns the offset in line# l+1 where
 * the end of line# l ends or -1 if no wrap was performed.
 */
function wrap_line(l)
{
	var ret=-1;
	var nline;
	var m;
	var m2;

	while(1) {
		/* Get line length without trailing whitespace */
		m=line[l].text.match(/^(.*?)\s*$/);
		if(m!=null) {
			if(m[1].length<80 && (m[1].length!=line[l].text.length || line[l+1]==undefined || line[l].hardcr==true)) {
				if(ret!=-1)
					draw_line(l);
				return(ret);
			}
		}
		m=line[l].text.match(/^(.{1,79}\s+)([^\s].*?)$/);
		if(m==null) {
			/*
			 * We couldn't apparently find a space after a char... this means
			 * we need to kludge wrap this (ick)
			 */
			m=line[l].text.match(/^(.{1,79})(.*?)$/);
			line[l].kludged=true;
		}
		if(m!=null) {
			if(line[l+1]==undefined)
				line.push(new Line);
			line[l+1].text=m[2]+line[l+1].text;
			line[l+1].attr=line[l].attr.substr(m[1].length)+line[l+1].attr;
			line[l].text=m[1];
			line[l].attr=line[l].attr.substr(0,line[l].text.length);
			draw_line(l);
			if(ret==-1)
				ret=m[2].length;
			l++;
		}
		else {
			/* This can't happen */
			alert("The impossible happened.");
			console.pause();
			exit();
		}
	}
	return(ret);
}

/*
 * Calls unwrap_line(ypos-1), unwrap_line(ypos) and wrap_line(ypos) and adjusts xpos/ypos accordingly.
 * Should be called after every modification (sheesh)
 *
 * Here's the deal... first, we try to unwrap the PREVIOUS line.  If that works, we're done.
 * Otherwise, we unwrap the CURRENT line... if that fails, then we WRAP the current line.
 *
 * The various cant_* means it's flat out impossible for that thing to succeed.  For example, a single
 * char delete cannot possibly cause a wrap (actually, that's not true... thanks to kludgewrapping)
 * It's seriously unlikely that any of these cant_* variables can ever be used.
 *
 * Returns TRUE if anything was done.
 *
 * This DOES assume that the various *wrap_line calls will remove the need for the rest of them.
 */
function rewrap(cant_unwrap_prev, cant_unwrap, cant_wrap)
{
	var i;
	var moved=false;
	var redrawn=false;
	var oldlen;

	if(line[ypos]==undefined)
		return;
	if(cant_unwrap_prev==undefined)
		cant_unwrap_prev=false;
	if(cant_unwrap==undefined)
		cant_unwrap=false;
	if(cant_wrap==undefined)
		cant_wrap=false;
	if(!cant_unwrap_prev) {
		if(ypos>0) {
			oldlen=line[ypos-1].text.length;
			if(!line[ypos-1].hardcr) {
				i=unwrap_line(ypos-1);
				if(i!=-1) {
					/*
					 * ToDo: Not actually sure if it's possible to have
					 * xpos and i differ at all
					 */
					if(xpos<=i) {
						xpos=line[ypos-1].text.length-(i-xpos);
						try_prev_line();
						moved=true;
					}
					redrawn=true;
				}
			}
		}
	}
	oldlen=line[ypos].text.length;
	if(!cant_unwrap) {
		if(!moved) {
			i=unwrap_line(ypos);
			if(i!=-1) {
				moved=true;
				redrawn=true;
			}
		}
	}
	if(!cant_wrap) {
		if(!moved && ypos<line.length) {
			i=wrap_line(ypos);
			if(i!=-1) {
				if(xpos>line[ypos].text.length) {
					xpos=xpos-oldlen+i;
					next_line();
					moved=true;
					redrawn=true;
				}
			}
		}
	}
	return(redrawn);
}

/* ToDo: Optimize movement... */
function edit()
{
	var key;

	set_cursor();
	while(1) {
		status_line();
		key=console.inkey(0,10000);
		if(key=='')
			continue;
		switch(key) {
			/* We're getting all the CTRL keys here... */
			case '\x00':	/* CTRL-@ (NULL) */
			case '\x01':	/* CTRL-A */
				break;
			case '\x02':	/* KEY_HOME */
				xpos=0;
				set_cursor();
				break;
			case '\x03':	/* CTRL-C */
			case '\x04':	/* CTRL-D */
				break;
			case '\x05':	/* KEY_END */
				last_xpos=-1;
				xpos=line[ypos].text.length;
				set_cursor();
				break;
			case '\x06':	/* KEY_RIGHT */
				last_xpos=-1;
				xpos++;
				if(xpos>line[ypos].text.length) {
					xpos--;
					console.beep();
				}
				if(xpos>line[ypos].text.length) {
					if(line[ypos].hardcr)
						console.beep();
					else {
						if(try_next_line())
							x=0;
					}
				}
				set_cursor();
				break;
			case '\x07':	/* CTRL-G (Beep) */
				break;
			case '\x08':	/* Backspace */
				last_xpos=-1;
				if(xpos>0) {
					line[ypos].text=line[ypos].text.substr(0,xpos-1)
							+line[ypos].text.substr(xpos);
					line[ypos].attr=line[ypos].attr.substr(0,xpos-1)
							+line[ypos].attr.substr(xpos);
					xpos--;
				}
				else {
					if(try_prev_line()) {
						xpos=line[ypos].text.length;
						line[ypos].hardcr=false;
					}
				}
				if(!rewrap())
					draw_line(ypos,xpos);
				set_cursor();
				break;
			case '\x09':	/* TAB... ToDo expand to spaces */
				break;
			case '\x0a':	/* KEY_DOWN */
				if(last_xpos==-1)
					last_xpos=xpos;
				try_next_line();
				set_cursor();
				break;
			case '\x0b':	/* CTRL-K */
			case '\x0c':	/* CTRL-L */
			case '\x0d':	/* CR */
				last_xpos=-1;
				if(insert) {
					add_new_line_below(ypos);
					/* Move all this to the next line... */
					line[ypos+1].text=line[ypos].text.substr(xpos);
					line[ypos+1].attr=line[ypos].attr.substr(xpos);
					line[ypos+1].hardcr=line[ypos].hardcr;
					line[ypos].text=line[ypos].text.substr(0,xpos);
					line[ypos].attr=line[ypos].attr.substr(0,xpos);
					line[ypos].hardcr=true;
					draw_line(ypos,xpos);
					if(line[ypos].kludged) {
						line[ypos+1].kludged=true;
						line[ypos].kludged=false;
					}
					draw_line(ypos+1);
				}
				try_next_line();
				xpos=0;
				rewrap();
				set_cursor();
				break;
			case '\x0e':	/* CTRL-N */
			case '\x0f':	/* CTRL-O */
			case '\x10':	/* CTRL-P */
			case '\x11':	/* CTRL-Q (XOff) */
				return;
			case '\x12':	/* CTRL-R */
			case '\x13':	/* CTRL-S (Xon)  */
			case '\x14':	/* CTRL-T */
			case '\x15':	/* CTRL-U */
			case '\x16':	/* CTRL-V */
				insert=!insert;
				break;
			case '\x17':	/* CTRL-W */
			case '\x18':	/* CTRL-X */
			case '\x19':	/* CTRL-Y */
			case '\x1a':	/* CTRL-Z (EOF) */
			case '\x1b':	/* ESC */
			case '\x1c':	/* CTRL-\ */
				break;
			case '\x1d':	/* KEY_LEFT */
				last_xpos=-1;
				xpos--;
				if(xpos<0) {
					console.beep();
					xpos=0;
				}
				set_cursor();
				break;
			case '\x1e':	/* KEY_UP */
				if(last_xpos==-1)
					last_xpos=xpos;
				try_prev_line();
				set_cursor();
				break;
			case '\x1f':	/* CTRL-_ */
				break;
			case '\x7f':	/* DELETE */
				last_xpos=-1;
				if(xpos>=line[ypos].text.length)
					/* delete the hardcr, */
					line[ypos].hardcr=false;
				line[ypos].text=line[ypos].text.substr(0,xpos)
						+line[ypos].text.substr(xpos+1);
				line[ypos].attr=line[ypos].attr.substr(0,xpos)
						+line[ypos].attr.substr(xpos+1);
				if(!rewrap())
					draw_line(ypos,xpos);
				set_cursor();
				break;
			default:		/* Insert the char */
				last_xpos=-1;
				/*
				 * A whitespace at the beginning of a line following a kludge wrap
				 * unkludges it.
				 */
				if(xpos==0 && ypos>0 && key.search(/\s/)!=-1)
					line[ypos-1].kludged=false;
				if(insert) {
					line[ypos].text=line[ypos].text.substr(0,xpos)
							+key
							+line[ypos].text.substr(xpos);
					line[ypos].attr=line[ypos].attr.substr(0,xpos)
							+ascii(curattr)
							+line[ypos].attr.substr(xpos);
				}
				else {
					line[ypos].text=line[ypos].text.substr(0,xpos)
							+key
							+line[ypos].text.substr(xpos+1);
					line[ypos].attr=line[ypos].attr.substr(0,xpos)
							+key
							+line[ypos].attr.substr(xpos+1);
				}
				xpos++;
				if(!rewrap())
					draw_line(ypos,xpos-1,false);
				set_cursor();
		}
	}
}

var oldpass=console.ctrlkey_passthru;
console.ctrlkey_passthru="+PV";
edit();
console.ctrlkey_passthru=oldpass;
