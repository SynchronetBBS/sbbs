/*
	BUBBLE BOGGLE 
	for Synchronet v3.15+ (javascript)
	by Matt Johnson (2009)
	
	Game uses standard "Big Boggle" rules, scoring, and dice
	Dictionary files created from ENABLe gaming dictionary
	
	for customization or installation help contact:
	Matt Johnson ( MCMLXXIX@MDJ.ATH.CX )
*/

var gameroot;
try { barfitty.barf(barf); } catch(e) { gameroot = e.fileName; }
gameroot = gameroot.replace(/[^\/\\]*$/,"");

load("graphic.js");
load("sbbsdefs.js");
load("helpfile.js");
load("funclib.js");
load("calendar.js");
load("commclient.js");
load(gameroot + "timer.js");
 
var stream=new ServiceConnection("boggle"); 
var oldpass=console.ctrl_key_passthru;

function boggle()
{
	var wordvalues=[];
	wordvalues[4]=1;
	wordvalues[5]=2;
	wordvalues[6]=3;
	wordvalues[7]=5;
	wordvalues[8]=11;
	
	var max_future=2;
	var calendar;
	var month;
	var current;
	var today;
	var lobby;
	var players;
	var player;
	var input;
	var info;
	var wordlist;
	var playing_game=false;
	var clearalert=false;
	
	function init()
	{
		calendar=new Calendar(58,4,"\1y","\0012\1g\1h");
		players=new PlayerList();
		player=players.findUser(user.alias);
		month=new MonthData();
		lobby=new Lobby(1,1);
		current=calendar.selected;
		today=calendar.selected;
	}
	function splashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		getFiles();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	}
	function splashExit()
	{
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		players.storePlayer();
		sendFiles();
		console.clear();
		var splash_filename=gameroot + "exit.bin";
		if(!file_exists(splash_filename)) exit();
		
		var splash_size=file_size(splash_filename);
		splash_size/=2;		
		splash_size/=80;	
		var splash=new Graphic(80,splash_size);
		splash.load(splash_filename);
		splash.draw();
		
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		while(console.inkey(K_NOECHO|K_NOSPIN)==="");
		console.clear();
		exit();
	}
	function updateCalendar()
	{
		for(var d in player.days)
		{
			calendar.highlights[player.days[d]]=true;
		}
	}
	function changeDate()
	{
		showMessage("\1r\1hJump to which date?\1n\1r: ");
		var newdate=console.getnum(calendar.daysinmonth);
		if(newdate>0) 
		{
			if(newdate>today+max_future)
			{
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
		if(calendar.selectDay(k)) 
		{
			if(calendar.selected>today+max_future)
			{
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
		console.putmsg(centerString("\1c\1h" + month.currentdate.getMonthName() + "\1n\1c - \1c\1h" + month.currentdate.getFullYear(),21));
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
		while(1)
		{
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				switch(k.toUpperCase())
				{
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						browseCalendar(k);
						break;
					case "\x1b":	
					case "Q":
						splashExit();
					case "P":
						for(var d in player.days)
						{
							if(player.days[d]==parseInt(current,10)) 
							{
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
		}
	}
	function playGame()
	{
		console.clear(ANSI_NORMAL);
		var game=month.games[parseInt(current,10)];
		playing_game=true;
		
		function play()
		{
			redraw();
			while(1)
			{
				if(!info.cycle()) endGame();
				var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
				if(k)
				{
					switch(k.toUpperCase())
					{
						case "\x1b":	
							if(playing_game)
							{
								notify("\1r\1hEnd this game? [N,y]: ");
								if(console.getkey().toUpperCase()=="N") break;
								input.clear();
								endGame();
							}
							return;
						case "\r":
						case "\n":
							if(input.buffer.length) submitWord();
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
							if(playing_game)	input.add(k);
							else notify("\1r\1hThis game has ended");
							break;
					}
				}
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
			game.board.clearSelected();
			
			if(word.length>3)
			{
				if(findWord(word))
				{
					game.board.drawSelected(false);
					notify("\1r\1h Word already entered");
				}
				else if(checkWord(word)) 
				{
					AddPoints(word);
					AddWord(word);
					info.draw();
				}
			}
			else
			{
				notify("\1r\1h Word minimum: 4 letters");
			}
		}
		function findWord(word)
		{
			for(w in wordlist.words)
			{
				if(word==wordlist.words[w]) return true;
			}
			return false;
		}
		function checkWord(word)
		{
			if(word.length<4 || word.length>25 || word.indexOf(" ")>=0) return false;
			if(!ValidateWord(word))
			{
				notify("\1r\1h Cannot make word");
				return false;
			}
			
			var firstletter=word.charAt(0);
			var filename=gameroot + "dict/" + firstletter + ".dic";
			var dict=new File(filename);
			dict.open('r+',true);
			
			if(!scanDictionary(word,0,dict.length,dict)) 
			{
				game.board.drawSelected(false);
				notify("\1r\1h Word invalid");
				dict.close();
				return false;
			}
			
			dict.close();
			game.board.drawSelected(true);
			return true;
		}
		function ValidateWord(word)
		{
			var match=false;
			var grid=game.board.grid;
			for(var x=0;x<grid.length;x++)
			{
				for(var y=0;y<grid[x].length;y++)
				{
					var position=grid[x][y];
					var letter=word.charAt(0).toUpperCase();
					if(position.letter==letter) 
					{
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

			for(var i=0;i<8;i++)
			{
				var posx=position.x+checkx[i];
				var posy=position.y+checky[i];
				
				if(posx>=0 && posx<5 && posy>=0 && posy<5)
				{
					var checkposition=grid[posx][posy];
					if(checkposition.letter==letter)
					{
						if(!checkposition.selected)
						{
							var start_index=1;
							checkposition.selected=true;
							if(string.length==1) 
							{
								return true;
							}
							var coords={'x':posx,'y':posy};
							if(letter=="Q" && string.charAt(1).toUpperCase()=="U")
							{
								start_index=2;
							}
							if(RouteCheck(coords,string.substr(start_index),grid)) 
							{
								match=true;
							}
							else checkposition.selected=false;
						}
					}
				}
			}
			return match;
		}
		function scanDictionary(word,lower,upper,dict)
		{
			middle=parseInt(lower+((upper-lower)/2),10);
			dict.position=middle-25;
			var checkword=dict.readln();
			while(dict.position<=middle) checkword=dict.readln();
			var comparison=word.localeCompare(checkword);
			if(comparison<0) 
			{
				return scanDictionary(word,lower,middle,dict);
			}
			if(dict.position>upper || upper-lower<2) return false;
			else if(comparison>0)
			{
				return scanDictionary(word,middle,upper,dict);
			}
			else return true;
		}
		function AddPoints(word)
		{
			var length=word.length;
			if(length>8) length=8;
			info.score.points+=wordvalues[length];
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
			if(!playing_game) return;
			var p=players.players[user.alias];
			var points=info.score.points;
			p.points+=points;
			p.days.push(parseInt(current,10));
			players.storePlayer();
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
		
		var scores=sortScores();
		for(var s in scores)
		{
			var score=scores[s];
			var plays=players.getDayCount(score.name);
			if(plays>0)
			{
				if(score.name==user.alias) console.attributes=LIGHTGREEN;
				else console.attributes=GREEN;
				console.gotoxy(posx,posy+index);
				console.putmsg(score.name,P_SAVEATR);
				console.right(23-score.name.length);
				console.putmsg(printPadded(score.points,5,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(players.getAverage(score.name),P_SAVEATR);
				console.right(3);
				console.putmsg(printPadded(plays,4,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(printPadded(players.formatDate(score.laston),8,undefined,"right"),P_SAVEATR);
				index++;
			}
		}
		if(month.winner)
		{
			console.gotoxy(48,18);
			console.putmsg("\1c\1h" + month.winner.name);
			console.gotoxy(75,18);
			console.putmsg("\1c\1h" + month.winner.points);
		}
	}
	function sortScores()
	{
		var sorted=[];
		for(var p in players.players)
		{
			var s=players.players[p];
			var plays=players.getDayCount(s.name);
			sorted.push(s);
		}
		var numScores=sorted.length;
		for(n=0;n<numScores;n++)
		{
			for(m = 0; m < (numScores-1); m++) 
			{
				if(sorted[m].points < sorted[m+1].points) 
				{
					holder = sorted[m+1];
					sorted[m+1] = sorted[m];
					sorted[m] = holder;
				}
			}
		}
		return sorted;
	}
	
	splashStart();
	init();
	main();
	splashExit();
	
	//GAME OBJECTS
	function Lobby(x,y)
	{
		this.graphic=new Graphic(80,24);
		this.graphic.load(gameroot + "lobby.bin");
		this.x=x;
		this.y=y;
		
		this.draw=function()
		{
			this.graphic.draw(this.x,this.y);
		}
	}
	function PlayerList()
	{
		this.file=new File(gameroot + "players.ini");
		this.players=[];
		this.init=function()
		{
			this.file.open(file_exists(this.file.name)?'r+':'w+',true);
			if(!this.file.iniGetValue(user.alias,"name"))
			{
				this.file.iniSetObject(user.alias,new Player());
			}
			var plyrs=this.file.iniGetAllObjects();
			this.file.close();
			for(p in plyrs)
			{
				var plyr=plyrs[p];
				var days=plyr.days;
				if(days)
				{
					days=days.toString().split(',');
					if(plyr.name==user.alias)
					{
						for(d in days)
						{
							calendar.highlights[days[d]]=true;
						}
					}
				}
				if(plyr.name==user.alias) plyr.laston=time();
				this.players[plyr.name]=new Player(plyr.name,plyr.points,days,plyr.laston);
			}
		}
		this.formatDate=function(timet)
		{
			var date=new Date(timet*1000);
			var m=date.getMonth()+1;
			var d=date.getDate();
			var y=date.getFullYear()-2000; //assuming no one goes back in time to 1999 to play this
			
			if(m<10) m="0"+m;
			if(d<10) d="0"+d;
			if(y<10) y="0"+y;
			
			return (m + "/" + d + "/" + y);
		}
		this.reset=function()
		{
			file_remove(this.file.name);
			this.init();
		}
		this.getAverage=function(alias)
		{
			var p=this.findUser(alias);
			var count=this.getDayCount(alias);
			var avg=p.points/count;
			if(avg>0) 
			{
				if(avg<10) return("0"+avg.toFixed(1));
				return avg.toFixed(1);
			}
			return 0;
		}
		this.getDayCount=function(alias)
		{
			var p=this.findUser(alias);
			var count=0;
			for(play in p.days) count++;
			return count;
		}
		this.storePlayer=function()
		{
			this.file.open('r+',true);
			this.file.iniSetObject(user.alias,this.players[user.alias]);
			this.file.close();
		}
		this.findUser=function(alias)
		{
			return this.players[alias];
		}
		this.init();
	}
	function Player(name,points,days,laston)
	{
		this.name=name?name:user.alias;
		this.points=points?points:0;
		this.days=days?days:[];
		this.laston=laston?laston:false;
	}
	function MonthData()
	{
		this.currentdate=new Date();
		this.currentmonth=this.currentdate.getMonth();
		this.games=[];
		this.winner=false;
		this.datafile=new File(gameroot + "month.ini");
		
		this.init=function()
		{
			this.loadMonth();
			this.loadGames();
		}
		this.loadMonth=function()
		{
			if(file_exists(this.datafile.name))
			{
				console.putmsg("\rPlease Wait. Loading games for this month...\r\n");
				var filedate=new Date(file_date(this.datafile.name)*1000);
				if(filedate.getMonth()!=this.currentmonth)
				{
					this.deleteOldGames();
					this.findRoundWinner();
					this.newMonth();
				}
				else
				{
					this.datafile.open('r',true);
					var name=this.datafile.iniGetValue(null,"winner");
					var points=this.datafile.iniGetValue(null,"points");
					this.datafile.close();
					if(points>0) 
					{
						this.winner={"name":name,"points":points};
					}
				}
			}
			else
			{
				console.putmsg("\rPlease Wait. Creating games for new month...\r\n");
				this.newMonth();
			}
		}
		this.deleteOldGames=function()
		{
			var games=directory(gameroot + "*.bog");
			for(g in games)
			{
				file_remove(games[g]);
			}
		}
		this.findRoundWinner=function()
		{
			for(p in players.players)
			{
				if(!this.winner) this.winner=players.players[p];
				else
				{
					if(players.players[p].points>this.winner.points) this.winner=players.players[p];
				}
			}
			this.datafile.open('w+',true);
			this.datafile.iniSetValue(null,"winner",this.winner.name);
			this.datafile.iniSetValue(null,"points",this.winner.points);
			this.datafile.close();
			players.reset();
		}
		this.newMonth=function()
		{
			this.games=[];
			file_touch(this.datafile.name);
			var numdays=this.currentdate.daysInMonth();
			for(var dn=1;dn<=numdays;dn++)
			{
				var game=new Game();
				game.init();
				this.games.push(game);
			}
		}
		this.loadGames=function()
		{
			var list=directory(gameroot + "*.bog");
			for(l in list)
			{
				var game=new Game();
				game.init(list[l]);
				this.games[game.gamenumber]=game;
			}
		}
		this.init();
	}
	function InfoBox(x,y)
	{
		this.x=x;
		this.y=y;
		this.timer;
		this.score;
		
		this.init=function(player)
		{
			this.score=new Score();
			this.timer=new Timer(65,12,"\1b\1h");
			this.timer.init(180);
		}
		this.cycle=function()
		{
			if(this.timer.countdown>0)
			{
				var current=time();
				var difference=current-this.timer.lastupdate;
				if(difference>0)
				{
					this.timer.countDown(current,difference);
					this.timer.redraw();
				}
				return true;
			}
			return false;
		}
		this.draw=function()
		{
			this.timer.redraw();
			console.gotoxy(64,19);
			console.putmsg("\1y\1h" + player.name.toUpperCase());
			console.gotoxy(73,21);
			console.putmsg("\1y\1h" + this.score.words);
			console.gotoxy(73,22);
			console.putmsg("\1y\1h" + this.score.points);
		}
	}
	function GameBoard(x,y)
	{ 
		this.graphic=new Graphic(80,24);
		this.graphic.load(gameroot + "board.bin");
		this.x=x;
		this.y=y;
		this.grid;
		
		this.init=function()
		{
			this.grid=[];
			for(x=0;x<5;x++)
			{
				this.grid.push(new Array(5));
			}
			this.scanGraphic();
		}
		this.scanGraphic=function()
		{
			for(x=0;x<this.graphic.data.length;x++) 
			{
				for(y=0;y<this.graphic.data[x].length;y++)
				{
					var location=this.graphic.data[x][y];
					if(location.ch=="@") 
					{
						var id=this.graphic.data[x+1][y].ch;
						if(id=="D")
						{
							
							this.dateline=new DateLine(x+1,y+1);
						}
						else
						{
							var posx=parseInt(this.graphic.data[x+1][y].ch,10);
							var posy=parseInt(this.graphic.data[x+2][y].ch,10);
							this.grid[posx][posy]=new LetterBox(x+1,y+1);
						}
						this.graphic.data[x][y].ch=" ";
						this.graphic.data[x+1][y].ch=" ";
						this.graphic.data[x+2][y].ch=" ";
					}
				}
			}
		}
		this.draw=function()
		{
			for(x=0;x<this.grid.length;x++)
			{
				for(y=0;y<this.grid[x].length;y++)
				{
					this.grid[x][y].draw();
				}
			}
		}
		this.drawSelected=function(valid)
		{
			for(x=0;x<this.grid.length;x++)
			{
				for(y=0;y<this.grid[x].length;y++)
				{
					if(this.grid[x][y].selected) this.grid[x][y].draw(true,valid);
				}
			}
		}
		this.clearSelected=function()
		{
			for(x=0;x<this.grid.length;x++)
			{
				for(y=0;y<this.grid[x].length;y++)
				{
					this.grid[x][y].selected=false;
					this.grid[x][y].draw();
				}
			}
		}
		this.init();
	}
	function LetterBox(x,y)
	{
		this.x=x;
		this.y=y;
		//this.letter;
		this.selected=false;
		
		this.draw=function(selected,valid)
		{
			var letter=(this.letter=="Q"?"Qu":this.letter);
			var oldattr=console.attributes;
			if(selected)
			{
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
			else 
			{
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
	}
	function DateLine(x,y)
	{
		this.x=x;
		this.y=y;
		//this.date;
		this.draw=function()
		{
			console.gotoxy(this.x,this.y);
			console.putmsg(this.date);
		}
	}
	function MessageLine(x,y)
	{
		this.x=x;
		this.y=y;
		this.draw=function(txt)
		{
			console.gotoxy(this.x,this.y);
			console.putmsg(txt);
		}
	}
	function InputLine(x,y)
	{
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
	function WordList(x,y)
	{
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
	function Score()
	{
		this.points=0;
		this.words=0;
	}
	function Game()
	{
		//this.file;
		//this.board;
		//this.gamenumber;
		//this.info;
		
		this.init=function(fileName)
		{
			this.board=new GameBoard(1,1);
			if(fileName)
			{
				this.file=new File(fileName);
				
				var f=file_getname(fileName);
				this.gamenumber=parseInt(f.substring(0,f.indexOf(".")),10);
				this.loadGame();
			}
			else
			{
				this.setFileInfo();
				this.generateBoard();
				this.storeGame();
			}
		}
		this.loadGame=function()
		{
			this.file.open('r',true);
			var grid=this.board.grid;
			for(x in grid)
			{
				for(y in grid[x])
				{
					grid[x][y].letter=this.file.readln();
				}
			}
			this.file.close();
		}
		this.storeGame=function()
		{
			this.file.open('w+');
			var grid=this.board.grid;
			for(x in grid)
			{
				for(y in grid[x])
				{
					this.file.writeln(grid[x][y].letter);
				}
			}
			this.file.close();
		}
		this.initDice=function()
		{
			var dice=new Array(25);
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
		this.generateBoard=function()
		{
			var dice=this.initDice();
			var shaken=new Array();
			for(var d=0;d<25;d++)
			{
				var spot=random(dice.length);
				shaken.push(dice.splice(spot,1).toString());
			}
			var die=0;
			for(var x=0;x<this.board.grid.length;x++)
			{
				for(var y=0;y<this.board.grid[x].length;y++)
				{
					var randletter=shaken[die].charAt(random(6));
					this.board.grid[x][y].letter=randletter;
					die++;
				}
			}
		}
		this.setFileInfo=function()
		{
			var gNum=1;
			if(gNum<10) gNum="0"+gNum;
			while(file_exists(gameroot+gNum+".bog"))
			{
				gNum++;
				if(gNum<10) gNum="0"+gNum;
			}
			var fName=gameroot + gNum + ".bog";
			this.file=new File(fName);
			this.gamenumber=parseInt(gNum,10);
		}
		this.draw=function()
		{
			this.board.graphic.draw(this.board.x,this.board.y);
			console.gotoxy(2,2);
			console.putmsg(centerString("\1c\1h" + calendar.date.getMonthName() + " " + this.gamenumber + ", " + calendar.date.getFullYear(),28));
			this.board.draw();
		}
	}
}

function getFiles()
{
	console.putmsg("\1nPlease wait. Synchronizing game files with hub...\r\n");
	stream.recvfile("*.bog");
	stream.recvfile("players.ini");
	stream.recvfile("month.ini");
}
function sendFiles()
{
	//stream.sendfile("*.bog");
	stream.sendfile("players.ini");
	//stream.sendfile("month.ini");		
}

boggle();