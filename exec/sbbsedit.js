// sbbsedit.js

// Full-screen message editor for Synchronet v3.10m+

// $Id$

const REVISION = "$Revision$".split(' ')[1];

const debug=true;

// Message header display format
var hdr_fmt	= "\1b\1h%-4s\1n\1b: \1h\1c%.60s\1>\r\n";
var stat_fmt	= "\1h\1w\0014 SBBSedit v" + REVISION + " - Type \1y/?\1w for help";
if(debug)
	stat_fmt += " row=%d l=%d ";
stat_fmt+="\1>\1n";
var tab_size	= 4;

load("sbbsdefs.js");

if(!argc) {
	alert("No filename specified");
	exit();
}

var subj,to,from;
var row = 0;
var rows = console.screen_rows;
var fname = argv[0];
var line = new Array();
var console_status=console.status;
var ctrlkey_passthru=console.ctrlkey_passthru;
console.ctrlkey_passthru|=(1<<3);	// Ctrl-C
console.ctrlkey_passthru|=(1<<11);	// Ctrl-K
console.ctrlkey_passthru|=(1<<16);	// Ctrl-P
console.ctrlkey_passthru|=(1<<20);	// Ctrl-T
console.ctrlkey_passthru|=(1<<21);	// Ctrl-U

f = new File(fname);
if(f.open("r")) {
	line = f.readAll();
	f.close();
	print("Read " + line.length + " lines");
}

console.attributes = LIGHTGRAY;
console.clear();

drop_file = new File(system.node_dir + "editor.inf");
if(drop_file.exists && drop_file.open("r")) {
	info = drop_file.readAll();
	delete drop_file;

	subj=info[0];
	to=info[1];
	from=info[3];
}

show_header();

show_tabline();

home = console.getxy();
row = home.y;

var l=0;
if(line.length)
	show_text(0);
update_status();

console.getstr_offset=0;
console.status|=CON_INSERT;	/* Default to insert mode */

var getstr_mode = K_EDIT|K_MSG|K_WRAP|K_NOCRLF|K_LEFTEXIT;

while(bbs.online) {
	update_status();
	console.clearline();
	console.write("\r");	/* make sure we're at column 0, always */
	str=line[l];
	if(str==undefined)
		str="";
	str = console.getstr(str,console.screen_columns-1,getstr_mode);

	/* Allow "inline" cursor up/down movement using getstr_offset */
	if(str.length) {
		if(console.status&(CON_UPARROW|CON_DOWNARROW) && console.getstr_offset!=str.length)
			getstr_mode|=K_USEOFFSET;
		else
			getstr_mode&=~K_USEOFFSET;
	}

	/* Commands */
	switch(str.toLowerCase()) {
		case "/s":		/* SAVE */
			if(!f.open("w"))
				alert("Error " + errno + " opening " + f.name);
			else {
				f.writeAll(line);
				f.close();
			}
			/* fall-through */
		case "/abt":	/* ABORT */
			bail();
			break;
		case "/clr":	/* CLEAR */
			line = new Array();
			console.gotoxy(home);
			show_text(0);
			continue;
		case "/?":		/* HELP */
		case "/help":
			console.clear();
			bbs.menu("editor");
			redraw(l);
			continue;
		case "/attr":	/* ATTRIBUTES */
			console.clear();
			bbs.menu("attr");
			redraw(l);
			continue;
		case "/l":
			redraw(l);
			continue;
		case "/d":	/* Delete current line */
			line.splice(l,1);
			show_text(0);
			continue;
	}

	line[l]=str;

	if(console.status&(CON_UPARROW|CON_BACKSPACE|CON_LEFTARROW|CON_DELETELINE)) {
		if(console.status&CON_DELETELINE) {
			/* Delete line */
			line.splice(l,1);
			show_text(0);	/* should be optimized */
			continue;
		}
		if(console.status&CON_BACKSPACE && str.length==0) {
			/* Delete line */
			line.splice(l,1);
			if(l) l--;
			if(row>home.y) {
				console.up();
				row--;
			}
			show_text(0);
			continue;
		}
		if(l) {
			l--;
			if(row>home.y) {
				console.up();
				row--;
			} else
				show_text(l);
		}
		console.write("\r");
	} else {
		if(console.status&CON_DOWNARROW && l+1>=line.length)
			continue;	/* Down allow arrow down past EOF */
		l++;
		if(console.status&CON_INSERT && !(console.status&CON_DOWNARROW)) {
			line.splice(l,0,"");	/* Insert a blank line */
			show_text(0);
		}
		if(row+1>=rows)
			l=show_text(first_line(l,row));
		else {
			console.crlf();
			row++;
		}
	}
}

bail();

function bail()
{
	console.clear();
	console.ctrlkey_passthru=ctrlkey_passthru;
	console.status=console_status;
	exit();
}

function show_text(begin)
{
	var save_row=row;

	console.pushxy();
	console.gotoxy(home);

	row=home.y;
	for(var l=begin; row<rows; l++,row++) {
		console.clearline();
		if(l < line.length)
			console.putmsg(line[l]);
		if(row<rows-1)
			console.crlf();
	}

	console.popxy();
	row=save_row;

	return(l-1);
}

function update_status()
{
	console.pushxy();
	console.gotoxy(1,rows);
	printf(stat_fmt,row,l);
	console.popxy();
}

function show_header()
{
	if(subj!=undefined)
		printf(hdr_fmt, "Subj", subj);
	if(to!=undefined)
		printf(hdr_fmt, "To",	to);
	if(from!=undefined)
		printf(hdr_fmt, "From", from);
}

function show_tabline()
{
	/* Display tab line */
	for(i=0;i<(console.screen_columns-1);i++)
		if(i && (i%tab_size)==0) {
			if((i%(tab_size*2))==0) {
				console.attributes=CYAN|HIGH;
				console.print('|');
			} else
				console.print(ascii(254));
		} else {
			console.attributes=YELLOW|HIGH;
			console.print(ascii(250));
		}
	console.crlf();
}

function redraw(l)
{
	save_row=row;
	console.attributes=LIGHTGRAY;
	console.clear();
	show_header();
	show_tabline();
	console.attributes=LIGHTGRAY;
	show_text(first_line(l,row));
	console.gotoxy(1,row=save_row);
}

function first_line(l,row)
{
	return(l-(row-home.y));
}

/* End of edit.js */