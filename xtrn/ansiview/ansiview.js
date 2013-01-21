// ansiview.js by echicken -at- bbs.electronicchicken.com

load("sbbsdefs.js");
load("tree.js");
load("frame.js");

// Configuration variables:
var ansiRoot = js.exec_dir + "library/"; // The location of your ANSI library, eg. "/dev/magicalansigenerator/"
var disallowedFiles = ["PASSWD", "USER.DAT", "*.COM", "*.EXE", "*.GIF", "*.JPG", "*.MP3", "*.PNG", "*.RAR"]; // Files matching these patterns will not be shown in directory listings
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

function getInput(slideshow, ansiName) {
	var retval = 1;
	userInput = ascii(console.inkey(K_NOECHO).toUpperCase());
	if(userInput == 0) return retval;
	if(userInput == 32) {
		console.saveline();
		console.clearline();
		if(slideshow) {
			console.putmsg(ascii(27) + "[1;37;40m[\1h\1c" + ansiName.substr(0, 18) + "\1w] Q\1cuit\1w, <\1cSpace\1w> \1cfor more\1w, S\1clow\1w, M\1cedium\1w, F\1cast\1w, P\1crevious\1w, N\1cext");
		} else {
			console.putmsg(ascii(27) + "[1;37;40m[\1h\1c" + ansiName.substr(0, 34) + "\1w] Q\1cuit\1w, <\1cSpace\1w> \1cfor more\1w, S\1clow\1w, M\1cedium\1w, F\1cast");
		}
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

function printAnsi(ansi, slideshow, ansiName) {
	var retval = 1;
	console.clear(LIGHTGRAY);
	forLoop:
	for(var a = 0; a < ansi.length; a++) {
		console.putmsg(ansi[a]);
		console.line_counter = 0;
		while(system.timer - lastPrint < ansiDelay) {
			retval = getInput(slideshow, ansiName);
			if(retval != 1)
				break forLoop;
		}
		lastPrint = system.timer;
	}
	if(retval == 1) {
		console.putmsg(ascii(27) + "[1;37;40m[\1c" + ansiName + " \1w - \1c Press any key to continue\1w]");
		console.getkey(K_NOECHO|K_NOCRLF);
	}
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
	var dirList = directory(curDir + "*");
	var frame = new Frame(2,6,76,15)
	var tree = new Tree(frame);
	tree.colors.lfg = WHITE; // The lightbar foreground colour, see sbbsdefs.js for valid values
	tree.colors.lbg = BG_CYAN; // The lightbar background colour, see sbbsdefs.js for valid values
	if(curDir != ansiRoot) 
		tree.addItem("[..]", parentDir);
	for(var d = 0; d < dirList.length; d++) {
		if(checkFile(dirList[d])) 
			continue;
		disp = dirList[d].split("/");
		if(file_isdir(dirList[d])) {
			disp = disp[disp.length - 2];
			disp = "[" + disp + "]";
		} else {
			disp = disp[disp.length - 1];
		}
		tree.addItem(disp, dirList[d]);
	}
	frame.open();
	tree.open();
	while(!js.terminated) {
		frame.cycle();
		userInput = console.inkey(K_NOECHO, 5).toUpperCase();
		if(userInput == "") 
			continue;
		if(userInput == "Q" || userInput == "S" || userInput == "H") 
			break;
		userInput = tree.getcmd(userInput);
		console.line_counter = 0;
		if(userInput !== true && userInput !== false) 
			break;
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

function checkFile(str) {
	if(file_isdir(str)) {
		var matchMe = str.split("/");
		matchMe = matchMe[matchMe.length - 2];
	} else {
		matchMe = file_getname(str);
	}
	for(var f = 0; f < disallowedFiles.length; f++) 
		if(wildmatch(false, matchMe, disallowedFiles[f])) 
			return true;
	return false;
}

function slideshow() {
	var dirList = directory(curDir + "*.*");
	var retval;
	ssForLoop:
	for(var d = 0; d< dirList.length; d++) {
		if(!file_isdir(dirList[d]) && !checkFile(dirList[d]) && !wildmatch(false, file_getname(dirList[d]), "*.ZIP")) retval = printAnsi(loadAnsiFile(dirList[d]), true, file_getname(dirList[d]));
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
	} else if(file_getext(choice).toUpperCase() == ".ZIP") {
		destDir = curDir + file_getname(choice).toUpperCase().replace(".ZIP", "") + "/";
		parentDir = curDir;
		curDir = destDir;
		if(file_isdir(destDir)) 
			continue;
		bbs.exec(system.exec_dir + "unzip -s -o -qq -d" + destDir + " " + choice);
	} else {
		var ansi = loadAnsiFile(choice);
		printAnsi(ansi, false, file_getname(choice));
	}
}
