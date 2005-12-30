// Selects a pre-loaded font
// Pass the desired fotn slot on the command line
// If nothing passed, changes to font 0 (CP437)

pickfont();
var CTerm_Version;

function pickfont()
{
	var detect=false;
	// Parse cmd line.
	for(i=0; i<argc; i++) {
		if(argv[i].toString().substr(0,1)=="-") {
			switch(argv[i].substr(1,1).toUpperCase()) {
				case 'D':   /* Attempt to detect CTerm */
					detect=true;
					break;
			}
		}
	}

	// Check if it's CTerm and supports font loading...
	var ver=new Array(0,0);
	if(CTerm_Version==undefined) {
		if(detect) {
			// Disable parsed input... we need to do ESC processing ourselves here.
			var oldctrl=console.ctrlkey_passthru;
			console.ctrlkey_passthru=-1;
	
			write("\x1b[c");
			var response='';

			while(1) {
				var ch=console.inkey(0, 5000);
				if(ch=="")
					break;
				response += ch;
				if(ch != '\x1b' && ch != '[' && (ch < ' ' || ch > '/') && (ch<'0' || ch > '?'))
					break;
			}
			// var printable=response;
			// printable=printable.replace(/\x1b/g,"");
			// alert("Response: "+printable);

			if(response.substr(0,21) != "\x1b[=67;84;101;114;109;") {	// Not CTerm
				CTerm_Version=0;
				console.ctrlkey_passthru=oldctrl;
				return(0);
			}
			if(response.substr(-1) != "c") {	// Not a DA
				CTerm_Version=0;
				console.ctrlkey_passthru=oldctrl;
				return(0);
			}
			var version=response.substr(21);
			version=version.replace(/c/,"");
			CTerm_Version=version;
			ver=version.split(/;/);
			console.terminal="CTerm;"+ver.join(";");
			console.ctrlkey_passthru=oldctrl;
		}
	}
	else {
		ver=CTerm_Version.split(/;/);
		if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
			// Too old for dynamic fonts
			return(0);
		}
	}

	if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
		// Too old for dynamic fonts
		return(0);
	}

	var slot=0;
	if(argc>0)
		slot=parseInt(argv[0]);

	write("\x1b[0;"+slot+" D");
}
