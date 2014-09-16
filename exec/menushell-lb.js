load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("menu-commands.js");

var frame, treeBorderFrame, treeFrame, tree, treeItems = [];

var parseCtrlA = function(str) {
	var arr = str.toUpperCase().split("\\1");
	var light = (arr.indexOf("H") > arr.indexOf("N"));
	var attr = 0;
	for(var a = 0; a < arr.length; a++) {
		switch(arr[a]) {
			case "K":
				attr |= ((light) ? DARKGRAY : BLACK);
				break;
			case "R":
				attr |= ((light) ? LIGHTRED : RED);
				break;
			case "G":
				attr |= ((light) ? LIGHTGREEN : GREEN);
				break;
			case "Y":
				attr |= ((light) ? YELLOW : BROWN);
				break;
			case "B":
				attr |= ((light) ? LIGHTBLUE : BLUE);
				break;
			case "M":
				attr |= ((light) ? LIGHTMAGENTA : MAGENTA);
				break;
			case "C":
				attr |= ((light) ? LIGHTCYAN : CYAN);
				break;
			case "W":
				attr |= ((light) ? WHITE : LIGHTGRAY);
				break;
			case "0":
				attr |= BG_BLACK;
				break;
			case "1":
				attr |= BG_RED;
				break;
			case "2":
				attr |= BG_GREEN;
				break;
			case "3":
				attr |= BG_BROWN;
				break;
			case "4":
				attr |= BG_BLUE;
				break;
			case "5":
				attr |= BG_MAGENTA;
				break;
			case "6":
				attr |= BG_CYAN;
				break;
			case "7":
				attr |= BG_LIGHTGRAY;
				break;
			default:
				break;
		}
	}
	return attr;
}

Frame.prototype.drawBorder = function(color) {
	var theColor = color;
	if(Array.isArray(color));
		var sectionLength = Math.round(this.width / color.length);
	this.pushxy();
	for(var y = 1; y <= this.height; y++) {
		for(var x = 1; x <= this.width; x++) {
			if(x > 1 && x < this.width && y > 1 && y < this.height)
				continue;
			var msg;
			this.gotoxy(x, y);
			if(y == 1 && x == 1)
				msg = ascii(218);
			else if(y == 1 && x == this.width)
				msg = ascii(191);
			else if(y == this.height && x == 1)
				msg = ascii(192);
			else if(y == this.height && x == this.width)
				msg = ascii(217);
			else if(x == 1 || x == this.width)
				msg = ascii(179);
			else
				msg = ascii(196);
			if(Array.isArray(color)) {
				if(x == 1)
					theColor = color[0];
				else if(x % sectionLength == 0 && x < this.width)
					theColor = color[x / sectionLength];
				else if(x == this.width)
					theColor = color[color.length - 1];
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

var launchItem = function(item, tree) {
	console.clear();
	frame.invalidate();
	var path = item.command.split(".");
	if(path[1] == "Externals")
		bbs.exec_xtrn(path[2]);
	else
		Commands[path[1]][path[2]].Action();
}

var toAttr = function(str) {
	return parseCtrlA(str.replace(/\\\\/g, "\\"));
}

var clearTree = function() {
	for(var item in treeItems)
		tree.deleteItem(tree.trace(treeItems[item].hash));
	treeItems = [];
}

var setTreeColors = function(menu) {
	tree.colors.fg = toAttr(Commands.Menus[menu].Lightbar.InactiveForeground);
	tree.colors.bg = toAttr(Commands.Menus[menu].Lightbar.InactiveBackground);
	tree.colors.lfg = toAttr(Commands.Menus[menu].Lightbar.ActiveForeground);
	tree.colors.lbg = toAttr(Commands.Menus[menu].Lightbar.ActiveBackground);
	tree.colors.kfg = toAttr(Commands.Menus[menu].Lightbar.HotkeyColor);
}

var loadMenu = function(menu) {

	clearTree();
	setTreeColors(menu);
	var widest = 0, shown = 0;
	for(var command in Commands.Menus[menu].Commands) {
		if(!user.compare_ars(Commands.Menus[menu].Commands[command].ARS))
			continue;
		var itemText = strip_ctrl(Commands.Menus[menu].Commands[command].text.replace(/\\1/g, ascii(1)));
		if(itemText.length < 1)
			continue;
		if(Commands.Menus[menu].Lightbar.Hotkeys) {
			if(itemText.substr(0, 1) != Commands.Menus[menu].Commands[command].hotkey)
				itemText = Commands.Menus[menu].Commands[command].hotkey + " " + itemText;
			itemText = "|" + itemText;
		}
		if(itemText.length > widest)
			widest = itemText.length;
		if(typeof Commands.Menus[menu].Commands[command].menu != "undefined") {
			var item = tree.addItem(
				itemText,
				loadMenu,
				Commands.Menus[menu].Commands[command].menu
			);		
		} else {
			var item = tree.addItem(
				itemText,
				launchItem,
				Commands.Menus[menu].Commands[command]
			);
		}
		treeItems.push(item);
		tree.index = 0;
		shown++;
	}

	treeBorderFrame.clear();
	if(Commands.Menus[menu].Lightbar.Position == "Fixed") {
		treeBorderFrame.x = Commands.Menus[menu].Lightbar.x;
		treeBorderFrame.y = Commands.Menus[menu].Lightbar.y;
		treeBorderFrame.width = Commands.Menus[menu].Lightbar.Width;
		treeBorderFrame.height = Commands.Menus[menu].Lightbar.Height;
		treeFrame.x = treeBorderFrame.x + 1;
		treeFrame.y = treeBorderFrame.y + 3;
		treeFrame.width = treeBorderFrame.width - 2;
		treeFrame.height = treeBorderFrame.height - 4;
	} else {
		resize({ 'w' : widest, 'h' : shown });
		if(Commands.Menus[menu].Lightbar.Position == "Centered") {
			hCenter(treeBorderFrame, frame);
			vCenter(treeBorderFrame, frame);
			hCenter(treeFrame, treeBorderFrame);
		} else if(Commands.Menus[menu].Lightbar.Position == "Left") {
			treeBorderFrame.x = frame.x;
			treeFrame.x = treeBorderFrame.x + 1;
		} else if(Commands.Menus[menu].Lightbar.Position == "Right") {
			treeBorderFrame.x = frame.x + frame.width - treeBorderFrame.width;
			treeFrame.x = treeBorderFrame.x + 1;
		}
		vCenter(treeFrame, treeBorderFrame);
		treeBorderFrame.height = treeBorderFrame.height + 2;;
		treeFrame.y = treeFrame.y + 2;
	}
	treeBorderFrame.drawBorder(toAttr(Commands.Menus[menu].Lightbar.BorderColor));
	treeBorderFrame.gotoxy(1, 2);
	treeBorderFrame.center(Commands.Menus[menu].Title);
	frame.invalidate();

}

var resize = function(wh) {
	if(wh.w < frame.width - 4) {
		treeBorderFrame.width = wh.w + 4;
		treeFrame.width = treeBorderFrame.width - 2;
	}
	if(wh.h < frame.height - 2) {
		treeBorderFrame.height = wh.h + 2;
		treeFrame.height = treeBorderFrame.height - 2;
	}
}

var hCenter = function(subFrame, parentFrame) {
	var x = Math.floor((parentFrame.width - subFrame.width) / 2);
	subFrame.x = parentFrame.x + x;
}

var vCenter = function(subFrame, parentFrame) {
	var y = Math.floor((parentFrame.height - subFrame.height) / 2);
	subFrame.y = parentFrame.y + y;
}

var init = function() {
	frame = new Frame(1, 1, console.screen_columns, console.screen_rows, LIGHTGRAY);
	// Load background image into frame here
	treeBorderFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		frame.height,
		LIGHTGRAY,
		frame
	);
	treeFrame = new Frame(
		treeBorderFrame.x,
		treeBorderFrame.y,
		treeBorderFrame.width,
		treeBorderFrame.height,
		LIGHTGRAY,
		treeBorderFrame
	);
	frame.open();
	tree = new Tree(treeFrame);
	tree.open();
	for(var m in Commands.Menus) {
		if(!Commands.Menus[m].Default)
			continue;
		var menu = m;
		break;
	}
	loadMenu(menu);
}

var main = function() {
	while(bbs.online) {
		var userInput = console.inkey(K_NONE, 5);
		tree.getcmd(userInput);
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
		if(ascii(userInput) == 27)
			break;
	}
}

var cleanUp = function() {
	tree.close();
	frame.close();
}

try {
	init();
	main();
	cleanUp();
} catch(err) {
	log(LOG_ERR, err);
}
//bbs.hangup();
exit();
