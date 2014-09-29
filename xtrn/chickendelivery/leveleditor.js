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
		time : 60,
		platforms : [],
		enemies : [],
		powerUps : [],
		player : false,
		door : false
	};

	var state = {
		'platform' : false,
		'player' : false,
		'enemy' : false,
		'door' : false,
		'powerUp' : false
	};

	var player;
	var door;
	var enemies = [];
	var powerUps = [];

	var loadSprites = function() {
		var files = directory(js.exec_dir + "sprites/*.ini");
		for(var f = 0; f < files.length; f++) {
			var file = new File(files[f]);
			file.open("r");
			var ini = file.iniGetObject();
			file.close();
			var shortName = file_getname(files[f]).replace(/\.ini$/, "");
			if(ini.type == "player")
				player = shortName;
			else if(ini.type == "door")
				door = shortName;
			else if(ini.type == "enemy")
				enemies.push(shortName);
			else if(ini.type == "powerup")
				powerUps.push(shortName);
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

	var powerUpChooser = function() {
		var tree = new Tree(treeSubFrame);
		for(var p in powerUps)
			tree.addItem(powerUps[p], powerUps[p]);
		treeFrame.top();
		tree.open();
		var choice = "";
		while(powerUps.indexOf(choice) < 0) {
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

	var enemyChooser = function() {
		var tree = new Tree(treeSubFrame);
		for(var e in enemies)
			tree.addItem(enemies[e], enemies[e]);
		treeFrame.top();
		tree.open();
		var choice = "";
		while(enemies.indexOf(choice) < 0) {
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

//		frame = new Frame(1, 1,	80, 24, LIGHTGRAY, parentFrame);
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
			frame.y + 1,
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

		headFrame.putmsg("Chicken Delivery Level Editor");
		footFrame.putmsg("P)latform +/- =/_, E)nemy, I)tem, C)hicken, D)oor, [DEL], [ENTER], [ESC]");
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
		stuff.enemies = [];
		stuff.platforms = [];
		stuff.powerUps = [];
		stuff.player = { 'x' : fieldFrame.x, 'y' : fieldFrame.y };
		stuff.door = { 'x' : fieldFrame.x, 'y' : fieldFrame.y };
		for(var property in state)
			state[property] = false;
		fieldFrame.clear();
		cursorFrame.moveTo(level.player.x, level.player.y);
		stuff.player = loadItem(player);
		cursorFrame.moveTo(level.door.x, level.door.y);
		stuff.door = loadItem(door);
		for(var e = 0; e < level.enemies.length; e++) {
			cursorFrame.moveTo(level.enemies[e].x, level.enemies[e].y);
			stuff.enemies.push(loadItem(level.enemies[e].type));
		}
		for(var p = 0; p < level.powerUps.length; p++) {
			cursorFrame.moveTo(level.powerUps[p].x, level.powerUps[p].y);
			stuff.powerUps.push(loadItem(level.powerUps[p].type));
		}
		for(var p = 0; p < level.platforms.length; p++) {
			stuff.platforms.push(new Frame(
				level.platforms[p].x,
				level.platforms[p].y,
				level.platforms[p].width,
				level.platforms[p].height,
				level.platforms[p].attr,
				fieldFrame
			));
			stuff.platforms[stuff.platforms.length - 1].upDown = level.platforms[p].upDown;
			stuff.platforms[stuff.platforms.length - 1].leftRight = level.platforms[p].leftRight;
			stuff.platforms[stuff.platforms.length - 1].speed = level.platforms[p].speed;
			for(var y = 0; y < stuff.platforms[stuff.platforms.length - 1].height; y++) {
				for(var x = 0; x < stuff.platforms[stuff.platforms.length - 1].width; x++)
					stuff.platforms[stuff.platforms.length - 1].setData(x, y, level.platforms[p].ch, level.platforms[p].attr);
			}
			stuff.platforms[stuff.platforms.length - 1].open();
		}
	}

	this.getcmd = function(userInput) {

		switch(userInput) {
			case KEY_UP:
				moveUp(cursorFrame, fieldFrame);
				if(state.platform)
					moveUp(state.platform, fieldFrame);
				else if(state.enemy)
					moveUp(state.enemy, fieldFrame);
				else if(state.player)
					moveUp(state.player, fieldFrame);
				else if(state.door)
					moveUp(state.door, fieldFrame);
				else if(state.powerUp)
					moveUp(state.powerUp, fieldFrame);
				break;
			case KEY_DOWN:
				moveDown(cursorFrame, fieldFrame);
				if(state.platform)
					moveDown(state.platform, fieldFrame);
				else if(state.enemy)
					moveDown(state.enemy, fieldFrame);
				else if(state.player)
					moveDown(state.player, fieldFrame);
				else if(state.door)
					moveDown(state.door, fieldFrame);
				else if(state.powerUp)
					moveDown(state.powerUp, fieldFrame);
				break;
			case KEY_LEFT:
				moveLeft(cursorFrame, fieldFrame);
				if(state.platform)
					moveLeft(state.platform, fieldFrame);
				else if(state.enemy)
					moveLeft(state.enemy, fieldFrame);
				else if(state.player)
					moveLeft(state.player, fieldFrame);
				else if(state.door)
					moveLeft(state.door, fieldFrame);
				else if(state.powerUp)
					moveLeft(state.powerUp, fieldFrame);
				break;
			case KEY_RIGHT:
				moveRight(cursorFrame, fieldFrame);
				if(state.platform)
					moveRight(state.platform, fieldFrame);
				else if(state.enemy)
					moveRight(state.enemy, fieldFrame);
				else if(state.player)
					moveRight(state.player, fieldFrame);
				else if(state.door)
					moveRight(state.door, fieldFrame);
				else if(state.powerUp)
					moveRight(state.powerUp, fieldFrame);
				break;
			case KEY_DEL:
				if(stuff.player && checkDelete(stuff.player)) {
					stuff.player.delete();
					stuff.player = false;
				}
				if(stuff.door && checkDelete(stuff.door)) {
					stuff.door.delete();
					stuff.door = false;
				}
				for(var e = 0; e < stuff.enemies.length; e++) {
					if(!checkDelete(stuff.enemies[e]))
						continue;
					var enemy = stuff.enemies.splice(e, 1)[0];
					enemy.delete();
				}
				for(var p = 0; p < stuff.platforms.length; p++) {
					if(!checkDelete(stuff.platforms[p]))
						continue;
					var platform = stuff.platforms.splice(p, 1)[0];
					platform.delete();
				}
				for(var p = 0; p < stuff.powerUps.length; p++) {
					if(!checkDelete(stuff.powerUps[p]))
						continue;
					var powerUp = stuff.powerUps.splice(p, 1)[0];
					powerUp.delete();
				}
				break;
			case "P":
				if(state.platform || state.player || state.enemy || state.door || state.powerUp)
					break;
				var attr = colorPicker(
					Math.floor((fieldFrame.width - 36) / 2),
					Math.floor((fieldFrame.height - 6) / 2),
					fieldFrame,
					(stuff.platforms.length < 1) ? WHITE : stuff.platforms[stuff.platforms.length - 1].attr
				);
				state.platform = new Frame(
					cursorFrame.x,
					cursorFrame.y,
					1,
					1,
					attr,
					fieldFrame
				);
				state.platform.upDown = noYes("Moving up/down");
				state.platform.leftRight = noYes("Moving left/right");
				state.platform.speed = .15;
				state.platform.open();
				state.platform.putmsg(ascii(219));
				break;
			case "E":
				if(state.platform || state.player || state.enemy || state.door || state.powerUp)
					break;
				state.enemy = enemyChooser();
				if(!state.enemy)
					break;
				state.enemy.moveTo(cursorFrame.x, cursorFrame.y);
				break;
			case "I":
				if(state.platform || state.player || state.enemy || state.door || state.powerUp)
					break;
				state.powerUp = powerUpChooser();
				if(!state.powerUp)
					break;
				state.powerUp.moveTo(cursorFrame.x, cursorFrame.y);
			case "C":
				if(state.platform || state.player || stuff.player || state.enemy || state.door || state.powerUp)
					break;
				state.player = loadItem(player);
				break;
			case "D":
				if(state.door || state.platform || stuff.door || state.player || state.enemy || state.powerUp)
					break;
				state.door = loadItem(door);
			case "I":
				break;
			case "+":
				if(!state.platform || state.platform.x + state.platform.width > fieldFrame.x + fieldFrame.width - 1)
					break;
				state.platform.width++;
				for(var y = 0; y < state.platform.height; y++) {
					for(var x = 0; x < state.platform.width; x++)
						state.platform.setData(x, y, ascii(219), state.platform.attr);
				}
				break;
			case "-":
				if(!state.platform || state.platform.width < 2)
					break;
				state.platform.width--;
				break;
			case "=":
				if(!state.platform || state.platform.y + state.platform.height > fieldFrame.y + fieldFrame.height - 1)
					break;
				state.platform.height++;
				for(var y = 0; y < state.platform.height; y++) {
					for(var x = 0; x < state.platform.width; x++)
						state.platform.setData(x, y, ascii(219), state.platform.attr);
				}
				break;
			case "_":
				if(!state.platform || state.platform.height < 2)
					break;
				state.platform.height--;
				break;
			case "\r":
				if(state.platform) {
					stuff.platforms.push(state.platform);
					state.platform = false;
				} else if(state.enemy) {
					stuff.enemies.push(state.enemy);
					state.enemy = false;
				} else if(state.player) {
					stuff.player = state.player;
					state.player = false;
				} else if(state.door) {
					stuff.door = state.door;
					state.door = false;
				} else if(state.powerUp) {
					stuff.powerUps.push(state.powerUp);
					state.powerUp = false;
				}
				break;
			default:
				break;

		}
	}

	this.cycle = function() {
//		if(typeof frame.parent == "undefined" && frame.cycle())
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
			'enemies' : [],
			'platforms' : [],
			'powerUps' : [],
			'player' : (!stuff.player) ? { 'x' : fieldFrame.x, 'y' : fieldFrame.y } : { 'x' : stuff.player.x, 'y' : stuff.player.y },
			'door' : (!stuff.door) ? {'x' : fieldFrame.x , 'y' : fieldFrame.y } : { 'x' : stuff.door.x, 'y' : stuff.door.y }
		};
		for(var e in stuff.enemies) {
			ret.enemies.push(
				{	'x' : stuff.enemies[e].x,
					'y' : stuff.enemies[e].y,
					'type' : stuff.enemies[e].type
				}
			);
		}
		for(var p in stuff.powerUps) {
			ret.powerUps.push(
				{	'x' : stuff.powerUps[p].x,
					'y' : stuff.powerUps[p].y,
					'type' : stuff.powerUps[p].type
				}
			);
		}
		for(var p in stuff.platforms) {
			ret.platforms.push(
				{	'x' : stuff.platforms[p].x,
					'y' : stuff.platforms[p].y,
					'width' : stuff.platforms[p].width,
					'height' : stuff.platforms[p].height,
					'ch' : ascii(219), // Make configurable?
					'attr' : stuff.platforms[p].attr,
					'upDown' : stuff.platforms[p].upDown,
					'leftRight' : stuff.platforms[p].leftRight,
					'speed' : stuff.platforms[p].speed // Make configurable?
				}
			);
		}
		if(typeof frame.parent != "undefined")
			frame.parent.invalidate();
		frame.close();
		console.clear(LIGHTGRAY);
		console.putmsg("Name this level: ");
		ret.name = console.getstr(ret.name, 60, K_LINE|K_EDIT);
		if(console.strlen(ret.name.replace(/\s/g, "")) < 1)
			ret.name = "Untitled";
		return ret;
	}

}
