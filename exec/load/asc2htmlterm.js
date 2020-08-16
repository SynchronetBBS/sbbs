// $Id: asc2htmlterm.js,v 1.5 2016/04/24 00:55:44 deuce Exp $

// Converts a ^A/@-code text to HTML for htmlterm usage.

require("graphic.js", 'Graphic');

function asc2htmlterm(buf, dospin, partial, mode)
{
	var need_table=false;
	var outbuf_table='';
	var outbuf_text='';
	var rows=console.screen_rows;
	var cols=console.screen_columns;
	var spinner = new Array('-','\\','|','/');
	var spin=0;
	var colour_vals=new Array(
		"black",		/* black */
		"#0000a8",		/* dark blue */
		"#00a800",		/* dark green */
		"#00a8a8",		/* cyan */
		"#a80000",		/* dark red */
		"#a800a8",		/* magenta */
		"#a85400",		/* brown (dark yellow) */
		"#a8a8a8",		/* white */

		"#545454",		/* bright black (grey) */
		"#5454fc",		/* bright blue */
		"#54fc54",		/* bright green */
		"#54fcfc",		/* bright cyan */
		"#fc5454",		/* bright red */
		"#fc54fc",		/* bright magenta */
		"#fcfc54",		/* bright yellow */
		"white"		/* bright white */
	);

	if(partial==undefined)
		partial=false;

	/* Remove "bad" at-codes */
	buf=buf.replace(/@BEEP@/g,'');
	buf=buf.replace(/@CLS@/g,'');
	buf=buf.replace(/@MORE@/g,'');
	buf=buf.replace(/@PAUSE@/g,'');

	if(dospin) {
		console.write("Converting to HTML  ");
		console.write("\x08"+spinner[(spin++)%spinner.length]);
	}

	if(!partial) {
		outbuf_table += "\2\2<html><head><title>"+title+"</title></head><body bgcolor=\"black\" text=\""+colour_vals[7]+"\">";
		outbuf_text += "\2\2<html><head><title>"+title+"</title></head><body bgcolor=\"black\" text=\""+colour_vals[7]+"\">";
	}
	outbuf_table += "<font face=\"monospace\" color=\""+colour_vals[7]+"\"><table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"black\">\n";
	outbuf_text += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"black\"><tr><td nowrap><font face=\"monospace\" color=\""+colour_vals[7]+"\">\n";
	while(buf.length) {
		var screen=new Graphic(cols,rows,7,' ');
		if(dospin)
			console.write("\x08"+spinner[(spin++)%spinner.length]);
		buf=screen.putmsg(1,1, buf, 7, true, true, mode);
		if(dospin)
			console.write("\x08"+spinner[(spin++)%spinner.length]);
		var total_rows = rows;
		/* Find blank rows at end and remove them */
		for(total_rows=rows-1; total_rows >= 0; total_rows--) {
			var i;
			var differ=false;

			for(i=cols-1; i>=0; i--) {
				if(screen.data[i][total_rows].attr != screen.data[cols-1][rows-1].attr
						|| screen.data[i][total_rows].ch != ' ') {
					total_rows++;
					differ=true;
					break;
				}
			}
			if(differ)
				break;
		}
		for(row=0; row < total_rows; row++) {
			if(dospin)
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
		}
	}
	outbuf_table += "</table></font>";
	if(!need_table) {
		outbuf_table = outbuf_text + "</font></td></tr></table>";
		outbuf_text="";
	}

	if(!partial)
		outbuf_table += "<br><a href=\" \">Click here to continue...</a></body>\n\2";

	return(outbuf_table);
}
