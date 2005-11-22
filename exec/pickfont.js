// Selects a pre-loaded font
// Pass the desired fotn slot on the command line
// If nothing passed, changes to font 0 (CP437)

load("sbbsdefs.js");

// Check if it's CTerm and supports font loading...
if(console.terminal.substr(0,6) != 'CTerm;')
	/* Not CTerm */
	exit(0);

var ver=console.terminal.substr(6).split(/;/);
if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
	// Too old for dynamic fonts
	exit(0);
}

var slot=0;
if(argc>0)
	slot=parseInt(argv[0]);

write("\x1b[0;"+slot+" D");
