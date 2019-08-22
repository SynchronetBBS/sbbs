function clean_ansi(f, f2)
{
	var l;
	var attr = 7;
	var line = 0;
	var i;
	var seen = {};
	var seenm = {};
	var m;
	var col;
	var x;
	var origattr;
	var ret;
	var j;
	var hoffset = 0;
	var started = false;
	var lastbg = '';
	var nextbg = '';

	while ((l = f.readln(65535)) != null) {
		if (!started) {
			if (l == 'START OF SCREEN DUMP...')
				started = true;
			continue;
		}
		line++;
		col = 1;
		if (nextbg.length > 0) {
			l = nextbg + l;
			nextbg = '';
		}
		for (i = 0; i < l.length; i++) {
			m = l.substr(i).match(/^\x1b\[([0-?]*[ -/]*)([@-~])/);
			if (m == null) {
				switch (l[i]) {
					case '\r':
					case '\n':
						break;
					default:
						col++;
						if (col == 81) {
							line++;
							col = 1;
						}
						break;
				}
				continue;
			}
			origattr = attr;
			ret = '';
			switch(m[2]) {
				case 'K':
					while (col < 80)
						col++;
					i += m[0].length - 1;
					break;
				case 'J':
					if (col != 1) {
						if((attr & 0x70) != 0) {
							ret += '\x1b[40m';
							origattr &= 0x8f;
						}
						ret += '\r\n';
						line++;
						col = 1;
					}
					l = l.replace(m[0], ret);
					i += ret.length - 1;
					hoffset = line-1;
					break;
				case 'C':
					if (m[1].length == 0)
						x = 1;
					else
						x = parseInt(m[1], 10);
					if (lastbg.length > 0)
						ret += '\x1b[40m';
					for (j = 0; j < x; j++) {
						col++;
						ret += ' ';
					}
					ret += lastbg;
					l = l.replace(m[0], ret);
					i += ret.length - 1;
					break;
				case 'H':
					x = m[1].split(';');
					if (x.length == 0)
						x[0] = 1;
					if (x.length == 1)
						x[1] = 1;
					x[0] = parseInt(x[0], 10);
					x[1] = parseInt(x[1], 10);
					x[0] += hoffset;
					if((attr & 0x70) != 0) {
						ret += '\x1b[40m';
						origattr &= 0x8f;
					}
					if (line > x[0])
						throw('Upward! '+line+'/'+col+' to '+x[0]+' ('+m[1]+')');
					while(line < x[0]) {
						ret += '\r\n';
						line++;
						col = 1;
					}
					while (col < x[1]) {
						ret += ' ';
						col++;
						if (col == 81)
							throw('WhoopsH');
					}
					if((attr & 0x70) != 0)
						ret += '\x1b[' + ['40','44','42','46','41','45','43','47'][(attr & 0x70)>>4] + 'm';
					l = l.replace(m[0], ret);
					i += ret.length - 1;
					break;
				case 'm':
					x = m[1].split(';');
					for(j=0; j<x.length; j++) {
						switch(x[j]) {
							case '0':
								attr = 7;
								lastbg = '';
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
							case '40':
								attr = (attr & 0x8f);
								lastbg = '';
								break;
							case '41':
								attr = (attr & 0x8f) | 0x40;
								lastbg = '\x1b[41m';
								break;
							case '42':
								attr = (attr & 0x8f) | 0x20;
								lastbg = '\x1b[42m';
								break;
							case '43':
								attr = (attr & 0x8f) | 0x60;
								lastbg = '\x1b[43m';
								break;
							case '44':
								attr = (attr & 0x8f) | 0x10;
								lastbg = '\x1b[44m';
								break;
							case '45':
								attr = (attr & 0x8f) | 0x50;
								lastbg = '\x1b[45m';
								break;
							case '46':
								attr = (attr & 0x8f) | 0x30;
								lastbg = '\x1b[46m';
								break;
							case '47':
								attr = (attr & 0x8f) | 0x70;
								lastbg = '\x1b[47m';
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
					i += m[0].length - 1;
					break;
			}
		}
		if (col > 1 && ((attr & 0x70) != 0)) {
			l += '\x1b[40m';
			nextbg = lastbg;
		}
		f2.writeln(l);
	}
}

var f;
var o;
var l;
var last;
var inf;
var out;
var all;
var isANSI = false;

var x;
var infiles = ['LORDTXT.DAT', 'LGAMETXT.DAT', 'LORDEXT.DAT', 'TRAINTXT.DAT'];
for (x in infiles) {
	f = new File(js.exec_dir+infiles[x]);
	o = undefined;
	if (!f.open('r', true))
		throw(f.name);

	while((l = f.readln()) != null) {
		if (l[0] == '@' && l[1] == '#') {
			if (o != undefined) {
				o.close();
				if (isANSI) {
					print('Processing '+o.name);
					system.exec('/synchronet/sbbs/exec/syncview -l -a '+o.name+' > '+js.exec_dir+'ansi/'+file_getname(o.name.replace(/\.tmp$/, ''))+'.new');
					inf = new File(js.exec_dir+'ansi/'+file_getname(o.name.replace(/\.tmp$/, ''))+'.new');
					inf.open('r');
					out = new File(o.name.replace(/\.tmp$/, '')+'.ans');
					out.open('w');
					clean_ansi(inf, out);
					inf.close();
					out.close();
					//file_remove(inf.name);
				}
				else {
					file_rename(o.name, o.name.replace(/\.tmp$/, '.lrd'));
				}
				isANSI = false;
			}
			if (l.substr(2).length > 0) {
				o = new File(js.exec_dir+'ansi/'+l.substr(2).toLowerCase()+'.tmp');
				o.open('w');
				print('Opened '+o.name);
			}
		}
		else {
			if (!isANSI && l.indexOf('\x1b') != -1) {
				print(o.name+' Is an ANSI file');
				isANSI = true;
			}
			o.writeln(l);
		}
	}
}
file_remove(js.exec_dir+'ansi/neww.ans');
