// typeasc.js

// Convert plain-text with (optional) Synchronet attribute (Ctrl-A) codes to HTML

// $Id$

load("sbbsdefs.js");
load("asc2htmlterm.js");
var f;

var title='';
var filename='';
var mode=P_NONE;

for(i in argv) {
	if(filename=='') {
		switch(argv) {
			case '-noatcodes':
				mode |= P_NOATCODES;
				break;
			case '-noabort':
				mode |= P_NOABORT;
				break;
			case '-saveatr':
				mode |= P_SAVEATR;
				break;
			case '-openclose':
				mode |= P_OPENCLOSE;
				break;
			case '-nopause':
				mode |= P_NOPAUSE;
				break;
			case '-nocrlf':
				mode |= P_NOCRLF;
				break;
			default:
				if(this.f==undefined) {
					f = new File(file_getcase(argv[i]));
					filename=file_getcase(argv[i]);
					filename=filename.replace(/^.*[\\\/]/,'');
				}
		}
	}
	else {
		if(title.length)
			title+=' ';
		title+=argv[i];
	}
}

if(this.f==undefined) {
	print("usage: typeasc [-noatcodes|-noabort|-saveatr|-openclose|-nopause|-nocrlf] <filename> [HTML title]");
	exit(1);
}

if(title=='')
	title=filename;

if(user.settings & USER_HTML) {

	if(!f.open("rb",true,f.length)) {
		alert("Error " + errno + " opening " + f.name);
		exit(errno);
	}

	var buf=f.read(f.length);
	f.close();

	console.clear(7);
	buf=asc2htmlterm(buf,true,false, mode);

	/* Disable autopause */
	var os = bbs.sys_status;
	bbs.sys_status |= SS_PAUSEOFF;
	bbs.sys_status &= ~SS_PAUSEON;
	console.write("\x08Done!");
	console.write(buf);
	bbs.sys_status=os;

	if(!(mode & P_NOPAUSE))
		console.getkey();
}
else
	console.printfile(f.name, mode);
