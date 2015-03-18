/*	This is a terrible modification of the terrible Chicken Delivery
	Level Editor.  This script provides the LevelEditor object, but
	doesn't actually do anything on its own.  See leveledit.js. */

load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("funclib.js");

var LevelEditor = function(parentFrame) {

	var frame,
		fieldFrame,
		cursorFrame,
		headFrame,
		footFrame,
		treeFrame,
		treeSubFrame;

	var stuff = {
		name : "",
		time : 120,
		blocks : [],
		hazards : [],
		shooters : [],
		entrance : false,
		exit : false
	};

	var state = {
		'block' : false,
		'entrance' : false,
		'exit' : false,
		'hazard' : false,
		'shooter' : false
	};

	var entrance,
		exit,
		blocks = [],
		hazards = []
		shooters = [];

	var loadSprites = function() {
		var files = directory(js.exec_dir + "sprites/*.ini");
		for(var f = 0; f < files.length; f++) {
			var file = new File(files[f]);
			file.open("r");
			var ini = file.iniGetObject();
			file.close();
			var shortName = file_getname(files[f]).replace(/\.ini$/, "");
			if(ini.type == "entrance")
				entrance = shortName;
			else if(ini.type == "exit")
				exit = shortName;
			else if(ini.type == "block")
				blocks.push(shortName);
			else if(ini.type == "hazard")
				hazards.push(shortName);
			else if(ini.type == "shooter")
				shooters.push(shortName);
		}
	}

	var noYes = function(question) {
		var nyFrame = new Frame(
			Math.floor((fieldFrame.width - question.length - 9) / 2),
			Math.floor((fieldFrame.height - 1) / 2),
			question.length + 9,
			1,
			BG_BLUE|WHITE,
			fieldFrame
		);
		nyFrame.open();
		nyFrame.putmsg(question + "? Y/N: ");
		frame.cycle();
		var ret = console.getkeys("YN");
		nyFrame.delete();
		return (ret == "Y");
	}

	var moveUp = function(subFrame, frame) {
		if(subFrame.y > frame.y)
			subFrame.move(0, -1);
	}

	var moveDown = function(subFrame, frame) {
		if(subFrame.y + subFrame.height - 1 < frame.y + frame.height - 1)
			subFrame.move(0, 1);
	}

	var moveLeft = function(subFrame, frame) {
		if(subFrame.x > frame.x)
			subFrame.move(-1, 0);
	}

	var moveRight = function(subFrame, frame) {
		if(subFrame.x + subFrame.width - 1 < frame.x + frame.width - 1)
			subFrame.move(1, 0);
	}

	var checkDelete = function(frame) {
		if(cursorFrame.x < frame.x)
			return false;
		if(cursorFrame.x >= frame.x + frame.width)
			return false;
		if(cursorFrame.y < frame.y)
			return false;
		if(cursorFrame.y >= frame.y + frame.height)
			return false;
		return true;
	}

	var blockChooser = function() {
		var tree = new Tree(treeSubFrame);
		for(var b in blocks)
			tree.addItem(blocks[b], blocks[b]);
		treeFrame.top();
		tree.open();
		var choice = "";
		while(blocks.indexOf(choice) < 0) {
			choice = console.inkey(K_NONE, 5);
			if(ascii(choice) == 27)
				break;
			choice = tree.getcmd(choice);
			if(frame.cycle())
				console.gotoxy(console.screen_columns, console.screen_rows);
		}
		tree.close();
		treeFrame.bottom();
		if(typeof choice != "string")
			return false;
		var f = loadItem(choice);
		f.type = choice;
		return f;
	}

	var hazardChooser = function() {
		var tree = new Tree(treeSubFrame);
		for(var h in hazards)
			tree.addItem(hazards[h], hazards[h]);
		treeFrame.top();
		tree.open();
		var choice = "";
		while(hazards.indexOf(choice) < 0) {
			choice = console.inkey(K_NONE, 5);
			if(ascii(choice) == 27)
				break;
			choice = tree.getcmd(choice);
			if(frame.cycle())
				console.gotoxy(console.screen_columns, console.screen_rows);
		}
		tree.close();
		treeFrame.bottom();
		if(typeof choice != "string")
			return false;
		var f = loadItem(choice);
		f.type = choice;
		return f;
	}

	var shooterChooser = function() {
		var tree = new Tree(treeSubFrame);
		for(var s in shooters)
			tree.addItem(shooters[s], shooters[s]);
		treeFrame.top();
		tree.open();
		var choice = "";
		while(shooters.indexOf(choice) < 0) {
			choice = console.inkey(K_NONE, 5);
			if(ascii(choice) == 27)
				break;
			choice = tree.getcmd(choice);
			if(frame.cycle())
				console.gotoxy(console.screen_columns, console.screen_rows);
		}
		tree.close();
		treeFrame.bottom();
		if(typeof choice != "string")
			return false;
		var f = loadItem(choice);
		f.type = choice;
		return f;
	}

	var initFrames = function() {

		console.clear(LIGHTGRAY);

		frame = new Frame(1, 1,	80, 24, LIGHTGRAY);

		headFrame = new Frame(
			frame.x,
			frame.y,
			frame.width,
			1,
			BG_BLUE|WHITE,
			frame
		);

		fieldFrame = new Frame(
			frame.x,
			frame.y,
			frame.width,
			frame.height - 2,
			LIGHTGRAY,
			frame
		);

		footFrame = new Frame(
			frame.x,
			frame.y + frame.height - 1,
			frame.width,
			1,
			BG_BLUE|WHITE,
			frame
		);

		cursorFrame = new Frame(
			fieldFrame.x,
			fieldFrame.y,
			1,
			1,
			WHITE,
			fieldFrame
		);

		treeFrame = new Frame(
			Math.floor(fieldFrame.x + (fieldFrame.width / 4)),
			fieldFrame.y + 2,
			Math.floor(fieldFrame.width / 2),
			fieldFrame.height - 4,
			BG_BLUE|WHITE,
			frame
		);

		treeSubFrame = new Frame(
			treeFrame.x + 1,
			treeFrame.y + 1,
			treeFrame.width - 2,
			treeFrame.height - 2,
			WHITE,
			treeFrame
		);

		headFrame.putmsg("Lemons Level Editor");
		footFrame.putmsg("B)lock, H)azard, S)hooter, E)ntrance, e(X)it, [DEL], [ENTER], [ESC]");
		treeFrame.center("Choose an item");
		cursorFrame.putmsg(ascii(219));

		frame.open();
		treeFrame.bottom();

	}

	var loadItem = function(item) {
		var f = new File(js.exec_dir + "sprites/" + item + ".ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
		ini.width = parseInt(ini.width);
		ini.height = parseInt(ini.height);
		if(cursorFrame.x + ini.width > fieldFrame.x + fieldFrame.width)
			return false;
		if(cursorFrame.y + ini.height > fieldFrame.y + fieldFrame.height)
			return false;
		var f = new Frame(cursorFrame.x, cursorFrame.y, ini.width, ini.height, 0, fieldFrame);
		f.open();
		f.load(js.exec_dir + "sprites/" + item + ".bin", ini.width, ini.height);
		f.type = item;
		return f;
	}

	this.open = function() {
		initFrames();
		loadSprites();
	}

	this.loadLevel = function(level) {
		stuff.name = level.name;
		stuff.time = (typeof level.time == "undefined") ? 60 : level.time;
		stuff.blocks = [];
		stuff.hazards = [];
		stuff.shooters = [];
		stuff.entrance = { 'x' : fieldFrame.x, 'y' : fieldFrame.y };
		stuff.exit = { 'x' : fieldFrame.x, 'y' : fieldFrame.y };
		for(var property in state)
			state[property] = false;
		fieldFrame.clear();
		cursorFrame.moveTo(level.entrance.x, level.entrance.y);
		stuff.entrance = loadItem(entrance);
		cursorFrame.moveTo(level.exit.x, level.exit.y);
		stuff.exit = loadItem(exit);
		for(var b = 0; b < level.blocks.length; b++) {
			cursorFrame.moveTo(level.blocks[b].x, level.blocks[b].y);
			stuff.blocks.push(loadItem(level.blocks[b].type));
		}
		for(var h = 0; h < level.hazards.length; h++) {
			cursorFrame.moveTo(level.hazards[h].x, level.hazards[h].y);
			stuff.hazards.push(loadItem(level.hazards[h].type));
		}
		for(var s = 0; s < level.shooters.length; s++) {
			cursorFrame.moveTo(level.shooters[s].x, level.shooters[s].y);
			stuff.shooters.push(loadItem(level.shooters[s].type));
		}
	}

	this.getcmd = function(userInput) {

		switch(userInput) {
			case KEY_UP:
				moveUp(cursorFrame, fieldFrame);
				if(state.block)
					moveUp(state.block, fieldFrame);
				else if(state.hazard)
					moveUp(state.hazard, fieldFrame);
				else if(state.entrance)
					moveUp(state.entrance, fieldFrame);
				else if(state.exit)
					moveUp(state.exit, fieldFrame);
				else if(state.shooter)
					moveUp(state.shooter, fieldFrame);
				break;
			case KEY_DOWN:
				moveDown(cursorFrame, fieldFrame);
				if(state.block)
					moveDown(state.block, fieldFrame);
				else if(state.hazard)
					moveDown(state.hazard, fieldFrame);
				else if(state.entrance)
					moveDown(state.entrance, fieldFrame);
				else if(state.exit)
					moveDown(state.exit, fieldFrame);
				else if(state.shooter)
					moveDown(state.shooter, fieldFrame);
				break;
			case KEY_LEFT:
				moveLeft(cursorFrame, fieldFrame);
				if(state.block)
					moveLeft(state.block, fieldFrame);
				else if(state.hazard)
					moveLeft(state.hazard, fieldFrame);
				else if(state.entrance)
					moveLeft(state.entrance, fieldFrame);
				else if(state.exit)
					moveLeft(state.exit, fieldFrame);
				else if(state.shooter)
					moveLeft(state.shooter, fieldFrame);
				break;
			case KEY_RIGHT:
				moveRight(cursorFrame, fieldFrame);
				if(state.block)
					moveRight(state.block, fieldFrame);
				else if(state.hazard)
					moveRight(state.hazard, fieldFrame);
				else if(state.entrance)
					moveRight(state.entrance, fieldFrame);
				else if(state.exit)
					moveRight(state.exit, fieldFrame);
				else if(state.shooter)
					moveRight(state.shooter, fieldFrame);
				break;
			case KEY_DEL:
				if(stuff.entrance && checkDelete(stuff.entrance)) {
					stuff.entrance.delete();
					stuff.entrance = false;
				}
				if(stuff.exit && checkDelete(stuff.exit)) {
					stuff.exit.delete();
					stuff.exit = false;
				}
				for(var b = 0; b < stuff.blocks.length; b++) {
					if(!checkDelete(stuff.blocks[b]))
						continue;
					var block = stuff.blocks.splice(b, 1)[0];
					block.delete();
				}
				for(var h = 0; h < stuff.hazards.length; h++) {
					if(!checkDelete(stuff.hazards[h]))
						continue;
					var hazard = stuff.hazards.splice(h, 1)[0];
					hazard.delete();
				}
				for(var s = 0; s < stuff.shooters.length; s++) {
					if(!checkDelete(stuff.shooters[s]))
						continue;
					var shooter = stuff.shooters.splice(s, 1)[0];
					shooter.delete();
				}
				break;
			case "B":
				if(state.shooter || state.block || state.entrance || state.exit || state.hazard)
					break;
				state.block = blockChooser();
				if(!state.block)
					break;
				state.block.moveTo(cursorFrame.x, cursorFrame.y);
				break;
			case "H":
				if(state.shooter || state.hazard || state.entrance || state.exit || state.block)
					break;
				state.hazard = hazardChooser();
				if(!state.hazard)
					break;
				state.hazard.moveTo(cursorFrame.x, cursorFrame.y);
				break;
			case "E":
				if(state.shooter || state.entrance || stuff.entrance || state.exit || state.block || state.hazard)
					break;
				state.entrance = loadItem(entrance);
				break;
			case "X":
				if(state.shooter || state.entrance || stuff.exit || state.exit || state.block || state.hazard)
					break;
				state.exit = loadItem(exit);
				break;
			case "I":
				// "I" don't know what this is for
				break;
			case "S":
				if(state.shooter || state.entrance || state.exit || state.hazard || state.block)
					break;
				state.shooter = shooterChooser();
				if(!state.shooter)
					break;
				state.shooter.moveTo(cursorFrame.x, cursorFrame.y);
				break;
			case "\r":
				if(state.block) {
					stuff.blocks.push(state.block);
					state.block = false;
				} else if(state.hazard) {
					stuff.hazards.push(state.hazard);
					state.hazard = false;
				} else if(state.entrance) {
					stuff.entrance = state.entrance;
					state.entrance = false;
				} else if(state.exit) {
					stuff.exit = state.exit;
					state.exit = false;
				} else if(state.shooter) {
					stuff.shooters.push(state.shooter);
					state.shooter = false;
				}
				break;
			default:
				break;

		}
	}

	this.cycle = function() {
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
		cursorFrame.top();
	}

	this.close = function() {
		var ret = {
			'name' : stuff.name,
			'time' : stuff.time,
			'author' : user.alias,
			'system' : system.name,
			'date' : time(),
			'blocks' : [],
			'hazards' : [],
			'shooters' : [],
			'lemons' : 1,
			'quotas' : {
				'basher' : 5,
				'blocker' : 5,
				'bomber' : 5,
				'builder' : 5,
				'climber' : 5,
				'digger' : 5
			},
			'entrance' : (!stuff.entrance) ? {'x' : fieldFrame.x , 'y' : fieldFrame.y } : { 'x' : stuff.entrance.x, 'y' : stuff.entrance.y },
			'exit' : (!stuff.exit) ? {'x' : fieldFrame.x , 'y' : fieldFrame.y } : { 'x' : stuff.exit.x, 'y' : stuff.exit.y }
		};
		for(var b in stuff.blocks) {
			ret.blocks.push(
				{	'x' : stuff.blocks[b].x,
					'y' : stuff.blocks[b].y,
					'type' : stuff.blocks[b].type
				}
			);
		}
		for(var h in stuff.hazards) {
			ret.hazards.push(
				{	'x' : stuff.hazards[h].x,
					'y' : stuff.hazards[h].y,
					'type' : stuff.hazards[h].type
				}
			);
		}
		for(var s in stuff.shooters) {
			ret.shooters.push(
				{	'x' : stuff.shooters[s].x,
					'y' : stuff.shooters[s].y,
					'type' : stuff.shooters[s].type
				}
			);
		}
		if(typeof frame.parent != "undefined")
			frame.parent.invalidate();
		frame.close();
		console.clear(LIGHTGRAY);
		console.putmsg("How many lemons: (1 - 99) ");
		ret.lemons = console.getnum(99);
		console.putmsg("Time limit, in seconds: (1 - 600) ");
		ret.time = console.getnum(600);
		console.putmsg("Quotas (1 - 99 of each skill type) \r\n");
		for(var q in ret.quotas) {
			console.putmsg("\t" + q + ": ");
			ret.quotas[q] = console.getnum(99);
		}
		console.putmsg("Name this level: ");
		ret.name = console.getstr(ret.name, 60, K_LINE|K_EDIT);
		if(console.strlen(ret.name.replace(/\s/g, "")) < 1)
			ret.name = "Untitled";
		return ret;
	}

}
