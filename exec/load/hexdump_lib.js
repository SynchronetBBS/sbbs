// $Id$

// Returns an array
function generate(title, val, include_ascii)
{
	var i;
	var output = [];

	if(include_ascii === undefined)
		include_ascii = true;

	if(!val) {
		return [format("%s: (null)\r\n", title)];
	}
	if(title)
		output.push(format("%s: %u bytes", title, val.length));
	var line = '';
	var ascii = '';
	for(i=0; i < val.length; i++) {
		var ch = val.charCodeAt(i);
		if(i && i%16 == 0) {
			output.push(line + format("  %s", ascii));
			ascii = '';
			line = '';
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
			if((i%16) < 8)
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
