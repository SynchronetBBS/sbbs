load("sbbsdefs.js");
load("tree.js");

// Configuration variables:
var ansiRoot = js.exec_dir + "library/"; // The location of your ANSI library, eg. "/dev/magicalansigenerator/"
tree_settings.lfg = WHITE; // The lightbar foreground colour, see sbbsdefs.js for valid values
tree_settings.lbg = BG_CYAN; // The lightbar background colour, see sbbsdefs.js for valid values
var disallowedExtensions = [".COM", ".EXE", ".GIF", ".JPG", ".MP3", ".PNG", ".RAR", ".ZIP"]; // Files with these extensions will not be viewable
var slow = .033;	// Seconds between printed characters, slow speed
var medium = .0033;	// Seconds between printed characters, medium speed
var fast = .00033;	// Seconds between printed characters, fast speed
var ansiDelay = medium; // Default output speed as one of slow, medium, or fast
// End of configuration variables

var lastPrint = system.timer;
var curDir = ansiRoot;
var parentDir = ansiRoot;
var choice;
var userInput;
var treeCmd;
var disp;

function getInput(slideshow) {
	var retval = 1;
	userInput = ascii(console.inkey(K_NOECHO).toUpperCase());
	if(userInput == 0) return retval;
	if(userInput == 32) {
		console.saveline();
		console.clearline();
		console.putmsg(ascii(27) + "[1;37;40mQ\1h\1cuit - \1w<\1cSpace\1w> \1cfor more - \1wS\1clow, \1wM\1cedium, \1wF\1cast");
		if(slideshow) console.putmsg("\1h\1c - \1wP\1crevious, \1wN\1cext");
		var userInput = ascii(console.getkey(K_NOECHO).toUpperCase());
		console.clearline();
		console.restoreline();
	}
	switch(userInput) {
		case 70:
			ansiDelay = fast;
			break;
		case 77:
			ansiDelay = medium;
			break;
		case 83:
			ansiDelay = slow;
			break;
		case 81:
			retval = 0;
			break;
		case 78:
			if(slideshow) retval = 2;
			break;
		case 80:
			if(slideshow) retval = 3;
			break;
	}
	return retval;
}

function printAnsi(ansi, slideshow) {
	var retval = 1;
	console.clear(LIGHTGRAY);
	forLoop:
	for(var a = 0; a < ansi.length; a++) {
		console.putmsg(ansi[a]);
		console.line_counter = 0;
		while(system.timer - lastPrint < ansiDelay) {
			retval = getInput(slideshow);
			if(retval != 1) break forLoop;
		}
		lastPrint = system.timer;
	}
	if(retval == 1) console.pause();
	return retval;
}

function fileChooser() {
	console.clear(LIGHTGRAY);
	var f = new File(js.exec_dir + "ansiview.ans");
	f.open("r");
	var g = f.read();
	f.close();
	console.putmsg(g);
	console.gotoxy(3, 4);
	console.putmsg(ascii(27) + "[1;37;40mpath: " + curDir.replace(ansiRoot, "/"));
	var dirList = directory(curDir + "*.*");
	var tree = new Tree(2, 6, 76, 15);
	tree.addItem("[..]", parentDir);
	for(var d = 0; d < dirList.length; d++) {
		disp = dirList[d].split("/");
		if(file_isdir(dirList[d])) {
			disp = disp[disp.length - 2];
			disp = "[" + disp + "]";
		} else {
			disp = disp[disp.length - 1];
		}
		tree.addItem(disp, dirList[d]);
	}
	tree.draw();
	while(!js.terminated) {
		userInput = console.inkey(K_NOECHO, 5).toUpperCase();
		if(userInput == "") continue;
		if(userInput == "Q" || userInput == "S" || userInput == "H") break;
		userInput = tree.handle_command(userInput);
		console.line_counter = 0;
		if(userInput != false) break;
	}
	return userInput;
}

function loadAnsiFile(ansi) {
	var f = new File(ansi);
	if(f.exists) f.open("r");
	if(!f.is_open) return;
	var ansiFileContents = f.read();
	f.close();
	return ansiFileContents;
}

function checkExt(str) {
	for(var d = 0; d < disallowedExtensions.length; d++) if(file_getext(str).toUpperCase() == disallowedExtensions[d].toUpperCase()) return true;
	return false;
}

function slideshow() {
	var dirList = directory(curDir + "*.*");
	var retval;
	ssForLoop:
	for(var d = 0; d< dirList.length; d++) {
		if(!file_isdir(dirList[d]) && (choice.match(/\./) === null || !checkExt(dirList[d]))) retval = printAnsi(loadAnsiFile(dirList[d]), true);
		switch(retval) {
			case 0:
				break ssForLoop;
			case 3:
				d = d - 2;
				if(d < -1) d = -1;
				break;
			default:
				break;
		}
	}
}

function helpScreen() {
	console.clear(LIGHTGRAY);
	var f = new File(js.exec_dir + "help.ans");
	f.open("r");
	var g = f.read();
	f.close();
	console.putmsg(g);
	while(console.getkey(K_NOECHO).toUpperCase() != "Q") { }
}

while(!js.terminated) {
	var choice = fileChooser();
	console.line_counter = 0;
	if(file_isdir(choice)) {
		if(choice.length > curDir.length) {
			parentDir = curDir;
		} else if(parentDir != ansiRoot && parentDir.length > ansiRoot.length) {
			parentDir = curDir.split("/");
			parentDir = parentDir.slice(0, parentDir.length - 3).join("/") + "/";
		} else {
			parentDir = ansiRoot;
		}
		curDir = choice;
	} else if(choice == "Q") {
		exit();
	} else if(choice == "S") {
		slideshow();
	} else if(choice == "H") {
		helpScreen();
	} else if(choice.match(/\./) === null || !checkExt(choice)) {
		var ansi = loadAnsiFile(choice);
		printAnsi(ansi, false);
	}
}
