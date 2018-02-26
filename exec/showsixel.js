if(bbs.mods.CTerm_Version === undefined) {
	// Disable parsed input... we need to do ESC processing ourselves here.
	var oldctrl=console.ctrlkey_passthru;
	console.ctrlkey_passthru=-1;

	write("\x1b[c");
	var response='';

	while(1) {
		var ch=console.inkey(0, 2000);
		if(ch=="")
			break;
		response += ch;
		if (/\x1b\[[=\?<>:]?[0-9;]+c/)
			break;
	}

	var m=response.match(/\x1b\[=67;84;101;114;109;([0-9]+;[0-9]+)[0-9;]*c/);
	if (m == null) {
		bbs.mods.CTerm_Version='0';
	}
	else {
		bbs.mods.CTerm_Version = m[1];
	}
	console.ctrlkey_passthru=oldctrl;
}

if (bbs.mods.CTerm_Version !== undefined) {
	var ctv = bbs.mods.CTerm_Version.split(/;/);

	if (ctv[0] >= 1 && ctv[1] >= 189) {
		var image=new File(argv[0]);
		if (image.exists) {
			if (image.open("rb", true)) {
				var readlen = console.output_buffer_level + console.output_buffer_space;
				readlen /= 2;
				console.clear();
				while (!image.eof) {
					var imagedata=image.read(readlen);
					while(console.output_buffer_space < imagedata.length)
						mswait(1);
					console.write(imagedata);
				}
				image.close();
			}
		}
	}
}
