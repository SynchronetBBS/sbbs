function convert(f, f2)
{
	var l;
	var attr = 7;
	var line = 1;
	var i;
	var seen = {};
	var seenm = {};
	var m;
	var col;
	var x;
	var origattr = -1;
	var j;

	function apply_attr() {
		if ((attr & 0x70) != (origattr & 0x70))
			f2.write('`r' + ['0','1','2','3','4','5','6','7'][(attr & 0x70)>>4]);
		if ((attr & 0x8f) != (origattr & 0x8f)) {
			if ((attr & 0x80) == 0)
				f2.write('`' + ['^', '1', '2','3','4','5','6','7','8','9','0','!','@','#','$','%'][attr & 0x0f]);
			else
				f2.write('`B' + ['^', '1', '2','3','4','5','6','7','8','9','0','!','@','#','$','%'][attr & 0x0f]);
		}
		origattr = attr;
	}

	while ((l = f.readln(65535)) != null) {
		if (l[0] == '@' && l[1] == '#') {
			attr = 2;
			line = 1;
			continue;
		}
		col = 1;
		for (i = 0; i < l.length; i++) {
			m = l.substr(i).match(/^\x1b\[([0-?]*[ -/]*)([@-~])/);
			if (m == null) {
				switch (l[i]) {
					case '\r':
					case '\n':
						throw('Newline!');
						break;
					default:
						apply_attr();
						f2.write(l[i]);
						col++;
						if (col == 81) {
							f2.write('`n');
							line++;
							col = 1;
						}
						break;
				}
				continue;
			}
			switch(m[2]) {
				case 'K':
					if (m[1].length > 0)
						throw('K ARG!');
					apply_attr();
					while (col < 80) {
						f2.write(' ');
						col++;
					}
					col = 1;
					line++;
					i += m[0].length - 1;
					break;
				case 'J':
					i += 1;
					break;
				case 'C':
					if((attr & 0x70) != 0) {
						f2.write('`r0');
						origattr &= 0x8f;
					}
					if (m[1].length == 0)
						x = 1;
					else
						x = parseInt(m[1], 10);
					for (j = 0; j < x; j++) {
						f2.write(' ');
						col++;
						if (col == 81) {
							throw('Whoops!');
						}
					}
					i += m[0].length - 1;
					break;
				case 'H':
					x = m[1].split(';');
					if (x.length == 0)
						x[0] = 1;
					if (x.length == 1)
						x[1] = 1;
					x[0] = parseInt(x[0], 10);
					x[1] = parseInt(x[1], 10);
					// Special case...
					if (x[0] == 1 && x[1] == 1) {
						i += m[0].length - 1;
						break;
					}
					if((attr & 0x70) != 0) {
						f2.write('`r0');
						origattr &= 0x8f;
					}
					if (line > x[0])
						throw('Upward! '+line+'/'+col+' to '+x[0]+' ('+m[1]+')');
					while(line < x[0]) {
						f2.write('\n');
						line++;
						col = 1;
					}
					while (col < x[1]) {
						f2.write(' ');
						col++;
						if (col == 81)
							throw('WhoopsH');
					}
					i += m[0].length - 1;
					break;
				case 'm':
					x = m[1].split(';');
					for(j=0; j<x.length; j++) {
						switch(x[j]) {
							case '0':
								attr = 7;
								break;
							case '1':
								attr |= 0x08;
								break;
							case '5':	// BLINK?!?! REALLY?
								attr |= 0x80;
								break;
							case '30':
								attr = attr & 0xf8;
								break;
							case '31':
								attr = (attr & 0xf8) | 4;
								break;
							case '32':
								attr = (attr & 0xf8) | 2;
								break;
							case '33':
								attr = (attr & 0xf8) | 6;
								break;
							case '34':
								attr = (attr & 0xf8) | 1;
								break;
							case '35':
								attr = (attr & 0xf8) | 5;
								break;
							case '36':
								attr = (attr & 0xf8) | 3;
								break;
							case '37':
							case '39':
								attr = (attr & 0xf8) | 7;
								break;
							case '49':
							case '40':
								attr = (attr & 0x8f);
								break;
							case '41':
								attr = (attr & 0x8f) | 0x40;
								break;
							case '42':
								attr = (attr & 0x8f) | 0x20;
								break;
							case '43':
								attr = (attr & 0x8f) | 0x60;
								break;
							case '44':
								attr = (attr & 0x8f) | 0x10;
								break;
							case '45':
								attr = (attr & 0x8f) | 0x50;
								break;
							case '46':
								attr = (attr & 0x8f) | 0x30;
								break;
							case '47':
								attr = (attr & 0x8f) | 0x70;
								break;
						}
					}
					i += m[0].length - 1;
					break;
				default:
					if (seen[m[2]] == undefined) {
						print('Code: "'+m[2]+'" in '+f.name);
						seen[m[2]] = 0;
					}
					seen[m[2]]++;
					break;
			}
		}
		if ((attr & 0x70) != (origattr & 0x70))
			f2.write('`r' + ['0','1','2','3','4','5','6','7'][(attr & 0x70)>>4]);
		origattr = (origattr & 0x8f) | (attr & 0x70);
		f2.write('\n');
		line++;
	}
}

var f;
var f2 = new File(js.exec_dir+'lordtxt.lrd');
f2.open('w');
var files = directory(js.exec_dir+'ansi/*.ans');
files = files.concat(directory(js.exec_dir+'ansi/*.lrd'));
files = files.sort();
var i;
var l;
var isANSI = false;
for (i in files) {
	isANSI = false;
	f = new File(files[i]);
	f2.writeln('@#'+file_getname(f.name).toUpperCase().replace(/\..*$/,''));
	f.open('r');
	while ((l = f.readln(65535)) != null) {
		if (l.indexOf('\x1b') != -1) {
			isANSI = true;
			break;
		}
	}
	f.position = 0;

	if (!isANSI) {
		while ((l = f.readln(65535)) != null) {
			f2.writeln(l);
		}
		f.close();
	}
	else {
		convert(f, f2);
	}
}
