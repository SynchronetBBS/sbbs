// typeasc.js

// Convert plain-text with (optional) Synchronet attribute (Ctrl-A) codes

require("sbbsdefs.js", "P_NONE");
var f;

var title='';
var filename='';
var mode=P_NONE;
var i;

for(i in argv) {
	if(filename=='') {
		switch(argv[i]) {
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
					this.f = new File(file_getcase(argv[i]) || argv[i]);
					filename=this.f.name.replace(/^.*[\\\/]/,'');
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
	print("usage: typeasc [-noatcodes|-noabort|-saveatr|-openclose|-nopause|-nocrlf] <filename>");
	exit(1);
}

if(title=='')
	title=filename;

console.printfile(f.name, mode);
