load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load(js.exec_dir + "leveleditor.js");

var frame, headFrame, footFrame, treeFrame, tree, treeItems = [];

var loadLevels = function() {
	if(!file_exists(js.exec_dir + "levels.json"))
		return [];
	var f = new File(js.exec_dir + "levels.json");
	f.open("r");
	var levels = JSON.parse(f.read());
	f.close();
	return levels;
}

var saveLevels = function(levels) {
	file_backup(js.exec_dir + "levels.json");
	var f = new File(js.exec_dir + "levels.json");
	f.open("w");
	f.write(JSON.stringify(levels));
	f.close();
}

var initFrames = function() {
	console.clear();
	frame = new Frame(1, 1, 80, 24, WHITE);
	headFrame = new Frame(1, 1, 80, 1, BG_BLUE|WHITE, frame);
	treeFrame = new Frame(1, 2, 80, 22, BG_BLACK|WHITE, frame);
	footFrame = new Frame(1, 24, 80, 1, BG_BLUE|WHITE, frame);
	headFrame.putmsg("Chicken Delivery Level Editor");
	footFrame.putmsg("[Enter], A)dd, D)elete, [ESC] quit");
	tree = new Tree(treeFrame);
	frame.open();
	tree.open();
}

var clearTree = function() {
	for(var item in treeItems)
		tree.deleteItem(tree.trace(treeItems[item].hash));
	treeItems = [];
}

var initMenu = function() {
	clearTree();
	var levels = loadLevels();
	for(var l = 0; l < levels.length; l++)
		treeItems.push(tree.addItem(levels[l].name, editLevel, l));
	tree.index = 0;
}

var editLevel = function(level) {
	var levels = loadLevels();
	var editor = new LevelEditor(frame);
	editor.open();
	if(typeof level != "undefined")
		editor.loadLevel(levels[level]);
	while(!js.terminated) {
		var userInput = console.inkey(K_UPPER, 5);
		if(ascii(userInput) == 27)
			break;
		editor.getcmd(userInput);
		editor.cycle();
		if(frame.cycle())
			console.gotoxy(80, 24);
	}
	var result = editor.close();
	if(typeof level == "undefined")
		levels.push(result);
	else
		levels[level] = result;
	saveLevels(levels);
	frame.invalidate();
}

var main = function() {
	initFrames();
	initMenu();
	while(!js.terminated) {
		var userInput = console.inkey(K_UPPER, 5);
		if(ascii(userInput) == 27)
			break;
		if(userInput == "A") {
			editLevel();
			initMenu();
		} else if(userInput == "D") {
			log(tree.index);
			var levels = loadLevels();
			levels.splice(tree.index, 1);
			saveLevels(levels);
			initMenu();
		} else {
			tree.getcmd(userInput);
		}
		if(frame.cycle())
			console.gotoxy(80, 24);
	}
}

var cleanUp = function() {
	tree.close();
	frame.close();
}

main();
cleanUp();
