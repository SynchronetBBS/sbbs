/*
	BUBBLE BOGGLE 
	for Synchronet v3.15+ (javascript)
	by Matt Johnson (2009)
	
	Game uses standard "Big Boggle" rules, scoring, and dice
	Dictionary files created from ENABLe gaming dictionary
	
	for customization or installation help contact:
	Matt Johnson ( MCMLXXIX@BBS.THEBROKENBUBBLE.COM )
*/

var game_id = "boggle";

load("graphic.js");
load("sbbsdefs.js");
load("funclib.js");
load("calendar.js");
load(root + "timer.js");
 
var word_value={
	4:1,
	5:2,
	6:3,
	7:5,
	8:11,
	9:15
};
var max_future=2;
var current;
var today;
var player;
var input;
var info;
var wordlist;
var playing_game=false;
var clearalert=false;
var calendar = new Calendar(58,4,"\1y","\0012\1g\1h");
var lobby = new Lobby(1,1);
var useralias = user.alias.replace(/\./g,"_");
var date = new Date();
var data = new GameData();

function boggle() {
	function init()
	{
		player=findUser(useralias);
		current=calendar.selected;
		today=calendar.selected;
		updateCalendar();
	}
	function updateCalendar()
	{
		for(var d in player.days){
			calendar.highlights[player.days[d]]=true;
		}
	}
	function changeDate()
	{
		showMessage("\1r\1hJump to which date?\1n\1r: ");
		var newdate=console.getnum(calendar.daysinmonth);
		if(newdate>0) {
			if(newdate>today+max_future) {
				showMessage("\1r\1hYou cannot play more than " + max_future + " days ahead");
				return false;
			}
			calendar.selected=newdate;
			calendar.drawDay(current);
			current=newdate;
			calendar.drawDay(current,calendar.sel);
			showMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
		}
		showMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
	}
	function browseCalendar(k)
	{
		showMessage("\1r\1hUse Arrow keys to change date and [\1n\1rEnter\1h] to select");
		if(calendar.selectDay(k)) {
			if(calendar.selected>today+max_future) {
				showMessage("\1r\1hYou cannot play more than " + max_future + " days ahead");
				calendar.drawDay(calendar.selected);
				calendar.selected=current;
				calendar.drawDay(current,calendar.sel);
				return false;
			}
			current=calendar.selected;
			showMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
		}
	}
	function showLobby()
	{
		lobby.draw();
		console.gotoxy(58,2);
		console.putmsg(centerString("\1c\1h" + date.getMonthName() + "\1n\1c - \1c\1h" + date.getFullYear(),21));
	}
	function showMessage(txt)
	{
		console.gotoxy(26,21);
		console.putmsg("\1-" + (txt?txt:""),P_SAVEATR);
		console.cleartoeol(ANSI_NORMAL);
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		showLobby();
		showScores();
		calendar.draw();
	}
	function main()
	{
		redraw();
		mainloop:
		while(1){
			if(data.updated) {
				data.updated=false;
				showScores();
			}
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k){
				switch(k.toUpperCase())	{	
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						browseCalendar(k);
						break;
					case "\x1b":	
					case "Q":
						return;
					case "P":
						for(var d in player.days) {
							if(player.days[d]==parseInt(current,10)) {
								showMessage("\1r\1hYou have already played this date");
								continue mainloop;
							}
						}
						playGame();
						updateCalendar();
						redraw();
						break;
					case "R":
						redraw();
						break;
					case "C":
						changeDate();
						break;
					default:
						break;
				}
			}
			cycle();
		}
	}
	function playGame()
	{
		console.clear(ANSI_NORMAL);
		var gameNumber=parseInt(current,10);
		var game=new GameBoard(gameNumber,data.boards[gameNumber]);
		playing_game=true;
		
		function play()
		{
			redraw();
			while(1){
				if(!info.cycle()) endGame();
				var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
				if(k){
					switch(k.toUpperCase()){
						case "\x1b":	
							if(!playing_game)
								return;
							notify("\1r\1hEnd this game? [N,y]: ");
							if(console.getkey().toUpperCase()=="Y") {
								endGame();
								return;
							} 
							input.clear();
							break;
						case "\r":
						case "\n":
							if(input.buffer.length) 
								submitWord();
							break;
						case KEY_UP:
						case KEY_DOWN:
						case KEY_LEFT:
						case KEY_RIGHT:
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case 7:
						case 8:
						case 9:
						case 0:
							break;
						case "?":
							redraw();
							break;
						default:
							if(playing_game) 
								input.add(k);
							else 
								notify("\1r\1hThis game has ended");
							break;
					}
				}
				cycle();
			}
		}
		function notify(msg)
		{
			input.draw(msg);
			clearalert=true;
		}
		function submitWord()
		{
			var word=truncsp(input.buffer);
			input.clear();
			game.clearSelected();
			
			if(word.length>3){
				if(findWord(word)){
					game.drawSelected(false);
					notify("\1r\1h Word already entered");
				}
				else if(checkWord(word)) {
					AddPoints(word);
					AddWord(word);
					info.draw();
				}
			}
			else{
				notify("\1r\1h Word minimum: 4 letters");
			}
		}
		function findWord(word)
		{
			for(w in wordlist.words){
				if(word==wordlist.words[w]) return true;
			}
			return false;
		}
		function checkWord(word)
		{
			if(word.length<4 || word.length>25 || word.indexOf(" ")>=0) return false;
			if(!ValidateWord(word))	{
				notify("\1r\1h Cannot make word");
				return false;
			}
			
			var firstletter=word.charAt(0);
			var filename=root + "dict/" + firstletter + ".dic";
			var dict=new File(filename);
			dict.open('r+',true);
			
			if(!scanDictionary(word,0,dict.length,dict)) {
				game.drawSelected(false);
				notify("\1r\1h Word invalid");
				dict.close();
				return false;
			}
			
			dict.close();
			game.drawSelected(true);
			return true;
		}
		function ValidateWord(word)
		{
			var match=false;
			var grid=game.grid;
			for(var x=0;x<grid.length;x++){
				for(var y=0;y<grid[x].length;y++){
					var position=grid[x][y];
					var letter=word.charAt(0).toUpperCase();
					if(position.letter==letter)	{
						position.selected=true;	

						var start_index=1;
						if(letter=="Q" && word.charAt(1).toUpperCase()=="U") start_index=2;
						var coords={'x':x,'y':y};
						if(RouteCheck(coords,word.substr(start_index),grid)) match=true;
						else position.selected=false;
					}
					if(match) break;
				}
				if(match) break;
			}
			return match;
		}
		function RouteCheck(position,string,grid)
		{
			var letter=string.charAt(0).toUpperCase();
			var checkx=[	0,	1,	1,	1,	0,	-1,	-1,	-1	];
			var checky=[	-1,	-1,	0,	1,	1,	1,	0,	-1	];
			var match=false;

			for(var i=0;i<8;i++){
				var posx=position.x+checkx[i];
				var posy=position.y+checky[i];
				
				if(posx>=0 && posx<5 && posy>=0 && posy<5){
					var checkposition=grid[posx][posy];
					if(checkposition.letter==letter){
						if(!checkposition.selected)	{
							var start_index=1;
							checkposition.selected=true;
							if(string.length==1) {
								return true;
							}
							var coords={'x':posx,'y':posy};
							if(letter=="Q" && string.charAt(1).toUpperCase()=="U") {
								start_index=2;
							}
							if(RouteCheck(coords,string.substr(start_index),grid)) {
								match=true;
							}
							else checkposition.selected=false;
						}
					}
				}
			}
			return match;
		}
		// Linear search of dictionary (not recursive, not fast)
		function scanDictionary(word,lower,upper,dict)
		{
			dict.rewind();
			while(!dict.eof) {
				if(word == dict.readln())
					return true;
			}
			return false;
		}
		function scanDictionaryFast(word,lower,upper,dict)
		{
			middle=parseInt(lower+((upper-lower)/2),10);
			dict.position=middle-25;
			var checkword=dict.readln();
			var bol = dict.position;
			while(dict.position<=middle) {
				checkword=dict.readln();
				bol = dict.position;
			}
			var comparison=word.localeCompare(checkword);
			if(comparison<0) {
				return scanDictionary(word,lower,bol,dict);
			}
			if(dict.position>upper || upper-lower<2)  {
				return false;
			}
			else if(comparison>0){
				return scanDictionary(word,bol,upper,dict);
			}
			else return true;
		}
		function AddPoints(word)
		{
			var length=word.length;
			if(length>9) length=9;
			info.score.points+=word_value[length];
		}
		function AddWord(word)
		{
			info.score.words++;
			wordlist.add(word);
		}
		function init()
		{
			info=new InfoBox(62,1);
			input=new InputLine(32,22);
			wordlist=new WordList(31,4);
			info.init(player);
		}
		function redraw()
		{
			game.draw();
			info.draw();
			wordlist.draw();
		}
		function endGame()
		{
			if(!playing_game) 
				return;
			var p=data.players[useralias];
			var points=info.score.points;
			p.points+=points;
			p.days.push(parseInt(current,10));
			data.storePlayer();
			playing_game=false;
		}
		init();
		play();
	}
	function showScores()
	{
		var posx=3;
		var posy=6;
		var index=0;
		var count=0;
		
		var scores=sortScores();
		for(var s=0;s<scores.length;s++) {
			if(scores[s].name == useralias) {
				index = (s-5);
				if(index < 0)
					index = 0;
				break;
			}
		}
		
		for(var i=0;i<11;i++) {
			var score=scores[index + i];
			if(!score) {
				break;
			}
			else if(score.points>0) {
				if(score.name==useralias) 
					console.attributes=LIGHTGREEN;
				else 
					console.attributes=GREEN;
				console.gotoxy(posx,posy+count);
				console.putmsg(score.name,P_SAVEATR);
				console.right(23-score.name.length);
				console.putmsg(printPadded(score.points,5,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(getAverage(score.name),P_SAVEATR);
				console.right(3);
				console.putmsg(printPadded(getDayCount(score.name),4,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(printPadded(formatDate(score.laston),8,undefined,"right"),P_SAVEATR);
				count++;
			}
		}
		
		if(data.winner && data.winner.name !== undefined && data.winner.points > 0)	{
			console.gotoxy(48,18);
			console.putmsg("\1c\1h" + data.winner.name);
			console.gotoxy(75,18);
			console.putmsg("\1c\1h" + data.winner.points);
		}
	}
	function sortScores()
	{
		var sorted=[];
		var index=0;
		for(var p in data.players) {
			sorted.push(data.players[p]);
		}
		var numScores=sorted.length;
		for(n=0;n<numScores;n++) {
			for(m = 0; m < (numScores-1); m++) {
				if(sorted[m].points < sorted[m+1].points) {
					holder = sorted[m+1];
					sorted[m+1] = sorted[m];
					sorted[m] = holder;
				}
			}
		}
		return sorted;
	}
	function formatDate(timet) 
	{
		if(!timet)
			return ("never");
		var date=new Date(timet*1000);
		var m=date.getMonth()+1;
		var d=date.getDate();
		var y=date.getFullYear()-2000; //assuming no one goes back in time to 1999 to play this
		
		if(m<10) m="0"+m;
		if(d<10) d="0"+d;
		if(y<10) y="0"+y;
		
		return (m + "/" + d + "/" + y);
	}
	function getAverage(alias)
	{
		var p=findUser(alias);
		var count=getDayCount(alias);
		var avg=p.points/count;
		if(avg>0) {
			if(avg<10) return("0"+avg.toFixed(1));
			return avg.toFixed(1);
		}
		return 0;
	}
	function getDayCount(alias)
	{
		var p=findUser(alias);
		var count=0;
		for(var d in p.days) count++;
		return count;
	}
	function findUser(alias)
	{
		return data.players[alias];
	}
	
	init();
	main();
}
function cycle() {
	client.cycle();
}
function processUpdates(update) {
	var p=update.location.split(".");
	/* if we received something for a different game? */
	if(p.shift().toUpperCase() != "BOGGLE") 
		return;
	var obj=data;
	while(p.length > 1) {
		var child=p.shift();
		obj=obj[child];
	}
	obj[p.shift()] = update.data;
	data.updated=true;
}
function open() {
	client.callback=processUpdates;
	client.subscribe(game_id,"players");
	data.players[useralias].laston = time();
	data.storePlayer();
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ";
	js.on_exit("bbs.sys_status = " + bbs.sys_status);
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status |= SS_PAUSEOFF;	
	if(file_exists(root + "boggle.bin")) {
		console.clear();
		var splash=new Graphic(80,22);
		splash.load(root + "boggle.bin");
		splash.draw();
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		while(console.inkey(K_NOECHO|K_NOSPIN)==="");
	}
	console.clear();
}
function close() {
	
	client.unsubscribe(game_id,"players");
	console.attributes=ANSI_NORMAL;
	console.clear();
	var splash_filename=root + "exit.bin";
	if(file_exists(splash_filename)) {
		var splash=new Graphic(80,21);
		splash.load(splash_filename);
		splash.draw();
		
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		while(console.inkey(K_NOECHO|K_NOSPIN)==="");
	}
	console.clear();
}

//GAME OBJECTS
function GameData() {
	this.updated=false;
	
	this.players={};
	this.boards={};
	this.winner={};
	this.month=date.getMonth();
	
	this.init=function() {
		client.lock(game_id,"month",2);
		var month = client.read(game_id,"month");
		if(month !== this.month) 
			this.newMonth();
		this.loadMonth();
		client.unlock(game_id,"month");
		
		if(!this.players)
			this.players = {};
		if(!this.players[useralias]) {
			this.players[useralias] = new Player();
			this.storePlayer();
		}
	}
	/* load this month's boggle boards */
	this.loadMonth=function() {
		console.putmsg("\rPlease wait. Loading puzzles for this month...\r\n");
		this.players=client.read(game_id,"players",1)
		this.winner=client.read(game_id,"winner",1);
		this.boards=client.read(game_id,"boards",1);
	}
	/* save current player data to database */
	this.storePlayer = function() {
		client.write(game_id,"players." + useralias,this.players[useralias],2);
	}
	/* save this round's winner to the database */
	this.storeRoundWinner=function() {
		this.players = client.read(game_id,"players",1);
		for each(var p in this.players) {
			if(this.winner.points == undefined) 
				this.winner=p;
			else if(p.points>this.winner.points) 
				this.winner=p;
		}
		client.write(game_id,"winner",this.winner,2);
	}
	/* generate a new month's list of boggle boards */
	this.newMonth=function() {
		this.storeRoundWinner();
		
		client.write(game_id,"month",this.month);
		client.remove(game_id,"players",2);
			
		console.putmsg("\rPlease wait. Creating puzzles for new month...\r\n");

		client.lock(game_id,"boards",2);
		client.remove(game_id,"boards");
		this.boards=[];
		var numdays=date.daysInMonth();
		for(var dn=1;dn<=numdays;dn++) {
			var board=this.newBoard();
			this.boards[dn]=board;
			client.write(game_id,"boards." + dn,board);
		}
		client.unlock(game_id,"boards");
	}
	/* generate a new boggle board */
	this.newBoard=function() {
		var dice=this.initDice();
		var shaken=new Array();
		for(var d=0;d<25;d++) {
			var spot=random(dice.length);
			shaken.push(dice.splice(spot,1).toString());
		}
		var grid=[];
		for(x=0;x<5;x++) {
			grid.push(new Array(5));
		}
		var die=0;
		for(var x=0;x<grid.length;x++) {
			for(var y=0;y<grid[x].length;y++) {
				var randletter=shaken[die].charAt(random(6));
				grid[x][y]=randletter;
				die++;
			}
		}
		return grid;
	}
	/* load dice values */
	this.initDice=function() {
		var dice=[];
		dice[0]="AAAFRS";
		dice[1]="AAEEEE";
		dice[2]="AAFIRS";
		dice[3]="ADENNN";
		dice[4]="AEEEEM";
		dice[5]="AEEGMU";
		dice[6]="AEGMNN";
		dice[7]="AFIRSY";
		dice[8]="BJKQXZ";
		dice[9]="CCENST";
		dice[10]="CEIILT";
		dice[11]="CEILPT";
		dice[12]="CEIPST";
		dice[13]="DDHNOT";
		dice[14]="DHHLOR";
		dice[15]="DHLNOR";
		dice[16]="DHLNOR";
		dice[17]="EIIITT";
		dice[18]="EMOTTT";
		dice[19]="ENSSSU";
		dice[20]="FIPRSY";
		dice[21]="GORRVW";
		dice[22]="IPRRRY";
		dice[23]="NOOTUW";
		dice[24]="OOOTTU";
		return dice;
	}

	this.init();
}

function Lobby(x,y) {
	this.graphic=new Graphic(80,24);
	this.graphic.load(root + "lobby.bin");
	this.x=x;
	this.y=y;
	
	this.draw=function() {
		this.graphic.draw(this.x,this.y);
	}
}

function Player(name,points,days,laston) {
	this.name=name?name:useralias;
	this.points=points?points:0;
	this.days=days?days:[];
	this.laston=laston?laston:time();
}

function InfoBox(x,y) {
	this.x=x;
	this.y=y;
	this.timer;
	this.score;
	
	this.init=function(player) {
		this.score=new Score();
		this.timer=new Timer(65,12,"\1b\1h");
		this.timer.init(180);
	}
	this.cycle=function() {
		if(this.timer.countdown>0) {
			var current=time();
			var difference=current-this.timer.lastupdate;
			if(difference>0) {
				this.timer.countDown(current,difference);
				this.timer.redraw();
			}
			return true;
		}
		return false;
	}
	this.draw=function() {
		this.timer.redraw();
		console.gotoxy(64,19);
		console.putmsg("\1y\1h" + player.name.toUpperCase());
		console.gotoxy(73,21);
		console.putmsg("\1y\1h" + this.score.words);
		console.gotoxy(73,22);
		console.putmsg("\1y\1h" + this.score.points);
	}
}

function GameBoard(gameNumber,grid) { 
	this.gameNumber=gameNumber;
	this.grid=grid;
	this.init();
}
GameBoard.prototype.init=function(x,y) {
	this.x=x;
	this.y=y;
	this.graphic=new Graphic(80,24);
	this.graphic.load(root + "board.bin");
	this.scanGraphic();
}
GameBoard.prototype.scanGraphic=function() {
	for(x=0;x<this.graphic.data.length;x++) {
		for(y=0;y<this.graphic.data[x].length;y++) {
			var location=this.graphic.data[x][y];
			if(location.ch=="@") {
				var id=this.graphic.data[x+1][y].ch;
				if(id=="D")	{
					this.dateline=new DateLine(x+1,y+1);
				}
				else{
					var posx=parseInt(this.graphic.data[x+1][y].ch,10);
					var posy=parseInt(this.graphic.data[x+2][y].ch,10);
					this.grid[posx][posy]=new LetterBox(x+1,y+1,this.grid[posx][posy]);
				}
				this.graphic.data[x][y].ch=" ";
				this.graphic.data[x+1][y].ch=" ";
				this.graphic.data[x+2][y].ch=" ";
			}
		}
	}
}
GameBoard.prototype.draw=function() {
	this.graphic.draw(this.x,this.y);
	console.gotoxy(2,2);
	console.putmsg(centerString(
		"\1c\1h" + calendar.date.getMonthName() + 
		" " + this.gameNumber + ", " + 
		calendar.date.getFullYear()
	,28));
	
	for(x=0;x<this.grid.length;x++) {
		for(y=0;y<this.grid[x].length;y++) {
			this.grid[x][y].draw();
		}
	}
}
GameBoard.prototype.drawSelected=function(valid) {
	for(x=0;x<this.grid.length;x++)	{
		for(y=0;y<this.grid[x].length;y++) {
			if(this.grid[x][y].selected) this.grid[x][y].draw(true,valid);
		}
	}
}
GameBoard.prototype.clearSelected=function() {
	for(x=0;x<this.grid.length;x++)	{
		for(y=0;y<this.grid[x].length;y++) {
			this.grid[x][y].selected=false;
			this.grid[x][y].draw();
		}
	}
}

function LetterBox(x,y,letter) {
	this.x=x;
	this.y=y;
	this.letter=letter;
	this.selected=false;
}
LetterBox.prototype.draw = function(selected,valid) {
	var letter=(this.letter=="Q"?"Qu":this.letter);
	var oldattr=console.attributes;
	if(selected){
		console.attributes=valid?LIGHTGREEN + BG_GREEN:LIGHTRED + BG_RED;
		console.gotoxy(this.x-2,this.y-1);
		console.putmsg("     ",P_SAVEATR);
		console.down();
		console.left(5);
		console.putmsg(centerString(letter,5),P_SAVEATR);
		console.down();
		console.left(5);
		console.putmsg("     ",P_SAVEATR);
	}
	else {
		console.gotoxy(this.x-2,this.y-1);
		console.attributes=BG_BROWN + YELLOW;
		console.putmsg("     ",P_SAVEATR);
		console.down();
		console.left(5);
		console.putmsg(centerString(letter,5),P_SAVEATR);
		console.down();
		console.left(5);
		console.putmsg("     ",P_SAVEATR);
	}
	console.attributes=oldattr;
}

function DateLine(x,y) {
	this.x=x;
	this.y=y;
	//this.date;
	this.draw=function()
	{
		console.gotoxy(this.x,this.y);
		console.putmsg(this.date);
	}
}

function MessageLine(x,y) {
	this.x=x;
	this.y=y;
	this.draw=function(txt)
	{
		console.gotoxy(this.x,this.y);
		console.putmsg(txt);
	}
}

function InputLine(x,y) {
	this.x=x;
	this.y=y;
	this.buffer="";
	this.add=function(letter)
	{
		if(clearalert) 
		{
			this.clear();
			clearalert=false;
		}
		if(letter=="\b")
		{
			if(!this.buffer.length) return;
			this.buffer=this.buffer.substring(0,this.buffer.length-1);
		}
		else
		{
			if(this.buffer.length==30) return;
			this.buffer+=letter.toLowerCase();
		}
		this.draw(this.buffer);
	}
	this.backSpace=function()
	{
		this.buffer=this.buffer.substring(0,this.buffer.length-2);
		this.draw();
		console.putmsg(" ");
	}
	this.clear=function()
	{
		this.buffer="";
		console.gotoxy(this.x,this.y);
		console.write("                              ");
	}
	this.draw=function(text)
	{
		console.gotoxy(this.x,this.y);
		console.putmsg(printPadded("\1n" + text,30));
	}
}

function WordList(x,y) {
	this.x=x;
	this.y=y;
	this.words=[];
	this.draw=function()
	{
		var longest=0;
		var x=this.x;
		var y=this.y;
		for(var w=0;w<this.words.length;w++)
		{
			if(w%17==0) 
			{
				x+=longest+1;
				longest=0;
			}
			if(this.words[w].length>longest) longest=this.words[w].length;
			console.gotoxy(x,y+(w%17));
			console.putmsg("\1g\1h" + this.words[w]);
		}
	}
	this.add=function(word)
	{
		this.words.push(word);
		this.draw();
	}
}

function Score() {
	this.points=0;
	this.words=0;
}

open();
boggle();
close();








