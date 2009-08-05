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
load("sbbsdefs.js")
load("logging.js");
load("helpfile.js");
load("funclib.js");
load("calendar.js");
load(gameroot + "timer.js");
 
var oldpass=console.ctrl_key_passthru;
//var gamelog=new Logger(gameroot,"boggle");
var gamelog=false;

function Boggle()
{
	var wordvalues=[];
	wordvalues[4]=1;
	wordvalues[5]=2;
	wordvalues[6]=3;
	wordvalues[7]=5;
	wordvalues[8]=11;
	
	var calendar;
	var month;
	var current;
	var lobby;
	var players;
	var player;
	var input;
	var info;
	var wordlist;
	var playinggame=false;
	
	var clearalert=false;
	
	function Init()
	{
		calendar=new Calendar(58,4,"\1y","\0012\1g\1h");
		players=new PlayerList();
		player=players.FindUser(user.alias);
		month=new MonthData();
		lobby=new Lobby(1,1);
		current=calendar.selected;
	}
	function SplashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	}
	function SplashExit()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		players.StorePlayer();
		exit(0);
	}
	function UpdateCalendar()
	{
		for(var d in player.days)
		{
			calendar.highlights[player.days[d]]=true;
		}
	}
	function ChangeDate()
	{
		ShowMessage("\1r\1hJump to which date?\1n\1r: ");
		var newdate=console.getnum(calendar.daysinmonth);
		if(newdate>0) 
		{
			calendar.selected=newdate;
			calendar.drawDay(current);
			current=newdate;
			calendar.drawDay(current,calendar.sel);
			ShowMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
			Log("Changed date to " + calendar.date.getMonthName() + " " + current);
		}
		ShowMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
		Log("Changed date to " + calendar.date.getMonthName() + " " + current);
	}
	function BrowseCalendar(k)
	{
		ShowMessage("\1r\1hUse Arrow keys to change date and [\1n\1rEnter\1h] to select");
		var newdate=calendar.SelectDay(k);
		if(newdate) 
		{
			current=newdate;
			ShowMessage("\1r\1hChanged date to " + calendar.date.getMonthName() + " " + current + ", " + calendar.date.getFullYear());
			Log("Changed date to " + calendar.date.getMonthName() + " " + current);
		}
	}
	function ShowLobby()
	{
		lobby.draw();
		console.gotoxy(58,2);
		console.putmsg(CenterString("\1c\1h" + month.currentdate.getMonthName() + "\1n\1c - \1c\1h" + month.currentdate.getFullYear(),21));
	}
	function ShowMessage(txt)
	{
		console.gotoxy(26,21);
		console.putmsg("\1-" + (txt?txt:""),P_SAVEATR);
		console.cleartoeol(ANSI_NORMAL);
	}
	function Redraw()
	{
		console.clear(ANSI_NORMAL);
		ShowLobby();
		ShowScores();
		calendar.draw();
	}
	function Main()
	{
		Redraw();
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
						BrowseCalendar(k);
						break;
					case "\x1b":	
					case "Q":
						SplashExit();
					case "P":
						for(var d in player.days)
						{
							if(player.days[d]==parseInt(current)) 
							{
								Log(user.alias + " has already played day: " + parseInt(current));
								ShowMessage("\1r\1hYou have already played this date");
								continue mainloop;
							}
						}
						PlayGame();
						UpdateCalendar();
						Redraw();
						break;
					case "R":
						Redraw();
						break;
					case "C":
						ChangeDate();
						break;
					default:
						break;
				}
			}
		}
	}
	function PlayGame()
	{
		console.clear(ANSI_NORMAL);
		Log(user.alias + " playing game " + current);
		var game=month.games[parseInt(current)];
		playinggame=true;
		
		function Play()
		{
			Redraw();
			while(1)
			{
				if(!info.Cycle()) EndGame();
				var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
				if(k)
				{
					switch(k.toUpperCase())
					{
						case "\x1b":	
							if(playinggame)
							{
								Alert("\1r\1hEnd this game? [N,y]: ");
								if(console.getkey().toUpperCase()=="N") break;
								input.Clear();
								EndGame();
							}
							return;
						case "\r":
						case "\n":
							if(input.buffer.length) SubmitWord();
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
							Redraw();
							break;
						default:
							if(playinggame)	input.Add(k);
							else Alert("\1r\1hThis game has ended");
							break;
					}
				}
			}
		}
		function Alert(msg)
		{
			input.draw(msg);
			clearalert=true;
		}
		function SubmitWord()
		{
			var word=truncsp(input.buffer);
			input.Clear();
			game.board.ClearSelected();
			
			if(word.length>3)
			{
				if(FindWord(word))
				{
					game.board.drawSelected(false);
					Log("Word already entered: " + word);
					Alert("\1r\1h Word already entered");
				}
				else if(CheckWord(word)) 
				{
					AddPoints(word);
					AddWord(word);
					info.draw();
				}
			}
			else
			{
				Log("Word too short: " + word);
				Alert("\1r\1h Word minimum: 4 letters");
			}
		}
		function FindWord(word)
		{
			for(w in wordlist.words)
			{
				if(word==wordlist.words[w]) return true;
			}
			return false;
		}
		function CheckWord(word)
		{
			if(word.length<4 || word.length>25 || word.indexOf(" ")>=0) return false;
			if(!ValidateWord(word))
			{
				Log("Word cannot be made: " + word);
				Alert("\1r\1h Cannot make word");
				return false;
			}
			
			var firstletter=word.charAt(0);
			var filename=gameroot + "dict/" + firstletter + ".dic";
			var dict=new File(filename);
			dict.open('r+',true);
			
			if(!ScanDictionary(word,0,dict.length,dict)) 
			{
				game.board.drawSelected(false);
				Log("Not a word: " + word);
				Alert("\1r\1h Word invalid");
				dict.close();
				return false;
			}
			
			dict.close();
			game.board.drawSelected(true);
			return true;
		}
		function ValidateWord(word)
		{
			Log("****validating word: " + word + "****");
			var match=false;
			var grid=game.board.grid;
			for(var x=0;x<grid.length;x++)
			{
				for(var y=0;y<grid[x].length;y++)
				{
					var position=grid[x][y];
					var letter=word.charAt(0).toUpperCase();
					Log("Checking letter for match: " + position.letter + " : " + letter);
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
			Log("Searching for match: " + string);
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
		function ScanDictionary(word,lower,upper,dict)
		{
			middle=parseInt(lower+((upper-lower)/2));
			dict.position=middle-25;
			var checkword=dict.readln();
			while(dict.position<=middle) checkword=dict.readln();
			var comparison=word.localeCompare(checkword);
			Log("upper: " + upper + " lower: " + lower + " pos: " + dict.position);
			Log("Comparing " + checkword + " to " + word + ": " + comparison);
			if(comparison<0) 
			{
				return ScanDictionary(word,lower,middle,dict);
			}
			if(dict.position>upper || upper-lower<2) return false;
			else if(comparison>0)
			{
				return ScanDictionary(word,middle,upper,dict);
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
			wordlist.Add(word);
		}
		function Init()
		{
			info=new InfoBox(62,1);
			input=new InputLine(32,22);
			wordlist=new WordList(31,4);
			info.Init(player);
		}
		function Redraw()
		{
			game.draw();
			info.draw();
			wordlist.draw();
		}
		function EndGame()
		{
			if(!playinggame) return;
			var p=players.players[user.alias];
			var points=info.score.points;
			p.points+=points;
			p.days.push(parseInt(current));
			players.StorePlayer();
			playinggame=false;
		}
		Init();
		Play();
	}
	SplashStart();
	Init();
	Main();
	
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
		this.Init=function()
		{
			this.file.open(file_exists(this.file.name)?'r+':'w+',true);
			if(!this.file.iniGetValue(user.alias,"name"))
			{
				Log("Creating new player data: " + user.alias);
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
				Log("Loading player data: " + plyr.name);
				this.players[plyr.name]=new Player(plyr.name,plyr.points,days,plyr.laston);
			}
		}
		this.FormatDate=function(timet)
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
		this.Reset=function()
		{
			file_remove(this.file.name);
			this.Init();
		}
		this.GetAverage=function(alias)
		{
			var p=this.FindUser(alias);
			var count=this.GetDayCount(alias);
			var avg=p.points/count;
			if(avg>0) 
			{
				if(avg<10) return("0"+avg.toFixed(1));
				return avg.toFixed(1);
			}
			return 0;
		}
		this.GetDayCount=function(alias)
		{
			var p=this.FindUser(alias);
			var count=0;
			for(play in p.days) count++;
			return count;
		}
		this.StorePlayer=function()
		{
			this.file.open('r+',true);
			this.file.iniSetObject(user.alias,this.players[user.alias]);
			this.file.close();
		}
		this.FindUser=function(alias)
		{
			return this.players[alias];
		}
		function Player(name,points,days,laston)
		{
			this.name=name?name:user.alias;
			this.points=points?points:0;
			this.days=days?days:[];
			this.laston=laston?laston:false;
		}
		this.Init();
	}
	function MonthData()
	{
		this.currentdate=new Date();
		this.currentmonth=this.currentdate.getMonth();
		this.games=[];
		this.winner=false;
		this.datafile=new File(gameroot + "month.ini");
		
		this.Init=function()
		{
			this.LoadMonth();
			this.LoadGames();
		}
		this.LoadMonth=function()
		{
			if(file_exists(this.datafile.name))
			{
				console.putmsg("\r\n\1r\1hPlease Wait. Loading games for this month...");
				var filedate=new Date(file_date(this.datafile.name)*1000);
				Log("Scanning data for month: " + filedate.getMonthName());
				if(filedate.getMonth()!=this.currentmonth)
				{
					Log("New month has started, resetting game data");
					this.DeleteOldGames();
					this.FindRoundWinner();
					this.NewMonth();
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
				Log("Creating new game data for month: " + this.currentdate.getMonthName());
				console.putmsg("\r\n\1r\1hPlease Wait. Creating games for new month...");
				this.NewMonth();
			}
		}
		this.DeleteOldGames=function()
		{
			Log("removing previous month's games");
			var games=directory(gameroot + "*.bog");
			for(g in games)
			{
				file_remove(games[g]);
			}
		}
		this.FindRoundWinner=function()
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
			players.Reset();
		}
		this.NewMonth=function()
		{
			this.games=[];
			file_touch(this.datafile.name);
			var numdays=this.currentdate.daysInMonth();
			for(var dn=1;dn<=numdays;dn++)
			{
				var game=new Game();
				game.Init();
				this.games.push(game);
			}
		}
		this.LoadGames=function()
		{
			var list=directory(gameroot + "*.bog");
			for(l in list)
			{
				Log("Loading file: " + list[l]);
				var game=new Game();
				game.Init(list[l]);
				this.games[game.gamenumber]=game;
			}
		}
		this.Init();
	}
	function InfoBox(x,y)
	{
		this.x=x;
		this.y=y;
		this.timer;
		this.score;
		
		this.Init=function(player)
		{
			this.score=new Score();
			this.timer=new Timer(65,12,"\1b\1h");
			this.timer.Init(180);
		}
		this.Cycle=function()
		{
			if(this.timer.countdown>0)
			{
				var current=time();
				var difference=current-this.timer.lastupdate;
				if(difference>0)
				{
					this.timer.Countdown(current,difference);
					this.timer.Redraw();
				}
				return true;
			}
			return false;
		}
		this.draw=function()
		{
			this.timer.Redraw();
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
		
		this.Init=function()
		{
			this.grid=[];
			for(x=0;x<5;x++)
			{
				this.grid.push(new Array(5));
			}
			this.ScanGraphic();
		}
		this.ScanGraphic=function()
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
							var posx=parseInt(this.graphic.data[x+1][y].ch);
							var posy=parseInt(this.graphic.data[x+2][y].ch);
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
		this.ClearSelected=function()
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
		function LetterBox(x,y)
		{
			this.x=x;
			this.y=y;
			this.letter;
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
					console.putmsg(CenterString(letter,5),P_SAVEATR);
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
					console.putmsg(CenterString(letter,5),P_SAVEATR);
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
			this.date;
			this.Draw=function()
			{
				console.gotoxy(this.x,this.y);
				console.putmsg(this.date);
			}
		}
		this.Init();
	}
	function MessageLine(x,y)
	{
		this.x=x;
		this.y=y;
		this.Draw=function(txt)
		{
			console.gotoxy(this.x,this.y);
			console.putmsg(txt)
		}
	}
	function InputLine(x,y)
	{
		this.x=x;
		this.y=y;
		this.buffer="";
		this.Add=function(letter)
		{
			if(clearalert) 
			{
				this.Clear();
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
		this.Backspace=function()
		{
			this.buffer=this.buffer.substring(0,this.buffer.length-2);
			this.draw();
			console.putmsg(" ");
		}
		this.Clear=function()
		{
			this.buffer="";
			console.gotoxy(this.x,this.y);
			console.write("                              ");
		}
		this.draw=function(text)
		{
			console.gotoxy(this.x,this.y);
			console.putmsg(PrintPadded("\1n" + text,30));
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
		this.Add=function(word)
		{
			this.words.push(word);
			this.draw();
		}
	}
	function ShowScores()
	{
		var posx=3;
		var posy=6;
		var index=0;
		
		var scores=SortScores();
		for(var s in scores)
		{
			var score=scores[s];
			var plays=players.GetDayCount(score.name);
			if(plays>0)
			{
				if(score.name==user.alias) console.attributes=LIGHTGREEN;
				else console.attributes=GREEN;
				console.gotoxy(posx,posy+index);
				console.putmsg(score.name,P_SAVEATR);
				console.right(23-score.name.length);
				console.putmsg(PrintPadded(score.points,5,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(players.GetAverage(score.name),P_SAVEATR);
				console.right(3);
				console.putmsg(PrintPadded(plays,4,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(PrintPadded(players.FormatDate(score.laston),8,undefined,"right"),P_SAVEATR);
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
	function SortScores()
	{
		var sorted=[];
		for(var p in players.players)
		{
			var s=players.players[p];
			var plays=players.GetDayCount(s.name);
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
	function Score()
	{
		this.points=0;
		this.words=0;
	}
	function Game()
	{
		this.file;
		this.board;
		this.gamenumber;
		this.info;
		
		this.Init=function(fileName)
		{
			this.board=new GameBoard(1,1);
			if(fileName)
			{
				this.file=new File(fileName);
				
				var f=file_getname(fileName);
				this.gamenumber=parseInt(f.substring(0,f.indexOf(".")),10);
				this.LoadGame();
			}
			else
			{
				this.SetFileInfo();
				this.GenerateBoard();
				this.StoreGame();
			}
			Log("Game " + this.gamenumber + " initialized");
		}
		this.LoadGame=function()
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
		this.StoreGame=function()
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
		this.InitDice=function()
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
		this.GenerateBoard=function()
		{
			var dice=this.InitDice();
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
		this.SetFileInfo=function()
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
			this.gamenumber=parseInt(gNum);
		}
		this.draw=function()
		{
			this.board.graphic.draw(this.board.x,this.board.y);
			console.gotoxy(2,2);
			console.putmsg(CenterString("\1c\1h" + calendar.date.getMonthName() + " " + this.gamenumber + ", " + calendar.date.getFullYear(),28));
			this.board.draw();
		}
	}
}
function Log(txt)
{
	if(gamelog) gamelog.Log(txt);
}

Boggle();