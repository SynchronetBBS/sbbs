// $Id: hexdump_lib.js,v 1.5 2019/05/09 22:21:25 rswindell Exp $

function num_digits(n)
{
	return ( n < 0x10         ? 1 
	       : n < 0x100        ? 2 
	       : n < 0x1000       ? 3 
	       : n < 0x10000      ? 4 
	       : n < 0x100000     ? 5 
	       : n < 0x1000000    ? 6 
	       : n < 0x10000000   ? 7 
	       : n < 0x100000000  ? 8 
           : n < 0x1000000000 ? 9 : 10 );
}


// Returns an array
function generate(title, val, include_ascii, include_offset)
{
	var i;
	var output = [];

	if(include_ascii === undefined)
		include_ascii = true;

	if(!val) {
		return [format("%s: (null)\r\n", title)];
	}
	var length = val.length;
	if(title)
		output.push(format("%s: %u bytes", title, length));
	var line = '';
	var ascii = '';
	var digits = num_digits(length - 1);
	if(include_offset)
		line = format("%0*x:  ", digits, 0);
	for(i=0; i < length; i++) {
		var ch = val.charCodeAt(i);
		if(i && i%16 == 0) {
			output.push(line + format("  %s", ascii));
			ascii = '';
			line = '';
			if(include_offset)
				line = format("%0*x:  ", digits, i);
		}
		else if(i && i%8 == 0)
			line += "- ";
		line += format("%02X ", ch);
		if(include_ascii)
			ascii += format("%c", (ch >= 0x20 && ch < 0x7f) ? ch : '.');
	}
	if(ascii) {
		var gap = 0;
		if(i%16) {
			gap = (16-(i%16))*3;
			if((i%16) < 9)
				gap += 2;
		}
		output.push(line + format("%*s  %s\r\n", gap, "", ascii));
	}
	return output;
}

function file(file, include_ascii)
{
	var buf = file.read();

	return this.generate(file.name, buf, include_ascii);
}

// Leave as last line, so maybe used as a lib
this;
