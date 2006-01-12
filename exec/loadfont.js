// Loads a font to the remote system
/*
 * Supported arguments:
 * -H causes the last sent font NOT be made active. (Default is activate font)
 * -S### sets the first font slot to ### default is 256 - number of fonts
 * -P shows progress indicator.
 * Multiple files can be sent at the same time.
 */

load("sbbsdefs.js");
if(bbs.mods.CTerm_Version==undefined)
	bbs.mods.CTerm_Version=null;
loadfont();

function loadfont()
{
	var filenames=new Array();
	var showfont=true;
	var showprogress=false;
	var firstslot=-1;
	var i;

	for(i=0; i<argc; i++) {
		if(argv[i].toString().substr(0,1)=="-") {
			switch(argv[i].substr(1,1).toUpperCase()) {
				case 'H':	/* Hidden update */
					showfont=false;
					break;
				case 'S':	/* First font slot */
					firstslot=parseInt(argv[i].substr(2));
					break;
				case 'P':	/* Show progress indicator */
					showprogress=true;
					break;
			}
		}
		else {
			if(argv[i].constructor==Array)
				filenames=filenames.concat(argv[i]);
			else
				filenames.push(argv[i].toString());
		}
	}

	if(filenames.length==0) {
		alert("No font file specified!");
		log(LOG_ERR, "!ERROR No font file specified!");
		return(0);
	}

	if(firstslot==-1) {
		firstslot=256-filenames.length;
	}

	if(firstslot+filenames.length > 256) {
		alert("No room for "+filenames.length+" fonts if first slot is "+firstslot);
		log(LOG_ERR, "!ERROR No room for "+filenames.length+" fonts if first slot is "+firstslot);
		return(-1);
	}

	if(firstslot < 32) {
		alert("Cannot overwrite default fonts!");
		log(LOG_ERR, "!ERROR Cannot overwrite default fonts!");
		return(-1);
	}

	if(showprogress) {
		writeln();
		write("Detecting font support... ");
	}

	// Check if it's CTerm and supports font loading...
	var ver=new Array(0,0);
	if(bbs.mods.CTerm_Version == undefined || bbs.mods.CTerm_Version == null) {
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
			bbs.mods.CTerm_Version=0;
			console.ctrlkey_passthru=oldctrl;
			if(showprogress)
				writeln("Not detected.");
			return(0);
		}
		if(response.substr(-1) != "c") {	// Not a DA
			bbs.mods.CTerm_Version=0;
			console.ctrlkey_passthru=oldctrl;
			if(showprogress)
				writeln("Not detected.");
			return(0);
		}
		var version=response.substr(21);
		version=version.replace(/c/,"");
		bbs.mods.CTerm_Version=version;
		ver=version.split(/;/);
		console.ctrlkey_passthru=oldctrl;
	}
	else {
		ver=bbs.mods.CTerm_Version.split(/;/);
		if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
			// Too old for dynamic fonts
			if(showprogress)
				writeln("Not detected.");
			return(0);
		}
	}
	if(parseInt(ver[0]) < 1 || (parseInt(ver[0])==1 && parseInt(ver[1]) < 61)) {
		// Too old for dynamic fonts
		if(showprogress)
			writeln("Not detected.");
		return(0);
	}

	if(showprogress) {
		writeln("Detected!");
		write("Loading fonts...");
	}

	/* From here on, it's socket access only */
	console.lock_input(true);
	while(console.output_buffer_level)
		mswait(1);

	var oldblock=client.socket.nonblocking;
	client.socket.nonblocking=false;

	for (i in filenames) {
		var font=new File(filenames[i]);

		if(!font.exists) {
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
				log(LOG_ERR, "!ERROR Illegal font file: "+filenames[i]+"!");
				continue;
		}

		if(!font.open("rb",true,4096)) {
			log(LOG_ERR,"!ERROR "+font.error+" Unable to open "+filenames[i]+"! errno="+errno);
			continue;
		}

		// Doesn't work on Win32.. Win32 sucks.
		var fontdata=font.read(font.length);
		var fonterr=font.error;

		if(fontdata.length != font.length) {
			log(LOG_ERR,"!ERROR Error "+fonterr+" reading font data (read "+fontdata.length+", expected "+font.length+") errno="+errno);
			font.close();
			continue;
		}
		font.close();

		client.socket.send("\x1b[="+(firstslot+parseInt(i))+";"+fontsize+"{");

		// This doesn't send it all...
		// write(fontdata);
		if(!(console.telnet_mode & TELNET_MODE_OFF))
			fontdata=fontdata.replace(/\xff/g,"\xff\xff");
		while(fontdata.length) {
			if(client.socket.poll(0,true)) {
			// Oh my aching head...
				if(client.socket.send(fontdata.substr(0,1024)))
					fontdata=fontdata.substr(1024);
			//	if(client.socket.send(fontdata.substr(0,1)))
			//		fontdata=fontdata.substr(1);
			}
		}
		if(showprogress)
			client.socket.send(".");
	}
	if(showprogress)
		client.socket.send("done.\r\n");
	if(showfont)
		client.socket.send("\x1b[0;"+(firstslot+filenames.length-1)+" D");
	client.socket.nonblocking=oldblock;
	console.ctrlkey_passthru=oldctrl;
	console.lock_input(false);
	return(0);
}
