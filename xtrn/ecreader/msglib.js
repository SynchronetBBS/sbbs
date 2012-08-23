// Functions for ecReader and messageAreaSelector
// echicken -at- bbs.electronicchicken.com

load("sbbsdefs.js");
load("frame.js");
load("tree.js");

function setSub(groupNumber, groupCode, subNumber, subCode) {
	user.cursub = subCode;
	bbs.curgrp = groupNumber;
	bbs.cursub = subNumber;
	bbs.cursub_code = subCode;
	return "EXIT";
}

function messageAreaSelector(x, y, width, height, parentFrame) {
	if(lbg === undefined)
		var lbg = BG_CYAN;		// Lightbar background
	if(lfg === undefined)
		var lfg = WHITE;		// Foreground colour of highlighted text
	if(nfg === undefined)
		var nfg = LIGHTGRAY;	// Foreground colour of non-highlighted text
	if(fbg === undefined)
		var fbg = BG_BLUE;		// Title, Header, Help frame background colour
	if(xfg === undefined)
		var xfg = LIGHTCYAN;	// Subtree expansion indicator colour
	if(tfg === undefined)
		var tfg = LIGHTCYAN;	// Uh, that line beside subtree items
	if(hfg === undefined)
		var hfg = "\1h\1w"; 	// Heading text (CTRL-A format, for now)
	if(sffg === undefined)
		var sffg = "\1h\1c";	// Heading sub-field text (CTRL-A format, for now)
	if(mfg === undefined)
		var mfg = "\1n\1w";		// Message text colour (CTRL-A format, for now)

	var selectorFrame = new Frame(x, y, width, height, fbg|WHITE, parentFrame);
	var selectorSubFrame = new Frame(x + 2, y + 1, width - 4, height - 2, BG_BLACK|WHITE, selectorFrame);
	var selectorTreeFrame = new Frame(x + 2, y + 2, width - 4, height - 3, BG_BLACK|WHITE, selectorSubFrame);
	var selectorTree = new Tree(selectorTreeFrame);
	selectorTree.colors.lbg = lbg;
	selectorTree.colors.lfg = lfg;
	selectorTree.colors.fg = nfg;
	selectorTree.colors.xfg = xfg;
	selectorTree.colors.tfg = tfg;
	
	for(var g = 0; g < msg_area.grp_list.length; g++) {
		if(!user.compare_ars(msg_area.grp_list[g].ars))
			continue;
		var groupTree = selectorTree.addTree(g + ") " + msg_area.grp_list[g].name);
		for(var s = 0; s < msg_area.grp_list[g].sub_list.length; s++) {
			if(!user.compare_ars(msg_area.grp_list[g].sub_list[s].ars))
				continue;
			groupTree.addItem(s + ") " + msg_area.grp_list[g].sub_list[s].name, setSub, g, msg_area.grp_list[g].code, s, msg_area.grp_list[g].sub_list[s].code);
		}
	}
	
	selectorFrame.open();
	selectorTree.open();
	selectorSubFrame.putmsg("Select a message area: (Q to quit)");
	selectorFrame.cycle();
	selectorTree.cycle();
	
	var userInput;
	var ret;
	while(ret != "EXIT") {
		userInput = console.getkey().toUpperCase();
		if(userInput == "Q")
			break;
		ret = selectorTree.getcmd(userInput);
		if(selectorFrame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
	}
	selectorTree.close();
	selectorFrame.close();
}