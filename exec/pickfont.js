// Selects a pre-loaded font
// Pass the desired fotn slot on the command line
// If nothing passed, changes to font 0 (CP437)

load("sbbsdefs.js");

// Disable parsed input... we need to do ESC processing ourselves here.
var oldctrl=console.ctrlkey_passthru;
console.ctrlkey_passthru=-1;

// Check if it's CTerm and supports font loading...
write("\x1b[c");
var response='';

while(1) {
	var ch=console.inkey(0, 1000);
	if(ch=="")
		break;
	response += ch;
	if(ch != '\x1b' && ch != '[' && (ch < ' ' || ch > '/') && (ch<'0' || ch > '?'))
		break;
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

var slot=0;
if(argc>0)
	slot=parseInt(argv[0]);

write("\x1b[0;"+slot+" D");
