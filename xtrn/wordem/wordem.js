// Wordem
// InterBBS Scrabble for Synchronet BBS 3.16+
// echicken -at- bbs.electronicchicken.com

load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("json-client.js");
load("funclib.js");
load("tiles.js");

var frame = new Frame(1, 1, 80, 24, 0);
var menuFrame = new Frame(1, 1, 80, 24, BG_BLUE|WHITE, frame);
var menuTreeFrame = new Frame(3, 2, 76, 22, BG_BLACK|WHITE, menuFrame);

var userInput = "";
var bgmask = (1<<4)|(1<<5)|(1<<6);
var letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_".split("");
var userID = md5_calc(user.alias + system.name, true);

var f = new File(js.exec_dir + "server.ini");
f.open("r");
var serverIni = f.iniGetObject();
f.close();
var client = new JSONClient(serverIni.host, serverIni.port);
var me = client.read("wordem", "users." + userID, 1);
if(me === undefined)
	client.write("wordem", "users." + userID, { games : [] }, 2);

var menuTree = false;
var play = false;
var game = false;
var games = [];
	
var Game = function(id) {

	this.client = new JSONClient(serverIni.host, serverIni.port);
	this.client.parent = this;

	this.frame = new Frame(1, 1, 80, 24, 0, frame);
	this.blankPrompt1 = new Frame(30, 10, 20, 3, BG_CYAN, this.frame);
	this.blankPrompt2 = new Frame(32, 11, 16, 1, BG_BLACK|WHITE, this.blankPrompt1);
	this.background = new Frame(1, 1, 80, 24, 0, this.frame);
	this.tileFrame = new Frame(2, 19, 45, 1, BG_BLACK|WHITE, this.background);
	this.tileCountFrame = new Frame(44, 18, 3, 1, BG_BLACK|WHITE, this.background);
	this.scoreBoard = new Frame(49, 20, 31, 3, BG_BLACK|WHITE, this.background);
	this.board = new Frame(2, 2, 45, 15, 0, this.background);
	this.cursor = new Frame(2, 2, 3, 1, BG_LIGHTGRAY|BLACK, this.board);
	this.board.load(js.exec_dir + "board.bin", 45, 15);
	this.background.load(js.exec_dir + "game.bin", 80, 24);

	this.currentWord = [];
		
	this.shared = {
		'random' : true,
		'turn' : 1,
		'dates' : {
			'start' : time(),
			'end' : false,
			'utime' : time()
		},
		'players' : {},
		'tiles' : tiles,
		'grid' : {}
	};
	
	this.countTiles = function() {
		var tt = 0;
		for(var t in this.shared.tiles) {
			if(this.shared.tiles[t].count < 1)
				continue;
			tt += this.shared.tiles[t].count;
		}
		return tt;
	}
	
	this.drawTile = function() {
		if(this.countTiles() < 1)
			return false;
		var t = false;
		while(!t) {
			var r = Math.floor(Math.random() * letters.length);
			if(this.shared.tiles[letters[r]].count < 1)
				continue;
			t = letters[r];
			this.shared.tiles[t].count = this.shared.tiles[t].count - 1;
			this.client.write("wordem", "games." + this.id + ".tiles", this.shared.tiles, 2);
		}
		return t;
	}
	
	this.tradeTiles = function() {
		this.blankPrompt1.top();
		this.blankPrompt2.clear();
		this.blankPrompt2.putmsg("Which tile?");
		var userInput = "";
		frame.cycle();
		while(!userInput.match(/[A-Z]/) && ascii(userInput) != 13) {
			userInput = console.getkey().toUpperCase();
		}
		if(userInput.match(/[A-Z]/) && this.shared.players[userID].tiles.indexOf(userInput) >= 0) {
			this.shared.players[userID].tiles.splice(this.shared.players[userID].tiles.indexOf(userInput), 1);
			this.shared.tiles[userInput].count++;
			this.shared.players[userID].tiles.push(this.drawTile());
			this.nextTurn();
			this.client.write("wordem", "games." + this.id + ".tiles", this.shared.tiles, 2);
			this.displayScores();
		}
		this.blankPrompt1.bottom();
	}
	
	this.putLetter = function(userInput) {
		if(this.shared.players[userID].tiles.indexOf(userInput) < 0)
			return false;
		var prop = this.cursor.x + "-" + (this.cursor.y - 1);
		if(!this.shared.grid.hasOwnProperty(prop))
			return false;
		if(this.shared.grid[prop].hasOwnProperty("letter"))
			return false;
		if(userInput == "_") {
			var wasBlank = true;
			this.blankPrompt1.top();
			this.blankPrompt2.clear();
			this.blankPrompt2.putmsg("Blank letter?");
			frame.cycle();
			userInput = "";
			while(!userInput.match(/[A-Z]/)) {
				userInput = console.getkey().toUpperCase();
			}
			this.blankPrompt1.bottom();
			this.shared.players[userID].tiles.splice(this.shared.players[userID].tiles.indexOf("_"), 1);
		} else {
			var wasBlank = false;
			this.shared.players[userID].tiles.splice(this.shared.players[userID].tiles.indexOf(userInput), 1);
		}
		this.board.gotoxy(this.cursor.x, this.cursor.y - 1);
		this.board.putmsg(userInput, this.shared.grid[prop].bg|WHITE);
		this.cursor.putmsg(" " + userInput + " ", BG_LIGHTGRAY|BLACK);
		this.shared.grid[prop].letter = userInput;
		this.currentWord.push( {
			'x' : this.cursor.x,
			'y' : this.cursor.y - 1,
			'letter' : (wasBlank)?"_":userInput
		});
	}

	this.retractWord = function() {
		for(var c = 0; c < this.currentWord.length; c++) {
			this.board.gotoxy(this.currentWord[c].x, this.currentWord[c].y);
			this.board.putmsg(" ", this.shared.grid[this.currentWord[c].x + "-" + this.currentWord[c].y].bg);
			delete this.shared.grid[this.currentWord[c].x + "-" + this.currentWord[c].y].letter;
			this.shared.players[userID].tiles.push(this.currentWord.splice(c, 1)[0].letter);
			c = c - 1;
		}
		this.currentWord = [];
	}

	this.getWord = function(currentWord, axis) {
		var theWord = [];
		var connected = false;
		if(axis === undefined && currentWord.length > 1) {
			if(currentWord[0].x == currentWord[1].x)
				axis = 'y';
			else if(currentWord[0].y == currentWord[1].y)
				axis = 'x';
			else
				return false;
			for(var c = 2; c < currentWord.length; c++) {
				if(axis == 'y' && currentWord[c].x != currentWord[0].x)
					return false;
				else if(axis == 'x' && currentWord[c].y != currentWord[0].y)
					return false;
			}
		}
		if(axis == 'x') {
			var xl;
			var xr;
			for(xl = currentWord[0].x; xl > 1; xl = xl - 3) {
				if(!this.shared.grid[xl + "-" + currentWord[0].y].hasOwnProperty("letter"))
					break;
			}
			xl = xl + 3;
			for(xr = xl; xr < 45; xr = xr + 3) {
				if(!this.shared.grid[xr + "-" + currentWord[0].y].hasOwnProperty("letter"))
					break;
				theWord.push(
					{	'x' : xr,
						'y' : currentWord[0].y,
						'letter' : this.shared.grid[xr + "-" + currentWord[0].y].letter
					}
				);
			}
		}
		if(axis == 'y') {
			var yu;
			var yd;
			for(yu = currentWord[0].y; yu > 0; yu = yu - 1) {
				if(!this.shared.grid[currentWord[0].x + "-" + yu].hasOwnProperty("letter"))
					break;
			}
			yu++;
			for(yd = yu; yd < 16; yd++) {
				if(!this.shared.grid[currentWord[0].x + "-" + yd].hasOwnProperty("letter"))
					break;
				theWord.push(
					{	'x' : currentWord[0].x,
						'y' : yd,
						'letter' : this.shared.grid[currentWord[0].x + "-" + yd].letter
					}
				);
			}
		}
		if(theWord.length < currentWord.length)
			return false; // Non-contiguous
		for(var c = 0; c < currentWord.length; c++) {
			for(var w = 0; w < theWord.length; w++) {
				if(currentWord[c].x == theWord[w].x && currentWord[c].x == theWord[w].y)
					continue;
				connected = true;
			}
		}
		if(!connected && this.shared.turn > 1)
			return false;
		return(
			{	'word' : theWord,
				'axis' : axis
			}
		);
	}
	
	this.getScore = function(word) {
		var totalScore = 0;
		var doubleWord = false;
		var quadrupleWord = false;
		var tripleWord = false;
		var nonupleWord = false;
		for(var w = 0; w < word.length; w++) {
			var letterScore = tiles[word[w].letter].value;
			var newLetter = false;
			for(var c in this.currentWord) {
				if(this.currentWord[c].x == word[w].x && this.currentWord[c].y == word[w].y)
					newLetter = true;
			}
			if(newLetter && this.shared.grid[word[w].x + "-" + word[w].y].bg == BG_CYAN) {
				letterScore = letterScore * 2;
			} else if(newLetter && this.shared.grid[word[w].x + "-" + word[w].y].bg == BG_BLUE) {
				letterScore = letterScore * 3;
			} else if(newLetter && this.shared.grid[word[w].x + "-" + word[w].y].bg == BG_RED) {
				if(tripleWord)
					nonupleWord = true;
				tripleWord = true;
			} else if(newLetter && this.shared.grid[word[w].x + "-" + word[w].y].bg == BG_GREEN) {
				if(doubleWord)
					quadrupleWord = true;
				doubleWord = true;
			}
			totalScore += letterScore;
		}
		if(quadrupleWord)
			totalScore = totalScore * 4;
		else if(doubleWord)
			totalScore = totalScore * 2;
		else if(nonupleWord)
			totalScore = totalScore * 9;
		else if(tripleWord)
			totalScore = totalScore * 3;
		return(totalScore);
	}
	
	this.addWord = function() {

		if(this.currentWord.length < 1)
			return false;
		if(this.shared.turn == 1 && (!this.shared.grid["23-8"].hasOwnProperty("letter") || this.currentWord.length < 2))
			return false;

		var words = [];
		if(this.currentWord.length > 1) {
			var word = this.getWord(this.currentWord);
			if(!word)
				return false;
			words.push(word.word);
		}
		for(var c = 0; c < this.currentWord.length; c++) {
			if(word === undefined) {
				var wx = this.getWord([this.currentWord[c]], 'x');
				if(wx && wx.word.length > 1)
					words.push(wx.word);					
				var wy = this.getWord([this.currentWord[c]], 'y');
				if(wy && wy.word.length > 1)
					words.push(wy.word);
			} else {
				if(word.axis == 'x')
					var w = this.getWord([this.currentWord[c]], 'y');
				else if(word.axis == 'y')
					var w = this.getWord([this.currentWord[c]], 'x');
				if(!w || w.word.length < 2)
					continue;
				words.push(w.word);
			}
		}
		
		var score = 0;
		for(var w in words) {
			var word = "";
			var wScore = this.getScore(words[w]);
			score += wScore;
			for(var ww in words[w])
				word += words[w][ww].letter;
			var f = new File(js.exec_dir + "dict/" + words[w][0].letter.toLowerCase() + ".txt");
			f.open("r");
			var d = f.readAll();
			f.close();
			if(d.indexOf(word.toLowerCase()) < 0)
				return false;
		}
		if(this.currentWord.length == 7)
			score += 50;
		this.currentWord = [];
		this.shared.players[userID].score += score;
		while(this.shared.players[userID].tiles.length < 7) {
			var t = this.drawTile();
			if(!t)
				break;
			this.shared.players[userID].tiles.push(t);
		}
		this.nextTurn();
		if(this.shared.players[userID].tiles.length == 0) {
			for(var p in this.shared.players) {
				if(p == userID)
					continue;
				for(var t in this.shared.players[p].tiles) {
					this.shared.players[p].score = this.shared.players[p].score - this.shared.players[p].tiles[t].value;
					this.shared.players[userID].score += this.shared.players[p].tiles[t].value;
				}
			}
			this.shared.dates.end = time();
			this.client.write("wordem", "games." + this.id + ".dates", this.shared.dates, 2);
			this.client.write("wordem", "games." + this.id + ".players", this.shared.players, 2);
		}
		this.client.write("wordem", "games." + this.id + ".grid", this.shared.grid, 2);
		this.displayScores();
		return true;
	}

	this.nextTurn = function() {
		this.shared.turn++;
		this.shared.players[userID].turn = false;
		for(var p in this.shared.players) {
			if(p != userID)
				this.shared.players[p].turn = true;
		}
		this.client.write("wordem", "games." + this.id + ".players", this.shared.players, 2);
	}
	
	this.getcmd = function(userInput) {
		switch(userInput) {
			case KEY_UP:
				this.cursor.clear();
				if(this.cursor.y > this.board.y)
					this.cursor.move(0, -1);
				break;
			case KEY_RIGHT:
				this.cursor.clear();
				if(this.cursor.x < this.board.x + this.board.width - 3)
					this.cursor.move(3, 0);
				break;
			case KEY_DOWN:
				this.cursor.clear();
				if(this.cursor.y < this.board.y + this.board.height - 1)
					this.cursor.move(0, 1);
				break;
			case KEY_LEFT:
				this.cursor.clear();
				if(this.cursor.x > this.board.x)
					this.cursor.move(-3, 0);
				break;
			default:
				if(ascii(userInput) == 27) {
					this.client.disconnect();
					this.frame.close();
					return false;
				}
				if(!this.shared.players[userID].turn) {
					break;
				} else if(ascii(userInput) == 13) {
					if(!this.addWord(this.currentWord))
						this.retractWord();
				} else if(userInput == "!") {
					this.retractWord();
				} else if(userInput == "@") {
					this.tradeTiles();
				} else if(userInput.match(/[a-zA-Z]/) || userInput == "_") {
					this.putLetter(userInput.toUpperCase());
				}
				this.displayTiles();
				break;
		}
		return true;
	}

	this.displayScores = function() {
		this.scoreBoard.clear();
		for(var p in this.shared.players) {
			if(this.shared.players[p].turn)
				this.scoreBoard.putmsg("\1h\1g* ");
			else
				this.scoreBoard.putmsg("  ");
			this.scoreBoard.putmsg("\1h\1w" + format("%-22s", this.shared.players[p].alias) + " " + this.shared.players[p].score + "\r\n");
		}
	}
	
	this.displayTiles = function() {
		this.tileFrame.clear();
		for(var t in this.shared.players[userID].tiles) {
			this.tileFrame.putmsg(this.shared.players[userID].tiles[t]);
			if(t < this.shared.players[userID].tiles.length - 1)
				this.tileFrame.putmsg(" ");
		}
		this.tileCountFrame.clear();
		this.tileCountFrame.putmsg(this.countTiles());
	}
	
	this.handleUpdates = function(update) {
		if(update.oper != "WRITE")
			return false;
		var location = update.location.split(".");
		if(location[0] != "games" || location[1] != this.parent.id)
			return false;
		location.splice(0, 2);
		location = location.join(".");
		this.parent.shared[location] = update.data;
		this.parent.drawGrid();
		this.parent.displayScores();
	}
	
	this.drawGrid = function() {
		for(var g in this.shared.grid) {
			if(!this.shared.grid[g].hasOwnProperty("letter"))
				continue;
			var xy = g.split("-");
			this.board.gotoxy(xy[0], xy[1]);
			this.board.putmsg(this.shared.grid[g].letter, this.shared.grid[g].bg|WHITE);
		}
	}
	
	if(id === undefined) {
		id = this.client.keys("wordem", "games", 1);
		if(id === undefined)
			this.id = 0;
		else
			this.id = id.length;
		this.shared.players[userID] = {
			'alias' : user.alias,
			'score' : 0,
			'tiles' : [],
			'turn' : true
		};
		for(var x = 0; x < 7; x++)
			this.shared.players[userID].tiles.push(this.drawTile());
		for(var x = 2; x <= 44; x = x + 3) {
			for(var y = 1; y <= 15; y++) {
				var gd = this.board.getData(x - 1, y - 1);
				this.shared.grid[x + "-" + y] = {
					'ch' : gd.ch,
					'bg' : gd.attr&bgmask
				};
			}
		}
		this.client.write("wordem", "games." + this.id, this.shared, 2);
		this.client.push("wordem", "users." + userID + ".games", this.id, 2);
	} else {
		this.id = id;
		this.shared = this.client.read("wordem", "games." + this.id, 1);
		if(!this.shared.players.hasOwnProperty(userID)) {
			var myTurn = true;
			for(var p in this.shared.players) {
				if(this.shared.players[p].turn)
					myTurn = false;
			}
			this.shared.players[userID] = {
				'alias' : user.alias,
				'score' : 0,
				'tiles' : [],
				'turn' : myTurn
			};
			for(var x = 0; x < 7; x++)
				this.shared.players[userID].tiles.push(this.drawTile());
			this.client.write("wordem", "games." + this.id + ".players", this.shared.players, 2);
			this.client.push("wordem", "users." + userID + ".games", this.id, 2);
		}
		for(var g in this.shared.grid) {
			if(!this.shared.grid[g].hasOwnProperty("letter"))
				continue;
			var gs = g.split("-");
			this.board.gotoxy(gs[0], gs[1]);
			this.board.putmsg(this.shared.grid[g].letter, this.shared.grid[g].bg|WHITE);
		}
	}
	this.client.callback = this.handleUpdates;
	this.client.subscribe("wordem", "games." + this.id);
	this.displayTiles();
	this.displayScores();
	this.frame.open();
}

var doGame = function(id) {
	if(id === undefined) {
		var keys = client.keys("wordem", "games", 1);
		for(var k = 0; k < keys.length; k++) {
			var g = client.read("wordem", "games." + keys[k], 1);
			if(g.end || g.players.hasOwnProperty(userID) || countMembers(g.players) > 1)
				continue;
			id = k;
			break;
		}
	}
	return new Game(id);
}

frame.open();
while(ascii(userInput) != 27) {
	userInput = console.inkey(K_NONE, 5);
	if(!play) {
		if(!menuTree) {
			menuTree = new Tree(menuTreeFrame);
			menuTree.colors.lfg = WHITE;
			menuTree.colors.lbg = BG_CYAN;
			menuTree.addItem("[ New game ]", doGame);
			games = client.read("wordem", "users." + userID + ".games", 1);
			for(var g = 0; g < games.length; g++) {
				var gg = client.read("wordem", "games." + games[g], 1);
				if(gg.dates.end)
					continue;
				var str = games[g] + ": ";
				var pp = 0;
				for(var p in gg.players) {
					pp++;
					if(p == userID)
						continue;
					str += gg.players[p].alias + "(" + gg.players[p].score + "), ";
				}
				if(pp == 1)
					str += "Waiting for opponent. ";
				if(gg.players[userID].turn)
					str += "Your turn. ";
				else
					str += "Waiting. ";
				menuTree.addItem(str, doGame, games[g]);
			}
			menuTree.open();
		}
		game = menuTree.getcmd(userInput);
		if(game.hasOwnProperty("id")) {
			menuTree = false;
			play = true;
		}
	} else {
		game.client.cycle();
		play = game.getcmd(userInput);
		userInput = "";
	}
	if(frame.cycle())
		console.gotoxy(80, 24);
}