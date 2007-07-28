// typeasc.js

// Convert plain-text with (optional) Synchronet attribute (Ctrl-A) codes to HTML

// $Id$

load("sbbsdefs.js");
load("graphic.js");
var f;

var colour_vals=new Array(
	"black",		/* black */
	"#a80000",		/* dark red */
	"#00a800",		/* dark green */
	"#a85400",		/* brown (dark yellow) */
	"#0000a8",		/* dark blue */
	"#a800a8",		/* magenta */
	"#00a8a8",		/* cyan */
	"#a8a8a8",		/* white */
	"#545454",		/* bright black (grey) */
	"#fc5454",		/* bright red */
	"#54fc54",		/* bright green */
	"#fcfc54",		/* bright yellow */
	"#5454fc",		/* bright blue */
	"#fc54fc",		/* bright magenta */
	"#54fcfc",		/* bright cyan */
	"white"		/* bright white */
);

for(i in argv) {
	switch(argv[i].toLowerCase()) {
	default:
		f = new File(file_getcase(argv[i]));
		break;
	}
}

if(this.f==undefined) {
	print("usage: typeasc <filename>");
	exit(1);
}

if(1 || (user.settings & USER_HTML)) {

	if(!f.open("rb",true,f.length)) {
		alert("Error " + errno + " opening " + f.name);
		exit(errno);
	}

	var buf=f.read(f.length);
	f.close();
	var need_table=false;
	var outbuf_table;
	var outbuf_text;
	var rows=console.screen_rows;
	var cols=console.screen_columns;
	var spinner = new Array('-','\\','|','/');
	var spin=0;

	/* Remove "bad" at-codes */
	buf=buf.replace(/@BEEP@/g,'');
	buf=buf.replace(/@CLS@/g,'');
	buf=buf.replace(/@MORE@/g,'');
	buf=buf.replace(/@PAUSE@/g,'');

	console.clear(7);
	console.write("Converting to HTML  ");
	console.write("\x08"+spinner[(spin++)%spinner.length]);

	console.line_counter=0;
	outbuf_table = "\2\2<html><head><title>No title</title></head><body bgcolor=\"black\" text=\""+colour_vals[7]+"\"><font face=\"monospace\"><table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"black\">\n";
	outbuf_text = "\2\2<html><head><title>No title</title></head><body bgcolor=\"black\" text=\""+colour_vals[7]+"\"><table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"black\"><tr><td nowrap><font face=\"monospace\">\n";
	while(buf.length) {
		console.line_counter=0;
		var screen=new Graphic(cols,rows,7,' ');
		console.write("\x08"+spinner[(spin++)%spinner.length]);
		buf=screen.putmsg(1,1, buf, 7, true, true);
		console.write("\x08"+spinner[(spin++)%spinner.length]);
		var total_rows = rows;
		/* Find blank rows at end and remove them */
		for(total_rows=rows-1; total_rows >= 0; total_rows--) {
			var i;
			var differ=false;

			for(i=cols-1; i>=0; i--) {
				if(screen.data[i][total_rows].attr != screen.data[cols-1][rows-1].attr
						|| screen.data[i][total_rows].ch != ' ') {
					differ=true;
					break;
				}
			}
			if(differ)
				break;
		}
		for(row=0; row < total_rows; row++) {
			console.write("\x08"+spinner[(spin++)%spinner.length]);
			outbuf_table += "<tr>";
			for(col=0; col<cols; col++) {
				var blinking=screen.data[col][row].attr & 0x80;
				var celltext="";

				/* Create cell... set background color */
				if(screen.data[col][row].attr & 0x70) {
					outbuf_table += "<td nowrap bgcolor=\""+colour_vals[(screen.data[col][row].attr & 0x70)>>4]+"\">";
					need_table=true;
					outbuf_text='';
				}
				else
					outbuf_table += "<td nowrap>";

				/* Set font (forground colour) */
				if((screen.data[col][row].attr & 0x0f)!=7)
					celltext += "<font color=\""+colour_vals[screen.data[col][row].attr & 0x0f]+"\">";
				/* Blink? */
				if(blinking)
					celltext += "<blink>";
				/* Transpate the char and put 'er in the cell */
				var ch=html_encode(screen.data[col][row].ch, true, false, false, false);
				if(ch==' ')
					ch="&nbsp;";
				celltext += ch;
				if(blinking)
					celltext += "</blink>";
				if((screen.data[col][row].attr & 0x0f)!=7)
					celltext += "</font>";
				if(!need_table)
					outbuf_text += celltext;
				outbuf_table += celltext;
				outbuf_table += "</td>";
			}
			outbuf_table += "</tr>\n";
			if(!need_table)
				outbuf_text += "<br>\n";
			console.line_counter=0;
		}
	}
	outbuf_table += "</table></font><br><a href=\" \">Click here to continue...</a></body>\n\2";
	if(!need_table)
		outbuf_text += "</font></td></tr></table><br><a href=\" \">Click here to continue...</a></body>\n\2";
	/* Disable autopause */
	var os = bbs.sys_status;

	bbs.sys_status |= SS_PAUSEOFF;
	bbs.sys_status &= ~SS_PAUSEON;
	console.write("\x08Done!");
	if(need_table)
		console.write(outbuf_table);
	else
		console.write(outbuf_text);
	bbs.sys_status=os;

	console.getkey();
}
else
	console.printfile(f.name);
