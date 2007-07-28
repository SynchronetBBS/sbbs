// typeasc.js

// Convert plain-text with (optional) Synchronet attribute (Ctrl-A) codes to HTML

// $Id$

load("sbbsdefs.js");
load("asc2htmlterm.js");
var f;

var title='';
var filename='';

for(i in argv) {
	if(this.f==undefined) {
		f = new File(file_getcase(argv[i]));
		filename=file_getcase(argv[i]);
		filename=filename.replace(/^.*[\\\/]/,'');
	}
	else {
		if(title.length)
			title+=' ';
		title+=argv[i];
	}
}

if(this.f==undefined) {
	print("usage: typeasc <filename> [HTML title]");
	exit(1);
}

if(title=='')
	title=filename;

if(1 || (user.settings & USER_HTML)) {

	if(!f.open("rb",true,f.length)) {
		alert("Error " + errno + " opening " + f.name);
		exit(errno);
	}

	var buf=f.read(f.length);
	f.close();

	console.clear(7);
	buf=asc2htmlterm(buf,true,false);

	/* Disable autopause */
	var os = bbs.sys_status;
	bbs.sys_status |= SS_PAUSEOFF;
	bbs.sys_status &= ~SS_PAUSEON;
	console.write("\x08Done!");
	console.write(buf);
	bbs.sys_status=os;

	console.getkey();
}
else
	console.printfile(f.name);
