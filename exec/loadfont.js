// Loads a font to the remote system
/*
 * Supported arguments:
 * -H causes the last sent font NOT be made active. (Default is activate font)
 * -S### sets the first font slot to ### default is 256 - number of fonts
 * Multiple files can be sent at the same time.
 */

load("sbbsdefs.js");
var filenames=new Array();
var showfont=true;
var firstslot=-1;
var i;

for(i=0; i<argc; i++) {
	if(argv[i].substr(0,1)=="-") {
		switch(argv[i].substr(1,1).toUpperCase()) {
			case 'H':	/* Hidden update */
				showfont=false;
				break;
			case 'S':	/* First font slot */
				firstslot=parseInt(argv[i].substr(2));
				break;
		}
	}
	else {
		filenames.push(argv[i]);
	}
}

if(filenames.length==0) {
	alert("No font file specified!");
	log(LOG_ERR, "!ERROR No font file specified!");
	exit(1);
}

if(firstslot==-1) {
	firstslot=256-filenames.length;
}

if(firstslot+filenames.length > 256) {
	alert("No room for "+filenames.length+" fonts if first slot is "+firstslot);
	log(LOG_ERR, "!ERROR No room for "+filenames.length+" fonts if first slot is "+firstslot);
	exit(1);
}

if(firstslot < 32) {
	alert("Cannot overwrite default fonts!");
	log(LOG_ERR, "!ERROR Cannot overwrite default fonts!");
	exit(1);
}

// Disable parsed input... we need to do ESC processing ourselves here.
var oldctrl=console.ctrlkey_passthru;
console.ctrlkey_passthru=-1;

// Check if it's CTerm and supports font loading...
console.write("\x1b[c");
var response='';

while(1) {
	var ch=console.inkey(0, 1000);
	if(ch=="")
		break;
	response += ch;
	if(ch != '\x1b' && ch != '[' && ch != '=' && ch != ';' && (ch<'0' && ch > '9')) {
		break;
	}
}

if(response.substr(0,21) != "\x1b[=67;84;101;114;109;") {	// Not CTerm
	console.ctrlkey_passthru=oldctrl;
	exit(0);
}
if(response.substr(-1) != "c") {	// Not a DA
	console.ctrlkey_passthru=oldctrl;
	exit(0);
}
var version=response.substr(21);
version=version.replace(/c/,"");
var ver=version.split(/;/);
if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
	// Too old for dynamic fonts
	console.ctrlkey_passthru=oldctrl;
	exit(0);
}

for (i in filenames) {
	var font=new File(filenames[i]);

	if(!font.exists) {
		alert("Cannot load font "+filenames[i]+"!");
		log(LOG_ERR, "!ERROR Cannot load font "+filenames[i]+"!");
		continue;
	}

	var fontsize;
	switch(font.length) {
		case 4096:
			fontsize=0;
			break;
		case 3586:
			fontsize=1;
			break;
		case 2048:
			fontsize=2;
			break;
		default:
			alert("Illegal font file: "+filenames[i]+"!");
			log(LOG_ERR, "!ERROR Illegal font file: "+filenames[i]+"!");
			continue;
	}

	if(!font.open(argv[0],"rb",true,4096)) {
		alert("Unable to open "+filenames[i]+"!");
		log(LOG_ERR,"!ERROR Unable to open "+filenames[i]+"!");
		continue;
	}

	var fontdata=font.read(font.length);
	if(fontdata.length != font.length) {
		alert("Error reading font data!");
		log(LOG_ERR,"!ERROR Error reading font data!");
		font.close();
		continue;
	}

	console.write("\x1b[=255;"+fontsize+"{");

	// This doesn't send it all...
	// console.write(fontdata);
	while(console.output_buffer_level)
		mswait(1);
	if(!(console.telnet_mode & TELNET_MODE_OFF))
		fontdata=fontdata.replace(/\xff/g,"\xff\xff");
	while(fontdata.length) {
		client.socket.send(fontdata.substr(0,1024));
		fontdata=fontdata.substr(1024);
	}
	font.close();
}
if(showfont)
	console.write("\x1b[0;"+(firstslot+filenames.length-1)+" D");
console.ctrlkey_passthru=oldctrl;
