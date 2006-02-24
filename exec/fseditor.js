var xpos=1;
var topline=0;								/* Index into line[] of line at edit_top */
var line=new Array();
var edit_top=2;								/* First line of edit window */
var ypos=edit_top;
var edit_bottom=console.screen_rows;		/* Last line of edit window */
var lines_on_screen=edit_bottom-edit_top+1;	/* Height of edit window */
var curattr=7;								/* Current attribute */
var insert=true;							/* Insert mode */
line[0]=new Line(curattr);

function Line(attr)
{
	var i;
	if(attr==undefined)
		attr=7;

	/* The actual text of this line */
	this.text='';
	/* The attributes of this line */
	this.attr=new Array();
	for(i=1; i<80; i++)
		this.attr[i]=attr;
	this.canwrap=true;
	this.draw=Line_draw;
}

/* Draws the line at given ypos */
/* ToDo: Optimize with cleartoeol() */
function Line_draw(ypos)
{
	var i;

	console.gotoxy(1,ypos);
	for(i=1; i<=this.text.length; i++) {
		console.attributes=this.attr[i];
		console.write(this.text.substr(i-1,1));
	}
	if(i<81) {
		console.attributes=7;
		console.cleartoeol();
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
	for(i=0; i<lines_on_screen; i++) {
		if(line[i+topline]!=undefined)
			line[i+topline].draw(edit_top+i);
		else {
			console.attributes=7;
			console.cleartoeol();
		}
	}
}

/*
 * Adds a new line below the current one.
 */
function add_new_line_below()
{
	line.splice(topline+ypos-edit_top+1,0,new Line(curattr));
	/* Scroll lines below down */
	for(i=ypos-edit_top+1; i<lines_on_screen; i++) {
		if(line[i+topline]!=undefined)
			line[i+topline].draw(edit_top+i);
		else {
			console.attributes=7;
			console.cleartoeol();
		}
	}
}

/*
 * Advances to the next line creating a new line if necessary.
 */
function next_line()
{
	if(topline+ypos-edit_top>=line.length-1)
		line.push(new Line(curattr));
	ypos++;
	if(ypos>edit_top+lines_on_screen-1) {
		scroll(1);		/* Scroll up one line */
		ypos--;
	}
	if(xpos>line[ypos-edit_top+topline].text.length+1)
		xpos=line[ypos-edit_top+topline].text.length+1;
}

/*
 * Moves to the previous line if it's possible to do so.
 * Will not create another line.
 */
function try_prev_line()
{
	/* Yes, it's possible */
	if(ypos-edit_top+topline>0) {
		ypos--;
		if(ypos<edit_top) {
			scroll(-1);		/* Scroll down one line */
			ypos++;
		}
		if(xpos>line[ypos-edit_top+topline].text.length+1)
			xpos=line[ypos-edit_top+topline].text.length+1;
	}
	else
		console.beep();
}

/*
 * Advances to the next line if it's possible to do so.
 * Will not create another line.
 */
function try_next_line()
{
	/* Yes, it's possible */
	if(topline+ypos-edit_top<line.length-1) {
		ypos++;
		if(ypos>edit_top+lines_on_screen-1)
			scroll(1);		/* Scroll up one line */
		if(xpos>line[ypos-edit_top+topline].text.length+1)
			xpos=line[ypos-edit_top+topline].text.length+1;
	}
	else
		console.beep();
}

function status_line()
{
	console.gotoxy(1,1);
	if(insert)
		console.write("Insert Mode");
	else
		console.write("Overwrite Mode");
	console.cleartoeol();
	console.gotoxy(xpos,ypos);
}

/* ToDo: Optimize movement... */
function edit()
{
	var key;

	console.gotoxy(xpos,ypos);
	while(1) {
		status_line();
		key=console.inkey(0,10000);
		if(key=='')
			continue;
		switch(key) {
			/* We're getting all the CTRL keys here... */
			case '\x00':
			case '\x01':
				break;
			case '\x02':	/* KEY_HOME */
				xpos=1;
				console.gotoxy(xpos,ypos);
				break;
			case '\x03':
			case '\x04':
				break;
			case '\x05':	/* KEY_END */
				xpos=line[ypos-edit_top+topline].text.length+1;
				if(xpos>80)
					xpos=80;
				console.gotoxy(xpos,ypos);
				break;
			case '\x06':	/* KEY_RIGHT */
				xpos++;
				if(xpos>line[ypos-edit_top+topline].text.length+1) {
					xpos--;
					console.beep();
				}
				if(xpos>80) {
					xpos=1;
					try_next_line();
				}
				console.gotoxy(xpos,ypos);
				break;
			case '\x07':
				break;
			case '\x08':	/* Backspace */
				if(xpos>1) {
					line[ypos-edit_top+topline].text=line[ypos-edit_top+topline].text.substr(0,xpos-2)
							+line[ypos-edit_top+topline].text.substr(xpos-1);
					line[ypos-edit_top+topline].attr.splice(ypos,1);
					xpos--;
				}
				else {
					/* ToDo: Backspace onto the previous row and re-wrap... */
					console.beep();
				}
				line[ypos-edit_top+topline].draw(ypos);
				break;
			case '\x09':	/* TAB... ToDo expand to spaces */
				break;
			case '\x0a':	/* KEY_DOWN */
				try_next_line();
				console.gotoxy(xpos,ypos);
				break;
			case '\x0b':
			case '\x0c':
			case '\x0d':	/* CR */
				/* ToDo: Break current line at current pos... */
				if(insert)
					add_new_line_below();
				next_line();
				xpos=1;
				console.gotoxy(xpos,ypos);
				break;
			case '\x0e':
			case '\x0f':	/* CTRL-O */
				if(insert)
					insert=false;
				else
					insert=true;
				break;
			case '\x10':
			case '\x11':	/* CTRL-Q */
				return;
			case '\x12':
			case '\x13':
			case '\x14':
			case '\x15':
			case '\x16':
			case '\x17':
			case '\x18':
			case '\x19':
			case '\x1a':
			case '\x1b':
			case '\x1c':
				break;
			case '\x1d':	/* KEY_LEFT */
				xpos--;
				if(xpos<=0) {
					console.beep();
					xpos=1;
				}
				console.gotoxy(xpos,ypos);
				break;
			case '\x1e':	/* KEY_UP */
				try_prev_line();
				console.gotoxy(xpos,ypos);
				break;
			case '\x1f':
				break;
			case '\x7f':	/* DELETE */
				line[ypos-edit_top+topline].text=line[ypos-edit_top+topline].text.substr(0,xpos-1)
						+line[ypos-edit_top+topline].text.substr(xpos);
				line[ypos-edit_top+topline].attr.splice(xpos,1);
				line[ypos-edit_top+topline].draw(ypos);
				break;
			default:		/* Insert the char */
				if(insert) {
					line[ypos-edit_top+topline].text=line[ypos-edit_top+topline].text.substr(0,xpos-1)
							+key
							+line[ypos-edit_top+topline].text.substr(xpos-1);
					line[ypos-edit_top+topline].attr.splice(ypos,0,curattr);
					if(line[ypos-edit_top+topline].text.length > 80) {
						/* ToDo: Wrap */
					}
				}
				else {
					line[ypos-edit_top+topline].text=line[ypos-edit_top+topline].text.substr(0,xpos-1)
							+key
							+line[ypos-edit_top+topline].text.substr(xpos);
					line[ypos-edit_top+topline].attr[xpos]=curattr;
				}
				line[ypos-edit_top+topline].draw(ypos);
				console.gotoxy(xpos,ypos);
				xpos++;
				if(xpos>80) {
					xpos=1;
					if(insert)
						add_new_line_below();
					next_line();
					break;
				}
				console.gotoxy(xpos,ypos);
		}
	}
}

var oldpass=console.ctrlkey_passthru;
console.ctrlkey_passthru="+PO";
edit();
console.ctrlkey_passthru=oldpass;
