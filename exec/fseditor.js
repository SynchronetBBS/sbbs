/* ToDo: At what point should trailing whitespace be removed? */

load("sbbsdefs.js");

var REVISION = "1.110";
require("text.js", 'FileNotReceived');

if(!bbs.mods.userprops)
	bbs.mods.userprops = load({}, "userprops.js");

var line=new Array();
var quote_line=new Array();
var xpos=0;									/* Current xpos of insert point */
var last_xpos=-1;							/* The xpos you'd like to be at.  -1 when not valid
												Used to retain horiz. position when moving vertically */
var ypos=0;									/* Current index into line[] of insert point */
var insert=true;							/* Insert mode */
var topline=0;								/* Index into line[] of line at edit_top */
var edit_top=5;								/* First line of edit window */
var edit_bottom=console.screen_rows-1;		/* Last line of edit window */
var lines_on_screen=edit_bottom-edit_top+1;	/* Height of edit window */
var scroll_lines=parseInt(lines_on_screen/3);	/* Number of lines to scroll by in one jump */
var curattr=7;								/* Current attribute */
var colour_box_displayed=0;					/* Row the colour box is displayed
												on. used for redraw */
var graphics_box_displayed=0;				/* Row the graphic box is displayed on */
var quote_window_displayed=0;				/* Row the quote window is displayed on */
var quote_topline=0;						/* Current index into quote_line[] of quote_top */
var quote_ypos=0;							/* Current index into line[] of selection point */
var quote_height=0;							/* Number of quote lines to be displayed */
var quote_scroll=0;							/* Number of lines to scroll by in one jump */
var quote_sep_pos=0;						/* Line number the quote seperator is displayed on */
var quote_ontop=false;						/* true if quote window is at the top */
var quote_top;								/* Line number of the first quote line */
var quote_bottom;							/* Line number of the last quote line */
var drop_file;
var info;

// Message header display format
var hdr_fmt	= "\1b\1h%-4s\1n\1b: \1h\1c%.*s\1>\r\n";
var hdr_field_width = console.screen_columns - 7;
var stat_attr	= 0x1f;
var stat_fmt	= "\1h\1w\0014 FSEditor v" + REVISION + " - Type \1yCTRL-K\1w for help         %s\1>\1n";
var subj,to,from;

var options = load('modopts.js', 'fseditor');
if(!options)
	options = {};
if(!options.default_tabstop)
	options.default_tabstop = 8;

var pmode = 0;
if(console.term_supports(USER_UTF8))
	pmode |= P_UTF8;

var tab_width = bbs.mods.userprops.get("fseditor", "tabstop", options.default_tabstop);

/* Spell checking (see load/fseditor_spell.js) */
require("userdefs.js", 'UREST_EDIT_DEFAULTS');
var spell = load({}, "fseditor_spell.js");
/* Sysop master switch: spell_enabled=false in the [fseditor] section of modopts.ini */
var spell_enabled = (options.spell_enabled === false || String(options.spell_enabled).toLowerCase() == "false") ? false : true;
var spell_opts = {
	before_send:	spell_enabled && bbs.mods.userprops.get("fseditor", "spell_before_send", options.default_spell_before_send ? true : false),
	inline:			spell_enabled && bbs.mods.userprops.get("fseditor", "spell_inline", options.default_spell_inline ? true : false),
	language:		bbs.mods.userprops.get("fseditor", "spell_language", options.default_spell_language || "en-US"),
	skip_quotes:	bbs.mods.userprops.get("fseditor", "spell_skip_quotes", options.default_spell_skip_quotes === false ? false : true),
	skip_caps:		bbs.mods.userprops.get("fseditor", "spell_skip_caps", options.default_spell_skip_caps === false ? false : true)
};
var spell_ignore = {};		/* Words ignored for this editing session ("$word" -> true) */
var spell_gen = 0;			/* Bumped whenever cached inline results become stale */

function Line(copyfrom)
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
	/* For selection */
	this.selected=false;
	if (copyfrom != undefined) {
		this.text = copyfrom.text;
		this.attr = copyfrom.attr;
		this.hardcr = copyfrom.hardcr;
		this.kludged = copyfrom.kludged && copyfrom.text.length > 0;
		this.firstchar = copyfrom.firstchar;
		this.selected = copyfrom.selected;
	}
}

function ch_to_string(c)
{
	var derp;
	var i;
	var ret = '';

	if (pmode & P_UTF8) {
		derp = utf8_encode(c.charCodeAt(0));
		for (i = 0; i < derp.length; i++)
			ret += derp[i];
	}
	else {
		ret += c;
	}
	return ret;
}

function shitty_putc(c)
{
	var derp;
	var i;

	if (pmode & P_UTF8) {
		derp = utf8_encode(c.charCodeAt(0));
		for (i = 0; i < derp.length; i++)
			console.putbyte(ascii(derp[i]));
	}
	else {
		console.putbyte(ascii(c));
	}
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
	if(x>=console.screen_columns)
		x=console.screen_columns-1;
	if(clear==undefined)
		clear=true;
	if(l==undefined || isNaN(l))
		return;
	yp=l-topline+edit_top;
	/* Does this line even exist? */
	if(line[l]==undefined) {
		console.attributes=7;
		console.gotoxy(x+1,yp);
		console.cleartoeol();
	}
	else {
		/* Is line on the current screen? */
		if(yp<edit_top)
			return;
		if(yp>edit_bottom)
			return;

		console.gotoxy(x+1,yp);
		var sp=null;
		if(spell_opts.inline && spell.is_loaded(spell_opts.language))
			sp=spell_marks(l);
		for(; x<line[l].text.length && x<(console.screen_columns-1); x++) {
			var a=ascii(line[l].attr.substr(x,1));
			if(sp!==null && sp.sp_marks[x] !== undefined
			    && !(l==ypos && sp.sp_marks[x].start<=xpos && xpos<=sp.sp_marks[x].end))
				a=(a&0xf0)|RED|HIGH;
			console.attributes=a;
			shitty_putc(line[l].text[x]);
		}
		if(clear && x<(console.screen_columns-1)) {
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
		scroll(scroll_lines);		/* Scroll up */
	if(last_xpos>=0)
		xpos=last_xpos;
	var text_length = line[ypos].text.length;
	if(xpos>text_length)
		xpos=text_length;
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
			scroll(0-scroll_lines);		/* Scroll down */
		if(last_xpos>=0)
			xpos=last_xpos;
		var text_length = line[ypos].text.length;
		if(xpos>text_length)
			xpos=text_length;
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
			scroll(scroll_lines);		/* Scroll up */
		if(last_xpos>=0)
			xpos=last_xpos;
		var text_length = line[ypos].text.length;
		if(xpos>text_length)
			xpos=text_length;
		return(true);
	}
	console.beep();
	return(false);
}

function set_cursor()
{
	var x;
	var y;

	y=edit_top+(ypos-topline);
	x=xpos+1;
	if(xpos>console.screen_columns)
		xpos=console.screen_columns;
	console.gotoxy(x,y);
	console.attributes=curattr;
}

var lastinsert=false;
function status_line()
{
	console.gotoxy(1,console.screen_rows);
	printf(stat_fmt,(insert?"Insert Mode    ":"Overwrite Mode "));
	console.attributes=curattr;
	console.write(" Current Colour ");
	console.attributes=stat_attr;
	console.cleartoeol();
	set_cursor();
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
	var old_lines=line.length;

	/* An empty line cannot end in the middle of a word. */
	if(line[l] != undefined && line[l].text.length == 0)
		line[l].kludged=false;
	while(!done && line[l+1]!=undefined) {
		if(line[l]==undefined) {
			draw_line(l);
			l++;
			break;
		}
		if(line[l].text.length == 0)
			line[l].kludged=false;
		/* There's a hardcr... all done now. */
		if(line[l].hardcr)
			break;
		space=(console.screen_columns-1)-line[l].text.length;
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
		/* The unkludge above may have consumed and removed the next line. */
		if(line[l+1] == undefined || line[l].hardcr)
			break;
		/* Get first word(s) of next line */
		if(space < 1) {
			words=null;
		}
		else {
			re=new RegExp("^(.{1,"+space+"}(?![^\\s])\\s*)","");
			words=line[l+1].text.match(re);
		}
		if(words != null) {
			line[l].text+=words[1];
			line[l].attr+=line[l+1].attr.substr(0,words[1].length);
			if (line[l+1] !== undefined) {
				line[l+1].text=line[l+1].text.substr(words[1].length);
				line[l+1].attr=line[l+1].attr.substr(words[1].length);
			}
			if(first) {
				if(ret==-1)
					ret=words[1].length;
				else
					ret+=words[1].length;
			}
			if(line[l+1] !== undefined && line[l+1].text.length==0) {
				line[l].hardcr=line[l+1].hardcr;
				line.splice(l+1,1);
			}
			lines++;
		}
		else {
			/* Nothing on the next line... can't be kludged */
			if(line[l].kludged)
				line[l].kludged=false;
			// Make sure we didn't splice() the next line out...
			if(line[l+1] !== undefined && line[l+1].text.length==0) {
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
	if(lines>0) {
		draw_line(l++);
	}
	if(old_lines!=line.length) {
		/* We need to redraw everything... line(s) deleted */
		for(;l<=line.length;l++)
			draw_line(l);
	}
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
	var i;
	var first_check=new RegExp("^(.{1,"+(console.screen_columns-1)+"}\\s+)([^\\s].*?)$");
	var kludge_check=new RegExp("^(.{1,"+(console.screen_columns-1)+"})(.*?)$");

	while(1) {
		if(line[l]==undefined)
			return(ret);
		/* Get line length without trailing whitespace */
		m=line[l].text.match(/^(.*?)\s*$/);
		if(m!=null) {
			if(m[1].length<console.screen_columns && (m[1].length!=line[l].text.length || line[l+1]==undefined || line[l].hardcr==true)) {
				if(ret!=-1)
					draw_line(l);
				return(ret);
			}
		}
		m=line[l].text.match(first_check);
		if(m==null) {
			/*
			 * We couldn't apparently find a space after a char... this means
			 * we need to kludge wrap this (ick)
			 */
			m=line[l].text.match(kludge_check);
			line[l].kludged=true;
		}
		if(m!=null) {
			if(line[l+1]==undefined)
				line.push(new Line);
			else {
				if(line[l].hardcr) {
					line.splice(l+1,0,new Line);
					line[l].hardcr=false;
					line[l+1].hardcr=true;
					for(i=l+2; i<line.length; i++)
						draw_line(i);
				}
			}
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
			if(line[ypos]==undefined)
				return;
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

function draw_graphic_box()
{
	if(ypos==topline)
		graphics_box_displayed=edit_bottom;
	else
		graphics_box_displayed=edit_top;

	console.attributes=7;
	console.gotoxy(1,graphics_box_displayed);
	if(lines_on_screen < 15) {
		/* Not enough room for the graphics list (40 chars) */
		console.putmsg("Enter Graphics Code (\x01HCTRL-C\x01N to cancel): ");
	}
	else {
		/* 54 chars displayed */
		console.putmsg("Enter Graphics Code (\x01H?\x01N for a list, \x01HCTRL-C\x01N to cancel): ");
	}
	console.attributes=31;
	console.write(format("%3.3s",""));
}

function erase_graphic_box()
{
	draw_line(topline+graphics_box_displayed-edit_top);
	graphics_box_displayed=0;
}

function shitty_inkey(mode, timeout)
{
	var b = console.inkey(mode | K_UTF8, timeout);
	var ab = ascii(b);
	var bit;
	var val;
	var tmp;
	var ret = b;
	if (pmode & P_UTF8) {
		if (ab & 0x80) {
			for (bit = 7; ab & (1<<bit); bit--)
				val = ab & ((1<<bit) - 1);

			for (bit = 6; bit > 3 && (ab & (1<<bit)); bit--) {
				tmp = console.getbyte(timeout);
				val = (val << 6) | (tmp & 0x3f);
			}

			ret = String.fromCharCode(val);
		}
	}

	return ret;
}

function get_graphic()
{
	var key='';
	var inp='';
	var ch='';
	var inppos;

	draw_graphic_box();
	while(1) {
		if(lines_on_screen<15)
			console.gotoxy(41,graphics_box_displayed);
		else
			console.gotoxy(55,graphics_box_displayed);
		console.attributes=31;
		console.write(format("%-3.3s",inp));
		/* Not enough room to display the graphic reference */
		if(lines_on_screen<15)
			console.gotoxy(41+inp.length,graphics_box_displayed);
		else
			console.gotoxy(55+inp.length,graphics_box_displayed);
		ch=shitty_inkey(0,10000);
		switch(ch) {
			case '':
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				switch(inp.length) {
					case 0:
						if(ch!='1' && ch!='2') {
							console.beep();
							ch='';
						}
						break;
					case 1:
						if(inp.substr(0,1)=='1' && ch<'2') {
							console.beep();
							ch='';
						}
						if(inp.substr(0,1)=='2' && ch>'5') {
							console.beep();
							ch='';
						}
						break;
					case 2:
						if(inp.substr(0,2)=='25' && ch>'5') {
							console.beep();
							ch='';
						}
						if(inp.substr(0,2)=='12' && ch<'8') {
							console.beep();
							ch='';
						}
						break;
					default:
						console.beep();
						ch='';
						break;
				}
				inp=inp+ch;
				break;
			case ctrl('R'):
				redraw_screen();
				break;
			case '\x08':	/* Backspace */
				inp=inp.substr(0,inp.length-1);
				break;
			case '\x0d':	/* CR */
				if(inp.length<3) {
					console.beep();
					break;
				}
				erase_graphic_box();
				return(ascii(parseInt(inp)));
			case '\x03':	/* CTRL-C */
				erase_graphic_box();
				return('');
			case '?':
				/* Draws a graphics reference box then waits for a keypress. */
				if(lines_on_screen<15) {
					console.beep();
					break;
				}

				var l;
				var x;
				if(graphics_box_displayed==edit_top)
					l=graphics_box_displayed+1;
				else
					l=graphics_box_displayed-15;
				console.gotoxy(1,l);
				console.attributes=7;
				for(x=128; x<256; x++) {
					console.write(' ');
					console.attributes=curattr;
					console.write(ascii(x));
					console.attributes=7;
					console.write(':'+x+'  ');
				}
				console.cleartoeol();
				console.gotoxy(1,l+13);
				console.putmsg("PRESS ANY KEY TO CONTINUE");
				console.cleartoeol();
				console.getkey();
				for(x=0; x<15; x++)
					draw_line(topline+l-edit_top+x);
				break;
			default:
				console.beep();
				break;
		}
	}
}

function erase_colour_box()
{
	if(colour_box_displayed) {
		draw_line(colour_box_displayed-edit_top+topline);
		draw_line(colour_box_displayed-edit_top+topline+1);
		draw_line(colour_box_displayed-edit_top+topline+2);
	}
	set_cursor();
	colour_box_displayed=0;
}

function draw_colour_box()
{
	/*
	 * Try to draw at the top unless the cursor is too close... then do it at
	 * the bottom.
	 */
	if(ypos-topline<3)
		colour_box_displayed=edit_bottom-2;
	else
		colour_box_displayed=edit_top;

	console.gotoxy(1,colour_box_displayed);
	console.attributes=7;
	console.write("Foreground:");
	console.putmsg("\x01N \x01H\x01WK:");
	console.attributes=curattr&0xf8;
	console.write("Black");
	console.putmsg("\x01N \x01H\x01WR:");
	console.attributes=(curattr&0xf8)|RED;
	console.write("Red");
	console.putmsg("\x01N \x01H\x01WG:");
	console.attributes=(curattr&0xf8)|GREEN;
	console.write("Green");
	console.putmsg("\x01N \x01H\x01WY:");
	console.attributes=(curattr&0xf8)|BROWN;
	console.write("Yellow");
	console.putmsg("\x01N \x01H\x01WB:");
	console.attributes=(curattr&0xf8)|BLUE;
	console.write("Blue");
	console.putmsg("\x01N \x01H\x01WM:");
	console.attributes=(curattr&0xf8)|MAGENTA;
	console.write("Magenta");
	console.putmsg("\x01N \x01H\x01WC:");
	console.attributes=(curattr&0xf8)|CYAN;
	console.write("Cyan");
	console.putmsg("\x01N \x01H\x01WW:");
	console.attributes=(curattr&0xf8)|LIGHTGRAY;
	console.write("White");
	console.attributes=7;
	console.cleartoeol();
	console.write("\r\nBackground:");
	console.putmsg("\x01N \x01H\x01W0:");
	console.attributes=curattr&0x8f;
	console.write("Black");
	console.putmsg("\x01N \x01H\x01W1:");
	console.attributes=(curattr&0x8f)|(RED<<4);
	console.write("Red");
	console.putmsg("\x01N \x01H\x01W2:");
	console.attributes=(curattr&0x8f)|(GREEN<<4);
	console.write("Green");
	console.putmsg("\x01N \x01H\x01W3:");
	console.attributes=(curattr&0x8f)|(BROWN<<4);
	console.write("Yellow");
	console.putmsg("\x01N \x01H\x01W4:");
	console.attributes=(curattr&0x8f)|(BLUE<<4);
	console.write("Blue");
	console.putmsg("\x01N \x01H\x01W5:");
	console.attributes=(curattr&0x8f)|(MAGENTA<<4);
	console.write("Magenta");
	console.putmsg("\x01N \x01H\x01W6:");
	console.attributes=(curattr&0x8f)|(CYAN<<4);
	console.write("Cyan");
	console.putmsg("\x01N \x01H\x01W7:");
	console.attributes=(curattr&0x8f)|(LIGHTGRAY<<4);
	console.write("White");
	console.attributes=7;
	console.cleartoeol();
	console.write("\r\nSpecial:");
	console.putmsg("\x01N \x01H\x01WH:");
	console.attributes=curattr|0x08;
	console.write("High Intensity");
	console.putmsg("\x01N \x01H\x01WI:");
	console.attributes=curattr|0x80;
	console.write("Blinking");
	console.putmsg("\x01N \x01H\x01WN:");
	console.attributes=7;
	console.write("Normal");
	console.attributes=7;
	console.write("  Choose Colour: ");
	console.cleartoeol();
}

/* Redraws the current screen */
/* *should* redraw at any prompt. */
function redraw_screen()
{
	var last_tab='|';
	status_line();
	if(edit_top == 5) {
		console.gotoxy(1,1);
		console.print(format(hdr_fmt, "Subj", hdr_field_width, subj), pmode);
		console.gotoxy(1,2);
		console.print(format(hdr_fmt, "To",	hdr_field_width, to), pmode);
		console.gotoxy(1,3);
		console.print(format(hdr_fmt, "From", hdr_field_width, from), pmode);
	}
	else {
		console.gotoxy(1,1);
		printf(hdr_fmt, "File", hdr_field_width, subj);
	}
	/* Display tab line */
	for(i=0;i<(console.screen_columns-1);i++) {
		if(i && (i%tab_width)==0) {
			if((i%(tab_width*2))==0) {
				console.attributes=CYAN|HIGH;
				console.print('|');
				last_tab='|';
			} else {
				console.print(ascii(254));
				last_tab=ascii(254);
			}
		} else {
			console.attributes=YELLOW|HIGH;
			console.print(ascii(250));
		}
	}
	if(last_tab=='|') {
		console.attributes=YELLOW|HIGH;
		console.print(ascii(254));
	}
	else {
		console.attributes=CYAN|HIGH;
		console.print('|');
	}
	if(colour_box_displayed)
		draw_colour_box();
	if(graphics_box_displayed)
		draw_graphic_box();
	if(quote_window_displayed)
		draw_quote_window();
	if (quote_window_displayed && line.length === 1 && line[0].text === '') {
		quote_help();
		console.gotoxy(1, quote_ypos - quote_topline + quote_top);
	}
	else {
		for(i=edit_top; i<=edit_bottom; i++) {
			if(colour_box_displayed>0 && i>=colour_box_displayed && i<colour_box_displayed+3)
				continue;
			if(graphics_box_displayed>0 && i==graphics_box_displayed)
				continue;
			if(quote_window_displayed>0) {
				if(i>=quote_top && i<=quote_bottom)
					continue;
				if(i==quote_sep_pos)
					continue;
			}
			draw_line(i-edit_top+topline);
		}
		set_cursor();
	}
}

/*
 * Converts a pair of strings to an array of lines.
 * 
 * str:          The text of the message.
 * attr:         An attribute string of the message
 * nl_is_hardcr: Indicates that every NL is a hard cr
 */
function make_lines(str, attr, nl_is_hardcr)
{
	nl=new Array();
	var spos=0;
	var apos=0;
	var last_was_whitespace=false;
	thisattr=7;

	while(spos < str.length) {
		var done=false;

		nl[nl.length]=new Line;
		while(spos < str.length && !done) {
			if(apos < attr.length) {
				thisattr=ascii(attr.charAt(apos));
				apos++;
			}
			switch(str.charAt(spos)) {
				case '\r':
					spos++;
					break;
				case '\x01':
					spos++;
					switch(str.charAt(spos).toUpperCase()) {
						case 'K':
							thisattr&=0xf8;
							break;
						case 'R':
							thisattr=(thisattr&0xf8)|RED;
							break;
						case 'G':
							thisattr=(thisattr&0xf8)|GREEN;
							break;
						case 'Y':
							thisattr=(thisattr&0xf8)|BROWN;
							break;
						case 'B':
							thisattr=(thisattr&0xf8)|BLUE;
							break;
						case 'M':
							thisattr=(thisattr&0xf8)|MAGENTA;
							break;
						case 'C':
							thisattr=(thisattr&0xf8)|CYAN;
							break;
						case 'W':
							thisattr=(thisattr&0xf8)|LIGHTGRAY;
							break;
						case '0':
							thisattr=(thisattr&0x8f);
							break;
						case '1':
							thisattr=(thisattr&0x8f)|(RED<<4);
							break;
						case '2':
							thisattr=(thisattr&0x8f)|(GREEN<<4);
							break;
						case '3':
							thisattr=(thisattr&0x8f)|(BROWN<<4);
							break;
						case '4':
							thisattr=(thisattr&0x8f)|(BLUE<<4);
							break;
						case '5':
							thisattr=(thisattr&0x8f)|(MAGENTA<<4);
							break;
						case '6':
							thisattr=(thisattr&0x8f)|(CYAN<<4);
							break;
						case '7':
							thisattr=(thisattr&0x8f)|(LIGHTGRAY<<4);
							break;
						case 'H':
							thisattr|=0x08;
							break;
						case 'I':
							thisattr|=0x80;
							break;
						case 'N':
							thisattr=7;
							break;
						case '\x01':
							/* ToDo: This needs to fall through to the wrapping stuff. */
							nl[nl.length-1].text+=str.charAt(spos);
							nl[nl.length-1].attr+=ascii(thisattr);
							break;
					}
					spos++;
					break;
				case '\n':
					spos++;
					done=true;
					if(nl_is_hardcr)
						nl[nl.length-1].hardcr=true;
					else {
						/* Deduce if this is a hard or soft CR as follows */
						/* If the next char is a nl, or whitespace, it is hard. */
						/* If there is room for the first word of the next line on this
						 * line, it is hard */
						/* Otherwise, it is soft. */
						if(str.charAt(spos)==' ' || str.charAt(spos)=='\t')
							nl[nl.length-1].hardcr=true;
						else {
							var len;
							for(len=0; str.length>=len+spos && str.charAt(spos+len)!=' ' && str.charAt(spos+len,1)!='\t'; len++);
							if(nl[nl.length-1].text.length+len < console.screen_columns)
								nl[nl.length-1].hardcr=true;
							else {
								// Insert a space before soft CRs if there isn't one
								if(!last_was_whitespace) {
									nl[nl.length-1].text+=' ';
									nl[nl.length-1].attr+=ascii(thisattr);
								}
							}
						}
					}
					break;
				case ' ':		/* Whitespace... never wrap here. */
					last_was_whitespace=true;
					nl[nl.length-1].text+=str.charAt(spos);
					nl[nl.length-1].attr+=ascii(thisattr);
					spos++;
					break;
				case '\t':		/* Whitespace... never wrap here. */
					last_was_whitespace=true;
					nl[nl.length-1].text+=' ';
					nl[nl.length-1].attr+=ascii(thisattr);
					while(nl[nl.length-1].text.length%tab_width) {
						nl[nl.length-1].text+=' ';
						nl[nl.length-1].attr+=ascii(thisattr);
					}
					spos++;
					break;
				default:		/* Printable char... may need to wrap */
					last_was_whitespace=false;
					if(nl[nl.length-1].text.length>(console.screen_columns-1)) {	/* Need to have wrapped */
						var offset;
						for(offset=nl[nl.length-1].text.length-1; offset>=0; offset--) {
							if(nl[nl.length-1].text.charAt(offset)!=' ' && nl[nl.length-1].text.charAt(offset)!='\t') {
								/* ToDo: Verify/test this... it's probobly wrong */
								spos-=nl[nl.length-1].text.length-1-offset;
								spos--;
								break;
							}
						}
						if(offset==-1) {
							/* There were no spaces this line. */
							nl[nl.length-1].kludged=true;
							spos--;
						}
						done=true;
					}
					else {
						nl[nl.length-1].text+=str.charAt(spos);
						nl[nl.length-1].attr+=ascii(thisattr);
						spos++;
					}
			}
		}
	}

	return(nl);
}

/*
 * Converts the current lines to a single string and returns an array.
 * the array contains two strings... the first string is the text string and the
 * second string is the attribute string.  If embed_colour is true, the second
 * string is blank.
 * 
 * soft and embed_colour are bools indicating that soft CRs should become hard, and
 * colour codes should be added
 */
function make_strings(soft,embed_colour)
{
	var i;
	var str='';
	var attrs='';
	/* ToDo: Do we want to explicitly init the attr if it's normal? */
	var lastattr=7;
	var thisattr;

	// Force the last line to be a hard CR.
	line[line.length-1].hardcr=true;
	for(i=0; i<line.length; i++) {
		if(embed_colour) {
			for(j=0;j<line[i].text.length;j++) {
				if((thisattr=ascii(line[i].attr.substr(j,1)))!=lastattr) {
					/* Disable HIGH and BLINK if required */
					if(((!(thisattr&0x80)) && (lastattr&0x80)) || ((!(thisattr&0x08)) && (lastattr&0x08))) {
						lastattr=7;
						str+='\x01N';
					}
					if((thisattr&0x80) && (!(lastattr&0x80)))			/* Blink */
						str+='\x01I';
					if((thisattr&0x08) && (!(lastattr&0x08)))			/* High Intensity */
						str+='\x01H';
					if((thisattr&0x07) != (lastattr&0x07)) {
						switch(thisattr&0x07) {
							case BLACK:
								str+='\x01K';
								break;
							case RED:
								str+='\x01R';
								break;
							case GREEN:
								str+='\x01G';
								break;
							case BROWN:
								str+='\x01Y';
								break;
							case BLUE:
								str+='\x01B';
								break;
							case MAGENTA:
								str+='\x01M';
								break;
							case CYAN:
								str+='\x01C';
								break;
							case LIGHTGRAY:
								str+='\x01N';
								break;
						}
					}
					if((thisattr&0x70) != (lastattr&0x70)) {
						switch((thisattr&0x70)>>4) {
							case BLACK:
								str+='\x010';
								break;
							case RED:
								str+='\x011';
								break;
							case GREEN:
								str+='\x012';
								break;
							case BROWN:
								str+='\x013';
								break;
							case BLUE:
								str+='\x014';
								break;
							case MAGENTA:
								str+='\x015';
								break;
							case CYAN:
								str+='\x016';
								break;
							case LIGHTGRAY:
								str+='\x017';
								break;
						}
					}
					lastattr=thisattr;
				}
				str+=ch_to_string(line[i].text.substr(j,1));
			}
		}
		else {
			// This code will break with unicode, but that's OK since it's never used.
			str+=line[i].text;
			attrs+=line[i].attr;
		}
		if(soft || line[i].hardcr) {
			/* Trim whitespace from end, except from a signature delimiter ("-- ") */
			if(line[i].text != "-- ") {
				str=str.replace(/([ \t]*)$/,function (str, spaces, offset, s) {
					if(!embed_colour) {
						/* Remove attributes for trimmed spaces */
						attrs=attrs.substr(0,attrs.length-spaces.length);
					}
					return('');
				});
			}
			str+='\r\n';
			attrs+=attrs.substr(-1)+attrs.substr(-1);
		}
	}
	return(new Array(str,attrs));
}

function draw_quote_selection(l)
{
	var yp;

	if(l==undefined || isNaN(l))
		return;
	yp=l-quote_topline+quote_top;
	/* Does this line even exist? */
	if(quote_line[l]==undefined) {
		console.attributes=7;
		console.gotoxy(1,yp);
		console.cleartoeol();
	}
	else {
		/* Is line on the current screen? */
		if(yp<quote_top)
			return;
		if(yp>quote_bottom)
			return;

		console.gotoxy(1,yp);
		if(l==quote_ypos) {
			console.attributes=YELLOW;
			if(quote_line[l].selected)
				console.write('*');
			else
				console.write('-');
		}
		else {
			if(quote_line[l].selected) {
				console.attributes=7;
				console.write('*');
			}
			else {
				console.attributes=ascii(quote_line[l].attr.substr(0,1));
				console.print(quote_line[l].text.substr(0,1), pmode);
			}
		}
	}
}

/*
 * Draws a quote line on a screen.  l is the index into quote_line[]
 */
function draw_quote_line(l)
{
	var yp;
	var x;

	if(l==undefined || isNaN(l))
		return;
	yp=l-quote_topline+quote_top;
	/* Does this line even exist? */
	if(quote_line[l]==undefined) {
		console.attributes=7;
		console.gotoxy(1,yp);
		console.cleartoeol();
	}
	else {
		/* Is line on the current screen? */
		if(yp<quote_top)
			return;
		if(yp>quote_bottom)
			return;

		console.gotoxy(1,yp);
		x=0;
		if(l==quote_ypos) {
			console.attributes=YELLOW;
			if(quote_line[l].selected)
				console.write('*');
			else
				console.write('-');
			x++;
		}
		else {
			if(quote_line[l].selected) {
				console.attributes=7;
				console.write('*');
				x++;
			}
		}
		for(; x<quote_line[l].text.length && x<(console.screen_columns-1); x++) {
			console.attributes=ascii(quote_line[l].attr.substr(x,1));
			console.print(quote_line[l].text.substr(x,1), pmode);
		}
		if(x<(console.screen_columns-1)) {
			console.attributes=7;
			console.cleartoeol();
		}
	}
}

function draw_quote_window()
{
	var i;

	if(quote_ypos>=quote_line.length)
		quote_ypos=quote_line.length-1;
	if(quote_ypos<0)
		quote_ypos=0;
	if(quote_ypos<quote_topline)
		quote_topline=quote_ypos;
	if(quote_topline+quote_height>quote_line.length)
		quote_topline=quote_line.length-quote_height;
	if(quote_ypos>=quote_topline+quote_height)
		quote_topline=quote_ypos-quote_height-1;
	if(quote_topline<0)
		quote_topline=0;
	/* Draw seperater */
	console.gotoxy(1,quote_sep_pos);
	console.attributes=7;
	console.print("\xc4\xc4\xb4 "+(quote_ontop?"^^^":"vvv")+" Quote "+(quote_ontop?"^^^":"vvv")
		+ " \xc3"
		+ new Array(Math.max(console.screen_columns - 18, 3)).join('\xc4')
		);
	for(i=0; i<quote_height; i++) {
		draw_quote_line(quote_topline+i);
	}
	if (quote_window_displayed && line.length === 1 && line[0].text === '')
		quote_help();
}

function quote_help()
{
	console.attributes=7;
	if(quote_ontop)
		console.gotoxy(1,quote_sep_pos+1);
	else
		console.gotoxy(1,edit_top);
	console.write("Quote mode keys: (SPACE to select a line, ENTER to accept and continue)");
	console.cleartoeol();
	console.write("\r\n CTRL-A - Select all                     CTRL-R - Redraw screen");
	console.cleartoeol();
	console.write("\r\n CTRL-B - Move to beginning of message   CTRL-^ - Move up one line");
	console.cleartoeol();
	console.write("\r\n CTRL-E - Move to end of message         CTRL-_ - Quick exit (no save)");
	console.cleartoeol();
	console.write("\r\n CTRL-J - Move down one line             SPACE  - Select/Unselect current line");
	console.cleartoeol();
	console.write("\r\n CTRL-K - Show this help                 ENTER  - Paste lines into message");
	console.cleartoeol();
	console.write("\r\n CTRL-M - Paste lines into message       A      - Select all");
	console.cleartoeol();
	console.write("\r\n CTRL-N - Move down one page             B      - Toggle block select mode");
	console.cleartoeol();
	console.write("\r\n CTRL-P - Move up one page               N      - Unselect all");
	console.cleartoeol();
	console.write("\r\n CTRL-Q - Quick exit (no save)");
	console.cleartoeol();
	console.write('\r\n');
}

function quote_mode()
{
	var i;
	var select_mode=false;
	var select_start=0;

	quote_height=parseInt(lines_on_screen/2)-1;	/* Rounds down */
	quote_scroll=parseInt(quote_height/2);
	if(quote_scroll<1)
		quote_scroll=1;
	var curr_ypos=ypos-topline+edit_top;

	/* Decide if quote window should go at top or at bottom */
	if(curr_ypos>edit_top+quote_height)
		quote_ontop=true;
	else
		quote_ontop=false;
	if (line.length === 1 && line[0].text === '')
		quote_ontop = true;

	if(quote_ontop) {
		quote_sep_pos=edit_top+quote_height;
		quote_window_displayed=edit_top;
		quote_top=edit_top;
		quote_bottom=quote_sep_pos-1;
	}
	else {
		quote_sep_pos=edit_bottom-quote_height;
		quote_window_displayed=quote_sep_pos;
		quote_top=quote_sep_pos+1;
		quote_bottom=edit_bottom;
	}

	for(i=0;i<quote_line.length;i++)
		quote_line[i].selected=false;

	draw_quote_window();

	while(1) {
		if (quote_window_displayed && line.length === 1 && line[0].text === '') {
			console.gotoxy(1, quote_ypos - quote_topline + quote_top);
		}
		else {
			set_cursor();
		}
		key=shitty_inkey(0,10000);
		if(key=='')
			continue;
		switch(key) {
			case 'a':
			case 'A':
			case '\x01':	/* A and CTRL-A -- Select all */
				for(i=0;i<quote_line.length;i++)
					quote_line[i].selected=true;
				for(i=0; i< quote_height; i++)
					draw_quote_selection(quote_topline+i);
				break;
			case 'n':
			case 'N':	/* Unselect all */
				for(i=0;i<quote_line.length;i++)
					quote_line[i].selected=false;
				for(i=0; i< quote_height; i++)
					draw_quote_selection(quote_topline+i);
				break;
			case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
				redraw_screen();
				break;
			case KEY_HOME:
				quote_ypos=0;
				quote_topline=0;
				draw_quote_window();
				break;
			case KEY_END:
				quote_ypos=quote_line.length-1;
				quote_topline=quote_line.length-quote_height;
				draw_quote_window();
				break;
			case ' ':	/* Toggle selection of current line */
				quote_line[quote_ypos].selected=!quote_line[quote_ypos].selected;
				draw_quote_selection(quote_ypos);
				/* Fallthrough */
			case KEY_DOWN:
				quote_ypos++;
				if(quote_ypos>=quote_line.length) {
					quote_ypos=quote_line.length-1;
					console.beep();
					break;
				}
				if(select_mode) {
					if(quote_ypos > select_start) {
						quote_line[quote_ypos].selected=true;
					}
					else {
						quote_line[quote_ypos-1].selected=false;
					}
				}
				if(quote_ypos>=quote_topline+quote_height) {
					quote_topline+=quote_scroll;
					draw_quote_window();
					break;
				}
				draw_quote_selection(quote_ypos-1);
				draw_quote_selection(quote_ypos);
				break;
			case KEY_UP:
				quote_ypos--;
				if(quote_ypos<0) {
					quote_ypos=0;
					console.beep();
					break;
				}
				if(select_mode) {
					if(quote_ypos < select_start) {
						quote_line[quote_ypos].selected=true;
					}
					else {
						quote_line[quote_ypos+1].selected=false;
					}
				}
				if(quote_ypos<quote_topline) {
					quote_topline-=quote_scroll;
					draw_quote_window();
					break;
				}
				draw_quote_selection(quote_ypos+1);
				draw_quote_selection(quote_ypos);
				break;
			case 'B':	/* B toggles "block" mode */
			case 'b':
				select_mode=!select_mode;
				if(select_mode) {
					select_start=quote_ypos;
					quote_line[quote_ypos].selected=true;
					draw_quote_selection(quote_ypos);
				}
				break;
			case '\x0d':	/* CR */
				for(i=0; i<quote_line.length; i++) {
					if(quote_line[i].selected) {
						line.splice(ypos,0,new Line(quote_line[i]));
						ypos++;
						if(ypos-topline >= lines_on_screen)
							topline++;
					}
				}
				quote_window_displayed=0;
				redraw_screen();
				return(false);
			case '\x1f':
			case '\x11':	/* CTRL-Q (XOff) (Quick Abort in SyncEdit) */
				return(true);
			case KEY_PAGEDN:
				quote_ypos+=quote_height-1;
				quote_topline+=quote_height-1;
				draw_quote_window();
				break;
			case KEY_PAGEUP:
				quote_ypos-=quote_height-1;
				quote_topline-=quote_height-1;
				draw_quote_window();
				break;
			case '\x0b':	/* CTRL-K */
				quote_help();
				console.cleartoeol();
				console.write('\r\n');
				console.write("Press any key to return to editing..");
				console.cleartoeol();
				console.up();
				console.right(37);
				console.getkey();
				redraw_screen();
				break;
		}
	}
}

function add_char(key)
{
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
				+ascii(curattr)
				+line[ypos].attr.substr(xpos+1);
	}
	xpos++;
	if(!rewrap())
		draw_line(ypos,xpos-1,false);
}

function output_filename()
{
	return (argc==0?system.temp_dir+"INPUT.MSG":argv[0]);
}

function save_file()
{
	var f=new File(output_filename());
	if(!f.open("wb")) {
		console.clear();
		throw new Error(f.error + " (" + errno_str + ") opening " + f.name);
		return false;
	}
	var s=make_strings(/* soft-CRs: */Boolean(options.soft_cr), /* embed-colors: */true);
	f.write(s[0]);
	f.close();
	return true;
}

function handle_backspace()
{
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
	if(!rewrap()) {
		draw_line(ypos,xpos);
	}
}

function handle_delete()
{
	last_xpos=-1;
	if(xpos>=line[ypos].text.length) {
		/* delete the hardcr, */
		line[ypos].hardcr=false;
		if(!rewrap())
			draw_line(ypos,xpos);
		return;
	}
	line[ypos].text=line[ypos].text.substr(0,xpos)
			+line[ypos].text.substr(xpos+1);
	line[ypos].attr=line[ypos].attr.substr(0,xpos)
			+line[ypos].attr.substr(xpos+1);
	if(!rewrap())
		draw_line(ypos,xpos);

}

/* ------------------------------------------------------------------ */
/* Spell checking                                                      */
/* ------------------------------------------------------------------ */

function spell_save_opt(key, val)
{
	bbs.mods.userprops.set("fseditor", key, val);
}

/* The user's personal word list (null if this user can't save settings) */
function spell_personal_file()
{
	if(user.security.restrictions & UREST_EDIT_DEFAULTS)
		return null;
	return system.data_dir + format("user/%04u.fse_words", user.number);
}

/* Show a message on the status line, optionally pausing so it can be read */
function status_msg(text, pause_ms)
{
	console.gotoxy(1, console.screen_rows);
	console.attributes = stat_attr;
	console.write(text);
	console.cleartoeol();
	if(pause_ms) {
		mswait(pause_ms);
		status_line();
	}
}

/* Loads the dictionary for the user's language if needed.  Returns false if unavailable. */
function spell_ready()
{
	if(spell.is_loaded(spell_opts.language))
		return true;
	status_msg(" Loading dictionary...");
	var ok = spell.load_language(spell_opts.language, spell_personal_file());
	spell_gen++;
	if(!ok)
		status_msg(" " + spell.load_error, 2000);
	status_line();
	return ok;
}

/* Inline checking: finds (and caches on the line) the misspelled words of line l */
function spell_marks(l)
{
	var ln = line[l];
	if(ln.sp_gen !== spell_gen || ln.sp_text !== ln.text) {
		ln.sp_gen = spell_gen;
		ln.sp_text = ln.text;
		ln.sp_marks = [];
		if(!(spell_opts.skip_quotes && spell.is_skipped_line(ln.text))) {
			var errs = spell.find_errors(ln.text, { skip_caps: spell_opts.skip_caps, ignore: spell_ignore });
			for(var i = 0; i < errs.length; i++) {
				for(var x = errs[i].start; x < errs[i].end; x++)
					ln.sp_marks[x] = errs[i];
			}
		}
	}
	return ln;
}

/* Start of the word the cursor is in or touching, or -1 */
function cursor_word_start()
{
	var t = line[ypos].text;
	var s = xpos;
	var e = xpos;
	while(s > 0 && /[A-Za-z'\u2019]/.test(t.charAt(s - 1)))
		s--;
	while(e < t.length && /[A-Za-z'\u2019]/.test(t.charAt(e)))
		e++;
	return e > s ? s : -1;
}

/*
 * Inline checking doesn't flag the word being typed.  When the cursor moves
 * off a word (or to another line) redraw so that word gets checked.
 */
var inline_key = '';
var inline_last_y = -1;
function inline_refresh()
{
	if(!spell_opts.inline || !spell.is_loaded(spell_opts.language))
		return;
	if(line[ypos] === undefined)
		return;
	var key = ypos + ":" + cursor_word_start();
	if(key === inline_key)
		return;
	var py = inline_last_y;
	inline_key = key;
	inline_last_y = ypos;
	if(py >= 0 && py != ypos && line[py] !== undefined)
		draw_line(py);
	draw_line(ypos);
}

/* Scroll (if needed) so line l is visible and clear of the bottom prompt rows */
function spell_show_line(l)
{
	var reserve = lines_on_screen > 6 ? 3 : 0;
	if(l < topline || l >= topline + lines_on_screen - reserve) {
		topline = l - 2;
		if(topline < 0)
			topline = 0;
		for(var i = edit_top; i <= edit_bottom; i++)
			draw_line(i - edit_top + topline);
	}
}

/* Shows the suggestion list on the last two rows of the edit window */
function spell_draw_choices(word, sugg)
{
	var width = console.screen_columns - 1;
	console.gotoxy(1, edit_bottom - 1);
	console.attributes = YELLOW | HIGH;
	console.write('"' + word + '"');
	console.attributes = 7;
	console.write(" is not in the dictionary.");
	console.cleartoeol();
	console.gotoxy(1, edit_bottom);
	if(sugg.length == 0) {
		console.attributes = 7;
		console.write("No suggestions.");
	}
	else {
		var used = 0;
		for(var i = 0; i < sugg.length; i++) {
			var item = ") " + sugg[i] + "  ";
			if(used + 1 + item.length > width)
				break;
			console.attributes = CYAN | HIGH;
			console.write(String(i + 1));
			console.attributes = 7;
			console.write(item);
			used += 1 + item.length;
		}
	}
	console.cleartoeol();
}

/* Replaces a misspelled word in line l and re-wraps */
function spell_replace(l, e, newtext)
{
	var ln = line[l];
	var a = ln.attr.substr(e.start, 1);
	if(a == '')
		a = ascii(7);
	var attr = '';
	for(var i = 0; i < newtext.length; i++)
		attr += a;
	ln.text = ln.text.substr(0, e.start) + newtext + ln.text.substr(e.end);
	ln.attr = ln.attr.substr(0, e.start) + attr + ln.attr.substr(e.end);
	last_xpos = -1;
	ypos = l;
	xpos = e.start + newtext.length;
	rewrap();
	for(var i = l - 1; i <= l + 1; i++) {
		if(i >= 0)
			draw_line(i);
	}
}

/*
 * Asks the user what to do about one misspelled word.
 * Returns { stop: true }, { replace: "text" } or {} (ignored / added to dictionary).
 */
function spell_prompt(l, e)
{
	var word = e.word;
	var sugg = spell.suggest(word, 9);
	var ret = {};
	var key;

	while(1) {
		spell_show_line(l);
		var row = edit_top + (l - topline);
		/* Highlight the word */
		if(e.start < console.screen_columns - 1) {
			console.gotoxy(e.start + 1, row);
			console.attributes = 0x4f;
			var t = line[l].text.substr(e.start, e.end - e.start);
			for(var i = 0; i < t.length && e.start + i < console.screen_columns - 1; i++)
				shitty_putc(t.charAt(i));
		}
		spell_draw_choices(word, sugg);
		status_msg(" (I)gnore  (A)dd to dictionary  (E)dit  (1-9) Replace  (ESC) Stop");

		key = console.getkey(K_UPPER);
		var done = false;
		if(key === '' && !bbs.online) {
			ret.stop = true;
			done = true;
		}
		else if(key == 'I') {
			spell_ignore["$" + word.toLowerCase()] = true;
			spell_gen++;
			done = true;
		}
		else if(key == 'A') {
			spell.add_personal(word);
			spell_gen++;
			done = true;
		}
		else if(key >= '1' && key <= '9' && (ascii(key) - 49) < sugg.length) {
			ret.replace = sugg[ascii(key) - 49];
			done = true;
		}
		else if(key == 'E') {
			console.gotoxy(1, console.screen_rows);
			console.attributes = 7;
			console.write(" Replace with: ");
			console.cleartoeol();
			/* K_NOCRLF: the CR/LF getstr() sends on Enter would scroll the whole screen */
			var newword = console.getstr(word, 40, K_LINE | K_EDIT | K_AUTODEL | K_NOCRLF);
			redraw_screen();
			if(newword !== undefined && newword !== null) {
				newword = newword.replace(/^\s+|\s+$/g, "");
				if(newword.length > 0) {
					ret.replace = newword;
					done = true;
				}
			}
		}
		else if(key == '\x1b' || key == 'Q' || key == '\x03') {
			ret.stop = true;
			done = true;
		}
		else
			console.beep();

		/* Remove the prompt rows and the highlight */
		draw_line(edit_bottom - edit_top + topline);
		draw_line(edit_bottom - 1 - edit_top + topline);
		draw_line(l);
		if(done)
			break;
	}
	return ret;
}

/*
 * Interactive spell check of the whole message (CTRL-O, and before sending).
 * Returns { available: false } if there's no dictionary, otherwise
 * { available: true, completed: <false if the user stopped early>, found, changed }
 */
function spell_check()
{
	var result = { available: false, completed: false, found: 0, changed: 0 };

	if(!spell_enabled || !spell_ready())
		return result;
	result.available = true;

	var save_x = xpos;
	var save_y = ypos;
	var save_top = topline;
	var opts = { skip_caps: spell_opts.skip_caps, ignore: spell_ignore };
	var stopped = false;
	var l = 0;

	while(l < line.length && !stopped) {
		if(spell_opts.skip_quotes && spell.is_skipped_line(line[l].text)) {
			l++;
			continue;
		}
		var errs = spell.find_errors(line[l].text, opts);
		if(errs.length == 0) {
			l++;
			continue;
		}
		result.found++;
		var r = spell_prompt(l, errs[0]);
		if(r.stop)
			stopped = true;
		else if(r.replace !== undefined) {
			spell_replace(l, errs[0], r.replace);
			result.changed++;
			/* Re-wrapping can pull text up from the line below, so look at the previous line again */
			l = Math.max(0, l - 1);
		}
		/* Otherwise the word was ignored or added; the same line is checked again for further errors */
	}
	result.completed = !stopped;

	if(result.completed) {
		if(result.found == 0)
			status_msg(" No spelling errors found.", 1200);
		else
			status_msg(format(" Spell check complete: %u misspelled word%s found, %u changed.",
				result.found, result.found == 1 ? "" : "s", result.changed), 1800);
	}

	/* Put the cursor back where it was */
	last_xpos = -1;
	ypos = Math.min(save_y, line.length - 1);
	xpos = Math.min(save_x, line[ypos].text.length);
	topline = Math.min(save_top, ypos);
	if(ypos >= topline + lines_on_screen)
		topline = ypos - lines_on_screen + 1;
	redraw_screen();
	return result;
}

/*
 * Called when the user saves with CTRL-Z and "spell check before sending" is on.
 * Returns false if the message shouldn't be sent (the user wants to keep editing).
 */
function spell_before_send()
{
	var r = spell_check();
	if(!r.available || r.completed)
		return true;
	status_msg(" Spell check was not finished.  Send the message anyway? (Y/N) ");
	var key = console.getkey(K_UPPER);
	status_line();
	return key == 'Y';
}

/* ------------------------------------------------------------------ */
/* Personal options menu (CTRL-D)                                      */
/* ------------------------------------------------------------------ */

function on_off(b)
{
	return b ? "On" : "Off";
}

function yes_no(b)
{
	return b ? "Yes" : "No";
}

function options_clear(rows)
{
	console.attributes = 7;
	for(var i = 0; i < rows; i++) {
		console.gotoxy(1, edit_top + i);
		console.cleartoeol();
	}
}

function options_row(n, key, label, value)
{
	console.gotoxy(1, edit_top + n);
	console.attributes = CYAN | HIGH;
	console.write("  " + key);
	console.attributes = 7;
	console.write(format(") %-42s", label));
	console.attributes = YELLOW | HIGH;
	console.write(value);
	console.attributes = 7;
	console.cleartoeol();
}

function options_draw()
{
	options_clear(12);
	console.gotoxy(1, edit_top);
	console.attributes = WHITE | HIGH;
	console.write("Personal options");
	console.attributes = 7;
	var lang_text = spell.lang_name(spell_opts.language);
	options_row(2, "1", "Spell check before sending",			on_off(spell_opts.before_send));
	options_row(3, "2", "Inline spell check (as you type)",	on_off(spell_opts.inline));
	options_row(4, "3", "Spell check language",				lang_text);
	options_row(5, "4", "Skip quoted text",					yes_no(spell_opts.skip_quotes));
	options_row(6, "5", "Ignore ALL-CAPS words",				yes_no(spell_opts.skip_caps));
	options_row(7, "6", "Clear personal dictionary",
		spell.is_loaded(spell_opts.language) || spell.personal_count() > 0
			? format("%u word%s", spell.personal_count(), spell.personal_count() == 1 ? "" : "s") : "");
	options_row(9, "Q", "Return to editing", "");
	console.gotoxy(1, edit_top + 11);
	console.attributes = 7;
	console.write("Press CTRL-O at any time to spell check the message.");
	console.gotoxy(1, edit_top + 10);
}

function options_language()
{
	var langs = spell.languages();
	options_clear(12);
	console.gotoxy(1, edit_top);
	console.attributes = WHITE | HIGH;
	console.write("Select spell check language");
	if(langs.length == 0) {
		console.gotoxy(1, edit_top + 2);
		console.attributes = 7;
		console.write("No dictionaries are installed.  Press any key.");
		console.getkey();
		return;
	}
	for(var i = 0; i < langs.length && i < 26; i++) {
		console.gotoxy(1, edit_top + 2 + i);
		if(edit_top + 2 + i > edit_bottom - 1)
			break;
		console.attributes = CYAN | HIGH;
		console.write("  " + String.fromCharCode(65 + i));
		console.attributes = 7;
		console.write(") " + langs[i].name);
		if(langs[i].code == spell_opts.language) {
			console.attributes = YELLOW | HIGH;
			console.write("  (current)");
		}
		console.attributes = 7;
		console.cleartoeol();
	}
	console.gotoxy(1, edit_bottom);
	console.write("Press a letter to select, or ESC to leave it as it is.");
	console.cleartoeol();
	var key = console.getkey(K_UPPER);
	var idx = ascii(key) - 65;
	if(key.length == 1 && idx >= 0 && idx < langs.length && idx < 26) {
		if(langs[idx].code != spell_opts.language) {
			spell_opts.language = langs[idx].code;
			spell_save_opt("spell_language", spell_opts.language);
			spell_gen++;
			/* Dictionary loading is deferred until needed, except for inline checking */
			if(spell_opts.inline)
				spell_ready();
		}
	}
}

function options_menu()
{
	var done = false;

	while(!done && bbs.online) {
		options_draw();
		var key = console.getkey(K_UPPER);
		switch(key) {
			case '1':
				spell_opts.before_send = !spell_opts.before_send;
				spell_save_opt("spell_before_send", spell_opts.before_send);
				break;
			case '2':
				if(!spell_opts.inline && !spell_ready())
					break;
				spell_opts.inline = !spell_opts.inline;
				spell_save_opt("spell_inline", spell_opts.inline);
				spell_gen++;
				inline_key = '';
				break;
			case '3':
				options_language();
				break;
			case '4':
				spell_opts.skip_quotes = !spell_opts.skip_quotes;
				spell_save_opt("spell_skip_quotes", spell_opts.skip_quotes);
				spell_gen++;
				break;
			case '5':
				spell_opts.skip_caps = !spell_opts.skip_caps;
				spell_save_opt("spell_skip_caps", spell_opts.skip_caps);
				spell_gen++;
				break;
			case '6':
				if(spell_ready() && spell.personal_count() > 0) {
					status_msg(" Remove all words from your personal dictionary? (Y/N) ");
					if(console.getkey(K_UPPER) == 'Y') {
						spell.clear_personal();
						spell_gen++;
					}
					status_line();
				}
				break;
			case 'Q':
			case '\x1b':
			case '\x04':
			case '\r':
				done = true;
				break;
		}
	}
	inline_key = '';
	redraw_screen();
}

function edit(quote_first)
{
	var prev_key;
	var key;

	function edit_subject()
	{
		if(edit_top==5) {
			console.gotoxy(7,1);
			var newsubj=console.getstr(subj, 70, K_LINE|K_EDIT|K_AUTODEL);
			if(newsubj!='')
				subj=newsubj;
			redraw_screen();
		}
	}

	if(spell_opts.inline)
		spell_ready();
	redraw_screen();
	if(quote_first) {
		if(quote_mode()) {
			console.line_counter=0;
			return 1;	/* aborted */
		}
	}
	while(1) {
		inline_refresh();
		set_cursor();
		key=shitty_inkey(/* mode: */0, /* timeout (ms): */10000);
		if(!bbs.online) {
			save_file();
			return 1;	// aborted
		}
		if(key=='')
			continue;
		switch(key) {
			/* We're getting all the CTRL keys here... */
			case '\x00':	/* CTRL-@ (NULL) */
			case '\x01':	/* CTRL-A (Colour) */
				key='';
				draw_colour_box();
				while(key=='') {
					key=console.getkey(K_UPPER);
					switch(key) {
						case 'K':
							curattr&=0xf8;
							break;
						case 'R':
							curattr=(curattr&0xf8)|RED;
							break;
						case 'G':
							curattr=(curattr&0xf8)|GREEN;
							break;
						case 'Y':
							curattr=(curattr&0xf8)|BROWN;
							break;
						case 'B':
							curattr=(curattr&0xf8)|BLUE;
							break;
						case 'M':
							curattr=(curattr&0xf8)|MAGENTA;
							break;
						case 'C':
							curattr=(curattr&0xf8)|CYAN;
							break;
						case 'W':
							curattr=(curattr&0xf8)|LIGHTGRAY;
							break;
						case '0':
							curattr=(curattr&0x8f);
							break;
						case '1':
							curattr=(curattr&0x8f)|(RED<<4);
							break;
						case '2':
							curattr=(curattr&0x8f)|(GREEN<<4);
							break;
						case '3':
							curattr=(curattr&0x8f)|(BROWN<<4);
							break;
						case '4':
							curattr=(curattr&0x8f)|(BLUE<<4);
							break;
						case '5':
							curattr=(curattr&0x8f)|(MAGENTA<<4);
							break;
						case '6':
							curattr=(curattr&0x8f)|(CYAN<<4);
							break;
						case '7':
							curattr=(curattr&0x8f)|(LIGHTGRAY<<4);
							break;
						case 'H':
							curattr|=0x08;
							break;
						case 'I':
							curattr|=0x80;
							break;
						case 'N':
							curattr=7;
							break;
						case ctrl('R'):
							redraw_screen();
							key='';
							break;
						case "\x1b":	/* ESC */
							break;
						default:
							console.beep();
							key='';
							break;
					}
				}
				status_line();
				erase_colour_box();
				break;
			case KEY_HOME:	/* CTRL-B */
				last_xpos=-1;
				xpos=0;
				break;
			case '\x03':	/* CTRL-C (Center Line) */
				last_xpos=-1;
				line[ypos].hardcr=true;
				/* Trim leading/trailing whitespace */
				line[ypos].text=line[ypos].text.replace(/^(\s*)(.*?)\s*$/,function (str, leading, t, offset, s) {
					line[ypos].attr=line[ypos].attr.substr(leading.length, t.length);
					xpos -= leading.length;
					if(xpos<0)
						xpos=0;
					return(t);
				});
				var add=parseInt((console.screen_columns-line[ypos].text.length)/2);
				for(add=parseInt((console.screen_columns-line[ypos].text.length)/2); add>0; add--) {
					line[ypos].text=' '+line[ypos].text;
					line[ypos].attr=ascii(curattr)+line[ypos].attr;
					xpos++;
				}
				if(xpos>line[ypos].length)
					xpos=line[ypos].length;
				draw_line(ypos);
				break;
			case '\x04':	/* CTRL-D (Quick Find in SyncEdit) - Personal options */
				if(spell_enabled)
					options_menu();
				break;
			case '\x0f':	/* CTRL-O (Spell check) */
				if(spell_enabled)
					spell_check();
				break;
			case KEY_END:	/* CTRL-E  */
				last_xpos=-1;
				xpos=line[ypos].text.length;
				break;
			case KEY_RIGHT:	/* CTRL-F  */
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
				break;
			case '\x07':	/* CTRL-G (Beep) */
				/* Enter Graphic Character */
				key=get_graphic();
				if(key=='')
					break;
				add_char(key);
				break;
			case '\x08':	/* CTRL-H Backspace */
				handle_backspace();
				break;
			case '\x09':	/* CTRL-I TAB... ToDo expand to spaces */
				add_char(' ');
				while(xpos%tab_width)
					add_char(' ');
				break;
			case '\x0a':	/* CTRL-J KEY_DOWN (Insert Line in SyncEdit) */
				if(last_xpos==-1)
					last_xpos=xpos;
				try_next_line();
				break;
			case '\x0b':	/* CTRL-K */
				console.attributes=7;
				console.gotoxy(1,edit_top);
				console.write("Editing keys:");
				console.cleartoeol();
				var help_keys=[
					["CTRL-@ - Change Colour",				"CTRL-O - Spell check message"],
					["CTRL-A - Change Colour",				"CTRL-P - Page Up"],
					["CTRL-B - Move to beginning of line",	"CTRL-Q - Quick Abort (no save)"],
					["CTRL-C - Center line on screen",		"CTRL-R - Redraw screen"],
					["CTRL-D - Personal options",			"CTRL-S - Edit Subject"],
					["CTRL-E - Move to end of line",		"CTRL-T - Adjust Tab Width"],
					["CTRL-F - Move right one character",	"CTRL-U - Enter quote mode"],
					["CTRL-G - Enter graphic character",	"CTRL-V - Toggle insert mode"],
					["CTRL-H - Backspace (BACKSPACE)",		"CTRL-W - Delete word backwards"],
					["CTRL-I - Move to next tabstop (TAB)",	"CTRL-Y - Delete Line"],
					["CTRL-J - Move down one line",			"CTRL-Z - Save message and exit"],
					["CTRL-K - Show This Help",				"CTRL-\\ - Edit Subject"],
					["CTRL-L - Insert Line",				"CTRL-] - Move left one character"],
					["CTRL-M - Carriage Return",			"CTRL-^ - Move up one line"],
					["CTRL-N - Page Down",					"CTRL-_ - Quick Abort (no save)"]
				];
				if(!spell_enabled) {
					help_keys[0][1]="";
					help_keys[4][0]="";
				}
				for(var hk=0; hk<help_keys.length; hk++) {
					console.write(format("\r\n %-40s%s", help_keys[hk][0], help_keys[hk][1]));
					console.cleartoeol();
				}
				console.write('\r\n');
				console.cleartoeol();
				console.write("Press any key to return to editing..");
				console.cleartoeol();
				console.getkey();
				redraw_screen();
				break;
			case '\x0c':	/* CTRL-L (Insert Line) */
				/* ToDo: Should this kill last_xpos? */
				add_new_line_below(ypos-1);
				xpos=0;
				break;
			case '\x0d':	/* CTRL-M CR */
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
					line[ypos+1].kludged=line[ypos].kludged
							&& line[ypos+1].text.length > 0;
					line[ypos].kludged=false;
					draw_line(ypos+1);
				}
				try_next_line();
				xpos=0;
				rewrap();
				break;
			case KEY_PAGEUP:
				if(last_xpos==-1)
					last_xpos=xpos;
				if(ypos==0) {
					console.beep();
					break;
				}
				ypos-=lines_on_screen-1;
				if(ypos<0)
					ypos=0;
				if(topline>ypos)
					topline=ypos;
				var i;
				for(i=edit_top; i<=edit_bottom; i++)
					draw_line(i-edit_top+topline);
				xpos=last_xpos;
				if(xpos>line[ypos].text.length)
					xpos=line[ypos].text.length;
				break;
			case KEY_PAGEDN:
				if(last_xpos==-1)
					last_xpos=xpos;
				if(ypos>=line.length-1) {
					console.beep();
					break;
				}
				ypos+=lines_on_screen-1;
				if(ypos>line.length-1)
					ypos=line.length-1;
				if(ypos>=topline+lines_on_screen)
					topline=ypos-lines_on_screen+1;
				var i;
				for(i=edit_top; i<=edit_bottom; i++)
					draw_line(i-edit_top+topline);
				xpos=last_xpos;
				if(xpos>line[ypos].text.length)
					xpos=line[ypos].text.length;
				break;
			case '\x11':	/* CTRL-Q (XOff) (Quick Abort in SyncEdit) */
				console.line_counter=0;
				return 1;	/* aborted */
			case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
				redraw_screen();
				break;
			case '\x13':	/* CTRL-S (Edit Subject) (Xon)  */
				edit_subject();
				break;
			case '\x14':	/* CTRL-T (Adjust Tabs) (Justify Line in SyncEdit) */
                tab_width/=2;
                if(tab_width==1)
                    tab_width=8;
                redraw_screen();
				break;
			case '\x15':	/* CTRL-U (Quick Quote in SyncEdit) */
				if(quote_line.length>0) {
					if(quote_mode()) {
						console.line_counter=0;
						return;
					}
				}
				else
					console.beep();
				break;
			case '\x16':	/* CTRL-V (Toggle insert mode) */
				insert=!insert;
				status_line();
				break;
			case '\x17':	/* CTRL-W (Delete Word) */
				last_xpos=-1;
				/* Remove trailing part of word */
				while(line[ypos].text.length>xpos
						&& line[ypos].text.substr(xpos,1).search(/\s/)==-1) {
					line[ypos].text=line[ypos].text.substr(0,xpos)
							+line[ypos].text.substr(xpos+1);
					line[ypos].attr=line[ypos].attr.substr(0,xpos)
							+line[ypos].attr.substr(xpos+1);
				}
				/* Remove leading part */
				while(xpos>0
						&& line[ypos].text.substr(xpos-1,1).search(/\s/)==-1) {
					line[ypos].text=line[ypos].text.substr(0,xpos-1)
							+line[ypos].text.substr(xpos);
					line[ypos].attr=line[ypos].attr.substr(0,xpos-1)
							+line[ypos].attr.substr(xpos);
					xpos--;
				}
				/* 
				 * Remove Space...
				 * Note that no matter what, we should delete a space.
				 * We're either deleting the space from the end of the word if
				 * this was the first word on the line, or the one before the
				 * word otherwise.  This puts the cursor on the previous word
				 * if there is one and the next word if there isn't.  This
				 * allows multiple ^W to keep deleting words in all cases.
				 */
				if(xpos) {
					/* Delete space before the deleted word */
					line[ypos].text=line[ypos].text.substr(0,xpos-1)
							+line[ypos].text.substr(xpos);
					line[ypos].attr=line[ypos].attr.substr(0,xpos-1)
							+line[ypos].attr.substr(xpos);
					xpos--;
				}
				else {
					/* Delete space after the word */
					line[ypos].text=line[ypos].text.substr(1);
					line[ypos].attr=line[ypos].attr.substr(1);
				}
				if(!rewrap())
					draw_line(ypos,xpos);
				break;
			case '\x18':	/* CTRL-X (PgDn in SyncEdit) */
				break;
			case '\x19':	/* CTRL-Y (Delete Line in SyncEdit) */
				/* Delete Line */
				/* ToDo: Should this kill last_xpos? */
				if(line.length==1)
					line[0]=new Line;
				else
					line.splice(ypos,1);
				if(ypos>=line.length) {
					ypos=line.length-1;
					scroll(0);		/* Scroll 'em */
				}
				if(last_xpos>=0)
					xpos=last_xpos;
				if(xpos>line[ypos].text.length)
					xpos=line[ypos].text.length;
				var i;
				console.write("\033[M");	/* Delete Line */
				/* Redraw bottom line */
				draw_line(topline+lines_on_screen-1);
				/* Old used to redraw everything after current
				for(i=ypos; i<topline+lines_on_screen; i++)
					draw_line(i);
				*/
				status_line();
				break;
			case '\x1a':	/* CTRL-Z (EOF) (PgUp in SyncEdit)  */
				if(spell_opts.before_send && !spell_before_send())
					break;
				save_file();
				console.line_counter=0;
				return;
			case '\x1b':	/* ESC (This should parse extra ANSI sequences) */
				break;
			case '\x1c':	/* CTRL-\ (RegExp) */
				/* Same as CTRL-S (Edit Subject) */
				edit_subject();
				break;
			case KEY_LEFT:	/* CTRL-] */
				last_xpos=-1;
				xpos--;
				if(xpos<0) {
					console.beep();
					xpos=0;
				}
				break;
			case KEY_UP:	/* CTRL-^ */
				if(last_xpos==-1)
					last_xpos=xpos;
				try_prev_line();
				break;
			case '\x1f':	/* CTRL-_ Safe quick-abort*/
				console.line_counter=0;
				return;
			case KEY_DEL:	/* DELETE */
				handle_delete();
				break;
			case 'B':
				if(prev_key == '\x18') {	/* CTRL-X, ZDLE */
					// ZMODEM-receive
					console.clear(LIGHTGRAY);
					console.print("ZMODEM upload detected");
					if(bbs.receive_file(output_filename(), 'Z', /* autohang: */false) == true) {
						console.line_counter=0;
						return;
					}
					while(shitty_inkey(/* mode: */0, /* timeout: */500) == '\x18')
						;
					console.print(format(bbs.text(FileNotReceived), "File"));
					console.pause();
					redraw_screen();
					break;
				}
				// Fall-through
			default:		/* Insert the char */
				add_char(key);
		}
		prev_key = key;
	}
}

var input_filename=system.node_dir+"QUOTES.TXT";
var use_quotes=true;
input_filename=file_getcase(input_filename);
if(input_filename==undefined) {
	use_quotes=false;
	if(argc==1 && input_filename==undefined)
		input_filename=argv[0];
}
else {
	var all_files=directory(system.node_dir+"*");
	var newest_filedate=-Infinity;
	var newest_filename;
	for(var file in all_files) {
		if(all_files[file].search(/quotes.txt$/i)!=-1) {
			var this_date=file_date(all_files[file]);
			if(this_date > newest_filedate) {
				newest_filename=all_files[file];
				newest_filedate=this_date;
			}
		}
	}
	if(newest_filename != undefined)
		input_filename=newest_filename;
}
js.on_exit("bbs.sys_status = " + bbs.sys_status);
bbs.sys_status&=~SS_PAUSEON;
bbs.sys_status|=SS_PAUSEOFF;
js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
console.ctrlkey_passthru="+ABCDGKLNOPQRTUVWXYZ_";
/* Enable delete line in SyncTERM (Disabling ANSI Music in the process) */
console.write("\033[=1M");
console.clear();
console.writeln("Opening " + input_filename);
var f=new File(input_filename);
if(f.open("rb",false)) {
	ypos=0;
	if(use_quotes) {
		var quote_width = console.screen_columns - 1;
		quote_line=make_lines(quote_msg(word_wrap(f.read(), quote_width - 3), quote_width),'');
	} else
		line=make_lines(word_wrap(f.read(), console.screen_columns - 1),'');
	f.close();
}
if(line.length==0)
	line.push(new Line());

subj='';
to=input_filename;
var drop_file_name;
var drop_file_time=-Infinity;
while((drop_file_name = file_getcase(system.node_dir + "editor.inf"))!=undefined) {
	if(file_date(drop_file_name)>=drop_file_time) {
		drop_file = new File(drop_file_name);
		if(drop_file.exists && drop_file.open("r")) {
			drop_file_time=drop_file.date;
			info = drop_file.readAll();
			drop_file.close();
			subj=strip_ctrl(info[0]);
			if (pmode & P_UTF8) {
				if (!str_is_utf8(subj))
					subj = utf8_encode(subj);
			}
			to=strip_ctrl(info[1]);
			if (pmode & P_UTF8) {
				if (!str_is_utf8(to))
					to = utf8_encode(to);
			}
			from=strip_ctrl(info[3]);
			if (pmode & P_UTF8) {
				if (!str_is_utf8(from))
					from = utf8_encode(from);
			}
		}
	}
	file_remove(drop_file_name);
}
if(subj=='') {
	edit_top=3;
	lines_on_screen=edit_bottom-edit_top+1;
	subj=to;
	subj=subj.replace(/^.*[\\\/]/,'');
}
var exit_code=edit(use_quotes);
/* Delete all existing result.ed files */
var result=file_getcase(system.node_dir + "result.ed");
while(result!=undefined) {
	if(!file_remove(result))
		break;
	result=file_getcase(result);
}
if(edit_top==5) {
	drop_file = new File(system.node_dir + "result.ed");
	if(drop_file.open("w")) {
		drop_file.writeln("0");	// anonymous
		drop_file.writeln(subj);
		drop_file.writeln("FSEditor.js v" + REVISION);
		drop_file.close();
	}
}
if(tab_width != options.default_tabstop)
	bbs.mods.userprops.set("fseditor", "tabstop", tab_width);
console.clear();
exit(exit_code);
