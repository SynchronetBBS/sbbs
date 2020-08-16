/************ Synchronet Global War game data library *************

		by mcmlxxix - March 4, 2015 
		www.thebrokenbubble.com
		
		$Id: gwarlib.js,v 1.5 2015/03/05 22:19:10 mcmlxxix Exp $
		$Revision: 1.5 $
*/

/********************** API usage example**************************

// load this library
load("gwar.js");

// create global war object instance
var GWAR = new GlobalWar("/sbbs/xtrn/globalwar/");

// get a list of player turns
var myturns = GWAR.getPlayerTurns(user.alias);
if(myturns.length > -1) {
	var msg = "\1n\1gIt is your turn in Global War game(s)\1g\1h: " + myturns.join(",");
	system.put_telegram(system.matchuser(user.alias),msg);
}

*/

/************************ API reference ***************************
	
GWAR.loadGames(); 

	called automatically upon loading script. populates GWAR.gameList object
	with all game data contained in the specified path
	
GWAR.loadGame(gameNumber);

	refreshes game data for specified game number within GWAR.gameList object
	and returns the game object
	
GWAR.hasContinent(gameNumber,playerName,continent);

	returns true or false depending on whether the specified player owns all
	of the countries in the specified continent in the specified game
	
GWAR.hasCards(gameNumber,playerName);
	
	returns true or false depending on whether the specified player has a
	redeemable set of cards in the specified game
	
GWAR.getPlayerTurns(playerName);

	scans all GWAR.gameList for games whose next turn is the specified player.
	returns an array of integers representing game numbers
	
GWAR.getGameTurn(gameNumber);

	returns a GlobalWarPlayer object representing the player whose turn is next
	in the specified game

*/

/* global war library object */
function GlobalWar(path) {

	/* game root data */
	this.lastUpdate = 0;
	this.gamePath = path;
	this.gameList = {};
	this.players = {};
	
	this.countryList = [
		"Afghanistan","Alaska","Alberta","Argentina","Brazil","Mexico",
		"China","Zaire","East Africa","Eastern Australia","Eastern United States",
		"Libya","France","Greenland","England","India","Borneo","Irkutsk","Japan",
		"Kamchatka","Madagascar","Iran","Mongolia","New Guinea","West Africa",
		"Germany","Yukon","Ontario","Peru","Quebec","Scandinavia","Siam",
		"Siberia","South Africa","Italy","Ukraine","Ural","Venezuela",
		"Western Australia","Spain","Western United States","Taimyr"
	];	
	
	this.continentList = {
		"north america":	[1,2,5,10,13,26,27,29,40],
		"south america":	[3,4,28,37],
		"europe":			[12,14,25,30,34,35,39],
		"africa":			[7,8,11,20,24,33],
		"asia":				[0,6,15,17,18,19,21,22,31,32,36,41],
		"australia":		[9,16,23,38]
	};
	
	/* constructor */
	this.loadGames();
	this.loadScores();
}

/* global war library methods */
GlobalWar.prototype.loadGames = function() {
	var gameList = directory(this.gamePath + "WAR??.DAT");
	for(var g=0;g<gameList.length;g++) {
		var rootFileName = truncstr(file_getname(gameList[g]),'.');
		var gameNumber = Number(rootFileName.substring(3));
		var globalWar = new GlobalWarGame(this.gamePath + rootFileName,gameNumber);
		this.gameList[gameNumber] = globalWar;
	}
}
GlobalWar.prototype.loadGame = function(gameNumber) {
	if(this.gameList[gameNumber]) {
		this.gameList[gameNumber].loadGame();
		return this.gameList[gameNumber];
	}
}
GlobalWar.prototype.loadScores = function() {
	var w = new File(this.gamePath + "winners.war");
	w.open('r',true);
	while(!w.eof) {
		var p = {
			score:Number(w.readln()),
			name:w.readln(),
			system:w.readln()
		};
		this.players[p.name] = p;
	}
	w.close();
}
GlobalWar.prototype.hasContinent = function(gameNumber,playerName,continent) {
	var g = this.gameList[gameNumber];
	var c = this.continentList[continent.toLowerCase()];
	var p;
	for(var i=0;i<g.players.length;i++) {
		if(g.players[i].playerName.toUpperCase() == playerName.toUpperCase()) {
			p = i;
			break;
		}
	}
	for(var i=0;i<c.length;i++) {
		if(g.countries[c[i]].owner !== p)
			return false;
	}
	return true;
}
GlobalWar.prototype.hasCards = function(gameNumber,playerName) {
	var g = this.gameList[gameNumber];
	var p;
	for(var i=0;i<g.players.length;i++) {
		if(g.players[i].playerName.toUpperCase() == playerName.toUpperCase()) {
			p = i;
			break;
		}
	}
	if(p.cards.general > 0 && p.cards.king > 0 && p.cards.queen > 0)
		return true;
	if(p.cards.general > 2 || p.cards.king > 2 || p.cards.queen > 2)
		return true;
	return false;
}
GlobalWar.prototype.getPlayerTurns = function(playerName) {
	var turnList = [];
	for(var g in this.gameList) {
		var game = this.gameList[g];
		if(game == undefined)
			continue;
		var nextPlayer = game.getNextTurnPlayer();
		if(nextPlayer.playerName.toUpperCase() == playerName.toUpperCase()) {
			turnList.push(game.gameNumber);
		}
	}
	return turnList;
}
GlobalWar.prototype.getGameTurn = function(gameNumber) {
	var game = this.gameList[gameNumber];
	var turn = game.getNextTurnPlayer();
	return turn;
}

/* global war game data object */
function GlobalWarGame(rootFileName, gameNumber) {

	this.lastUpdate = 0;
	this.gameNumber = gameNumber;
	this.dataFile = new File(rootFileName + ".DAT"); 
	this.logFile = new File(rootFileName + ".LOG"); 
	this.msgFile = new File(rootFileName + ".MSG"); 
	
	/* Note: "daynum" refers to a real number which is the number of days since
	January 1, 1985.   Example: a daynum of 1671.6039 works out to be
	July 30, 1989 (1/1/85 + 1671 days).  Adding another .6039 days works out
	to be 14.4936 hours, which puts us at about 2:29 pm. */
		
	this.baseDate = new Date("January 1, 1985 00:00:00");
	this.dayMS = 24 * 60 * 60 * 1000;

	this.gameEvent;			/* line 1 */
	this.eventDay;			/* line 2 */
	this.players = [];		/* lines 3 - 38 */
	this.cardSets;			/* line 39 */
	this.countries = [];	/* lines 40 - 123 */
	this.passwords = [];	/* lines 124 - 126 */
	this.maxForts;			/* line 127 */
	
	/* populated after load */
	this.settings = { 		
		team:false,
		started:false,
		completed:false,
		minPlayers:3,
		maxPlayers:6,
		hidden:false,
		adjacent:false,
		sequential:true,
		date:undefined
	};
	
	/* constructor */
	this.loadGame();
	this.parseEvent();
}

/* global war game data methods */
GlobalWarGame.prototype.loadGame = function() {
	var f = this.dataFile;
	
	f.open('r',true);
	this.gameEvent = f.readln();
	this.eventDay = f.readln();
	for(var p=0;p<6;p++) {
		this.players[p] = this.loadPlayer(f, p);
	}
	this.cardSets = f.readln();
	for(var c=0;c<42;c++) {
		this.countries[c] = this.loadCountry(f, c);
	}
	this.passwords[0] = f.readln();
	this.passwords[1] = f.readln();
	this.passwords[2] = f.readln();
	this.maxForts = f.readln();
	for(var p=0;p<6;p++) {
		this.players[p].system = f.readln();
	}
	f.close();
	
	this.lastUpdate = Date.now();
}
GlobalWarGame.prototype.parseEvent = function() {
	this.settings.date = new Date(this.baseDate.getTime() + (this.dayMS * this.eventDay));
	var event = this.gameEvent.substr(1);
	if(event > 10000) 
		this.settings.sequential = false;
	if(event.substr(1) > 1000)
		this.settings.adjacent = true;
	if(event.substr(2) > 100)
		this.settings.hidden = true;
	event = event.substr(3);
	if(Number(event) < 6) {
		switch(event) {
		case 1:
			this.settings.started = true;
			break;
		case 2:
			this.settings.started = true;
			this.settings.completed = true;
			break;
		case 3:
			this.settings.team = true;
			break;
		case 4:
			this.settings.team = true;
			this.settings.started = true;
			break;
		case 5:
			this.settings.team = true;
			this.settings.started = true;
			this.settings.completed = true;
			break;
		}
	}
	else {
		this.settings.minPlayers = event[0];
		this.settings.maxPlayers = event[1];
	}
}
GlobalWarGame.prototype.loadPlayer = function(file, playerNumber) {
	var playerName = file.readln(); 
	var vote = file.readln(); 
	var lastTurn = file.readln(); 
	var cards = {
		general : file.readln(),
		king : file.readln(),
		queen : file.readln()
	}
	return new GlobalWarPlayer(
		playerNumber, 
		playerName, 
		(vote=="T"?true:false), 
		new Date(this.baseDate.getTime() + this.dayMS * Number(lastTurn)),
		cards
	);
}
GlobalWarGame.prototype.loadCountry = function(file, countryNumber) {
	return {
		number:countryNumber-1,
		armies:file.readln(),
		owner:file.readln()-1
	};
}	
GlobalWarGame.prototype.getNextTurnPlayer = function() {
	var nextPlayer;
	for(var i=0;i<this.players.length;i++) {
		var p = this.players[i];
		if(nextPlayer == undefined || Number(p.lastTurn) > Number(nextPlayer.lastTurn)) {
			nextPlayer = p;
		}
	}
	return nextPlayer;
}

/* global war player data object */
function GlobalWarPlayer(playerNumber, playerName, vote, lastTurn, cards) {
	this.playerNumber = playerNumber;
	this.playerName = playerName;
	this.vote = vote;
	this.lastTurn = lastTurn;
	this.cards = cards;
	this.system;
}
