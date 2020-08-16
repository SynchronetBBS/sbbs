/*
	***************BBS DICE WARZ***************
	$Id: dice.js,v 1.55 2014/01/07 16:31:27 mcmlxxix Exp $
	$Revision: 1.55 $
	for use with Synchronet v3.14+
	by Matt Johnson (2008)  
	*******************************************
*/

load("sbbsdefs.js");
load("graphic.js");
var root = js.exec_dir;

load(root + "diceroll.js");
load(root + "lock.js");
load(root + "maps.js");
load(root + "territory.js");
load(root + "menu.js");
load(root + "player.js");
load(root + "display.js");
load(root + "ai.js");

var activeGame="";

//######################### INITIALIZE PROGRAM VARIABLES #########################
const	scorefile=		"rankings.dat";
const	settingsfile=	"dice.ini"
const	halloffame=		"hof.dat";
const	instructions=	"dice.doc";
//MAP BACKGROUND COLORS
const 	bColors=		[BG_BLUE,	BG_CYAN,	BG_RED,		BG_GREEN,	BG_BROWN,	BG_MAGENTA,	BG_LIGHTGRAY]; 		
//MAP BACKGROUND COLORS (FOREGROUND CHARS)
const 	bfColors=		[BLUE,		CYAN,		RED,		GREEN,		BROWN,		MAGENTA,	LIGHTGRAY]; 		
//MAP FOREGROUND COLORS
const	fColors=		["\1h\1w",	"\1h\1c",	"\1h\1r",	"\1h\1g",	"\1h\1y", 	"\1h\1m",	"\1k"];				
const	border_color=	"\1n\1k\1h";


//######################### DO NOT CHANGE THIS SECTION ##########################	

var userFileName=root + user.alias + ".usr";
var userFile=new File(userFileName);
userFile.open('a');
userFile.close();

const	daySeconds=		86400;
const 	startRow=		4;
const	startColumn=	3;
const	menuRow=		1;
const	menuColumn=		50;
const 	columns=		9;
const 	rows=			9;
const 	blackbg=		console.ansi(ANSI_NORMAL);

//TODO: FIX SCORING SYSTEM... BECAUSE RIGHT NOW IT SUCKS
var		scores=			[];
var		messages=		[];								//MESSAGES QUEUED FOR DISPLAY UPON RELOADING MAIN MENU
var		games=			new GameStatusInfo();
var		settings=		new GameSettings();
var		gamedice=		loadDice();
var 	oldpass=		console.ctrlkey_passthru;

js.on_exit("cleanUp();");
console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
bbs.sys_status|=SS_MOFF;

//########################## MAIN FUNCTIONS ###################################
function 	scanProximity(location) 	
{										
	var prox=[];
	var odd=false;
	var offset=location%columns;
	if((offset%2)==1) 
		odd=true;

	if(location>=columns) 					//if not in the first row
		prox[0]=(location-columns);				//north
	if(location<(columns*(rows-1)))			//if not in the last row
		prox[1]=(location+columns);				//south
	
	if(odd)	{
		if(((location+1)%columns)!=1)			//if not in the first column
			prox[2]=location-1;						//northwest
		if(((location+1)%columns)!=0)			//if not in the last column
			prox[3]=location+1;						//northeast
		if(location<(columns*(rows-1))) {								//if not in the last row
			if(((location+1)%columns)!=1)			//if not in the first column
				prox[4]=prox[1]-1;						//southwest
			if(((location+1)%columns)!=0)			//if not in the last column
				prox[5]=prox[1]+1;						//southeast		
		}
	}
	else {
		if(location>=columns) {										//if not in the first row
			if(((location+1)%columns)!=1)			//if not in the first column
				prox[2]=prox[0]-1;						//northwest
			if(((location+1)%columns)!=0)			//if not in the last column
				prox[3]=prox[0]+1;						//northeast		
		}
		if(((location+1)%columns)!=1)			//if not in the first column
			prox[4]=location-1;						//southwest
		if(((location+1)%columns)!=0)			//if not in the last column
			prox[5]=location+1;						//southeast
	}
	return prox;
}
function 	getNextGameNumber()
{
	var next=1;
	while(games.gameData[next]) {
		next++;
	}
	return next;
}
function	CountSparseArray(data)
{
	var count=0;
	for(var tt in data) {
		count++;
	}
	if(count==0) 
		return false;
	else 
		return count;
}
function	viewInstructions()
{
	console.clear();
	console.printfile(root + instructions);
	if(!(user.settings & USER_PAUSE)) 
		console.pause();
	console.aborted=false;
}
function	viewRankings()
{
	games.hallOfFame();
	games.loadRankings();
	var scoredata=sortScores();
	console.clear();
	
	printf("\1k\1h\xDA");
	printf(printPadded("\xC2",27,"\xC4","right"));
	printf(printPadded("\xC2",8,"\xC4","right"));
	printf(printPadded("\xC2",8,"\xC4","right"));
	printf(printPadded("\xC2",7,"\xC4","right"));
	printf(printPadded("\xC2",9,"\xC4","right"));
	printf(printPadded("\xBF",10,"\xC4","right"));
	console.crlf();
	printf("\1k\1h\xB3\1n\1g DICE WARZ RANKINGS       \1k\1h\xB3 \1n\1gSCORE \1k\1h\xB3 \1n\1gKILLS \1k\1h\xB3 \1n\1gWINS \1k\1h\xB3 \1n\1gLOSSES \1k\1h\xB3 \1n\1gWIN %   \1k\1h\xB3");
	console.crlf();
	printf("\xC3");
	printf(printPadded("\xC5",27,"\xC4","right"));
	printf(printPadded("\xC5",8,"\xC4","right"));
	printf(printPadded("\xC5",8,"\xC4","right"));
	printf(printPadded("\xC5",7,"\xC4","right"));
	printf(printPadded("\xC5",9,"\xC4","right"));
	printf(printPadded("\xB4",10,"\xC4","right"));
	console.crlf();
 	
	for(var hs in scoredata) { 
		thisuser=scoredata[hs];
		if(scores[thisuser].score!=0 || scores[thisuser].wins>0 || scores[thisuser].losses>0 || scores[thisuser].kills>0) {
			var winPercentage=0;
			if(scores[thisuser].wins>0) 
				winPercentage=(scores[thisuser].wins/(scores[thisuser].wins+scores[thisuser].losses))*100;
			printf("\1k\1h\xB3");
			printf(printPadded("\1y\1h " + system.username(thisuser),26," ","left"));
			printf("\1k\1h\xB3");
			printf(printPadded("\1w\1h" + scores[thisuser].score + " \1k\1h\xB3",8," ","right"));
			printf(printPadded("\1w\1h" + scores[thisuser].kills + " \1k\1h\xB3",8," ","right"));
			printf(printPadded("\1w\1h" + scores[thisuser].wins + " \1k\1h\xB3",7," ","right"));
			printf(printPadded("\1w\1h" + scores[thisuser].losses + " \1k\1h\xB3",9," ","right"));
			printf(printPadded("\1w\1h" + winPercentage.toFixed(2) + " % \1k\1h\xB3",10," ","right"));
			console.crlf();
		}
	}
	
	printf("\1k\1h\xC0");
	printf(printPadded("\xC1",27,"\xC4","right"));
	printf(printPadded("\xC1",8,"\xC4","right"));
	printf(printPadded("\xC1",8,"\xC4","right"));
	printf(printPadded("\xC1",7,"\xC4","right"));
	printf(printPadded("\xC1",9,"\xC4","right"));
	printf(printPadded("\xD9",10,"\xC4","right"));
	console.crlf();
	
	console.putmsg("\r\n\1y\1h Scores will be reset when a player reaches \1c" + settings.pointsToWin + " \1ypoints.");
	
	console.gotoxy(1,24);
	console.pause();
	console.aborted=false;
}
function	sort(iiii)
{
	var sorted=[];
	for(var cc in iiii) {
		sorted.push(cc);
	}
	sorted.sort();
	return sorted;
}
function	attackMessage(txt)
{
	var x=menuColumn;
	clearArea(16,menuColumn,8);
	console.gotoxy(x,16);
	console.putmsg("\1r\1h Choose a territory");
	console.gotoxy(x,17);
	console.putmsg("\1r\1h " + txt);
	console.gotoxy(x,18);
	console.putmsg("\1r\1h or 'Q' to cancel.");
	console.gotoxy(x,20);
	console.putmsg("\1r\1h Use the arrow keys");
	console.gotoxy(x,21);
	console.putmsg("\1r\1h to move, and \1n\1r[\1hENTER\1n\1r]");
	console.gotoxy(x,22);
	console.putmsg("\1r\1h to select a tile.");

	wipeCursor("left");
}

function	compressScores()
{
	var compressed=[];
	for(var score in scores) {
		compressed.push(score);
	}
	return compressed;
}
function 	sortScores()
{ 
	var data=compressScores();
	var numScores=data.length;
	for(var n=0;n<numScores;n++)
	{
		for(var m = 0; m < (numScores-1); m++) 
		{
			if(scores[data[m]].score < scores[data[m+1]].score) 
			{
				var holder = data[m+1];
				data[m+1] = data[m];
				data[m] = holder;
			}
		}
	}
	return data;
}
function 	deliverMessage(nextTurnPlayer,gameNumber)
{
	var message="\1r\1hIt is your turn in \1yDice-Warz\1r game #" + gameNumber + "\r\n\r\n";
	if(!storeMessage(nextTurnPlayer,message)) system.put_telegram(nextTurnPlayer, message);
}
function 	deliverKillMessage(killer,eliminated,gameNumber)
{
	if(eliminated < 0)
		return;
	var message="\1r\1h" + killer + " has eliminated you in \1yDice\1r-\1yWarz\1r game #\1y" + gameNumber + "\1r!\r\n\r\n";
	if(!storeMessage(eliminated,message)) 
		system.put_telegram(eliminated, message);
}
function 	deliverForfeitMessage(unum,gameNumber)
{
	if(unum < 0)
		return;
	var message="\1r\1hYou have forfeited \1yDice-Warz\1r game #" + gameNumber + " due to inactivity\r\n\r\n";
	if(!storeMessage(unum,message)) 
		system.put_telegram(unum, message);
}
function	storeMessage(unum,msg)
{
	var ufname=root + system.username(unum) + ".usr";
	if(!file_exists(ufname)) 
		return false;
	else {
		var user_online=false;
		for(var n=0;n<system.node_list.length && !user_online;n++) {
			if(system.node_list[n].useron==unum) 
				user_online=true;
		}
		if(!user_online) {
			file_remove(ufname);
			return false;
		}
		var uf=new File(ufname);
		uf.open('a',true);
		uf.writeln(msg);
		uf.close();
		return true;
	}
}

//########################GAME MENU FUNCTIONS################################
function	splashStart()
{
	console.clear();
	var splash_filename=root + "dicewarz.bin";
	var splash_size=file_size(splash_filename);
	splash_size/=2;		
	splash_size/=80;	
	var splash=new Graphic(80,splash_size);
	splash.load(splash_filename);
	splash.draw();
	
	console.gotoxy(1,23);
	console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
	while(console.inkey(K_NOECHO|K_NOSPIN)==="");
}
function 	splashExit() 
{
	cleanUp();
	console.clear();
	var splash_filename=root + "exit.bin";
	if(!file_exists(splash_filename)) 
		exit();
	var splash_size=file_size(splash_filename);
	splash_size/=2;		
	splash_size/=80;	
	var splash=new Graphic(80,splash_size);
	splash.load(splash_filename);
	splash.draw();
	
	console.gotoxy(1,23);
	console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
	while(console.inkey(K_NOECHO|K_NOSPIN)==="");
	exit();
}
function 	cleanUp() 
{
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	file_remove(userFileName);
}
function 	gameMenu()
{
	var gMenu=new Menu(	""								,1,19,"\1n\1g","\1g\1h");
	var gmenu_items=[		"~Rankings"						, 
							"~Select Game"					,
							"~Begin New Game"				,
							"~Instructions"					,
							"~Quit to BBS"					];
	gMenu.add(gmenu_items);
	games.init();

	while(1) {
		//LOOPING MAIN MENU, UPDATES GAME STATUS INFO UPON REFRESH
		//TODO: make recently completed games disappear from "your turn" list upon refresh
		
		games.updateGames();
		console.clear();
		console.putmsg("\1r\1hB B S - D I C E - W A R Z ! \1n\1c- Matt Johnson - 2008");
		console.crlf();
		drawLine("\1n\1w\1h",79);
		console.crlf();
		console.gotoxy(1,18);
		drawLine("\1n\1w\1h",79);
		
		console.gotoxy(1,24);
		drawLine("\1n\1w\1h",79);
		gMenu.display();

		console.gotoxy(1,3);
		
		wrap("\1g Games in progress          ",games.inProgress);
		wrap("\1g Games needing more players ",games.notFull);
		wrap("\1g Completed games            ",games.completed);
		wrap("\1g You are involved in games  ",games.yourGames);
		wrap("\1g It is your turn in games   ",games.yourTurn);
		wrap("\1g You are eliminated in games",games.eliminated);
		wrap("\1g You have won games         ",games.youWin);
		wrap("\1g Single-player games        ",games.singleGames);
		
		displayMessages();
		
		wipeCursor("right");
		
		var cmd=console.getkey(K_NOSPIN|K_NOECHO|K_NOCRLF|K_UPPER);
		switch(cmd)	{
		case "R":
			viewRankings();
			break;
		case "S":
			var choice=chooseGame();
			if(choice) processSelection(choice);
			break;
		case "I":
			viewInstructions();
			break;
		case "B":
			createNewGame();
			break;
		case "Q":
			return false;
		default:
			break;
		}
		wipeCursor("right");
	}
}
function	chooseGame()
{
	var x=30; 
	var y=19;
	while(1) {
		console.gotoxy(x,y);
		console.cleartoeol();
		if(!games.gameData.length) {
			putMessage("\1r\1hThere are currently no games.",x,y);
			break;
		}
		console.putmsg("\1n\1gEnter game number or [\1hQ\1n\1g]uit\1h: ");
		var game_num=console.getkeys("Q",settings.maxGames);
		if(game_num=="Q") 
			return false;
		if(games.gameData[game_num]) {
			var game_data=games.gameData[game_num];
			var num=game_num;
			if(game_num<10) 
				num="0" + num;
			var gamefile="game" + num;

			if(Locked(gamefile)) 
				putMessage("\1r\1hThat game is in use by another node.",x,y);
			else {
				if(game_data.singlePlayer) {
					if(!(games.gameData[game_num].users[user.number]>=0)) {
						putMessage("\1r\1hGame #" + game_num + " is private.",x,y);
						continue;
					}
				}
				return game_num;
			}
		}
		else 
			putMessage("\1r\1hNo Such Game!",x,y);
	}
	return false;
}
function 	processSelection(gameNumber)
{	
	var num=gameNumber;
	if(gameNumber<10) 
		num="0" + num;
	var gamefile="game" + num;

	Lock(gamefile);
	var g=games.gameData[gameNumber];
	var fileName=g.fileName;
	var lastModified=file_date(fileName);
	if(lastModified>g.lastModified) 
		games.gameData[gameNumber]=games.loadGame(fileName,gameNumber,lastModified);
	
	if(g.status>=0)
		playGame(gameNumber);
	else {
		viewGameInfo(gameNumber);
		if(g.users[user.number]>=0)	{
			var gamePlayer=g.users[user.number];
			if(g.fixedPlayers) 
				askRemove(gameNumber,gamePlayer);
			else 
				changeVote(gameNumber,user.number);
		}
		else 
			joinGame(gameNumber);
	}
	Unlock(gamefile);
	games.filterData();
	clearArea(19,30,4);
}
function	changeVote(gameNumber,userNumber)
{
	console.gotoxy(30,19);
	console.cleartoeol();
	var g=games.gameData[gameNumber];
	var gamePlayer=g.users[userNumber];
	
	if(console.noyes("\1n\1gChange your vote?")) {
		console.gotoxy(30,20);
		console.cleartoeol();
		askRemove(gameNumber,gamePlayer);
	}
	else {
		var vote=g.players[gamePlayer].vote;
		if(vote==1) g.players[gamePlayer].vote=0;
		else g.players[gamePlayer].vote=1;
		if(g.tallyVotes()) {
			g.maxPlayers=g.countPlayers();
			startGame(gameNumber);
		}
		games.storeGame(gameNumber);
	}
}
function	askRemove(gameNumber,playerNumber)
{
	g=games.gameData[gameNumber];
	if(!(console.noyes("\1n\1gRemove yourself from this game?"))) {
		g.removePlayer(playerNumber);
		if(g.countHumanPlayers()==0) 
			file_remove(g.fileName);
		else 
			games.storeGame(gameNumber);
	}
}
function	viewGameInfo(gameNumber)
{
	var g=games.gameData[gameNumber];
	clearArea(3,1,14);
	console.gotoxy(2,4);
	console.putmsg("\1g[ \1hGAME: #" + gameNumber + " \1n\1g]\r\n");

	if(g.maxPlayers>g.minPlayers)
		console.putmsg("\1n\1g Minimum Players: \1h" + g.minPlayers + " \1n\1gMaximum: \1h" + g.maxPlayers + "\r\n");
	else 
		console.putmsg("\1n\1g Players Needed to Start: \1h" + g.maxPlayers + "\r\n");
	
	console.putmsg("\1n\1g Player names hidden: \1h" + g.hiddenNames + "\r\n");
	console.putmsg("\1n\1g Players In This Game:\r\n");
	for(var playerNumber=0;playerNumber<g.players.length;playerNumber++) {
		var player=g.players[playerNumber];
		var name=getUserName(player,playerNumber);
		if(g.hiddenNames && name!=user.alias)
			name="Player " + (playerNumber+1);
		console.putmsg("\1g\1h  " + name);
		if(player.vote>=0) {
			if(g.maxPlayers>g.minPlayers)
				console.putmsg(" \1n\1gvotes to \1h" + g.getVote(playerNumber));
		}
		console.crlf();
	}
	console.crlf();
}
function	startGame(gameNumber)
{
	var maxPlayers=games.gameData[gameNumber].maxPlayers;
	var players=games.gameData[gameNumber].players;
	var oldFn=games.gameData[gameNumber].fileName;
	var oldSp=games.gameData[gameNumber].singlePlayer;
	var oldhn=games.gameData[gameNumber].hiddenNames;
	games.gameData[gameNumber]=new Map(columns,rows,maxPlayers,gameNumber);
	games.gameData[gameNumber].fileName = oldFn;
	games.gameData[gameNumber].singlePlayer = oldSp;
	games.gameData[gameNumber].hiddenNames = oldhn;
	var g=games.gameData[gameNumber];
	games.gameData[gameNumber].players=players;
	games.inProgress.push(gameNumber);
	games.gameData[gameNumber].init();
	games.gameData[gameNumber].lastModified=time();

	queueMessage("\1r\1hGame " + gameNumber + " initialized!");
	games.gameData[gameNumber].notify();
	/* Set up computer players */
	var aifile=new File(root + "ai.ini");
	aifile.open("r");
	var possibleplayers=aifile.iniGetSections();
	for(var i=0; i<g.players.length; i++) {
		if(g.players[i].user==-1) {
			if(possibleplayers.length > 0) {
				var p=random(possibleplayers.length);
				while(!possibleplayers[p])
					p = random(possibleplayers.length);
				g.players[i].AI.name=possibleplayers[p];
				possibleplayers.splice(p,1);
				g.players[i].AI.sort=aifile.iniGetValue(g.players[i].AI.name, "sort", "Random");
				g.players[i].AI.check=aifile.iniGetValue(g.players[i].AI.name, "Check", "Random");
				g.players[i].AI.qty=aifile.iniGetValue(g.players[i].AI.name, "Quantity", "Random");
				g.players[i].AI.forfeit=aifile.iniGetValue(g.players[i].AI.name, "Forfeit", "Random");
				if(AISortFunctions[g.players[i].AI.sort]===undefined)
					g.players[i].AI.sort="Random";
				if(AICheckFunctions[g.players[i].AI.check]===undefined)
					g.players[i].AI.check="Random";
				if(AIQtyFunctions[g.players[i].AI.qty]===undefined)
					g.players[i].AI.qty="Random";
			}
		}
	}
	aifile.close();
	games.storeGame(gameNumber);
}
function	joinGame(gameNumber)
{
	var numplayergames=games.yourGames.length-games.eliminated.length;
	if(numplayergames>=settings.maxPerPlayer) {
		console.pause();
		return;
	}
	var vote=-1;
	g=games.gameData[gameNumber];
	if(!(console.noyes("\1n\1gjoin this game?"))) {
		if(g.maxPlayers>g.minPlayers && g.players.length+1<g.maxPlayers) vote=getVote();
		g.addPlayer(user.number,vote);
		if(g.players.length>=g.minPlayers) {
			if(g.players.length==g.maxPlayers || g.tallyVotes()) {
				g.maxPlayers=g.players.length;
				startGame(gameNumber);
			}
		}
		games.storeGame(gameNumber);
	}
}
function 	createNewGame()
{
	var numplayergames=games.yourGames.length-games.eliminated.length;
	if(numplayergames>=settings.maxPerPlayer) {
		queueMessage("\1r\1hYou can only be active in " + settings.maxPerPlayer + " games at a time");
		return false;
	}

	var minNumPlayers=-1;
	var maxNumPlayers=7;
	var numComputerPlayers=-1;
	var singlePlayer=false;
	var fixedPlayers=false;
	var vote=-1;
	var hiddenNames=false;
	
	var x=30; 
	var y=19;
	console.gotoxy(x,y);
	console.cleartoeol();
	if(console.yesno("\1n\1gBegin a new game?")) {
		clearArea(3,1,14);
		clearArea(19,30,5);
		var gameNumber=getNextGameNumber();
		var gnum=gameNumber;
		if(gameNumber<10) 
			gnum="0" + gnum;
		var gamefile="game" + gnum;
		Lock(gamefile);
	}
	else {
		console.gotoxy(x,y);
		console.cleartoeol();
		return false;
	}
	x=2; 
	y=3;
	console.gotoxy(x,y);
	console.cleartoeol();
	y++;
	if(!console.noyes("\1n\1gSingle player game?")) {
		singlePlayer=true;
		while(1) {
			console.gotoxy(x,y);
			console.cleartoeol();
			console.putmsg(
				"\1n\1gEnter number of opponents [\1h" + 
				(settings.minPlayers-1) + "\1n\1g-\1h" + 
				(settings.maxPlayers-1) + "\1n\1g] or [\1hQ\1n\1g]uit: "
			);
			var num=console.getkeys("Q",settings.maxPlayers-1);
			if(num>=settings.minPlayers-1 && num<settings.maxPlayers) {
				minNumPlayers=num+1;
				maxNumPlayers=num+1;
				numComputerPlayers=num;
				y++;
				break;
			}
			else if(num=="Q") 
				return false; 
			else 
				putMessage("\1r\1h Please enter a number within the given range.",x,y);
		}
	}
	else {
		console.gotoxy(x,y);
		y++;
		if(!console.noyes("\1n\1gKeep player names hidden?")) 
			hiddenNames=true;
		while(1) {
			console.gotoxy(x,y);
			console.cleartoeol();
			console.putmsg("\1n\1gEnter minimum number of players [\1h" + settings.minPlayers + "\1n\1g-\1h" + settings.maxPlayers + "\1n\1g] or [\1hQ\1n\1g]uit: ");
			var num=console.getkeys("Q",settings.maxPlayers);
			if(num>=settings.minPlayers && num<=settings.maxPlayers) {
				minNumPlayers=num;
				y++;
				break;
			}
			else if(num=="Q") 
				return false; 
			else 
				putMessage("\1r\1h Please enter a number within the given range.",x,y);
		}
		if(minNumPlayers<settings.maxPlayers) {
			while(1) {
				console.gotoxy(x,y);
				console.cleartoeol();
				console.putmsg("\1n\1gEnter maximum number of players [\1h" + minNumPlayers + "\1n\1g-\1h" + settings.maxPlayers + "\1n\1g] or [\1hQ\1n\1g]uit: ");
				var num=console.getkeys("Q",settings.maxPlayers);
				if(num>=minNumPlayers && num<=settings.maxPlayers) {
					maxNumPlayers=num;
					y++;
					break;
				}
				else if(num=="Q") 
					return false;
				else 
					putMessage("\1r\1h Please enter a number within the given range.",x,y);
			}
		}
		else 
			fixedPlayers=true;
		while(1) {
			console.gotoxy(x,y);
			console.cleartoeol();
			if(console.noyes("\1n\1gInclude computer players?")) 
				break;
			console.gotoxy(x,y);
			console.cleartoeol();
			console.putmsg("\1n\1gHow many? [\1h1\1n\1g-\1h" + (maxNumPlayers-1) + "\1n\1g] or [\1hQ\1n\1g]uit: ");
			var cnum=console.getkeys("Q",maxNumPlayers-1);
			if(cnum<=(maxNumPlayers-1) && cnum>0) {
				numComputerPlayers=cnum;
				break;
			}
			else if(cnum=="Q") 
				return false;
			else 
				putMessage("\1r\1h Please enter a number within the given range.",x,y);
		}
	}
	games.gameData[gameNumber]=new NewGame(minNumPlayers,maxNumPlayers,gameNumber);
	games.gameData[gameNumber].fileName=getFileName(gameNumber);
	for(var cp=0;cp<numComputerPlayers;cp++)
		games.gameData[gameNumber].addPlayer(-1, 1); //NO USER NUMBER, NO VOTE
	if(!singlePlayer) {
		if(hiddenNames) 
			games.gameData[gameNumber].hiddenNames=true;
		if(fixedPlayers) 
			games.gameData[gameNumber].fixedPlayers=true;
		else {
			console.crlf();
			vote=getVote();
		}
	}
	games.gameData[gameNumber].addPlayer(user.number, vote);
	if(singlePlayer) {
		games.gameData[gameNumber].singlePlayer=true;
		startGame(gameNumber);
		games.singleGames.push(gameNumber);
		games.yourGames.push(gameNumber);
		games.yourTurn.push(gameNumber);
	}
	else 
		games.notFull.push(gameNumber);

	games.storeGame(gameNumber);
	queueMessage("\1r\1hGame " + gameNumber + " Created!");
	Unlock(gamefile);
	return true;
}
function	getFileName(gameNumber)
{
	var num=gameNumber;
	if(gameNumber<10) 
		num="0" + gameNumber;
	var gamefile="game" + num;
	return (root + gamefile + ".dat");
}
function 	getVote()
{
	var vote=0;
	console.putmsg("\1n\1gDo you wish to start the game immediately or wait for more players?");
	console.crlf();
	if(console.yesno("\1g\1hYes to start, No to wait"))
		vote = 1;
	return vote;
}
function 	getUserName(playerData,playerNumber)
{
	if(playerData.user>0)
		return(system.username(playerData.user));
	if(playerData.AI.name.length > 0)
		return(playerData.AI.name);
	return("Computer " + (playerNumber+1));
}

//###########################GAMEPLAY FUNCTIONS#############################
function	selectTile(gameNumber,playerNumber,attackPosition,startPosition)
{
	var g=games.gameData[gameNumber];
	var terr;
	var attackpos;
	if(attackPosition>=0) 
	{
		attackpos=g.grid[attackPosition];		//IF A BOARD POSITION HAS BEEN SUPPLIED TO ATTACK FROM, START THERE
		terr=attackpos;
	}
	else if(startPosition>=0) 
	{
		terr=g.grid[startPosition];				//IF A BOARD POSITION HAS BEEN SUPPLIED TO START FROM, START THERE
		showSelected(terr,"\1n\1w\1h");
	}
	else														//OTHERWISE, START WITH THE FIRST PLAYER TERRITORY THAT IS CAPABLE OF ATTACKING
	{
		var p=g.players[playerNumber];
		for(tile in p.territories)
		{
			terr=g.grid[p.territories[tile]];
			if(terr.dice>1) 
			{
				showSelected(terr,"\1n\1w\1h");
				break;
			}
		}
	}
	var dir=terr.location;
	while(1)
	{
		var key=console.getkey(K_NOECHO|K_NOCRLF|K_UPPER);
		if(key=="\r" || key===undefined) 
		{
			if(attackPosition>=0)
			{
				if(terr.player!=playerNumber)
				{
					if(terr.location==g.grid[attackPosition].north) return terr;
					if(terr.location==g.grid[attackPosition].south) return terr;
					if(terr.location==g.grid[attackPosition].northeast) return terr;
					if(terr.location==g.grid[attackPosition].northwest) return terr;
					if(terr.location==g.grid[attackPosition].southeast) return terr;
					if(terr.location==g.grid[attackPosition].southwest) return terr;
				}
			}
			else if(g.canAttack(playerNumber,terr.location)) return terr;
		}
		switch(key)
		{
			case KEY_DOWN:
			case '2':
				if(g.grid[terr.south])
					dir=terr.south;
				break;
			case KEY_UP:
			case '5':
				if(g.grid[terr.north])
					dir=terr.north;
				break;
			case '1':
				if(g.grid[terr.southwest])
					dir=terr.southwest;
				break;
			case '4':
				if(g.grid[terr.northwest])
					dir=terr.northwest;
				break;
			case '3':
				if(g.grid[terr.southeast])
					dir=terr.southeast;
				break;
			case '6':
				if(g.grid[terr.northeast])
					dir=terr.northeast;
				break;
			case KEY_LEFT:
				if(g.grid[terr.southwest])
					dir=terr.southwest;
				else if(g.grid[terr.northwest])
					dir=terr.northwest;
				break;
			case KEY_RIGHT:
				if(g.grid[terr.northeast])
					dir=terr.northeast;
				else if(g.grid[terr.southeast])
					dir=terr.southeast;
				break;
			case "Q":
				if(attackpos) attackpos.displayBorder(border_color);
				terr.displayBorder(border_color);
				return false;
			default:
				continue;
		}
		if(terr.location != attackPosition) terr.displayBorder(border_color);
		if(attackpos) showSelected(attackpos,"\1n\1r\1h");
		var terr=g.grid[dir];
		showSelected(terr,"\1n\1w\1h");
	}
	wipeCursor("left");
}
function	showSelected(selection,color)
{
	selection.displaySelected(color);
	wipeCursor("left");
}
function	attack(gameNumber,playerNumber)
{
	clearArea(16,menuColumn,8);
	attackMessage("from which to Attack,");
	var attackFrom=selectTile(gameNumber,playerNumber,-1,games.gameData[gameNumber].lastMapPosition);
	if(attackFrom==false) 
		return false;
	
	clearArea(16,menuColumn,8);
	attackMessage("to Attack,");
	var attackTo=selectTile(gameNumber,playerNumber,attackFrom.location);
	if(attackTo==false) 
		return false;
	var tofrom=battle(attackFrom,attackTo,gameNumber);
	if(tofrom=true)	
		games.gameData[gameNumber].lastMapPosition=attackTo.location;
	else 
		games.gameData[gameNumber].lastMapPosition=attackFrom.location;
	return true;
}
function	battle(attackFrom,attackTo,gameNumber)
{
	var g=games.gameData[gameNumber];
	clearArea(16,menuColumn,8);
	showSelected(attackFrom,"\1n\1r\1h");
	showSelected(attackTo,"\1n\1r\1h");
	var totals=rollDice(attackFrom.dice,attackTo.dice,gamedice);
	
	var defender=attackTo.player;
	var attacker=attackFrom.player;
	var defending=attackTo.location;
	var attacking=attackFrom.location;
	if(totals[0]>totals[1]) {
		g.players[defender].removeTerritory(defending); 	//REMOVE TILE FROM DEFENDER'S LIST
		g.players[attacker].territories.push(defending);	//ADD TILE TO ATTACKER'S LIST
		g.players[defender].totalDice-=(attackTo.dice);
		
		g.grid[defending].assign(attacker,g.players[attacker]);
		g.grid[defending].dice=(attackFrom.dice-1);
		if(g.players[defender].territories.length==0) 
			g.eliminatePlayer(defender,attacker);
	}
	g.grid[attacking].dice=1;
	attackFrom.show();
	attackFrom.displayBorder(border_color);
	attackTo.displayBorder(border_color);
	attackTo.show();
	g.displayPlayers();
}
function	endTurn(gameNumber,pl)
{
	var g=games.gameData[gameNumber];
	var placed=g.reinforce(pl);
	clearArea(16,menuColumn,9);
	console.gotoxy(menuColumn,16);
	console.putmsg("\1r\1hPlaced " + placed.length + " reinforcements");
	mswait(1000);
	clearArea(16,menuColumn,9);
	for(var place in placed) 
		g.grid[placed[place]].show();
	g.takingTurn=false;
	games.storeGame(gameNumber);
}
function	forfeit(gameNumber,playerNumber)
{
	games.loadRankings();
	var g=games.gameData[gameNumber];
	var p=g.players[playerNumber];
	
	if(p.user > 0) {
		Log("forfeiting " + system.username(g.players[playerNumber].user) + " in game " + gameNumber);
		scores[p.user].losses+=1;
		scores[p.user].score+=settings.forfeitPoints;
	}
	else {
		Log("forfeiting " + g.players[playerNumber].AI.name + " in game " + gameNumber);
	}
	
	if(g.singlePlayer && p.user > 0) 
		file_remove(g.fileName);
	else {
		var activePlayers=g.countActivePlayers();
		if(activePlayers.length==2) {
			g.status=0;
			for(player in activePlayers) {
				var ply=activePlayers[player];
				if(g.players[ply].user!=p.user)	{
					g.winner=g.players[ply].user;
					if(g.winner>=0) {
						scores[g.winner].score+=settings.winPointsMulti;
						scores[g.winner].wins+=1;
					}
					break;
				}
			}
			if(g.singlePlayer) 
				file_remove(g.fileName);
		}
		else {
			delete g.users[p.user];
			g.players[playerNumber].AI.name=system.username(p.user)+" AI";
			g.players[playerNumber].user=-1;
		}
		games.storeGame(gameNumber);
	}
	g.displayPlayers();
	games.storeRankings();
	return true;
}
//MAIN GAMEPLAY FUNCTION
function	playGame(gameNumber)
{
	console.clear();
	var g=games.gameData[gameNumber];
	var userInGame;
	var currentPlayer;
	g.redraw();
	
	var pMenu=new Menu(		""							,1,1,"\1n\1g","\1g\1h");
	var pmenu_items=[		"~Take Turn"					, 
							"~Attack"						,
							"~Forfeit"						,
							"~End"							,
							"~Redraw"						,
							"~Quit"							];
	pMenu.add(pmenu_items);	
	pMenu.disable(["A","E","T","F"]);
	clearArea(16,menuColumn,9);

	while(1) {
		if(g.users[user.number]>=0) {
			userInGame=true;
			currentPlayer=g.users[user.number];
		}
		else {
			currentPlayer=-1;
			userInGame=false;
		}
		var turn=g.turnOrder[g.nextTurn];
		while(g.players[turn].user<0 && userInGame && g.status==1) {
			var name=g.players[turn].AI.name;
			if(g.hiddenNames) {
				name="Player " + (parseInt(turn,10)+1);
			}
			clearLine(1,48);
			console.gotoxy(2,1);
			console.putmsg("\1r\1hPlease wait. " + name + " taking turn.");
			mswait(750);
			g.players[turn].AI.turns=0;
			g.players[turn].AI.moves=0;
			while(g.canAttack(turn)) {
				if(!takeTurnAI(gameNumber,turn))
					break;
			}
			g.takingTurn=true;
			endTurn(gameNumber,turn);
			g.displayPlayers();
			turn=g.turnOrder[g.nextTurn];
		}
		if(!g.takingTurn) {
			clearArea(16,50,8);
			console.gotoxy(51,16);
			console.putmsg("\1r\1hGame \1n\1r#\1h" + gameNumber);
			if(g.status==0)	{
				pMenu.disable(["A","E","T","F"]);
				pMenu.enable(["Q"]);
				showWinner(g);
			}
			else if(turn==currentPlayer) 
				pMenu.enable("T");
			else  {
				var name=getUserName(g.players[turn],turn);
				if(g.hiddenNames) {
					name="Player " + (parseInt(turn,10)+1);
				}
				console.gotoxy(51,18);
				console.putmsg("\1r\1hIt is " + name + "'s turn");
				var daysOld=g.findDaysOld();
				var hoursOld=parseInt(daysOld*24,10);
				if(daysOld>0) {
					console.gotoxy(51,19);
					console.putmsg("\1r\1hLast turn taken " + hoursOld + " hours ago");
				}
			}
		}
		pMenu.displayHorizontal();
		var cmd=console.getkey(K_NOECHO|K_NOCRLF|K_UPPER);
		wipeCursor("right");
		if(pMenu.items[cmd] && pMenu.items[cmd].enabled) {
			switch(cmd)	{
			case "T":
				pMenu.enable(["F"]);
				pMenu.disable(["Q","T"]);
				g.takingTurn=true;
				if(g.canAttack(currentPlayer))
					pMenu.enable("A");
				pMenu.enable("E");
				continue;
			case "F":
				console.home();
				if(console.noyes("\1n\1gforfeit this game?")) {
					g.displayPlayers();
					continue;
				}
				forfeit(gameNumber,currentPlayer);
				pMenu.disable(["A","F","E"]);
				pMenu.enable(["Q"]);
				g.displayPlayers();
				break; 
			case "A":
				if(!attack(gameNumber,currentPlayer)) 
					continue;
 				g.displayPlayers();
				if(!g.canAttack(currentPlayer)) 
					pMenu.disable(["A"]);
				break;
			case "E":
				pMenu.disable(["A","E","F"]);
				pMenu.enable(["Q"]);
				g.takingTurn=false;
				endTurn(gameNumber,currentPlayer);
				g.displayPlayers();
				break;
			case "R":
				console.clear();
				g.redraw();
				continue;
			case "Q": 
				return;
			default:
				wipeCursor("right");
				continue;
			}
		}
	}
}
function 	takeTurnAI(gameNumber,playerNumber)
{
	var g=games.gameData[gameNumber];
	var computerPlayer=g.players[playerNumber];
	var targets=[];
	
	/* if we are down to two players */
	if(g.countActivePlayers().length == 2) {
		var perc = computerPlayer.countTerritory() / g.mapSize;
		/* if this ai occupies less it's preferred threshold, forfeit the game */
		if(perc < AIForfeitValues[computerPlayer.AI.forfeit]) {
			forfeit(gameNumber,playerNumber);
			return false;
		}
	}

	/* For each owned territory */
	for(var territory in computerPlayer.territories) {
		var base=computerPlayer.territories[territory];
		/* If we have enough to attack */
		if(g.grid[base].dice>1)	{
			/* Find places we can attack */
			var attackOptions=g.canAttack(playerNumber,base,computerPlayer,g);
			if(attackOptions!==false) {
				var basetargets=[];

				/* Randomize the order to check in */
				attackOptions.sort(RandomSort);
				for(var option in attackOptions) {
					var target=attackOptions[option];
					/* Check if this is an acceptable attack */
					if(AICheckFunctions[computerPlayer.AI.check](gameNumber, playerNumber, base, target))
						basetargets.push({gameNumber:gameNumber, target:target, base:base, target_grid:g.grid[target], base_grid:g.grid[base]});
				}
				/* If we found acceptable attacks, sort them and choose the best one */
				if(basetargets.length > 0) {
					basetargets.sort(AISortFunctions[computerPlayer.AI.sort]);
					targets.push(basetargets.shift());
				}
			}
		}
	}
	/* Randomize the targets array */
	targets.sort(RandomSort);
	var attackQuantity=AIQtyFunctions[computerPlayer.AI.qty](targets.length);
	if(attackQuantity < 1)
		return false;
	targets.sort(AISortFunctions[computerPlayer.AI.sort]);
	for(var attackNum=0;attackNum<attackQuantity;attackNum++) {
		var attackFrom=g.grid[targets[attackNum].base];
		var attackTo=g.grid[targets[attackNum].target];
		if(attackFrom.dice>1 && attackTo.player!=playerNumber) {
			battle(attackFrom,attackTo,gameNumber);
			computerPlayer.AI.moves++;
		}
	}
	computerPlayer.AI.turns++;
	return true;
}

//#######################MAIN GAME CLASS###################################
function	GameSettings()
{
	//hardcoded defaults (in case of missing settings file)
	this.pointsToWin=100;
	this.minScore=-2;
	this.maxGames=100;
	this.maxPerPlayer=30;
	this.maxDice=8;
	this.logEnabled=false;
	this.maxPlayers=7;
	this.minPlayers=3;
	this.killPointsSolo=1;
	this.killPointsMulti=2;
	this.deathPointsSolo=-3;
	this.deathPointsMulti=-2;
	this.forfeitPoints=-1
	this.winPointsSolo=2;
	this.winPointsMulti=4;
	this.skipDays=2;
	this.abortDays=10;
	this.keepGameData=2;
	
	this.load=function()
	{
		var sfile=new File(root + settingsfile);
		if(!file_exists(sfile.name)) 
			return;
		
		sfile.open('r',true);
		this.logEnabled=		sfile.iniGetValue(null,"enablelogging");
		this.pointsToWin=		parseInt(sfile.iniGetValue(null,"pointstowin"),10);
		this.minScore=			parseInt(sfile.iniGetValue(null,"minscore"),10);
		this.maxGames=			parseInt(sfile.iniGetValue(null,"maxgames"),10);
		this.maxPerPlayer=		parseInt(sfile.iniGetValue(null,"maxperplayer"),10);
		this.minPlayers=		parseInt(sfile.iniGetValue(null,"minplayers"),10);
		this.maxPlayers=		parseInt(sfile.iniGetValue(null,"maxplayers"),10);
		this.maxDice=			parseInt(sfile.iniGetValue(null,"maxdice"),10);
		this.killPointsSolo=	parseInt(sfile.iniGetValue(null,"killpointssolo"),10);
		this.killPointsMulti=	parseInt(sfile.iniGetValue(null,"killpointsmulti"),10);
		this.deathPointsSolo=	parseInt(sfile.iniGetValue(null,"deathpointssolo"),10);
		this.deathPointsMulti=	parseInt(sfile.iniGetValue(null,"deathpointsmulti"),10);
		this.forfeitPoints=		parseInt(sfile.iniGetValue(null,"forfeitpoints"),10);
		this.winPointsSolo=		parseInt(sfile.iniGetValue(null,"winpointssolo"),10);
		this.winPointsMulti=	parseInt(sfile.iniGetValue(null,"winpointsmulti"),10);
		this.SkipDays=			parseInt(sfile.iniGetValue(null,"skipdays"),10);
		this.abortDays=			parseInt(sfile.iniGetValue(null,"abortdays"),10);
		this.keepGameData=		parseInt(sfile.iniGetValue(null,"keepgamedata"),10);
		sfile.close();
	}
	this.load();
}
function	GameStatusInfo()
{
	this.gameData=[]; //master game data array
	this.inProgress=[];
	this.notFull=[];
	this.completed=[];
	this.youWin=[];
	this.yourGames=[];
	this.yourTurn=[];
	this.eliminated=[];
	this.singleGames=[];

	
	//TODO: REWORK SCOREFILE CODE....BECAUSE IT SUCKS
	this.storeRankings=function()
	{
		Log("storing rankings");
		var sfilename=root+scorefile;
		var sfile=new File(sfilename);
		if(!Locked(scorefile,true)) {
			Lock(scorefile);
			sfile.open((file_exists(sfilename)?'r+':'w+'), true);
			for(var s in scores) {
				var score=scores[s];
				var points=score.score>=settings.minScore?score.score:settings.minScore;
				sfile.iniSetValue(s,"score",points);
				sfile.iniSetValue(s,"kills",score.kills);
				sfile.iniSetValue(s,"wins",score.wins);
				sfile.iniSetValue(s,"losses",score.losses);
			}
			sfile.close();
			Unlock(scorefile);
		}
	}
	this.loadRankings=function()
	{
		Log("loading rankings");
		var sfilename=root+scorefile;
		if(file_exists(sfilename)) {
			var lfile=new File(sfilename);
			lfile.open('r',true);
			var plyrs=lfile.iniGetSections();
			for(var p=0;p<plyrs.length;p++) {
				var player=plyrs[p];
				var score=parseInt(lfile.iniGetValue(player,"score"),10);
				var kills=parseInt(lfile.iniGetValue(player,"kills"),10);
				var wins=parseInt(lfile.iniGetValue(player,"wins"),10);
				var losses=parseInt(lfile.iniGetValue(player,"losses"),10);
				scores[player]={'score':score,'kills':kills,'wins':wins,'losses':losses};
				if(score>=settings.pointsToWin) {	
					lfile.close();
					file_remove(lfile.name);
					this.winRound(player);
					scores=[];
					break;
				}
			}
			lfile.close();
		}
	}
	this.winRound=function(player)
	{	
		var hfilename=root+halloffame;
		var hfile=new File(hfilename);
		hfile.open('a');
		hfile.writeln(" \1w\1h" + system.datestr() + "\1n: \1y" + system.username(player));
		hfile.close();
	}
	this.hallOfFame=function()
	{	
		if(file_exists(root+halloffame)) {
			console.clear();
			console.putmsg("\1y\1hBBS DICE WARZ HALL OF FAME (Previous Round Winners)");
			console.crlf();
			console.printtail(root + halloffame,10);
			console.gotoxy(1,24);
			console.pause();
			console.aborted=false;
		}
	}
	this.loadGame=function(gamefile,gameNumber,lastModified)
	{
		Log("loading game " + gameNumber);
		humans=0;
		var gfile=new File(gamefile);
		gfile.open('r',true);
		var lgame;

		var hn=gfile.readln()==0?false:true;
		var status=parseInt(gfile.readln(),10);
		if(status<0) {
			var minp=parseInt(gfile.readln(),10);
			var maxp=parseInt(gfile.readln(),10);
			lgame=new NewGame(minp,maxp,gameNumber);
			lgame.lastModified=lastModified;
			lgame.hiddenNames=hn;
			lgame.fileName=gamefile;
			if(minp==maxp) lgame.fixedPlayers=true;
			
			for(var nnn=0;!gfile.eof && nnn<maxp;nnn++)	{
				userNumber=gfile.readln();
				if(userNumber===null || userNumber===undefined || userNumber==="") 
					break;
				else {
					var userNumber=parseInt(userNumber,10);
					var vote=gfile.readln();
					lgame.addPlayer(userNumber,parseInt(vote,10));
					if(userNumber>0 && !scores[userNumber]) 
						scores[userNumber]={'score':0,'kills':0,'wins':0,'losses':0};
				}
			}
			gfile.close();
			return lgame;
		}
		var np=parseInt(gfile.readln(),10);
		var ms=parseInt(gfile.readln(),10);
		var nt=parseInt(gfile.readln(),10);
		var r=parseInt(gfile.readln(),10);
		var c=parseInt(gfile.readln(),10);
		var pt=parseInt(gfile.readln(),10);

		lgame=new Map(c,r,np,gameNumber);
		lgame.lastModified=lastModified;
		lgame.fileName=gamefile;
		lgame.mapSize=ms;
		lgame.nextTurn=nt;
		lgame.status=status;
		lgame.playerTerr=pt;
		lgame.hiddenNames=hn;
		
		for(var to=0;to<np;to++) {
			var ttoo=parseInt(gfile.readln());
			lgame.turnOrder[to]=ttoo;
		}
		var aifile=new File(root + "ai.ini");
		aifile.open("r");
		for(var pl=0;pl<np;pl++) {
			var uname=gfile.readln();
			var u=-1;
			if(uname.search(/^[0-9]+$/) != -1)
				u=parseInt(uname,10);
			if(uname=='-1')
				uname='';
			res=parseInt(gfile.readln(),10);

			lgame.players[pl]=new Player(u,-1);
			lgame.players[pl].setColors(pl);
			lgame.players[pl].reserve=res;

			if(u>0) {
				lgame.users[u]=pl;
				humans++;
				if(!scores[u])
					scores[u]={'score':0,'kills':0,'wins':0,'losses':0};
			}
			else {
				/* Set up computer players */
				lgame.players[pl].AI.name=uname;
				lgame.players[pl].AI.sort=aifile.iniGetValue(lgame.players[pl].AI.name, "Sort", "Random");
				lgame.players[pl].AI.check=aifile.iniGetValue(lgame.players[pl].AI.name, "Check", "Random");
				lgame.players[pl].AI.qty=aifile.iniGetValue(lgame.players[pl].AI.name, "Quantity", "Random");
				lgame.players[pl].AI.forfeit=aifile.iniGetValue(lgame.players[pl].AI.name, "Forfeit", "Random");
				if(AISortFunctions[lgame.players[pl].AI.sort]==undefined)
					lgame.players[pl].AI.sort="Random";
				if(AICheckFunctions[lgame.players[pl].AI.check]==undefined)
					lgame.players[pl].AI.check="Random";
				if(AIQtyFunctions[lgame.players[pl].AI.qty]==undefined)
					lgame.players[pl].AI.qty="Random";
			}
		}
		aifile.close();
		if(humans<2) 
			lgame.singlePlayer=true;
		for(var sec=0;sec<ms;sec++) {
			var spot_player=parseInt(gfile.readln(),10);
			var spot_index=parseInt(gfile.readln(),10);
			var spot_dice=parseInt(gfile.readln(),10);
			lgame.grid[spot_index]=new Territory(spot_index);
			lgame.used[spot_index]=true;
			lgame.grid[spot_index].assign(spot_player,lgame.players[spot_player]);
			lgame.players[spot_player].territories.push(spot_index);
			lgame.grid[spot_index].dice=spot_dice;
		}
		lgame.setGrid();
		lgame.setEliminated();
		lgame.countDiceAll();
		gfile.close();
		return lgame;
	}
	this.storeGame=function(gameNumber)
	{
		var g=this.gameData[gameNumber];
		var gamefullname=getFileName(gameNumber);
		var gfile=new File(gamefullname);
		gfile.open('w+',false);
		
		gfile.writeln(g.hiddenNames?1:0);
		gfile.writeln(g.status);
		if(g.status<0) {
			gfile.writeln(g.minPlayers);
			gfile.writeln(g.maxPlayers);
			for(var nnn=0;nnn<g.players.length;nnn++) {
				gfile.writeln(g.players[nnn].user);
				gfile.writeln(g.players[nnn].vote);
			}
			gfile.close();
			return;
		}
		gfile.writeln(g.maxPlayers);
		gfile.writeln(g.mapSize);
		gfile.writeln(g.nextTurn);
		gfile.writeln(g.rows);
		gfile.writeln(g.columns);
		gfile.writeln(g.playerTerr);
		for(var to=0;to<g.maxPlayers;to++)
			gfile.writeln(g.turnOrder[to]);
		for(var ply in g.players) {
			var p=g.players[ply];
			if(p.user==-1)
				gfile.writeln(p.AI.name);
			else
				gfile.writeln(p.user);
			gfile.writeln(p.reserve);
		}	
		for(var sector in g.used) {
			var location=sector;
			gfile.writeln(g.grid[location].player);
			gfile.writeln(g.grid[location].location);
			gfile.writeln(g.grid[location].dice);
		} 
		gfile.close();
	}	
	this.updateGames=function()
	{
		var u=false;
		for(var gd in this.gameData) {
			var fileName=this.gameData[gd].fileName;
			var lastModified=file_date(fileName);

			if(file_exists(fileName)) {
				if(lastModified>this.gameData[gd].lastModified) {
					this.gameData[gd]=this.loadGame(fileName,gd,lastModified);
					u=true;
				}
			}
			else {
				delete this.gameData[gd];
				u=true;
			}
		}
		if(u) 
			this.filterData();
	}
	this.sortArray=function(data)
	{
		var numItems=data.length;
		for(var n=0;n<numItems;n++) {
			for(var m = 0; m < (numItems-1); m++) {
				if(parseInt(data[m],10) > parseInt(data[m+1],10)) {
					var holder = data[m+1];
					data[m+1] = data[m];
					data[m] = holder;
				}
			}
		}
		return data;
	}
	this.filterData=function()
	{
		Log("sorting game data");
		this.singleGames=[]; 
		this.inProgress=[];
		this.notFull=[];
		this.completed=[];
		this.youWin=[];
		this.yourGames=[];
		this.yourTurn=[];
		this.eliminated=[];
		
		for(var ggg in this.gameData) {
			var gm=this.gameData[ggg];
			if(!file_exists(gm.fileName)) {
				Log("game file missing, removing data: " + gm.filename);
				delete this.gameData[ggg];
			}
			else {
				var	playerNumber=gm.users[user.number];
				if(gm.users[user.number]>=0) {
					if(gm.status<0) 
						this.notFull.push(ggg);
					if(gm.singlePlayer) 
						this.singleGames.push(ggg);
					this.yourGames.push(ggg);
					if(gm.status==0) {
						this.completed.push(ggg);
						if(gm.winner==user.number) 
							this.youWin.push(ggg);
					}
					else if(gm.status>0) {
						this.inProgress.push(ggg);
						if(gm.turnOrder[gm.nextTurn]==playerNumber || gm.singlePlayer) 
							this.yourTurn.push(ggg);
						else {
							var nextTurn=gm.nextTurn;
							while(gm.players[gm.turnOrder[nextTurn]].user<0) {
								nextTurn++;
								if(nextTurn==gm.maxPlayers) 
									nextTurn=0;
							}
							if(gm.turnOrder[nextTurn]==playerNumber) 
								this.yourTurn.push(ggg);
						}
					}
					if(gm.players[playerNumber].eliminated===true) 
						this.eliminated.push(ggg);
				}
				else {
					if(gm.status<0) 
						this.notFull.push(ggg);
					else if(!gm.singlePlayer) {
						if(gm.status==0) {
							this.completed.push(ggg);
							if(gm.winner>=0) {
								if(gm.winner==user.number) 
									this.youWin.push(ggg);
							}
						}
						else 
							this.inProgress.push(ggg);
					}
				}
			}
		}
	}
	this.loadGames=function()
	{	
		var open_list=directory(root + "game*.dat"); 	
		if(open_list.length) {
			for(var lg in open_list) {
				var temp_fname=file_getname(open_list[lg]);
				var lastModified=file_date(open_list[lg]);
				var daysOld=(time()-lastModified)/daySeconds;
				var gameNumber=parseInt(temp_fname.substring(4,temp_fname.indexOf(".")),10);
				var lgame=this.loadGame(open_list[lg],gameNumber,lastModified);
				this.gameData[gameNumber]=lgame;
			}
		}
		this.filterData();
		this.updatePlayers();
		this.deleteOld();
	}
	this.deleteOld=function()
	{	
		Log("deleting old game data");
		for(var oldgame in this.gameData) {
			var daysOld=(time()-this.gameData[oldgame].lastModified)/daySeconds;
			if(this.gameData[oldgame].singlePlayer===true && daysOld>=settings.keepGameData) {
				file_remove(this.gameData[oldgame].fileName);
				delete this.gameData[oldgame];
			}
		}
		for(var completed in this.completed) {
			var gm=this.completed[completed];
			/* skip games that have already been deleted */
			if(!this.gameData[gm])
				continue;
			var daysOld=(time()-this.gameData[gm].lastModified)/daySeconds;
			if(this.gameData[gm].singlePlayer===true || daysOld>=settings.keepGameData)	{
				file_remove(this.gameData[gm].fileName);
				delete this.gameData[gm];
			}
		}
	}
	this.updatePlayers=function()
	{
		Log("updating players");
		for(var inp in this.inProgress) {
			var gm=this.gameData[this.inProgress[inp]];
			if(gm) {
				var daysOld=(time()-gm.lastModified)/daySeconds;
				if(settings.abortDays>0 && daysOld>=settings.abortDays) {
					Log("removing expired game: " + gm.fileName);
					file_remove(gm.fileName);
				}
				else if(settings.skipDays>0 && daysOld>=settings.skipDays && !gm.singlePlayer) {
					var nextTurnPlayer=gm.turnOrder[gm.nextTurn];
					deliverForfeitMessage(gm.players[nextTurnPlayer].user,gm.gameNumber);
					forfeit(gm.gameNumber,nextTurnPlayer);
				}
			}
		}
	}
	this.init=function()
	{
		this.loadRankings();
		this.loadGames();
		this.storeRankings();
	}
}
function	Log(txt)
{
	if(settings.logEnabled) 
		log(LOG_DEBUG,txt);
}

splashStart();
gameMenu();
splashExit();
