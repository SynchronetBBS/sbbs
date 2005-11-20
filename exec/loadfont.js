// Loads a font to the remote system

load("sbbsdefs.js");

if(argc==0) {
	alert("No font file specified!");
	exit(1);
}

var font=new File(argv[0]);
if(!font.exists) {
	alert("Cannot load font "+argv[0]+"!");
	exit(1);
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
		alert("Illegal font file: "+argv[0]+"!");
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
if(parseInt(ver[0]) > 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) >= 61)) {
	// Can handle fonts... send it.
	if(!font.open(argv[0],"rb",true,4096)) {
		alert("Unable to open "+argv[0]+"!");
		console.ctrlkey_passthru=oldctrl;
		exit(1);
	}
	var fontdata=font.read(font.length);
	if(fontdata.length != font.length) {
		alert("Error reading font data!");
		console.ctrlkey_passthru=oldctrl;
		exit(1);
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
	console.write("\x1b[0;255 D");
}
console.ctrlkey_passthru=oldctrl;
